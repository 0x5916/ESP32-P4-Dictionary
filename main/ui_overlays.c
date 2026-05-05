#include "ui_overlays.h"

#include "ui/ui.h"
#include "custom_keyboard.h"
#include "screen_manager.h"
#include "settings_ui.h"
#include "settings_service.h"
#include "bsp/display.h"
#include "esp_log.h"
#include "ui_helpers.h"

#define STATUS_BAR_HEIGHT 50
#define SEARCH_HEADWORD_LIST_COMPACT_HEIGHT 295
#define SEARCH_HEADWORD_LIST_EXPANDED_HEIGHT 605

static const char *TAG = "ui_overlays";

static lv_obj_t *status_bar;
static lv_obj_t *time_label;
static lv_obj_t *wifi_label;

static void set_search_headword_list_height(lv_coord_t height)
{
    if (objects.search_headword_lst) {
        lv_obj_set_height(objects.search_headword_lst, height);
    }
}

static void hide_search_keyboard(void)
{
    if (!objects.search_kb) {
        return;
    }

    lv_obj_t *textarea = lv_keyboard_get_textarea(objects.search_kb);
    if (textarea) {
        ui_obj_set_state_if(textarea, LV_STATE_FOCUSED, false);
        ui_obj_set_state_if(textarea, LV_STATE_CHECKED, false);
        lv_obj_clear_flag(textarea, LV_OBJ_FLAG_STATE_TRICKLE);
    }

    lv_keyboard_set_textarea(objects.search_kb, NULL);
    ui_obj_set_hidden(objects.search_kb, true);
    set_search_headword_list_height(SEARCH_HEADWORD_LIST_EXPANDED_HEIGHT);
}

static void show_search_keyboard(lv_obj_t *textarea)
{
    if (!objects.search_kb || !textarea) {
        return;
    }

    lv_obj_add_state(textarea, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(objects.search_kb, textarea);
    ui_obj_set_hidden(objects.search_kb, false);
    lv_obj_move_foreground(objects.search_kb);
    set_search_headword_list_height(SEARCH_HEADWORD_LIST_COMPACT_HEIGHT);
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

static void open_search_screen_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED) || !objects.search) {
        return;
    }

    screen_navigate(objects.search, SCREEN_ANIM_LEFT);
    show_search_keyboard(objects.search_search_ta);
}

static void navigate_to_screen_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED)) {
        return;
    }

    lv_obj_t *target_screen = (lv_obj_t *)lv_event_get_user_data(event);
    if (!target_screen) {
        ESP_LOGW(TAG, "navigate_to_screen_cb: target screen is NULL");
        return;
    }

    screen_navigate(target_screen, SCREEN_ANIM_LEFT);
}

static void swipe_back_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_GESTURE)) return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;

    if (lv_indev_get_gesture_dir(indev) != LV_DIR_RIGHT) return;

    // After acting on the gesture, tell LVGL to ignore further
    // input events until the user lifts their finger
    lv_indev_wait_release(indev);

    lv_obj_t *screen = lv_event_get_target(event);
    if (screen == objects.search) hide_search_keyboard();

    lv_obj_t *target_screen = (lv_obj_t *)lv_event_get_user_data(event);
    if (!target_screen) return;

    screen_navigate(target_screen, SCREEN_ANIM_RIGHT);
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

    bool dark_mode = false;
    settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &dark_mode);
    ui_overlays_apply_theme(dark_mode);
}

static void create_keyboard(void)
{
    if (!objects.search_kb) {
        ESP_LOGW(TAG, "Generated keyboard object is missing");
        return;
    }

    apply_custom_keyboard_layout(objects.search_kb);
    lv_obj_add_event_cb(objects.search_kb, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(objects.search_kb, keyboard_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_move_background(objects.search_kb);
}

void ui_overlays_init(void)
{
    bool dark_mode = false;
    settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &dark_mode);
    settings_ui_apply_theme(dark_mode);
    ui_overlays_apply_theme(dark_mode);

    uint8_t brightness = 100;
    settings_get_u8(SETTINGS_KEY_BRIGHTNESS, 100, &brightness);
    bsp_display_brightness_set((int)brightness);

    settings_ui_build(objects.settings_cont);
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

void ui_overlays_bind_search_open_button(lv_obj_t *button)
{
    ui_bind_event(button, open_search_screen_cb, LV_EVENT_CLICKED, NULL);
}

void ui_overlays_bind_swipe_back(lv_obj_t *screen, lv_obj_t *target_screen)
{
    if (!screen || !target_screen) {
        return;
    }

    lv_obj_add_event_cb(screen, swipe_back_cb, LV_EVENT_GESTURE, target_screen);
}

void ui_overlays_set_time_text(const char *text)
{
    ui_label_set_text_if(time_label, text);
}

void ui_overlays_set_wifi_text(const char *text)
{
    ui_label_set_text_if(wifi_label, text);
}

void ui_overlays_apply_theme(bool dark_mode)
{
    if (!status_bar) {
        return;
    }

    lv_color_t bg_color = dark_mode
        ? lv_palette_darken(LV_PALETTE_GREY, 4)
        : lv_palette_main(LV_PALETTE_BLUE);
    lv_obj_set_style_bg_color(status_bar, bg_color, 0);
    if (time_label) {
        lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    }
    if (wifi_label) {
        lv_obj_set_style_text_color(wifi_label, lv_color_white(), 0);
    }
}

void ui_overlays_bind_navigation_button(lv_obj_t *button, lv_obj_t *target_screen)
{
    if (!button || !target_screen) {
        return;
    }

    lv_obj_add_event_cb(button, navigate_to_screen_cb, LV_EVENT_CLICKED, target_screen);
}
