#include "dict_lookup.h"

#include "dict_data.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "dict_lookup";

void dict_manager_init(void)
{
	dict_lookup_init();
}

esp_err_t dict_lookup_init(void)
{
	ESP_LOGI(TAG, "[INIT] dict_lookup_init()");
	return ESP_OK;
}

esp_err_t dict_lookup(const char *word, dict_entry_t *out_entry)
{
	ESP_LOGD(TAG, "[LOOKUP] Entry: word='%s'", word);
	
	if (!word || !out_entry) {
		ESP_LOGE(TAG, "[LOOKUP] Invalid arguments (word=%p out_entry=%p)", word, out_entry);
		return ESP_ERR_INVALID_ARG;
	}

	const dict_entry_t *entry = dict_data_find(word);
	if (!entry) {
		ESP_LOGD(TAG, "[LOOKUP] Word not found: '%s'", word);
		return ESP_ERR_NOT_FOUND;
	}

	memcpy(out_entry, entry, sizeof(dict_entry_t));
	ESP_LOGI(TAG, "[LOOKUP] Found word '%s' (senses=%u)", word, entry->sense_count);
	return ESP_OK;
}

void dict_lookup_deinit(void)
{
}
