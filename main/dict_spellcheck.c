#include "dict_spellcheck.h"

#include "dict_data.h"

#include <ctype.h>
#include <string.h>

static int dict_prefix_match(const char *word, const char *candidate)
{
	if (!word || !candidate) {
		return 0;
	}

	while (*word && *candidate) {
		int cw = tolower((unsigned char)*word++);
		int cc = tolower((unsigned char)*candidate++);
		if (cw != cc) {
			return 0;
		}
	}

	return *word == '\0';
}

esp_err_t dict_spellcheck_init(void)
{
	return ESP_OK;
}

size_t dict_spellcheck_suggest(const char *word, char suggestions[][64], size_t max_suggestions)
{
	if (!word || !suggestions || max_suggestions == 0) {
		return 0;
	}

	size_t count = 0;
	size_t entries_count = 0;
	const dict_entry_t *entries = dict_data_entries(&entries_count);

	for (size_t i = 0; i < entries_count && count < max_suggestions; ++i) {
		if (dict_prefix_match(word, entries[i].keyword)) {
			strncpy(suggestions[count], entries[i].keyword, sizeof(suggestions[count]));
			suggestions[count][sizeof(suggestions[count]) - 1] = '\0';
			++count;
		}
	}

	return count;
}

void dict_spellcheck_deinit(void)
{
}
