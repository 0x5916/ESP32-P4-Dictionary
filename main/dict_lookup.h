#pragma once
#include "dict.h"
#include <esp_err.h>

void dict_manager_init(void);

/**
 * @brief  Initialize the HTTP client and response buffer.
 *         Call once in app_main() before any lookup.
 */
esp_err_t dict_lookup_init(void);

/**
 * @brief  Fetch and parse the dictionary entry for `word`.
 *         Checks cache first. Falls back to HTTP if not cached.
 *
 * @param  word        Null-terminated search word (max 63 chars)
 * @param  out_entry   Caller-allocated struct to populate
 * @return ESP_OK on success
 *         ESP_ERR_NOT_FOUND if word not in dictionary
 *         ESP_FAIL on network or parse error
 */
esp_err_t dict_lookup(const char *word, dict_entry_t *out_entry);

/**
 * @brief  Free any resources held by the lookup module.
 *         Call on shutdown or before sleep.
 */
void dict_lookup_deinit(void);
