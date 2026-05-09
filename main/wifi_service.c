#include "wifi_service.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "settings_service.h"
#include "clock_service.h"
#include "geolocation_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "wifi_service";

#define WIFI_SERVICE_NVS_NAMESPACE "wifi_cfg"
#define WIFI_SERVICE_NVS_KEY_SSID  "ssid"
#define WIFI_SERVICE_NVS_KEY_PASS  "pass"

// ---- Internal state ----
static wifi_state_t s_state               = WIFI_STATE_DISABLED;
static bool         s_initialized         = false;
static bool         s_has_credentials     = false;
static bool         s_auto_reconnect      = false;
static bool         s_geolocation_started = false;
static char         s_connected_ssid[33]   = {0};
static char         s_saved_ssid[33]       = {0};
static char         s_saved_password[65]    = {0};

static wifi_scan_done_cb_t    s_scan_done_cb    = NULL;
static wifi_state_changed_cb_t s_state_changed_cb = NULL;

// ---- Background task for geolocation ----
static void geolocation_task(void *pvParam)
{
    (void)pvParam;
    ESP_LOGD(TAG, "[TASK] Geolocation detection task starting");
    geolocation_service_detect_timezone();
    vTaskDelete(NULL);
}

static void wifi_service_request_geolocation(void)
{
    if (s_geolocation_started) {
        return;
    }

    s_geolocation_started = true;
    xTaskCreatePinnedToCore(
        geolocation_task,
        "geolocation",
        8192,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL,
        1
    );
}

// ---- Helpers ----

static void set_state(wifi_state_t new_state, const char *ssid)
{
    s_state = new_state;
    
    // Start time sync when connected, stop when disconnected
    if (new_state == WIFI_STATE_CONNECTED) {
        clock_service_start_sntp();
    } else if (new_state == WIFI_STATE_DISCONNECTED || 
               new_state == WIFI_STATE_DISABLED ||
               new_state == WIFI_STATE_CONNECT_FAILED) {
        clock_service_stop_sntp();
        s_geolocation_started = false;
    }
    
    if (s_state_changed_cb) {
        s_state_changed_cb(new_state, ssid);
    }
}

static void wifi_service_clear_saved_credentials(void)
{
    memset(s_saved_ssid, 0, sizeof(s_saved_ssid));
    memset(s_saved_password, 0, sizeof(s_saved_password));
    s_has_credentials = false;
}

static esp_err_t wifi_service_load_saved_credentials(char *ssid, size_t ssid_len,
                                                     char *password, size_t password_len,
                                                     bool *found)
{
    if (!ssid || !password || !found || ssid_len == 0 || password_len == 0) {
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
    *found = (ssid[0] != '\0');
    return ESP_OK;
}

static esp_err_t wifi_service_save_credentials(const char *ssid, const char *password)
{
    if (!ssid || ssid[0] == '\0' || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_SERVICE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, WIFI_SERVICE_NVS_KEY_SSID, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, WIFI_SERVICE_NVS_KEY_PASS, password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        snprintf(s_saved_ssid, sizeof(s_saved_ssid), "%s", ssid);
        snprintf(s_saved_password, sizeof(s_saved_password), "%s", password);
        s_has_credentials = true;
    }

    return err;
}

static esp_err_t wifi_service_apply_saved_credentials(void)
{
    if (!s_has_credentials || s_saved_ssid[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    wifi_config_t wifi_cfg;
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    snprintf((char *)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid), "%s", s_saved_ssid);
    snprintf((char *)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), "%s", s_saved_password);
    wifi_cfg.sta.threshold.authmode = (s_saved_password[0] != '\0') ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[CONNECT] esp_wifi_set_config(saved) failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_connect();
    if (err == ESP_OK) {
        set_state(WIFI_STATE_CONNECTING, s_saved_ssid);
        s_auto_reconnect = true;
    }
    return err;
}

static void wifi_service_request_saved_reconnect(void)
{
    if (!s_auto_reconnect || !s_has_credentials || s_saved_ssid[0] == '\0') {
        return;
    }

    ESP_LOGI(TAG, "[EVT] Reconnecting to saved SSID '%s'", s_saved_ssid);
    if (esp_wifi_connect() == ESP_OK) {
        set_state(WIFI_STATE_CONNECTING, s_saved_ssid);
    }
}

// ---- ESP event handler ----

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGD(TAG, "[EVT] STA_START");
                wifi_service_request_saved_reconnect();
                break;

            case WIFI_EVENT_SCAN_DONE: {
                ESP_LOGI(TAG, "[EVT] SCAN_DONE");

                uint16_t ap_count = WIFI_SERVICE_MAX_APS;

                // ✅ FIXED: static so it lives in BSS, not on sys_evt's stack
                static wifi_ap_record_t ap_records[WIFI_SERVICE_MAX_APS];
                memset(ap_records, 0, sizeof(ap_records));

                esp_err_t err = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "[SCAN] esp_wifi_scan_get_ap_records failed: %s", esp_err_to_name(err));
                    set_state(WIFI_STATE_IDLE, NULL);
                    break;
                }

                ESP_LOGI(TAG, "[SCAN] Found %u APs", ap_count);

                static wifi_ap_info_t results[WIFI_SERVICE_MAX_APS];
                for (uint16_t i = 0; i < ap_count; i++) {
                    strncpy(results[i].ssid, (char *)ap_records[i].ssid, sizeof(results[i].ssid) - 1);
                    results[i].ssid[sizeof(results[i].ssid) - 1] = '\0';
                    results[i].rssi             = ap_records[i].rssi;
                    results[i].auth_mode        = ap_records[i].authmode;
                    results[i].requires_password = (ap_records[i].authmode != WIFI_AUTH_OPEN);
                }

                set_state(WIFI_STATE_IDLE, NULL);

                if (s_scan_done_cb) {
                    s_scan_done_cb(results, ap_count);
                }
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
                ESP_LOGW(TAG, "[EVT] STA_DISCONNECTED reason=%u", disc ? disc->reason : 0);
                if (s_auto_reconnect && s_has_credentials) {
                    wifi_service_request_saved_reconnect();
                    break;
                }

                memset(s_connected_ssid, 0, sizeof(s_connected_ssid));
                set_state(WIFI_STATE_DISCONNECTED, NULL);
                break;
            }

            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t *conn = (wifi_event_sta_connected_t *)event_data;
                if (conn) {
                    memcpy(s_connected_ssid, conn->ssid,
                           conn->ssid_len < sizeof(s_connected_ssid) - 1
                               ? conn->ssid_len
                               : sizeof(s_connected_ssid) - 1);
                    s_connected_ssid[conn->ssid_len < sizeof(s_connected_ssid) - 1
                                         ? conn->ssid_len
                                         : sizeof(s_connected_ssid) - 1] = '\0';
                }
                ESP_LOGI(TAG, "[EVT] STA_CONNECTED ssid='%s'", s_connected_ssid);
                set_state(WIFI_STATE_CONNECTED, s_connected_ssid);
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "[EVT] GOT_IP " IPSTR, IP2STR(&evt->ip_info.ip));
            set_state(WIFI_STATE_CONNECTED, s_connected_ssid);
            wifi_service_request_geolocation();
        }
    }
}

