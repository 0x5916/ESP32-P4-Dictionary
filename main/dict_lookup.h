#pragma once
#include "dict.h"
#include <esp_err.h>
#include <stdbool.h>

/**
 * @brief Callback invoked on the LVGL task when a lookup completes.
 *
 * @param entry   Parsed entry (valid only when success=true)
 * @param success true if the word was found and parsed successfully
 */
typedef void (*dict_lookup_result_cb_t)(const dict_entry_t *entry, bool success);

/**
 * @brief  Initialize the dictionary lookup module.
 *         Call once in app_main() before any lookup.
 */
esp_err_t dict_lookup_init(void);

/**
 * @brief  Register a callback to receive lookup results on the LVGL task.
 */
void dict_lookup_set_result_cb(dict_lookup_result_cb_t cb);

/**
 * @brief  Fetch dictionary entry for `word` from the Free Dictionary API.
 *         Runs HTTP request on a background task; result delivered via callback.
 *
 * @param  word  Null-terminated search word
 */
void dict_lookup_word(const char *word);

/**
 * @brief  Free any resources held by the lookup module.
 */
void dict_lookup_deinit(void);