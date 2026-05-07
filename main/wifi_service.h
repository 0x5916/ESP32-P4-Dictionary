#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include <esp_wifi_types_generic.h>

// Maximum number of APs returned from a scan
#define WIFI_SERVICE_MAX_APS 20

// WiFi connection/scan state
typedef enum {
    WIFI_STATE_DISABLED = 0,
    WIFI_STATE_IDLE,
    WIFI_STATE_SCANNING,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECT_FAILED,
} wifi_state_t;

// Single scan result entry
typedef struct {
    char ssid[33];
    int8_t rssi;
    bool requires_password;
    wifi_auth_mode_t auth_mode;   // ← ADD THIS
} wifi_ap_info_t;

// Callback types — called from the wifi_service internal task
// NOTE: These may be invoked from a background task; update LVGL inside bsp_display_lock()
typedef void (*wifi_scan_done_cb_t)(const wifi_ap_info_t *aps, uint16_t count);
typedef void (*wifi_state_changed_cb_t)(wifi_state_t new_state, const char *ssid);

// ---- Lifecycle ----
void wifi_service_init(void);
void wifi_service_deinit(void);

// ---- Control ----
esp_err_t wifi_service_enable(void);
esp_err_t wifi_service_disable(void);
esp_err_t wifi_service_scan(void);
esp_err_t wifi_service_connect(const char *ssid, const char *password);
esp_err_t wifi_service_disconnect(void);
esp_err_t wifi_service_connect_saved(void);
esp_err_t wifi_service_forget_credentials(void);
bool      wifi_service_has_credentials(void);

// ---- State queries ----
wifi_state_t wifi_service_get_state(void);
bool         wifi_service_is_enabled(void);
bool         wifi_service_is_connected(void);
const char  *wifi_service_get_connected_ssid(void); // NULL if not connected

// ---- Callbacks (set before calling init or scan) ----
void wifi_service_set_scan_done_cb(wifi_scan_done_cb_t cb);
void wifi_service_set_state_changed_cb(wifi_state_changed_cb_t cb);
