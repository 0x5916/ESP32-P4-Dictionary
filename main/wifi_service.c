#include "wifi_service.h"
#include "sdkconfig.h"

#define CONFIG_ESP_WIFI_ENABLED true

#if defined(CONFIG_ESP_WIFI_ENABLED) && CONFIG_ESP_WIFI_ENABLED

#include <string.h>

#include "esp_event.h"
#include <string.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define WIFI_SERVICE_SCAN_MAX_RESULTS 20
#define WIFI_SERVICE_NVS_NAMESPACE "wifi_cfg"
#define WIFI_SERVICE_NVS_KEY_SSID "ssid"
#define WIFI_SERVICE_NVS_KEY_PASS "pass"

static const char *TAG = "wifi_service";

static bool s_initialized;
static bool s_enabled;
static bool s_has_credentials;
static bool s_scan_running;
static bool s_sta_started;
static wifi_service_state_t s_state;
static wifi_service_scan_result_t s_scan_results[WIFI_SERVICE_SCAN_MAX_RESULTS];
static size_t s_scan_count;
static wifi_service_callbacks_t s_callbacks;
static SemaphoreHandle_t s_state_mutex;
static esp_netif_t *s_wifi_netif;
static esp_event_handler_instance_t s_wifi_event_instance;
static esp_event_handler_instance_t s_ip_event_instance;

static void wifi_service_copy_str(char *dest, size_t dest_len, const char *src)
{
    if (!dest || dest_len == 0) {
        return;
    }

    if (!src) {
        dest[0] = '\0';
        return;
    }

    size_t src_len = strlen(src);
    size_t copy_len = src_len < (dest_len - 1) ? src_len : (dest_len - 1);
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
}

static void wifi_service_lock(void)
{
	if (s_state_mutex) {
		xSemaphoreTake(s_state_mutex, portMAX_DELAY);
	}
}

static void wifi_service_unlock(void)
{
	if (s_state_mutex) {
		xSemaphoreGive(s_state_mutex);
	}
}

static void wifi_service_notify_status(void)
{
	wifi_service_callbacks_t callbacks = s_callbacks;
	if (!callbacks.on_status) {
		return;
	}

	wifi_service_state_t snapshot = {0};
	if (wifi_service_get_state(&snapshot) == ESP_OK) {
		callbacks.on_status(&snapshot, callbacks.user_ctx);
	}
}

static void wifi_service_notify_scan_state(bool scanning)
{
	wifi_service_callbacks_t callbacks = s_callbacks;
	if (callbacks.on_scan_state) {
		callbacks.on_scan_state(scanning, callbacks.user_ctx);
	}
}

static void wifi_service_notify_scan_done(void)
{
	wifi_service_callbacks_t callbacks = s_callbacks;
	if (callbacks.on_scan_done) {
		callbacks.on_scan_done(s_scan_results, s_scan_count, callbacks.user_ctx);
	}
}

static void wifi_service_set_status(wifi_service_status_t status,
									const char *ssid,
									const esp_ip4_addr_t *ip)
{
	wifi_service_lock();
	s_state.status = status;
	if (ssid) {
		wifi_service_copy_str(s_state.ssid, sizeof(s_state.ssid), ssid);
	}
	if (ip) {
		s_state.ip_v4 = ip->addr;
	} else if (status != WIFI_SERVICE_STATUS_GOT_IP) {
		s_state.ip_v4 = 0;
	}
	wifi_service_unlock();

	wifi_service_notify_status();
}

static esp_err_t wifi_service_save_credentials(const char *ssid, const char *password)
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open(WIFI_SERVICE_NVS_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		return err;
	}

	err = nvs_set_str(handle, WIFI_SERVICE_NVS_KEY_SSID, ssid ? ssid : "");
	if (err == ESP_OK) {
		err = nvs_set_str(handle, WIFI_SERVICE_NVS_KEY_PASS, password ? password : "");
	}
	if (err == ESP_OK) {
		err = nvs_commit(handle);
	}

	nvs_close(handle);
	return err;
}

