#include "timezone_data.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "timezone_data";

// Timezone list: 15 common timezones
static const timezone_entry_t s_timezones[] = {
    {"UTC (UTC+0)", 0},                      // 0
    {"EST/EDT (UTC-5/-4)", -300},            // 1
    {"CST/CDT (UTC-6/-5)", -360},            // 2
    {"MST/MDT (UTC-7/-6)", -420},            // 3
    {"PST/PDT (UTC-8/-7)", -480},            // 4
    {"GMT/BST (UTC+0/+1)", 0},               // 5
    {"CET/CEST (UTC+1/+2)", 60},             // 6
    {"IST (UTC+5:30)", 330},                 // 7
    {"JST (UTC+9)", 540},                    // 8
    {"AEST/AEDT (UTC+10/+11)", 600},         // 9
    {"NZST/NZDT (UTC+12/+13)", 720},         // 10
    {"SGT (UTC+8)", 480},                    // 11
    {"HKT (UTC+8)", 480},                    // 12
    {"BRT/BRST (UTC-3/-2)", -180},           // 13
    {"SAST (UTC+2)", 120},                   // 14
};

#define TIMEZONE_COUNT (sizeof(s_timezones) / sizeof(s_timezones[0]))

const char *timezone_get_name(uint8_t index)
{
    if (index == TIMEZONE_INDEX_AUTO) {
        return "Auto";
    }
    if (index < 1 || index > TIMEZONE_COUNT) {
        return "Unknown";
    }
    return s_timezones[index - 1].name;
}

int timezone_get_offset_minutes(uint8_t index)
{
    if (index == TIMEZONE_INDEX_AUTO || index > TIMEZONE_COUNT) {
        return 0;  // Default to UTC
    }
    return s_timezones[index - 1].offset_minutes;
}

bool timezone_is_valid(uint8_t index)
{
    return (index == TIMEZONE_INDEX_AUTO) || (index >= 1 && index <= TIMEZONE_COUNT);
}

uint8_t timezone_get_count(void)
{
    return TIMEZONE_COUNT;
}

uint8_t timezone_find_by_name(const char *tz_name)
{
    if (!tz_name) {
        return TIMEZONE_INDEX_AUTO;
    }

    // List of common IANA timezone names that map to our indices
    // These are what the geolocation API typically returns
    if (strcmp(tz_name, "UTC") == 0) return 1;                     // UTC
    if (strcmp(tz_name, "America/New_York") == 0) return 2;        // EST/EDT
    if (strcmp(tz_name, "America/Chicago") == 0) return 3;         // CST/CDT
    if (strcmp(tz_name, "America/Denver") == 0) return 4;          // MST/MDT
    if (strcmp(tz_name, "America/Los_Angeles") == 0) return 5;     // PST/PDT
    if (strcmp(tz_name, "Europe/London") == 0) return 6;           // GMT/BST
    if (strcmp(tz_name, "Europe/Paris") == 0) return 7;            // CET/CEST
    if (strcmp(tz_name, "Asia/Kolkata") == 0) return 8;            // IST
    if (strcmp(tz_name, "Asia/Tokyo") == 0) return 9;              // JST
    if (strcmp(tz_name, "Australia/Sydney") == 0) return 10;       // AEST/AEDT
    if (strcmp(tz_name, "Pacific/Auckland") == 0) return 11;       // NZST/NZDT
    if (strcmp(tz_name, "Asia/Singapore") == 0) return 12;         // SGT
    if (strcmp(tz_name, "Asia/Hong_Kong") == 0) return 13;         // HKT
    if (strcmp(tz_name, "America/Sao_Paulo") == 0) return 14;      // BRT/BRST
    if (strcmp(tz_name, "Africa/Johannesburg") == 0) return 15;    // SAST

    ESP_LOGW(TAG, "[TZ] Unknown timezone: %s, defaulting to UTC", tz_name);
    return TIMEZONE_INDEX_AUTO;  // Default to AUTO
}
