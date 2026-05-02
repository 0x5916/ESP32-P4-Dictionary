#include "sd_card.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"

static const char *TAG = "sd_card";
static bool sdcard_mounted = false;

esp_err_t sd_card_mount(void)
{
    ESP_LOGI(TAG, "Mounting SD card at %s", "/sdcard");
    esp_err_t ret = bsp_sdcard_mount();
    if (ret == ESP_OK) {
        sdcard_mounted = true;
        ESP_LOGI(TAG, "SD card mounted successfully");
    } else {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t sd_card_unmount(void)
{
    ESP_LOGI(TAG, "Unmounting SD card at %s", "/sdcard");
    esp_err_t ret = bsp_sdcard_unmount();
    if (ret == ESP_OK) {
        sdcard_mounted = false;
        ESP_LOGI(TAG, "SD card unmounted successfully");
    } else {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
    }
    return ret;
}

bool sd_card_is_mounted(void)
{
    return sdcard_mounted;
}
