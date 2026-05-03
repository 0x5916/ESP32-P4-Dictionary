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

esp_err_t sd_card_read_file(const char *path, void *buffer, size_t max_size, size_t *out_size)
{
    if (!sd_card_is_mounted()) {
        ESP_LOGW(TAG, "SD card is not mounted. Cannot read file: %s", path);
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);

    FILE *file = fopen(full_path, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", full_path);
        return ESP_FAIL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        ESP_LOGE(TAG, "Failed to determine file size: %s", full_path);
        fclose(file);
        return ESP_FAIL;
    }

    if ((size_t)file_size > max_size) {
        ESP_LOGW(TAG, "File size (%ld bytes) exceeds buffer size (%zu bytes). Truncating.", file_size, max_size);
        file_size = max_size;
    }

    size_t read_size = fread(buffer, 1, file_size, file);
    if (read_size != (size_t)file_size) {
        ESP_LOGE(TAG, "Failed to read entire file: %s. Read %zu of %ld bytes.", full_path, read_size, file_size);
        fclose(file);
        return ESP_FAIL;
    }

    fclose(file);

    if (out_size) {
        *out_size = read_size;
    }

    ESP_LOGI(TAG, "Successfully read file: %s (%zu bytes)", full_path, read_size);
    return ESP_OK;
}
