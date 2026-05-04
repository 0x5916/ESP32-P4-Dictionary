#pragma once

#include "dict.h"
#include <stddef.h>

const dict_entry_t *dict_data_entries(size_t *out_count);
const dict_entry_t *dict_data_find(const char *word);
