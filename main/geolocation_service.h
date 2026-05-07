#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialize geolocation service
 */
void geolocation_service_init(void);

/**
 * Detect timezone from IP geolocation
 * Only triggers detection if timezone is set to AUTO mode
 * Result is saved to settings and clock display is updated
 */
esp_err_t geolocation_service_detect_timezone(void);

/**
 * Check if a geolocation detection is in progress
 */
bool geolocation_service_is_detecting(void);

/**
 * Get the last detected timezone index for AUTO mode.
 */
uint8_t geolocation_service_get_detected_timezone_index(void);
