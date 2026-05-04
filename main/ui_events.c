#include "ui/ui.h"
#include "ui_overlays.h"
#include "wifi_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"

#define WIFI_UI_SCAN_MAX_RESULTS 20

static const char *TAG = "ui_events";
static wifi_service_scan_result_t s_wifi_scan_results[WIFI_UI_SCAN_MAX_RESULTS];
static size_t s_wifi_scan_count;
static bool s_ignore_wifi_switch;

static void wifi_network_item_cb(lv_event_t *event);
static void wifi_scan_button_cb(lv_event_t *event);
static void wifi_switch_cb(lv_event_t *event);
static void wifi_ui_on_status(const wifi_service_state_t *state, void *user_ctx);
static void wifi_ui_on_scan_state(bool scanning, void *user_ctx);
static void wifi_ui_on_scan_done(const wifi_service_scan_result_t *results,
                                 size_t count,
                                 void *user_ctx);

static const char *wifi_auth_to_string(wifi_auth_mode_t authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        return "Open";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK:
        return "WAPI";
    default:
        return "Unknown";
    }
}

static void wifi_ui_set_state_label(const char *text)
{
    if (objects.wi_fi_state_label && text) {
        lv_label_set_text(objects.wi_fi_state_label, text);
    }
}

static void wifi_ui_set_spinner_visible(bool visible)
{
    if (!objects.wi_fi_load_spinner) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(objects.wi_fi_load_spinner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(objects.wi_fi_load_spinner, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wifi_ui_set_switch_state(bool enabled)
{
    if (!objects.wi_fi_state_switch) {
        return;
    }

    s_ignore_wifi_switch = true;
    if (enabled) {
        lv_obj_add_state(objects.wi_fi_state_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(objects.wi_fi_state_switch, LV_STATE_CHECKED);
    }
    s_ignore_wifi_switch = false;
}

static void wifi_ui_clear_network_list(void)
{
    if (objects.wi_fi_network_lst) {
        lv_obj_clean(objects.wi_fi_network_lst);
    }
}

static void wifi_ui_add_network_item(const wifi_service_scan_result_t *result, size_t index)
{
    if (!objects.wi_fi_network_lst || !result) {
        return;
    }

    char label[96];
    snprintf(label,
             sizeof(label),
             "%s  (%s, %d dBm)",
             result->ssid,
             wifi_auth_to_string(result->authmode),
             result->rssi);

    lv_obj_t *btn = lv_list_add_btn(objects.wi_fi_network_lst, NULL, label);
    if (!btn) {
        return;
    }

    lv_obj_set_user_data(btn, (void *)(uintptr_t)index);
    lv_obj_add_event_cb(btn, wifi_network_item_cb, LV_EVENT_CLICKED, NULL);
}

static void wifi_network_item_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *target = lv_event_get_target(event);
    uintptr_t index = (uintptr_t)lv_obj_get_user_data(target);
    if (index >= s_wifi_scan_count) {
        return;
    }

    const wifi_service_scan_result_t *result = &s_wifi_scan_results[index];
    if (result->authmode == WIFI_AUTH_OPEN) {
        esp_err_t err = wifi_service_connect(result->ssid, "");
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to connect to %s: %s", result->ssid, esp_err_to_name(err));
        }
        return;
    }

    ui_overlays_set_wifi_text("WiFi: Auth required");
    wifi_ui_set_state_label("Password needed");
    // TODO: Prompt for password UI and call wifi_service_connect().
}

static void wifi_scan_button_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    wifi_ui_set_spinner_visible(true);
    wifi_ui_clear_network_list();

    esp_err_t err = wifi_service_scan_start();
    if (err != ESP_OK) {
        wifi_ui_set_spinner_visible(false);
        ESP_LOGW(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
    }
}

static void wifi_switch_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED || s_ignore_wifi_switch) {
        return;
    }

    bool enabled = lv_obj_has_state(objects.wi_fi_state_switch, LV_STATE_CHECKED);
    esp_err_t err = wifi_service_set_enabled(enabled);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to toggle WiFi: %s", esp_err_to_name(err));
    }
}