static esp_err_t wifi_service_load_credentials(char *ssid,
											   size_t ssid_len,
											   char *password,
											   size_t password_len,
											   bool *found)
{
	if (!ssid || !password || !found) {
		return ESP_ERR_INVALID_ARG;
	}

	*found = false;
	ssid[0] = '\0';
	password[0] = '\0';

	nvs_handle_t handle;
	esp_err_t err = nvs_open(WIFI_SERVICE_NVS_NAMESPACE, NVS_READONLY, &handle);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
		return ESP_OK;
	}
	if (err != ESP_OK) {
		return err;
	}

	size_t ssid_size = ssid_len;
	err = nvs_get_str(handle, WIFI_SERVICE_NVS_KEY_SSID, ssid, &ssid_size);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
		nvs_close(handle);
		return ESP_OK;
	}
	if (err != ESP_OK) {
		nvs_close(handle);
		return err;
	}

	size_t pass_size = password_len;
	esp_err_t pass_err = nvs_get_str(handle, WIFI_SERVICE_NVS_KEY_PASS, password, &pass_size);
	if (pass_err == ESP_ERR_NVS_NOT_FOUND) {
		password[0] = '\0';
	} else if (pass_err != ESP_OK) {
		nvs_close(handle);
		return pass_err;
	}

	nvs_close(handle);
	*found = true;
	return ESP_OK;
}

static void wifi_service_handle_scan_done(void)
{
	uint16_t ap_count = 0;
	esp_err_t err = esp_wifi_scan_get_ap_num(&ap_count);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Failed to get scan count: %s", esp_err_to_name(err));
		ap_count = 0;
	}

	uint16_t fetch_count = ap_count;
	if (fetch_count > WIFI_SERVICE_SCAN_MAX_RESULTS) {
		fetch_count = WIFI_SERVICE_SCAN_MAX_RESULTS;
	}

	wifi_ap_record_t ap_records[WIFI_SERVICE_SCAN_MAX_RESULTS] = {0};
	if (fetch_count > 0) {
		err = esp_wifi_scan_get_ap_records(&fetch_count, ap_records);
		if (err != ESP_OK) {
			ESP_LOGW(TAG, "Failed to get scan records: %s", esp_err_to_name(err));
			fetch_count = 0;
		}
	}

	s_scan_count = 0;
	for (uint16_t i = 0; i < fetch_count && s_scan_count < WIFI_SERVICE_SCAN_MAX_RESULTS; ++i) {
		const wifi_ap_record_t *record = &ap_records[i];
		if (record->ssid[0] == '\0') {
			continue;
		}

		wifi_service_scan_result_t *dest = &s_scan_results[s_scan_count++];
		wifi_service_copy_str(dest->ssid, sizeof(dest->ssid), (const char *)record->ssid);
		dest->rssi = record->rssi;
		dest->authmode = record->authmode;
	}

	s_scan_running = false;
	wifi_service_notify_scan_state(false);
	wifi_service_notify_scan_done();
}

static void wifi_service_event_handler(void *arg,
									   esp_event_base_t event_base,
									   int32_t event_id,
									   void *event_data)
{
	(void)arg;
	if (event_base == WIFI_EVENT) {
		switch (event_id) {
		case WIFI_EVENT_STA_START:
			s_sta_started = true;
			if (s_has_credentials) {
				wifi_service_set_status(WIFI_SERVICE_STATUS_CONNECTING, s_state.ssid, NULL);
				esp_wifi_connect();
			} else {
				wifi_service_set_status(WIFI_SERVICE_STATUS_DISCONNECTED, s_state.ssid, NULL);
			}
			break;
		case WIFI_EVENT_STA_CONNECTED:
			wifi_service_set_status(WIFI_SERVICE_STATUS_CONNECTED, s_state.ssid, NULL);
			break;
		case WIFI_EVENT_STA_DISCONNECTED:
			wifi_service_set_status(WIFI_SERVICE_STATUS_DISCONNECTED, s_state.ssid, NULL);
			if (s_enabled && s_has_credentials) {
				esp_wifi_connect();
			}
			break;
		case WIFI_EVENT_STA_STOP:
			wifi_service_set_status(WIFI_SERVICE_STATUS_STOPPED, s_state.ssid, NULL);
			break;
		case WIFI_EVENT_SCAN_DONE:
			wifi_service_handle_scan_done();
			break;
		default:
			break;
		}
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		wifi_service_set_status(WIFI_SERVICE_STATUS_GOT_IP, s_state.ssid, &event->ip_info.ip);
	}
}

