#include "wifi_ui.h"

#include "wifi_service.h"
#include "ui/screens.h"
#include "ui_helpers.h"
#include "ui_overlays.h"
#include "bsp/esp-bsp.h"
#include "screen_manager.h"
#include "esp_log.h"
#include "custom_keyboard.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi_ui";

// ---- Password modal state ----
static char s_pending_ssid[33]  = {0};

static lv_obj_t *s_pw_modal   = NULL;
static lv_obj_t *s_pw_ta      = NULL;
static lv_obj_t *s_pw_kb      = NULL;
static bool s_scan_refresh_pending = false;

// ---- Forward declarations ----
static void wifi_ui_update_status_bar(wifi_state_t state, const char *ssid);
static void wifi_ui_request_scan_refresh(void);

// ---- RSSI → signal bars string ----
static const char *auth_mode_to_str(wifi_auth_mode_t mode)
{
    switch (mode) {
        case WIFI_AUTH_OPEN:          return "Open";
        case WIFI_AUTH_WEP:           return "WEP";
        case WIFI_AUTH_WPA_PSK:       return "WPA";
        case WIFI_AUTH_WPA2_PSK:      return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
        case WIFI_AUTH_WPA3_PSK:      return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/3";
        default:                      return "Unknown";
    }
}

// ============================================================
//  Password modal
// ============================================================

static void pw_modal_destroy(void)
{
    // Delete keyboard first — it's a sibling of modal on lv_layer_top()
    if (s_pw_kb) {
        lv_obj_delete(s_pw_kb);
        s_pw_kb = NULL;
        s_pw_ta = NULL;  // ta is a child of modal, will be gone with it
    }
    if (s_pw_modal) {
        lv_obj_delete(s_pw_modal);
        s_pw_modal = NULL;
    }
    memset(s_pending_ssid, 0, sizeof(s_pending_ssid));
}

static void pw_cancel_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED)) return;
    pw_modal_destroy();
}

static void pw_connect_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED) &&
        !ui_event_is(event, LV_EVENT_READY)) return;

    const char *pw = "";
    if (s_pw_ta) {
        pw = lv_textarea_get_text(s_pw_ta);
    }

    ESP_LOGI(TAG, "[UI] Initiating connect to '%s'", s_pending_ssid);
    wifi_service_connect(s_pending_ssid, pw);
    ESP_LOGI(TAG, "[UI] Connect command sent, closing modal");
    pw_modal_destroy();
}

static void pw_kb_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY) {
        pw_connect_cb(event);
    } else if (code == LV_EVENT_CANCEL) {
        pw_modal_destroy();
    }
}

