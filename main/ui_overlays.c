#include "ui_overlays.h"

#include "ui/ui.h"
#include "custom_keyboard.h"
#include "screen_manager.h"
#include "esp_log.h"

#define STATUS_BAR_HEIGHT 50

static const char *TAG = "ui_overlays";

static lv_obj_t *status_bar;
static lv_obj_t *time_label;
static lv_obj_t *wifi_label;

static void hide_search_keyboard(void)
{
    if (!objects.kb_search) {
        return;
    }

    lv_obj_t *textarea = lv_keyboard_get_textarea(objects.kb_search);
    if (textarea) {
        lv_obj_clear_state(textarea, LV_STATE_FOCUSED);
        lv_obj_clear_state(textarea, LV_STATE_CHECKED);
        lv_obj_clear_flag(textarea, LV_OBJ_FLAG_STATE_TRICKLE);
    }

    lv_keyboard_set_textarea(objects.kb_search, NULL);
    lv_obj_add_flag(objects.kb_search, LV_OBJ_FLAG_HIDDEN);
}

static void show_search_keyboard(lv_obj_t *textarea)
{
    if (!objects.kb_search || !textarea) {
        return;
    }

    lv_obj_add_state(textarea, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(objects.kb_search, textarea);
    lv_obj_clear_flag(objects.kb_search, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(objects.kb_search);
}

static void keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        hide_search_keyboard();
    }
}

static void textarea_focus_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *textarea = lv_event_get_target(event);
    show_search_keyboard(textarea);
}

static void search_launcher_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !objects.search || !objects.ta_search) {
        return;
    }

    screen_navigate(objects.search, SCREEN_ANIM_LEFT);
    show_search_keyboard(objects.ta_search);
}

static void search_back_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !objects.main) {
        return;
    }

    hide_search_keyboard();
    screen_navigate(objects.main, SCREEN_ANIM_RIGHT);
}

static void create_status_bar(void)
{
    lv_display_t *display = lv_display_get_default();
    if (!display) {
        ESP_LOGW(TAG, "No default display available for status bar");
        return;
    }

    status_bar = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_size(status_bar, lv_display_get_horizontal_resolution(display), STATUS_BAR_HEIGHT);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(status_bar, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    time_label = lv_label_create(status_bar);
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_20, 0);
    lv_label_set_text(time_label, "--:--");
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 12, 0);

    wifi_label = lv_label_create(status_bar);
    lv_obj_set_style_text_color(wifi_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_20, 0);
    lv_label_set_text(wifi_label, "WiFi: --");
    lv_obj_align(wifi_label, LV_ALIGN_RIGHT_MID, -12, 0);
}

static void create_keyboard(void)
{
    if (!objects.kb_search) {
        ESP_LOGW(TAG, "Generated keyboard object is missing");
        return;
    }

    apply_custom_keyboard_layout(objects.kb_search);
    lv_obj_add_event_cb(objects.kb_search, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(objects.kb_search, keyboard_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_move_background(objects.kb_search);
}

void ui_overlays_init(void)
{
    create_status_bar();
    create_keyboard();
}

void ui_overlays_bind_textarea(lv_obj_t *textarea)
{
    if (!textarea) {
        return;
    }

    lv_obj_add_event_cb(textarea, textarea_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(textarea, textarea_focus_cb, LV_EVENT_CLICKED, NULL);
}

void ui_overlays_bind_search_launcher(lv_obj_t *launcher)
{
    if (!launcher) {
        return;
    }

    lv_obj_add_event_cb(launcher, search_launcher_cb, LV_EVENT_CLICKED, NULL);
}

void ui_overlays_bind_search_back_button(lv_obj_t *button)
{
    if (!button) {
        return;
    }

    lv_obj_add_event_cb(button, search_back_cb, LV_EVENT_CLICKED, NULL);
}

void ui_overlays_set_time_text(const char *text)
{
    if (time_label && text) {
        lv_label_set_text(time_label, text);
    }
}

void ui_overlays_set_wifi_text(const char *text)
{
    if (wifi_label && text) {
        lv_label_set_text(wifi_label, text);
    }
}
