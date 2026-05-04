#include "esp_log.h"
#include "esp_memory_utils.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "ui/ui.h"
#include "ui_overlays.h"
#include "ui_events.h"
#include "wifi_service.h"
#include "nvs_flash.h"

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
    esp_err_t ret = custom_nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE("main", "Failed to initialize NVS flash");
        return;
    }

    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0
        }};
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    wifi_service_init();

    bsp_display_lock(-1);
    ui_init();
    ui_overlays_init();
    ui_events_init();
    bsp_display_unlock();
}