static void pw_modal_show(const char *ssid)
{
    if (s_pw_modal) {
        pw_modal_destroy();
    }

    strncpy(s_pending_ssid, ssid, sizeof(s_pending_ssid) - 1);

    // Overlay dim
    s_pw_modal = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_pw_modal);
    lv_obj_set_size(s_pw_modal, 480, 800);
    lv_obj_set_pos(s_pw_modal, 0, 0);
    lv_obj_set_style_bg_color(s_pw_modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_pw_modal, LV_OPA_60, 0);
    lv_obj_clear_flag(s_pw_modal, LV_OBJ_FLAG_SCROLLABLE);

    // Card
    lv_obj_t *card = lv_obj_create(s_pw_modal);
    lv_obj_set_size(card, 440, 380);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(card);
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "Connect to: %s", ssid);
    lv_label_set_text(title, title_buf);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_width(title, lv_pct(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // Password label
    lv_obj_t *pw_lbl = lv_label_create(card);
    lv_label_set_text(pw_lbl, "Password:");
    lv_obj_set_style_text_font(pw_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_width(pw_lbl, lv_pct(100));

    // Textarea
    s_pw_ta = lv_textarea_create(card);
    lv_textarea_set_password_mode(s_pw_ta, true);
    lv_textarea_set_one_line(s_pw_ta, true);
    lv_textarea_set_placeholder_text(s_pw_ta, "Enter password...");
    lv_obj_set_width(s_pw_ta, lv_pct(100));
    lv_obj_set_style_text_font(s_pw_ta, &lv_font_montserrat_18, 0);

    // Buttons row
    lv_obj_t *btn_row = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(btn_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(btn_row, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *cancel_btn = lv_button_create(btn_row);
    lv_obj_set_size(cancel_btn, 180, 44);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_set_style_text_font(cancel_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(cancel_btn, pw_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *connect_btn = lv_button_create(btn_row);
    lv_obj_set_size(connect_btn, 180, 44);
    lv_obj_t *connect_lbl = lv_label_create(connect_btn);
    lv_label_set_text(connect_lbl, "Connect");
    lv_obj_set_style_text_font(connect_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(connect_lbl);
    lv_obj_add_event_cb(connect_btn, pw_connect_cb, LV_EVENT_CLICKED, NULL);

    // Keyboard (within modal, anchored to bottom of screen)
    // In pw_modal_show(), after lv_keyboard_create():
    s_pw_kb = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(s_pw_kb, 480, 300);
    lv_obj_align(s_pw_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(s_pw_kb, s_pw_ta);

    // ✅ ADD THIS — reuses your already-working shift key handler
    apply_custom_keyboard_layout(s_pw_kb);

    lv_obj_add_event_cb(s_pw_kb, pw_kb_event_cb, LV_EVENT_READY,   NULL);
    lv_obj_add_event_cb(s_pw_kb, pw_kb_event_cb, LV_EVENT_CANCEL,  NULL);
}

// ============================================================
//  Network list population
// ============================================================

static void network_item_click_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED)) return;

    const char *ssid = (const char *)lv_event_get_user_data(event);
    if (!ssid || ssid[0] == '\0') return;

    ESP_LOGI(TAG, "[UI] Network selected: '%s'", ssid);

    // Check if already connected to this SSID
    const char *current = wifi_service_get_connected_ssid();
    if (current && strcmp(current, ssid) == 0) {
        ESP_LOGI(TAG, "[UI] Already connected to '%s', disconnecting", ssid);
        wifi_service_disconnect();
        return;
    }

    lv_obj_t *btn = lv_event_get_target(event);
    bool needs_pw = (bool)(uintptr_t)lv_obj_get_user_data(btn);

    if (needs_pw) {
        bsp_display_lock(-1);
        pw_modal_show(ssid);
        bsp_display_unlock();
    } else {
        wifi_service_connect(ssid, "");
    }
}

// Per-item SSID storage — statically allocated for WIFI_SERVICE_MAX_APS items
static char s_ssid_store[WIFI_SERVICE_MAX_APS][33];

static void wifi_ui_populate_list(const wifi_ap_info_t *aps, uint16_t count)
{
    if (!objects.wi_fi_network_lst) return;

    lv_obj_clean(objects.wi_fi_network_lst);

    if (count == 0) {
        lv_obj_t *no_result = lv_list_add_text(objects.wi_fi_network_lst, "No networks found");
        lv_obj_set_style_text_font(no_result, &lv_font_montserrat_18, 0);
        return;
    }

    const char *connected_ssid = wifi_service_get_connected_ssid();

    for (uint16_t i = 0; i < count && i < WIFI_SERVICE_MAX_APS; i++) {
        // Copy SSID into stable storage so the button callback can reference it
        strncpy(s_ssid_store[i], aps[i].ssid, sizeof(s_ssid_store[i]) - 1);
        s_ssid_store[i][sizeof(s_ssid_store[i]) - 1] = '\0';

        char label_buf[80];
        bool is_connected = (connected_ssid && strcmp(connected_ssid, aps[i].ssid) == 0);
        const char *conn_mark = is_connected ? " Yes" : "";

        snprintf(label_buf, sizeof(label_buf), "%ddBm  [%s]  %s%s",
            aps[i].rssi,
            auth_mode_to_str(aps[i].auth_mode),
            aps[i].ssid,
            conn_mark);

        lv_obj_t *btn = lv_list_add_button(objects.wi_fi_network_lst, NULL, label_buf);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_18, 0);

        // Store whether password is needed as user_data on the button
        lv_obj_set_user_data(btn, (void *)(uintptr_t)aps[i].requires_password);

        lv_obj_add_event_cb(btn, network_item_click_cb, LV_EVENT_CLICKED, s_ssid_store[i]);
    }
}

// ============================================================
//  Service callbacks (called from ESP-IDF event task context)
// ============================================================

static void on_scan_done(const wifi_ap_info_t *aps, uint16_t count)
{
    ESP_LOGI(TAG, "[CB] scan_done: %u APs", count);
    bsp_display_lock(-1);
    ui_obj_set_hidden(objects.wi_fi_load_spinner, true);
    wifi_ui_populate_list(aps, count);
    bsp_display_unlock();
}

static void on_state_changed(wifi_state_t new_state, const char *ssid)
{
    ESP_LOGI(TAG, "[CB] state_changed: %d ssid='%s'", (int)new_state, ssid ? ssid : "(null)");

    bsp_display_lock(-1);

    // Status bar WiFi indicator
    wifi_ui_update_status_bar(new_state, ssid);

    // Spinner management
    bool scanning = (new_state == WIFI_STATE_SCANNING || new_state == WIFI_STATE_CONNECTING);
    ui_obj_set_hidden(objects.wi_fi_load_spinner, !scanning);

    // WiFi toggle switch reflects enabled/disabled state
    if (objects.wi_fi_state_switch) {
        bool enabled = (new_state != WIFI_STATE_DISABLED);
        ui_obj_set_state_if(objects.wi_fi_state_switch, LV_STATE_CHECKED, enabled);
    }

    // State label text
    if (objects.wi_fi_state_label) {
        const char *label_text = "Off";
        switch (new_state) {
            case WIFI_STATE_DISABLED:     label_text = "Off";          break;
            case WIFI_STATE_IDLE:         label_text = "On";           break;
            case WIFI_STATE_SCANNING:     label_text = "Scanning...";  break;
            case WIFI_STATE_CONNECTING:   label_text = "Connecting..."; break;
            case WIFI_STATE_CONNECTED:    label_text = "Connected";    break;
            case WIFI_STATE_DISCONNECTED: label_text = "Disconnected"; break;
            case WIFI_STATE_CONNECT_FAILED: label_text = "Failed";     break;
        }
        lv_label_set_text(objects.wi_fi_state_label, label_text);
    }

    // Refresh the visible list after connection state changes, but defer the
    // scan so we do not re-enter the service from inside the event callback.
    if (new_state == WIFI_STATE_DISCONNECTED ||
        new_state == WIFI_STATE_CONNECT_FAILED) {
        wifi_ui_request_scan_refresh();
    }

    bsp_display_unlock();
}

// ============================================================
//  Status bar helper
// ============================================================

static void wifi_ui_update_status_bar(wifi_state_t state, const char *ssid)
{
    char buf[48];
    switch (state) {
        case WIFI_STATE_DISABLED:
            snprintf(buf, sizeof(buf), "WiFi: Off");
            break;
        case WIFI_STATE_IDLE:
            snprintf(buf, sizeof(buf), "WiFi: On");
            break;
        case WIFI_STATE_SCANNING:
            snprintf(buf, sizeof(buf), "WiFi: Scanning");
            break;
        case WIFI_STATE_CONNECTING:
            snprintf(buf, sizeof(buf), "WiFi: Connecting");
            break;
        case WIFI_STATE_CONNECTED:
            if (ssid && ssid[0]) {
                snprintf(buf, sizeof(buf), "WiFi: %s", ssid);
            } else {
                snprintf(buf, sizeof(buf), "WiFi: Connected");
            }
            break;
        case WIFI_STATE_DISCONNECTED:
            snprintf(buf, sizeof(buf), "WiFi: --");
            break;
        case WIFI_STATE_CONNECT_FAILED:
            snprintf(buf, sizeof(buf), "WiFi: Failed");
            break;
        default:
            snprintf(buf, sizeof(buf), "WiFi: --");
            break;
    }
    ui_overlays_set_wifi_text(buf);
}

// ============================================================
//  UI event handlers (toggle switch, scan button)
// ============================================================

static void wifi_toggle_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_VALUE_CHANGED)) return;

    lv_obj_t *sw = lv_event_get_target(event);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);

    ESP_LOGI(TAG, "[UI] WiFi toggle -> %s", enabled ? "ON" : "OFF");

    if (enabled) {
        wifi_service_enable();
    } else {
        pw_modal_destroy();  // close modal if open
        wifi_service_disable();
        // Clear the list
        bsp_display_lock(-1);
        if (objects.wi_fi_network_lst) {
            lv_obj_clean(objects.wi_fi_network_lst);
        }
        bsp_display_unlock();
    }
}

