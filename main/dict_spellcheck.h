#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "dict.h"
#include "esp_err.h"

/**
 * @brief  Load the wordlist.bin from SD card into PSRAM.
 *         Must be called once after SD card is mounted.
 */
esp_err_t dict_spellcheck_init(void);

/**
 * @brief  Suggest words for a misspelled input using edit distance + frequency.
 *
 * @param  word            Input word (may be misspelled)
 * @param  suggestions     Output array of suggested words
 * @param  max_suggestions Maximum number of suggestions to return
 * @return Number of suggestions written
 */
size_t dict_spellcheck_suggest(const char *word, char suggestions[][MAX_WORD_LEN], size_t max_suggestions);

/**
 * @brief  Search for words matching a given prefix (autocomplete).
 *         Results are sorted by frequency rank (most common first).
 *
 * @param  prefix       Prefix string to match
 * @param  results      Output array of matching words
 * @param  max_results  Maximum number of results to return
 * @return Number of results written
 */
size_t dict_spellcheck_prefix_search(const char *prefix, char results[][MAX_WORD_LEN], size_t max_results);

/**
 * @brief  Check if the spellcheck engine is initialized and wordlist is loaded.
 */
bool dict_spellcheck_is_ready(void);

/**
 * @brief  Free PSRAM buffer and cleanup.
 */
void dict_spellcheck_deinit(void);