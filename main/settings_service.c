#include "settings_service.h"

#include "nvs.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "settings_service";
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

    if (strcmp(key, SETTINGS_KEY_WIFI) == 0) {
        return &s_cached.wifi_default_on;
    }
    if (strcmp(key, SETTINGS_KEY_FALLBACK) == 0) {
        return &s_cached.online_fallback_enabled;
    }
    if (strcmp(key, SETTINGS_KEY_SHOW_ZH) == 0) {
        return &s_cached.show_chinese_definition;
    }
    if (strcmp(key, SETTINGS_KEY_HISTORY) == 0) {
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
    ESP_LOGD(TAG, "[NVS] Loading settings from NVS");
    
    if (!settings) {
        ESP_LOGE(TAG, "[NVS] Invalid argument: settings=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "[NVS] Namespace not found, using defaults");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[NVS] Failed to open namespace: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t value = 0;
    if (nvs_get_u8(handle, SETTINGS_KEY_WIFI, &value) == ESP_OK) {
        settings_apply_nvs_value(SETTINGS_KEY_WIFI, value);
        ESP_LOGD(TAG, "[NVS] wifi_on=%u", value);
    }
    if (nvs_get_u8(handle, SETTINGS_KEY_FALLBACK, &value) == ESP_OK) {
        settings_apply_nvs_value(SETTINGS_KEY_FALLBACK, value);
        ESP_LOGD(TAG, "[NVS] fallback_en=%u", value);
    }
    if (nvs_get_u8(handle, SETTINGS_KEY_SHOW_ZH, &value) == ESP_OK) {
        settings_apply_nvs_value(SETTINGS_KEY_SHOW_ZH, value);
        ESP_LOGD(TAG, "[NVS] show_zh=%u", value);
    }
    if (nvs_get_u8(handle, SETTINGS_KEY_HISTORY, &value) == ESP_OK) {
        settings_apply_nvs_value(SETTINGS_KEY_HISTORY, value);
        ESP_LOGD(TAG, "[NVS] history=%u", value);
    }
    if (nvs_get_u8(handle, SETTINGS_KEY_DARK_MODE, &value) == ESP_OK) {
        settings_apply_nvs_value(SETTINGS_KEY_DARK_MODE, value);
        ESP_LOGD(TAG, "[NVS] dark_mode=%u", value);
    }
    if (nvs_get_u8(handle, SETTINGS_KEY_BRIGHTNESS, &value) == ESP_OK) {
        settings_apply_nvs_u8_value(SETTINGS_KEY_BRIGHTNESS, value);
        ESP_LOGD(TAG, "[NVS] brightness=%u", value);
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "[NVS] Settings loaded from NVS successfully");
    return ESP_OK;
}

static esp_err_t settings_save_to_nvs(const app_settings_t *settings)
{
    ESP_LOGD(TAG, "[NVS] Saving settings to NVS");
    
    if (!settings) {
        ESP_LOGE(TAG, "[NVS] Invalid argument: settings=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[NVS] Failed to open namespace: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, SETTINGS_KEY_WIFI, settings->wifi_default_on ? 1 : 0);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "[NVS] Set wifi_on=%u", settings->wifi_default_on);
        err = nvs_set_u8(handle, SETTINGS_KEY_FALLBACK, settings->online_fallback_enabled ? 1 : 0);
    }
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "[NVS] Set fallback_en=%u", settings->online_fallback_enabled);
        err = nvs_set_u8(handle, SETTINGS_KEY_SHOW_ZH, settings->show_chinese_definition ? 1 : 0);
    }
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "[NVS] Set show_zh=%u", settings->show_chinese_definition);
        err = nvs_set_u8(handle, SETTINGS_KEY_HISTORY, settings->save_history ? 1 : 0);
    }
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "[NVS] Set history=%u", settings->save_history);
        err = nvs_set_u8(handle, SETTINGS_KEY_DARK_MODE, settings->dark_mode_enabled ? 1 : 0);
    }
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "[NVS] Set dark_mode=%u", settings->dark_mode_enabled);
        err = nvs_set_u8(handle, SETTINGS_KEY_BRIGHTNESS, settings->brightness_percent);
    }
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "[NVS] Set brightness=%u", settings->brightness_percent);
        err = nvs_commit(handle);
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[NVS] Save failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "[NVS] Settings saved to NVS successfully");
    }

    nvs_close(handle);
    return err;
}

static esp_err_t settings_write_bool_key(const char *key, bool value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[NVS] Failed to open namespace for key '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, key, value ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "[NVS] Successfully saved bool key '%s' = %u", key, value ? 1 : 0);
        } else {
            ESP_LOGE(TAG, "[NVS] Commit failed for key '%s': %s", key, esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "[NVS] nvs_set_u8 failed for key '%s': %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

static esp_err_t settings_write_u8_key(const char *key, uint8_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[NVS] Failed to open namespace for key '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "[NVS] Successfully saved u8 key '%s' = %u", key, value);
        } else {
            ESP_LOGE(TAG, "[NVS] Commit failed for key '%s': %s", key, esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "[NVS] nvs_set_u8 failed for key '%s': %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

esp_err_t settings_init(void)
{
    ESP_LOGI(TAG, "[INIT] settings_init()");
    
    settings_apply_defaults(&s_cached);
    ESP_LOGD(TAG, "[INIT] Defaults applied - dark_mode=%u, brightness=%u, show_zh=%u", 
             s_cached.dark_mode_enabled, s_cached.brightness_percent, s_cached.show_chinese_definition);
    
    esp_err_t err = settings_load_from_nvs(&s_cached);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[INIT] NVS load failed: %s, using defaults", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "[INIT] Settings loaded from NVS - dark_mode=%u, brightness=%u, show_zh=%u",
                 s_cached.dark_mode_enabled, s_cached.brightness_percent, s_cached.show_chinese_definition);
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "[INIT] Settings initialization complete");
    return ESP_OK;
}

esp_err_t settings_load(app_settings_t *out)
{
    ESP_LOGD(TAG, "[API] settings_load()");
    
    if (!out) {
        ESP_LOGE(TAG, "[API] Invalid argument: out=NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        ESP_LOGD(TAG, "[API] Not initialized, calling settings_init()");
        settings_init();
    }

    *out = s_cached;
    ESP_LOGD(TAG, "[API] Settings loaded");
    return ESP_OK;
}

esp_err_t settings_save(const app_settings_t *in)
{
    ESP_LOGD(TAG, "[API] settings_save()");
    
    if (!in) {
        ESP_LOGE(TAG, "[API] Invalid argument: in=NULL");
        return ESP_ERR_INVALID_ARG;
    }

    s_cached = *in;
    s_initialized = true;
    
    esp_err_t err = settings_save_to_nvs(in);
    ESP_LOGD(TAG, "[API] Settings save result: %s", esp_err_to_name(err));
    return err;
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
