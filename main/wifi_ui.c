#include "wifi_ui.h"

#include "ui/ui.h"
#include "ui_overlays.h"
#include "wifi_service.h"
#include "ui_helpers.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"

#define WIFI_UI_SCAN_MAX_RESULTS 20

static const char *TAG = "wifi_ui";
static wifi_service_scan_result_t s_wifi_scan_results[WIFI_UI_SCAN_MAX_RESULTS];
static size_t s_wifi_scan_count;
static bool s_ignore_wifi_switch;
static wifi_service_state_t s_pending_state;
static bool s_pending_state_valid;
static bool s_pending_scan_state;
static bool s_pending_scan_state_valid;
static bool s_pending_scan_results;

static lv_obj_t *s_pwd_modal = NULL;
typedef struct {
    lv_obj_t *ta;
    uint8_t   ap_index;
    char      ssid[33];   // ← add this
} wifi_pwd_ctx_t;

static void wifi_network_item_cb(lv_event_t *event);
static void wifi_scan_button_cb(lv_event_t *event);
static void wifi_switch_cb(lv_event_t *event);
static void wifi_ui_on_status(const wifi_service_state_t *state, void *user_ctx);
static void wifi_ui_on_scan_state(bool scanning, void *user_ctx);
static void wifi_ui_on_scan_done(const wifi_service_scan_result_t *results,
                                 size_t count,
                                 void *user_ctx);
static void wifi_ui_apply_state(void *user_ctx);
static void wifi_ui_apply_scan_state(void *user_ctx);
static void wifi_ui_apply_scan_results(void *user_ctx);

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
    ui_label_set_text_if(objects.wi_fi_state_label, text);
}

static void wifi_ui_set_spinner_visible(bool visible)
{
    ui_obj_set_hidden(objects.wi_fi_load_spinner, !visible);
}

static void wifi_ui_set_switch_state(bool enabled)
{
    if (!objects.wi_fi_state_switch) {
        return;
    }

    s_ignore_wifi_switch = true;
    ui_obj_set_state_if(objects.wi_fi_state_switch, LV_STATE_CHECKED, enabled);
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

static void pwd_cancel_cb(lv_event_t *e)
{
    if (s_pwd_modal) {
        lv_obj_delete(s_pwd_modal);
        s_pwd_modal = NULL;
    }
}

static void pwd_join_cb(lv_event_t *e)
{
    wifi_pwd_ctx_t *ctx = lv_event_get_user_data(e);
    const char *pwd = lv_textarea_get_text(ctx->ta);

    // Use wifi_service_connect(ssid, password) — NOT wifi_service_connect_with_password
    esp_err_t err = wifi_service_connect(ctx->ssid, pwd);
    if (err != ESP_OK) {
        ESP_LOGW("wifi_ui", "Failed to connect: %s", esp_err_to_name(err));
    }

    free(ctx);
    pwd_cancel_cb(e);  // close modal
}

static void pwd_show_toggle_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_user_data(e);
    bool hidden = lv_textarea_get_password_mode(ta);
    lv_textarea_set_password_mode(ta, !hidden);
    lv_obj_t *btn = lv_event_get_target(e);
    lv_label_set_text(lv_obj_get_child(btn, 0), hidden ? "Hide" : "Show");
}

void wifi_ui_show_password_prompt(const char *ssid, uint8_t ap_index)
{
    // Backdrop
    s_pwd_modal = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_pwd_modal, 480, 800);
    lv_obj_set_style_bg_color(s_pwd_modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_pwd_modal, LV_OPA_50, 0);
    lv_obj_remove_flag(s_pwd_modal, LV_OBJ_FLAG_SCROLLABLE);

    // Card container
    lv_obj_t *card = lv_obj_create(s_pwd_modal);
    lv_obj_set_size(card, 440, 320);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_radius(card, 16, 0);

    // Title label
    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text_fmt(title, "Join \"%s\"", ssid);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // Password textarea
    lv_obj_t *ta = lv_textarea_create(card);
    lv_textarea_set_placeholder_text(ta, "Password");
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_size(ta, 380, 50);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_22, 0);

    // Show/hide password toggle
    lv_obj_t *show_btn = lv_button_create(card);
    lv_obj_set_size(show_btn, 120, 40);
    lv_obj_align(show_btn, LV_ALIGN_TOP_LEFT, 8, 118);
    lv_obj_t *show_lbl = lv_label_create(show_btn);
    lv_label_set_text(show_lbl, "Show");
    lv_obj_center(show_lbl);
    lv_obj_add_event_cb(show_btn, pwd_show_toggle_cb, LV_EVENT_CLICKED, ta);

    // Cancel button
    lv_obj_t *cancel_btn = lv_button_create(card);
    lv_obj_set_size(cancel_btn, 160, 50);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(cancel_btn, pwd_cancel_cb, LV_EVENT_CLICKED, NULL);

    // Join button
    lv_obj_t *join_btn = lv_button_create(card);
    lv_obj_set_size(join_btn, 160, 50);
    lv_obj_align(join_btn, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_t *join_lbl = lv_label_create(join_btn);
    lv_label_set_text(join_lbl, "Join");
    lv_obj_center(join_lbl);

    // Pass both ta and ap_index to the join callback
    wifi_pwd_ctx_t *ctx = malloc(sizeof(wifi_pwd_ctx_t));
    ctx->ta       = ta;
    ctx->ap_index = ap_index;
    strncpy(ctx->ssid, ssid, sizeof(ctx->ssid) - 1);   // ← add this
    ctx->ssid[sizeof(ctx->ssid) - 1] = '\0';
    lv_obj_add_event_cb(join_btn, pwd_join_cb, LV_EVENT_CLICKED, ctx);

    // Attach keyboard to textarea
    ui_overlays_bind_textarea(ta);
}