esp_err_t wifi_service_init(void)
{
	if (s_initialized) {
		return ESP_OK;
	}

	s_state_mutex = xSemaphoreCreateMutex();
	if (!s_state_mutex) {
		return ESP_ERR_NO_MEM;
	}

	s_state.status = WIFI_SERVICE_STATUS_STOPPED;
	s_state.ssid[0] = '\0';
	s_state.ip_v4 = 0;

	esp_err_t err = esp_netif_init();
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		return err;
	}

	err = esp_event_loop_create_default();
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		return err;
	}

	if (!s_wifi_netif) {
		s_wifi_netif = esp_netif_create_default_wifi_sta();
		if (!s_wifi_netif) {
			return ESP_FAIL;
		}
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	err = esp_wifi_init(&cfg);
	if (err != ESP_OK) {
		return err;
	}

	err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
	if (err != ESP_OK) {
		return err;
	}

	err = esp_event_handler_instance_register(WIFI_EVENT,
											   ESP_EVENT_ANY_ID,
											   &wifi_service_event_handler,
											   NULL,
											   &s_wifi_event_instance);
	if (err != ESP_OK) {
		return err;
	}

	err = esp_event_handler_instance_register(IP_EVENT,
											   IP_EVENT_STA_GOT_IP,
											   &wifi_service_event_handler,
											   NULL,
											   &s_ip_event_instance);
	if (err != ESP_OK) {
		return err;
	}

	err = esp_wifi_set_mode(WIFI_MODE_STA);
	if (err != ESP_OK) {
		return err;
	}

	char ssid[33] = {0};
	char password[65] = {0};
	bool found = false;
	esp_err_t load_err = wifi_service_load_credentials(ssid, sizeof(ssid), password, sizeof(password), &found);
	if (load_err != ESP_OK) {
		ESP_LOGW(TAG, "Failed to load credentials: %s", esp_err_to_name(load_err));
	}

	if (found && ssid[0] != '\0') {
		s_has_credentials = true;
		wifi_service_copy_str(s_state.ssid, sizeof(s_state.ssid), ssid);

		wifi_config_t config = {0};
		wifi_service_copy_str((char *)config.sta.ssid, sizeof(config.sta.ssid), ssid);
		wifi_service_copy_str((char *)config.sta.password, sizeof(config.sta.password), password);
		config.sta.threshold.authmode = WIFI_AUTH_OPEN;
		config.sta.pmf_cfg.capable = true;
		config.sta.pmf_cfg.required = false;

		esp_err_t cfg_err = esp_wifi_set_config(WIFI_IF_STA, &config);
		if (cfg_err != ESP_OK) {
			ESP_LOGW(TAG, "Failed to apply saved WiFi config: %s", esp_err_to_name(cfg_err));
		}
	}

	err = esp_wifi_start();
	if (err != ESP_OK) {
		return err;
	}
	s_enabled = true;

	s_initialized = true;

	if (!s_has_credentials) {
		wifi_service_set_status(WIFI_SERVICE_STATUS_DISCONNECTED, s_state.ssid, NULL);
	}

	return ESP_OK;
}

