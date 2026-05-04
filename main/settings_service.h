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
} app_settings_t;

#define SETTINGS_KEY_DARK_MODE "dark_mode_enabled"

esp_err_t settings_init(void);
esp_err_t settings_load(app_settings_t *out);
esp_err_t settings_save(const app_settings_t *in);
esp_err_t settings_set_bool(const char *key, bool value);
esp_err_t settings_get_bool(const char *key, bool def, bool *out);
esp_err_t settings_reset(void);
const app_settings_t *settings_get_cached(void);
