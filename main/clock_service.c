#include "clock_service.h"
#include "ui_overlays.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_event.h"
#include "geolocation_service.h"
#include "settings_service.h"
#include "timezone_data.h"

#include <time.h>
#include <stdio.h>

static const char *TAG = "clock_service";
static bool s_sntp_initialized = false;

static void clock_update_label(void) {
    time_t now;
    struct tm timeinfo;
    char buf[8];

    time(&now);
    
    // Get current timezone setting and apply offset
    uint8_t tz_index = 0;
    esp_err_t err = settings_get_u8(SETTINGS_KEY_TIMEZONE, TIMEZONE_INDEX_AUTO, &tz_index);
    
    int tz_offset_minutes = 0;
    if (err == ESP_OK) {
        if (tz_index == TIMEZONE_INDEX_AUTO) {
            tz_offset_minutes = timezone_get_offset_minutes(geolocation_service_get_detected_timezone_index());
        } else {
            tz_offset_minutes = timezone_get_offset_minutes(tz_index);
        }
    }
    
    // Apply timezone offset to time
    time_t adjusted_time = now + (tz_offset_minutes * 60);
    gmtime_r(&adjusted_time, &timeinfo);

    bool time_valid = (timeinfo.tm_year >= (2024 - 1900));
    if (!time_valid) {
        snprintf(buf, sizeof(buf), "--:--");
        ESP_LOGD(TAG, "[TIME] Time not synced yet");
    } else {
        strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
        ESP_LOGD(TAG, "[TIME] Time: %s (year=%d, tz_offset=%d min)", buf, timeinfo.tm_year + 1900, tz_offset_minutes);
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

static void sntp_time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "[SNTP] Time synced successfully. Unix timestamp: %ld", tv->tv_sec);
    clock_update_label();
}

void clock_service_start_sntp(void)
{
    if (s_sntp_initialized) {
        ESP_LOGD(TAG, "[SNTP] Already initialized, skipping");
        return;
    }

    ESP_LOGI(TAG, "[SNTP] Starting SNTP time synchronization");
    
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    esp_sntp_set_time_sync_notification_cb(sntp_time_sync_notification_cb);
    esp_sntp_init();
    
    s_sntp_initialized = true;
    ESP_LOGI(TAG, "[SNTP] SNTP initialized");
}

void clock_service_stop_sntp(void)
{
    if (!s_sntp_initialized) {
        ESP_LOGD(TAG, "[SNTP] Not initialized, nothing to stop");
        return;
    }

    ESP_LOGI(TAG, "[SNTP] Stopping SNTP");
    esp_sntp_stop();
    s_sntp_initialized = false;
    ESP_LOGI(TAG, "[SNTP] SNTP stopped");
}
