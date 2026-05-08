#include "esp_log.h"
#include "esp_memory_utils.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "ui/ui.h"
#include "ui_overlays.h"
#include "ui_events.h"
#include "wifi_service.h"
#include "clock_service.h"
#include "geolocation_service.h"
#include "nvs_flash.h"
#include "settings_service.h"
#include "dict_spellcheck.h"

static const char *TAG = "main";

esp_err_t custom_nvs_flash_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    return ret;
}

void app_main(void)
{
    ESP_LOGI(TAG, "[INIT] app_main() starting");
    
    ESP_LOGD(TAG, "[INIT] Initializing NVS flash");
    esp_err_t ret = custom_nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[INIT] Failed to initialize NVS flash: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "[INIT] NVS flash initialized");
    
    ESP_LOGD(TAG, "[INIT] Initializing settings");
    settings_init();
    ESP_LOGI(TAG, "[INIT] Settings loaded");
    
    ESP_LOGD(TAG, "[INIT] Initializing clock service");
    clock_service_init();
    ESP_LOGI(TAG, "[INIT] Clock service initialized");
    
    ESP_LOGD(TAG, "[INIT] Initializing geolocation service");
    geolocation_service_init();
    ESP_LOGI(TAG, "[INIT] Geolocation service initialized");
    
    ESP_LOGI(TAG, "[TASK] WiFi task starting");
    wifi_service_init();
    ESP_LOGI(TAG, "[TASK] WiFi service initialized");

    ESP_LOGD(TAG, "[INIT] Starting display");
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = {
            .task_stack_size = ESP_LV_ADAPTER_DEFAULT_STACK_SIZE,
            .task_priority = ESP_LV_ADAPTER_DEFAULT_TASK_PRIORITY,
            .task_core_id = ESP_LV_ADAPTER_DEFAULT_TASK_CORE_ID,
            .tick_period_ms = ESP_LV_ADAPTER_DEFAULT_TICK_PERIOD_MS,
            .task_min_delay_ms = 1,
            .task_max_delay_ms = 33,
            .stack_in_psram = false,
        },
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0
        }};
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "[INIT] Display started and backlight on");

    ESP_LOGD(TAG, "[INIT] Initializing UI");
    bsp_display_lock(-1);
    ui_init();
    ESP_LOGI(TAG, "[TASK] Overlays task starting");
    ui_overlays_init();
    ESP_LOGD(TAG, "[TASK] UI overlays initialized");
    ui_events_init();
    ESP_LOGI(TAG, "[TASK] UI events initialized");
    bsp_display_unlock();
    ESP_LOGI(TAG, "[INIT] UI initialized");

    ESP_LOGD(TAG, "[INIT] Initializing spellcheck engine");
    esp_err_t sc_err = dict_spellcheck_init();
    if (sc_err == ESP_OK) {
        ESP_LOGI(TAG, "[INIT] Spellcheck engine initialized");
    } else {
        ESP_LOGW(TAG, "[INIT] Spellcheck engine failed to initialize: %s (suggestions disabled)", esp_err_to_name(sc_err));
    }

    ESP_LOGI(TAG, "[INIT] Initialization complete");
}
