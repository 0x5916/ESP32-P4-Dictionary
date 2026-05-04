#include "settings_service.h"

#include <string.h>

static app_settings_t s_cached;
static bool s_initialized;

static void settings_apply_defaults(app_settings_t *settings)
{
    if (!settings) {
        return;
    }

    settings->wifi_default_on = true;
    settings->online_fallback_enabled = true;
    settings->show_chinese_definition = true;
    settings->save_history = true;
}

static bool *settings_get_bool_field(const char *key)
{
    if (!key) {
        return NULL;
    }

    if (strcmp(key, "wifi_default_on") == 0) {
        return &s_cached.wifi_default_on;
    }
    if (strcmp(key, "online_fallback_enabled") == 0) {
        return &s_cached.online_fallback_enabled;
    }
    if (strcmp(key, "show_chinese_definition") == 0) {
        return &s_cached.show_chinese_definition;
    }
    if (strcmp(key, "save_history") == 0) {
        return &s_cached.save_history;
    }

    return NULL;
}

esp_err_t settings_init(void)
{
    settings_apply_defaults(&s_cached);
    s_initialized = true;
    return ESP_OK;
}

esp_err_t settings_load(app_settings_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        settings_init();
    }

    *out = s_cached;
    return ESP_OK;
}

esp_err_t settings_save(const app_settings_t *in)
{
    if (!in) {
        return ESP_ERR_INVALID_ARG;
    }

    s_cached = *in;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t settings_set_bool(const char *key, bool value)
{
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        settings_init();
    }

    bool *field = settings_get_bool_field(key);
    if (!field) {
        return ESP_ERR_NOT_FOUND;
    }

    *field = value;
    return ESP_OK;
}

esp_err_t settings_get_bool(const char *key, bool def, bool *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        settings_init();
    }

    bool *field = settings_get_bool_field(key);
    if (!field) {
        *out = def;
        return ESP_ERR_NOT_FOUND;
    }

    *out = *field;
    return ESP_OK;
}

esp_err_t settings_reset(void)
{
    settings_apply_defaults(&s_cached);
    s_initialized = true;
    return ESP_OK;
}

const app_settings_t *settings_get_cached(void)
{
    if (!s_initialized) {
        settings_init();
    }

    return &s_cached;
}
