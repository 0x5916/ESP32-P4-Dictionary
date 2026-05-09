#include "status_bar.h"
#include "ui/ui.h"
#include "ui_helpers.h"
#include "bsp/display.h"
#include "esp_log.h"
#include "settings_service.h"

#define STATUS_BAR_HEIGHT 50

static const char *TAG = "status_bar";

static lv_obj_t *status_bar;
static lv_obj_t *time_label;
static lv_obj_t *wifi_label;

void status_bar_create(void)
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
    status_bar_apply_theme(dark_mode);
}

void status_bar_set_time_text(const char *text)
{
    ui_label_set_text_if(time_label, text);
}

void status_bar_set_wifi_text(const char *text)
{
    ui_label_set_text_if(wifi_label, text);
}

void status_bar_apply_theme(bool dark_mode)
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