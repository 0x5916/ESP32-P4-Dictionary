#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct
{
    bool wifi_default_on;
    bool online_fallback_enabled;
    bool show_chinese_definition;
    bool save_history;
    bool dark_mode_enabled;
    uint8_t brightness_percent;
} app_settings_t;

#define SETTINGS_KEY_DARK_MODE "dark_mode"
#define SETTINGS_KEY_BRIGHTNESS "brightness"
#define SETTINGS_KEY_SHOW_ZH "show_zh"
#define SETTINGS_KEY_WIFI "wifi_on"
#define SETTINGS_KEY_FALLBACK "fallback_en"
#define SETTINGS_KEY_HISTORY "history"

esp_err_t settings_init(void);
esp_err_t settings_load(app_settings_t *out);
esp_err_t settings_save(const app_settings_t *in);
esp_err_t settings_set_bool(const char *key, bool value);
esp_err_t settings_get_bool(const char *key, bool def, bool *out);
esp_err_t settings_set_u8(const char *key, uint8_t value);
esp_err_t settings_get_u8(const char *key, uint8_t def, uint8_t *out);
esp_err_t settings_reset(void);
const app_settings_t *settings_get_cached(void);