esp_err_t wifi_service_register_callbacks(const wifi_service_callbacks_t *callbacks)
{
	if (callbacks) {
		s_callbacks = *callbacks;
	} else {
		memset(&s_callbacks, 0, sizeof(s_callbacks));
	}

	wifi_service_notify_status();
	wifi_service_notify_scan_state(s_scan_running);
	if (s_scan_count > 0) {
		wifi_service_notify_scan_done();
	}

	return ESP_OK;
}

esp_err_t wifi_service_set_enabled(bool enabled)
{
	if (!s_initialized) {
		return ESP_ERR_INVALID_STATE;
	}
	if (enabled == s_enabled) {
		return ESP_OK;
	}

	if (!enabled) {
		esp_wifi_disconnect();
		esp_err_t err = esp_wifi_stop();
		s_enabled = false;
		wifi_service_set_status(WIFI_SERVICE_STATUS_STOPPED, s_state.ssid, NULL);
		return err;
	}

	esp_err_t err = esp_wifi_start();
	if (err == ESP_OK) {
		s_enabled = true;
		wifi_service_set_status(WIFI_SERVICE_STATUS_DISCONNECTED, s_state.ssid, NULL);
	}

	return err;
}

bool wifi_service_is_enabled(void)
{
	return s_enabled;
}

esp_err_t wifi_service_connect(const char *ssid, const char *password)
{
	if (!ssid || ssid[0] == '\0') {
		return ESP_ERR_INVALID_ARG;
	}
	if (!s_initialized) {
		return ESP_ERR_INVALID_STATE;
	}

	wifi_config_t config = {0};
	wifi_service_copy_str((char *)config.sta.ssid, sizeof(config.sta.ssid), ssid);
	if (password) {
		wifi_service_copy_str((char *)config.sta.password, sizeof(config.sta.password), password);
	} else {
		config.sta.password[0] = '\0';
	}
	config.sta.threshold.authmode = WIFI_AUTH_OPEN;
	config.sta.pmf_cfg.capable = true;
	config.sta.pmf_cfg.required = false;

	esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);
	if (err != ESP_OK) {
		return err;
	}

	esp_err_t save_err = wifi_service_save_credentials(ssid, password ? password : "");
	if (save_err == ESP_OK) {
		s_has_credentials = true;
		wifi_service_copy_str(s_state.ssid, sizeof(s_state.ssid), ssid);
	} else {
		ESP_LOGW(TAG, "Failed to save credentials: %s", esp_err_to_name(save_err));
	}

	wifi_service_set_status(WIFI_SERVICE_STATUS_CONNECTING, ssid, NULL);

	if (!s_enabled) {
		err = esp_wifi_start();
		if (err == ESP_OK) {
			s_enabled = true;
			return ESP_OK;
		}
		return err;
	}

	return esp_wifi_connect();
}

esp_err_t wifi_service_connect_saved(void)
{
	char ssid[33] = {0};
	char password[65] = {0};
	bool found = false;
	esp_err_t err = wifi_service_load_credentials(ssid, sizeof(ssid), password, sizeof(password), &found);
	if (err != ESP_OK) {
		return err;
	}
	if (!found || ssid[0] == '\0') {
		return ESP_ERR_INVALID_STATE;
	}

	return wifi_service_connect(ssid, password);
}

esp_err_t wifi_service_disconnect(void)
{
	if (!s_initialized) {
		return ESP_ERR_INVALID_STATE;
	}

	esp_err_t err = esp_wifi_disconnect();
	wifi_service_set_status(WIFI_SERVICE_STATUS_DISCONNECTED, s_state.ssid, NULL);
	return err;
}