static void wifi_network_item_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED)) {
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
    wifi_ui_show_password_prompt(result->ssid, (uint8_t)index);
}

static void wifi_scan_button_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED)) {
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
    if (!ui_event_is(event, LV_EVENT_VALUE_CHANGED) || s_ignore_wifi_switch) {
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

    s_pending_state = *state;
    s_pending_state_valid = true;
    lv_async_call(wifi_ui_apply_state, NULL);
}

static void wifi_ui_on_scan_state(bool scanning, void *user_ctx)
{
    (void)user_ctx;

    s_pending_scan_state = scanning;
    s_pending_scan_state_valid = true;
    lv_async_call(wifi_ui_apply_scan_state, NULL);
}

static void wifi_ui_on_scan_done(const wifi_service_scan_result_t *results,
                                 size_t count,
                                 void *user_ctx)
{
    (void)user_ctx;

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

    s_pending_scan_results = true;
    lv_async_call(wifi_ui_apply_scan_results, NULL);
}

static void wifi_ui_apply_state(void *user_ctx)
{
    (void)user_ctx;

    if (!s_pending_state_valid) {
        return;
    }

    wifi_service_state_t state = s_pending_state;
    s_pending_state_valid = false;

    bsp_display_lock(-1);

    const char *status_text = "Unknown";
    char status_line[64];

    switch (state.status) {
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
        if (state.ssid[0] != '\0') {
            snprintf(status_line, sizeof(status_line), "WiFi: %s", state.ssid);
        } else {
            snprintf(status_line, sizeof(status_line), "WiFi: Connected");
        }
        wifi_ui_set_switch_state(true);
        break;
    case WIFI_SERVICE_STATUS_GOT_IP:
        status_text = "Online";
        if (state.ssid[0] != '\0') {
            snprintf(status_line, sizeof(status_line), "WiFi: %s", state.ssid);
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

static void wifi_ui_apply_scan_state(void *user_ctx)
{
    (void)user_ctx;

    if (!s_pending_scan_state_valid) {
        return;
    }

    bool scanning = s_pending_scan_state;
    s_pending_scan_state_valid = false;

    bsp_display_lock(-1);
    wifi_ui_set_spinner_visible(scanning);
    if (scanning) {
        wifi_ui_set_state_label("Scanning");
    }
    bsp_display_unlock();
}

static void wifi_ui_apply_scan_results(void *user_ctx)
{
    (void)user_ctx;

    if (!s_pending_scan_results) {
        return;
    }

    s_pending_scan_results = false;

    bsp_display_lock(-1);

    wifi_ui_set_spinner_visible(false);
    wifi_ui_clear_network_list();

    for (size_t i = 0; i < s_wifi_scan_count; ++i) {
        wifi_ui_add_network_item(&s_wifi_scan_results[i], i);
    }

    bsp_display_unlock();
}

void wifi_ui_init(void)
{
    ui_bind_event(objects.wifi_scan_btn, wifi_scan_button_cb, LV_EVENT_CLICKED, NULL);
    ui_bind_event(objects.wi_fi_state_switch, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    wifi_service_callbacks_t callbacks = {
        .on_status = wifi_ui_on_status,
        .on_scan_state = wifi_ui_on_scan_state,
        .on_scan_done = wifi_ui_on_scan_done,
        .user_ctx = NULL
    };
    wifi_service_register_callbacks(&callbacks);
}
