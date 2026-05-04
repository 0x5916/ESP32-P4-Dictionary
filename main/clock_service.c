#include "clock_service.h"
#include "ui_overlays.h"
#include "bsp/esp-bsp.h"
#include <time.h>
#include <stdio.h>

static void clock_update_label(void) {
    time_t now;
    struct tm timeinfo;
    char buf[8];

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2024 - 1900)) {
        snprintf(buf, sizeof(buf), "--:--");
    } else {
        strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    }

    bsp_display_lock(-1);
    ui_overlays_set_wifi_text(buf);
    bsp_display_unlock();
}

void clock_service_init(void) {
    // Initialization code for the clock service
}

void clock_service_notify_time_synced(void) {
    clock_update_label();
}
