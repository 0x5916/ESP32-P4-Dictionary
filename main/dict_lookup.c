#include "dict_lookup.h"

#include "dict_data.h"

#include <string.h>

void dict_manager_init(void)
{
	dict_lookup_init();
}

esp_err_t dict_lookup_init(void)
{
	return ESP_OK;
}

esp_err_t dict_lookup(const char *word, dict_entry_t *out_entry)
{
	if (!word || !out_entry) {
		return ESP_ERR_INVALID_ARG;
	}

	const dict_entry_t *entry = dict_data_find(word);
	if (!entry) {
		return ESP_ERR_NOT_FOUND;
	}

	memcpy(out_entry, entry, sizeof(dict_entry_t));
	return ESP_OK;
}

void dict_lookup_deinit(void)
{
}
