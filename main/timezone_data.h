#pragma once

#include <stdint.h>
#include <stdbool.h>

#define TIMEZONE_INDEX_AUTO 0  // Special index for auto-detect mode

typedef struct {
    const char *name;           // Display name (e.g., "EST/EDT (UTC-5)")
    int offset_minutes;         // Offset from UTC in minutes
} timezone_entry_t;

// Get timezone name by index
const char *timezone_get_name(uint8_t index);

// Get UTC offset in minutes by index
int timezone_get_offset_minutes(uint8_t index);

// Validate timezone index
bool timezone_is_valid(uint8_t index);

// Get total number of timezones
uint8_t timezone_get_count(void);

// Get timezone index by name (for geolocation matching)
uint8_t timezone_find_by_name(const char *tz_name);