esp_err_t wifi_service_scan_start(void)
{

	if (!s_initialized || !s_enabled || !s_sta_started) {
		return ESP_ERR_INVALID_STATE;
	}
	if (s_scan_running) {
		return ESP_ERR_INVALID_STATE;
	}

	wifi_scan_config_t scan_cfg = {0};
	scan_cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
	scan_cfg.scan_time.active.min = 100;
	scan_cfg.scan_time.active.max = 300;
	scan_cfg.show_hidden = false;

	s_scan_running = true;
	wifi_service_notify_scan_state(true);

	esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
	if (err != ESP_OK) {
		s_scan_running = false;
		wifi_service_notify_scan_state(false);
	}

	return err;
}

bool wifi_service_has_credentials(void)
{
	return s_has_credentials;
}

esp_err_t wifi_service_forget_credentials(void)
{
	nvs_handle_t handle;
	esp_err_t err = nvs_open(WIFI_SERVICE_NVS_NAMESPACE, NVS_READWRITE, &handle);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
		s_has_credentials = false;
		s_state.ssid[0] = '\0';
		return ESP_OK;
	}
	if (err != ESP_OK) {
		return err;
	}

	nvs_erase_key(handle, WIFI_SERVICE_NVS_KEY_SSID);
	nvs_erase_key(handle, WIFI_SERVICE_NVS_KEY_PASS);
	err = nvs_commit(handle);
	nvs_close(handle);

	s_has_credentials = false;
	s_state.ssid[0] = '\0';
	return err;
}

esp_err_t wifi_service_get_state(wifi_service_state_t *out_state)
{
	if (!out_state) {
		return ESP_ERR_INVALID_ARG;
	}

	wifi_service_lock();
	*out_state = s_state;
	wifi_service_unlock();

	return ESP_OK;
}

#else

#include "esp_log.h"

static const char *TAG = "wifi_service";
static wifi_service_state_t s_state = {
	.status = WIFI_SERVICE_STATUS_STOPPED,
	.ssid = {0},
	.ip_v4 = 0
};
static wifi_service_callbacks_t s_callbacks;

static void wifi_service_notify_status(void)
{
	if (s_callbacks.on_status) {
		s_callbacks.on_status(&s_state, s_callbacks.user_ctx);
	}
}

static void wifi_service_notify_scan_state(bool scanning)
{
	if (s_callbacks.on_scan_state) {
		s_callbacks.on_scan_state(scanning, s_callbacks.user_ctx);
	}
}

static void wifi_service_notify_scan_done(void)
{
	if (s_callbacks.on_scan_done) {
		s_callbacks.on_scan_done(NULL, 0, s_callbacks.user_ctx);
	}
}

esp_err_t wifi_service_init(void)
{
	ESP_LOGW(TAG, "WiFi is not enabled in sdkconfig for this target");
	wifi_service_notify_status();
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t wifi_service_register_callbacks(const wifi_service_callbacks_t *callbacks)
{
	if (callbacks) {
		s_callbacks = *callbacks;
	} else {
		memset(&s_callbacks, 0, sizeof(s_callbacks));
	}

	wifi_service_notify_status();
	wifi_service_notify_scan_state(false);
	wifi_service_notify_scan_done();
	return ESP_OK;
}

esp_err_t wifi_service_set_enabled(bool enabled)
{
	(void)enabled;
	s_state.status = WIFI_SERVICE_STATUS_STOPPED;
	wifi_service_notify_status();
	return ESP_ERR_NOT_SUPPORTED;
}

bool wifi_service_is_enabled(void)
{
	return false;
}

esp_err_t wifi_service_connect(const char *ssid, const char *password)
{
	(void)ssid;
	(void)password;
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t wifi_service_connect_saved(void)
{
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t wifi_service_disconnect(void)
{
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t wifi_service_scan_start(void)
{
	return ESP_ERR_NOT_SUPPORTED;
}

bool wifi_service_has_credentials(void)
{
	return false;
}

esp_err_t wifi_service_forget_credentials(void)
{
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t wifi_service_get_state(wifi_service_state_t *out_state)
{
	if (!out_state) {
		return ESP_ERR_INVALID_ARG;
	}
	*out_state = s_state;
	return ESP_OK;
}

#endif