static void wifi_ui_on_status(const wifi_service_state_t *state, void *user_ctx)
{
    if (!state) {
        return;
    }

    (void)user_ctx;

    bsp_display_lock(-1);

    const char *status_text = "Unknown";
    char status_line[64];

    switch (state->status) {
    case WIFI_SERVICE_STATUS_STOPPED:
        status_text = "Off";
        snprintf(status_line, sizeof(status_line), "WiFi: Off");
        wifi_ui_set_switch_state(false);
        break;
    case WIFI_SERVICE_STATUS_DISCONNECTED:
        status_text = "Disconnected";
        snprintf(status_line, sizeof(status_line), "WiFi: Disconnected");
        wifi_ui_set_switch_state(true);
        break;
    case WIFI_SERVICE_STATUS_CONNECTING:
        status_text = "Connecting";
        snprintf(status_line, sizeof(status_line), "WiFi: Connecting");
        wifi_ui_set_switch_state(true);
        break;
    case WIFI_SERVICE_STATUS_CONNECTED:
        status_text = "Connected";
        if (state->ssid[0] != '\0') {
            snprintf(status_line, sizeof(status_line), "WiFi: %s", state->ssid);
        } else {
            snprintf(status_line, sizeof(status_line), "WiFi: Connected");
        }
        wifi_ui_set_switch_state(true);
        break;
    case WIFI_SERVICE_STATUS_GOT_IP:
        status_text = "Online";
        if (state->ssid[0] != '\0') {
            snprintf(status_line, sizeof(status_line), "WiFi: %s", state->ssid);
        } else {
            snprintf(status_line, sizeof(status_line), "WiFi: Online");
        }
        wifi_ui_set_switch_state(true);
        break;
    default:
        snprintf(status_line, sizeof(status_line), "WiFi: --");
        break;
    }

    ui_overlays_set_wifi_text(status_line);
    wifi_ui_set_state_label(status_text);

    bsp_display_unlock();
}

static void wifi_ui_on_scan_state(bool scanning, void *user_ctx)
{
    (void)user_ctx;

    bsp_display_lock(-1);
    wifi_ui_set_spinner_visible(scanning);
    if (scanning) {
        wifi_ui_set_state_label("Scanning");
    }
    bsp_display_unlock();
}

static void wifi_ui_on_scan_done(const wifi_service_scan_result_t *results,
                                 size_t count,
                                 void *user_ctx)
{
    (void)user_ctx;

    bsp_display_lock(-1);

    size_t copy_count = count;
    if (copy_count > WIFI_UI_SCAN_MAX_RESULTS) {
        copy_count = WIFI_UI_SCAN_MAX_RESULTS;
    }

    if (results && copy_count > 0) {
        memcpy(s_wifi_scan_results, results, copy_count * sizeof(wifi_service_scan_result_t));
        s_wifi_scan_count = copy_count;
    } else {
        s_wifi_scan_count = 0;
    }

    wifi_ui_set_spinner_visible(false);
    wifi_ui_clear_network_list();

    for (size_t i = 0; i < s_wifi_scan_count; ++i) {
        wifi_ui_add_network_item(&s_wifi_scan_results[i], i);
    }

    bsp_display_unlock();
}

void ui_events_init(void)
{
    ui_overlays_bind_textarea(objects.search_search_ta);

    ui_overlays_bind_search_open_button(objects.main_search_fake_ta);
    ui_overlays_bind_search_back_button(objects.search_back_btn);

    ui_overlays_bind_navigation_button(objects.main_settings_btn, objects.settings);
    ui_overlays_bind_search_back_button(objects.settings_back_btn);

    if (objects.wifi_scan_btn) {
        lv_obj_add_event_cb(objects.wifi_scan_btn, wifi_scan_button_cb, LV_EVENT_CLICKED, NULL);
    }

    if (objects.wi_fi_state_switch) {
        lv_obj_add_event_cb(objects.wi_fi_state_switch, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    wifi_service_callbacks_t callbacks = {
        .on_status = wifi_ui_on_status,
        .on_scan_state = wifi_ui_on_scan_state,
        .on_scan_done = wifi_ui_on_scan_done,
        .user_ctx = NULL
    };
    wifi_service_register_callbacks(&callbacks);
}
