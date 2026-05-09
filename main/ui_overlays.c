#include "ui_overlays.h"

#include "ui/screens.h"
#include "status_bar.h"
#include "keyboard.h"
#include "search_ui.h"
#include "definition_ui.h"
#include "navigation.h"
#include "settings_service.h"
#include "settings_ui.h"
#include "dict_lookup.h"
#include "esp_log.h"
#include "bsp/display.h"

static const char *TAG = "ui_overlays";

void ui_overlays_init(void)
{
    ESP_LOGI(TAG, "[INIT] ui_overlays_init() - Applying saved settings");
    
    bool dark_mode = false;
    esp_err_t err = settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &dark_mode);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[INIT] Loaded dark_mode from settings: %u", dark_mode);
    } else {
        ESP_LOGW(TAG, "[INIT] Failed to load dark_mode, using default: false");
    }
    settings_ui_apply_theme(dark_mode);

    uint8_t brightness = 100;
    err = settings_get_u8(SETTINGS_KEY_BRIGHTNESS, 100, &brightness);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[INIT] Loaded brightness from settings: %u%%", brightness);
    } else {
        ESP_LOGW(TAG, "[INIT] Failed to load brightness, using default: 100%%");
    }
    int set_err = bsp_display_brightness_set((int)brightness);
    if (set_err != 0) {
        ESP_LOGE(TAG, "[INIT] Failed to apply brightness: %d", set_err);
    }

    bool show_zh = true;
    err = settings_get_bool(SETTINGS_KEY_SHOW_ZH, true, &show_zh);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[INIT] Loaded show_chinese_definition from settings: %u", show_zh);
    } else {
        ESP_LOGW(TAG, "[INIT] Failed to load show_chinese_definition, using default: true");
    }

    settings_ui_build(objects.settings_cont);
    status_bar_create();
    keyboard_create();
    search_ui_create_suggest_timer();

    /* Initialize dictionary lookup and register result callback */
    dict_lookup_init();
    dict_lookup_set_result_cb(definition_ui_lookup_result_cb);
}

void ui_overlays_bind_textarea(lv_obj_t *textarea)
{
    search_ui_bind_textarea(textarea);
}

void ui_overlays_bind_search_open_button(lv_obj_t *button)
{
    search_ui_bind_search_open_button(button);
}

void ui_overlays_bind_swipe_back(lv_obj_t *screen, lv_obj_t *target_screen)
{
    navigation_bind_swipe_back(screen, target_screen);
}

void ui_overlays_set_time_text(const char *text)
{
    status_bar_set_time_text(text);
}

void ui_overlays_set_wifi_text(const char *text)
{
    status_bar_set_wifi_text(text);
}

void ui_overlays_apply_theme(bool dark_mode)
{
    status_bar_apply_theme(dark_mode);
}

void ui_overlays_bind_navigation_button(lv_obj_t *button, lv_obj_t *target_screen)
{
    navigation_bind_button(button, target_screen);
}

void ui_overlays_open_definition(const char *word)
{
    definition_ui_open(word);
}