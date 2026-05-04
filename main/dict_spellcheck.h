#pragma once

#include <stddef.h>
#include "esp_err.h"

esp_err_t dict_spellcheck_init(void);
size_t dict_spellcheck_suggest(const char *word, char suggestions[][64], size_t max_suggestions);
void dict_spellcheck_deinit(void);