// ---- Lifecycle ----

void wifi_service_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "[INIT] Already initialized");
        return;
    }

    ESP_LOGI(TAG, "[INIT] wifi_service_init()");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,  wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    bool found = false;
    if (wifi_service_load_saved_credentials(s_saved_ssid, sizeof(s_saved_ssid),
                                            s_saved_password, sizeof(s_saved_password), &found) == ESP_OK && found) {
        s_has_credentials = true;
        ESP_LOGI(TAG, "[INIT] Loaded saved WiFi credentials for '%s'", s_saved_ssid);
    } else {
        wifi_service_clear_saved_credentials();
    }

    bool wifi_on = true;
    settings_get_bool("wifi_default_on", true, &wifi_on);

    if (wifi_on) {
        ESP_ERROR_CHECK(esp_wifi_start());
        if (s_has_credentials) {
            s_auto_reconnect = true;
            ESP_LOGI(TAG, "[INIT] WiFi started, waiting for saved SSID reconnect");
        } else {
            s_state = WIFI_STATE_IDLE;
            s_auto_reconnect = false;
            ESP_LOGI(TAG, "[INIT] WiFi started (default on)");
        }
    } else {
        s_state = WIFI_STATE_DISABLED;
        s_auto_reconnect = false;
        ESP_LOGI(TAG, "[INIT] WiFi disabled by settings");
    }

    s_initialized = true;
    ESP_LOGI(TAG, "[INIT] wifi_service_init() complete");
}

void wifi_service_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    esp_wifi_stop();
    esp_wifi_deinit();
    s_initialized = false;
    s_state = WIFI_STATE_DISABLED;
    s_auto_reconnect = false;
    ESP_LOGI(TAG, "[DEINIT] wifi_service_deinit() complete");
}

// ---- Control ----