static void wifi_scan_btn_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED)) return;

    ESP_LOGI(TAG, "[UI] Scan button pressed");
    if (!wifi_service_is_enabled()) {
        ESP_LOGW(TAG, "[UI] WiFi not enabled, ignoring scan");
        return;
    }

    bsp_display_lock(-1);
    if (objects.wi_fi_network_lst) {
        lv_obj_clean(objects.wi_fi_network_lst);
    }
    ui_obj_set_hidden(objects.wi_fi_load_spinner, false);
    bsp_display_unlock();

    wifi_service_scan();
}

static void wifi_ui_refresh_scan_async(void *user_data)
{
    (void)user_data;

    s_scan_refresh_pending = false;
    if (wifi_service_is_enabled()) {
        wifi_service_scan();
    }
}

static void wifi_ui_request_scan_refresh(void)
{
    if (s_scan_refresh_pending) {
        return;
    }

    s_scan_refresh_pending = true;
    lv_async_call(wifi_ui_refresh_scan_async, NULL);
}

// ============================================================
//  Init
// ============================================================

void wifi_ui_init(void)
{
    ESP_LOGI(TAG, "[INIT] wifi_ui_init()");

    // Register service callbacks
    wifi_service_set_scan_done_cb(on_scan_done);
    wifi_service_set_state_changed_cb(on_state_changed);

    // Bind toggle switch
    if (objects.wi_fi_state_switch) {
        bool enabled = wifi_service_is_enabled();
        ui_obj_set_state_if(objects.wi_fi_state_switch, LV_STATE_CHECKED, enabled);
        lv_obj_add_event_cb(objects.wi_fi_state_switch, wifi_toggle_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    // Bind scan button
    if (objects.wifi_scan_btn) {
        lv_obj_add_event_cb(objects.wifi_scan_btn, wifi_scan_btn_cb, LV_EVENT_CLICKED, NULL);
    }

    // Sync initial state label
    wifi_state_t cur = wifi_service_get_state();
    bsp_display_lock(-1);
    wifi_ui_update_status_bar(cur, wifi_service_get_connected_ssid());
    if (objects.wi_fi_state_label) {
        bool enabled = wifi_service_is_enabled();
        lv_label_set_text(objects.wi_fi_state_label, enabled ? "On" : "Off");
    }
    ui_obj_set_hidden(objects.wi_fi_load_spinner, true);
    bsp_display_unlock();

    // Auto-scan if WiFi is already on
    if (wifi_service_is_enabled()) {
        bsp_display_lock(-1);
        ui_obj_set_hidden(objects.wi_fi_load_spinner, false);
        bsp_display_unlock();
        wifi_service_scan();
    }

    ESP_LOGI(TAG, "[INIT] wifi_ui_init() complete");
}
