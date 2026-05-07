#include "clock_service.h"
#include "ui_overlays.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include <time.h>
#include <stdio.h>

static const char *TAG = "clock_service";

static void clock_update_label(void) {
    time_t now;
    struct tm timeinfo;
    char buf[8];

    time(&now);
    localtime_r(&now, &timeinfo);

    bool time_valid = (timeinfo.tm_year >= (2024 - 1900));
    if (!time_valid) {
        snprintf(buf, sizeof(buf), "--:--");
        ESP_LOGD(TAG, "[TIME] Time not synced yet");
    } else {
        strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
        ESP_LOGD(TAG, "[TIME] Time: %s (year=%d)", buf, timeinfo.tm_year + 1900);
    }

    bsp_display_lock(-1);
    ui_overlays_set_time_text(buf);
    bsp_display_unlock();
}

void clock_service_init(void) {
    ESP_LOGI(TAG, "[INIT] clock_service_init()");
    clock_update_label();
}

void clock_service_notify_time_synced(void) {
    ESP_LOGI(TAG, "[TIME] Time sync notification received");
    clock_update_label();
}
