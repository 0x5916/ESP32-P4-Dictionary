#include "settings_service.h"

#include "nvs.h"

#include <string.h>

static const char *SETTINGS_NVS_NAMESPACE = "app_settings";

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
    settings->dark_mode_enabled = false;
    settings->brightness_percent = 100;
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
    if (strcmp(key, SETTINGS_KEY_DARK_MODE) == 0) {
        return &s_cached.dark_mode_enabled;
    }

    return NULL;
}

static uint8_t *settings_get_u8_field(const char *key)
{
    if (!key) {
        return NULL;
    }

    if (strcmp(key, SETTINGS_KEY_BRIGHTNESS) == 0) {
        return &s_cached.brightness_percent;
    }

    return NULL;
}

static void settings_apply_nvs_value(const char *key, uint8_t value)
{
    bool *field = settings_get_bool_field(key);
    if (field) {
        *field = value != 0;
    }
}

static void settings_apply_nvs_u8_value(const char *key, uint8_t value)
{
    uint8_t *field = settings_get_u8_field(key);
    if (field) {
        *field = value;
    }
}

static esp_err_t settings_load_from_nvs(app_settings_t *settings)
{
    if (!settings) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    uint8_t value = 0;
    if (nvs_get_u8(handle, "wifi_default_on", &value) == ESP_OK) {
        settings_apply_nvs_value("wifi_default_on", value);
    }
    if (nvs_get_u8(handle, "online_fallback_enabled", &value) == ESP_OK) {
        settings_apply_nvs_value("online_fallback_enabled", value);
    }
    if (nvs_get_u8(handle, "show_chinese_definition", &value) == ESP_OK) {
        settings_apply_nvs_value("show_chinese_definition", value);
    }
    if (nvs_get_u8(handle, "save_history", &value) == ESP_OK) {
        settings_apply_nvs_value("save_history", value);
    }
    if (nvs_get_u8(handle, SETTINGS_KEY_DARK_MODE, &value) == ESP_OK) {
        settings_apply_nvs_value(SETTINGS_KEY_DARK_MODE, value);
    }
    if (nvs_get_u8(handle, SETTINGS_KEY_BRIGHTNESS, &value) == ESP_OK) {
        settings_apply_nvs_u8_value(SETTINGS_KEY_BRIGHTNESS, value);
    }

    nvs_close(handle);
    return ESP_OK;
}

static esp_err_t settings_save_to_nvs(const app_settings_t *settings)
{
    if (!settings) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, "wifi_default_on", settings->wifi_default_on ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "online_fallback_enabled", settings->online_fallback_enabled ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "show_chinese_definition", settings->show_chinese_definition ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "save_history", settings->save_history ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, SETTINGS_KEY_DARK_MODE, settings->dark_mode_enabled ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, SETTINGS_KEY_BRIGHTNESS, settings->brightness_percent);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

static esp_err_t settings_write_bool_key(const char *key, bool value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, key, value ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

static esp_err_t settings_write_u8_key(const char *key, uint8_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t settings_init(void)
{
    settings_apply_defaults(&s_cached);
    settings_load_from_nvs(&s_cached);
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
    return settings_save_to_nvs(in);
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
    return settings_write_bool_key(key, value);
}

esp_err_t settings_set_u8(const char *key, uint8_t value)
{
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        settings_init();
    }

    uint8_t *field = settings_get_u8_field(key);
    if (!field) {
        return ESP_ERR_NOT_FOUND;
    }

    *field = value;
    return settings_write_u8_key(key, value);
}

esp_err_t settings_get_u8(const char *key, uint8_t def, uint8_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        settings_init();
    }

    uint8_t *field = settings_get_u8_field(key);
    if (!field) {
        *out = def;
        return ESP_ERR_NOT_FOUND;
    }

    *out = *field;
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
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_erase_all(handle);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    return err;
}

const app_settings_t *settings_get_cached(void)
{
    if (!s_initialized) {
        settings_init();
    }

    return &s_cached;
}
