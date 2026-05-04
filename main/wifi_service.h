#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	WIFI_SERVICE_STATUS_STOPPED = 0,
	WIFI_SERVICE_STATUS_DISCONNECTED,
	WIFI_SERVICE_STATUS_CONNECTING,
	WIFI_SERVICE_STATUS_CONNECTED,
	WIFI_SERVICE_STATUS_GOT_IP
} wifi_service_status_t;

typedef struct {
	wifi_service_status_t status;
	char ssid[33];
	uint32_t ip_v4;
} wifi_service_state_t;

typedef struct {
	char ssid[33];
	int8_t rssi;
	wifi_auth_mode_t authmode;
} wifi_service_scan_result_t;

typedef void (*wifi_service_status_cb_t)(const wifi_service_state_t *state, void *user_ctx);
typedef void (*wifi_service_scan_state_cb_t)(bool scanning, void *user_ctx);
typedef void (*wifi_service_scan_done_cb_t)(const wifi_service_scan_result_t *results,
											size_t count,
											void *user_ctx);

typedef struct {
	wifi_service_status_cb_t on_status;
	wifi_service_scan_state_cb_t on_scan_state;
	wifi_service_scan_done_cb_t on_scan_done;
	void *user_ctx;
} wifi_service_callbacks_t;

esp_err_t wifi_service_init(void);
esp_err_t wifi_service_register_callbacks(const wifi_service_callbacks_t *callbacks);

esp_err_t wifi_service_set_enabled(bool enabled);
bool wifi_service_is_enabled(void);

esp_err_t wifi_service_connect(const char *ssid, const char *password);
esp_err_t wifi_service_connect_saved(void);
esp_err_t wifi_service_disconnect(void);

esp_err_t wifi_service_scan_start(void);

bool wifi_service_has_credentials(void);
esp_err_t wifi_service_forget_credentials(void);
esp_err_t wifi_service_get_state(wifi_service_state_t *out_state);

#ifdef __cplusplus
}
#endif