esp_err_t wifi_service_enable(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_state != WIFI_STATE_DISABLED) {
        ESP_LOGD(TAG, "[CTRL] Already enabled");
        return ESP_OK;
    }

    esp_err_t err = esp_wifi_start();
    if (err == ESP_OK) {
        if (s_has_credentials) {
            s_auto_reconnect = true;
            ESP_LOGI(TAG, "[CTRL] WiFi enabled, waiting for saved SSID reconnect");
        } else {
            s_auto_reconnect = false;
            set_state(WIFI_STATE_IDLE, NULL);
        }
        ESP_LOGI(TAG, "[CTRL] WiFi enabled");
    } else {
        ESP_LOGE(TAG, "[CTRL] esp_wifi_start failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t wifi_service_disable(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_auto_reconnect = false;
    esp_wifi_disconnect();
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK) {
        memset(s_connected_ssid, 0, sizeof(s_connected_ssid));
        set_state(WIFI_STATE_DISABLED, NULL);
        ESP_LOGI(TAG, "[CTRL] WiFi disabled");
    } else {
        ESP_LOGE(TAG, "[CTRL] esp_wifi_stop failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t wifi_service_scan(void)
{
    if (!s_initialized || s_state == WIFI_STATE_DISABLED) {
        ESP_LOGW(TAG, "[SCAN] WiFi not enabled");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_state == WIFI_STATE_SCANNING) {
        ESP_LOGD(TAG, "[SCAN] Already scanning");
        return ESP_OK;
    }

    wifi_scan_config_t scan_cfg = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,
        .show_hidden = false,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err == ESP_OK) {
        set_state(WIFI_STATE_SCANNING, NULL);
        ESP_LOGI(TAG, "[SCAN] Scan started");
    } else {
        ESP_LOGE(TAG, "[SCAN] esp_wifi_scan_start failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t wifi_service_connect(const char *ssid, const char *password)
{
    if (!s_initialized || s_state == WIFI_STATE_DISABLED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!ssid) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "[CONNECT] Connecting to '%s'", ssid);

    wifi_config_t wifi_cfg;
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    snprintf((char *)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid), "%s", ssid);
    if (password && password[0] != '\0') {
        snprintf((char *)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), "%s", password);
    }
    wifi_cfg.sta.threshold.authmode = (password && password[0] != '\0')
                                          ? WIFI_AUTH_WPA2_PSK
                                          : WIFI_AUTH_OPEN;

    esp_wifi_disconnect();
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[CONNECT] esp_wifi_set_config failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_err_t save_err = wifi_service_save_credentials(ssid, password ? password : "");
    if (save_err != ESP_OK) {
        ESP_LOGW(TAG, "[CONNECT] Failed to save credentials: %s", esp_err_to_name(save_err));
    }

    err = esp_wifi_connect();
    if (err == ESP_OK) {
        set_state(WIFI_STATE_CONNECTING, ssid);
        s_auto_reconnect = true;
    } else {
        ESP_LOGE(TAG, "[CONNECT] esp_wifi_connect failed: %s", esp_err_to_name(err));
        set_state(WIFI_STATE_CONNECT_FAILED, ssid);
    }
    return err;
}

esp_err_t wifi_service_disconnect(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_auto_reconnect = false;
    esp_err_t err = esp_wifi_disconnect();
    if (err == ESP_OK) {
        memset(s_connected_ssid, 0, sizeof(s_connected_ssid));
        set_state(WIFI_STATE_DISCONNECTED, NULL);
        ESP_LOGI(TAG, "[CTRL] Disconnected");
    }
    return err;
}

// ---- State queries ----

wifi_state_t wifi_service_get_state(void)
{
    return s_state;
}

bool wifi_service_is_enabled(void)
{
    return s_state != WIFI_STATE_DISABLED;
}

bool wifi_service_is_connected(void)
{
    return s_state == WIFI_STATE_CONNECTED;
}

const char *wifi_service_get_connected_ssid(void)
{
    if (!wifi_service_is_connected()) {
        return NULL;
    }
    return s_connected_ssid;
}

esp_err_t wifi_service_connect_saved(void)
{
    if (!s_initialized || s_state == WIFI_STATE_DISABLED) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = wifi_service_apply_saved_credentials();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[CONNECT] Connecting to saved SSID '%s'", s_saved_ssid);
    } else if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "[CONNECT] No saved WiFi credentials available");
    }
    return err;
}

esp_err_t wifi_service_forget_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_SERVICE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        wifi_service_clear_saved_credentials();
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    bool erased = false;

    err = nvs_erase_key(handle, WIFI_SERVICE_NVS_KEY_SSID);
    if (err == ESP_OK) {
        erased = true;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return err;
    }

    esp_err_t pass_err = nvs_erase_key(handle, WIFI_SERVICE_NVS_KEY_PASS);
    if (pass_err == ESP_OK) {
        erased = true;
    } else if (pass_err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return pass_err;
    }

    if (erased) {
        err = nvs_commit(handle);
    } else {
        err = ESP_OK;
    }

    nvs_close(handle);
    wifi_service_clear_saved_credentials();
    return err;
}

bool wifi_service_has_credentials(void)
{
    return s_has_credentials;
}

// ---- Callbacks ----

void wifi_service_set_scan_done_cb(wifi_scan_done_cb_t cb)
{
    s_scan_done_cb = cb;
}

void wifi_service_set_state_changed_cb(wifi_state_changed_cb_t cb)
{
    s_state_changed_cb = cb;
}
