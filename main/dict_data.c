#include "dict_data.h"

#include <ctype.h>
#include <string.h>

static int dict_casecmp(const char *a, const char *b)
{
    if (!a || !b) {
        return (a == b) ? 0 : (a ? 1 : -1);
    }

    while (*a && *b) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) {
            return ca - cb;
        }
    }

    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static const dict_entry_t s_entries[] = {
    {
        .keyword = "hello",
        .pos_count = 1,
        .pos_groups = {
            {
                .pos = "interjection",
                .pron_count = 1,
                .pronunciations = {
                    { .ipa = "/heh-LOH/", .dialect = "US" }
                },
                .sense_count = 1,
                .senses = {
                    {
                        .definition = "Used to greet someone, answer a phone, or express surprise.",
                        .definition_zh = "",
                        .example = "Hello! How are you?"
                    }
                }
            }
        }
    },
    {
        .keyword = "dictionary",
        .pos_count = 2,
        .pos_groups = {
            {
                .pos = "noun",
                .pron_count = 1,
                .pronunciations = {
                    { .ipa = "/DIK-shuh-ner-ee/", .dialect = "US" }
                },
                .sense_count = 1,
                .senses = {
                    {
                        .definition = "A reference book or resource containing words and their meanings.",
                        .definition_zh = "",
                        .example = "I looked it up in the dictionary."
                    }
                }
            },
            {
                .pos = "verb",
                .pron_count = 1,
                .pronunciations = {
                    { .ipa = "/DIK-shuh-ner-ee/", .dialect = "US" }
                },
                .sense_count = 1,
                .senses = {
                    {
                        .definition = "To compile or arrange in a dictionary.",
                        .definition_zh = "",
                        .example = "They decided to dictionary the new terms."
                    }
                }
            }
        }
    },
    {
        .keyword = "offline",
        .pos_count = 2,
        .pos_groups = {
            {
                .pos = "adjective",
                .pron_count = 1,
                .pronunciations = {
                    { .ipa = "/OFF-line/", .dialect = "US" }
                },
                .sense_count = 1,
                .senses = {
                    {
                        .definition = "Not connected to the internet or a network.",
                        .definition_zh = "",
                        .example = "The device can work offline."
                    }
                }
            },
            {
                .pos = "adverb",
                .pron_count = 1,
                .pronunciations = {
                    { .ipa = "/OFF-line/", .dialect = "US" }
                },
                .sense_count = 1,
                .senses = {
                    {
                        .definition = "While not connected to the internet or a network.",
                        .definition_zh = "",
                        .example = "I'll work offline today."
                    }
                }
            }
        }
    }
};

const dict_entry_t *dict_data_entries(size_t *out_count)
{
    if (out_count) {
        *out_count = sizeof(s_entries) / sizeof(s_entries[0]);
    }

    return s_entries;
}

const dict_entry_t *dict_data_find(const char *word)
{
    size_t count = 0;
    const dict_entry_t *entries = dict_data_entries(&count);

    for (size_t i = 0; i < count; ++i) {
        if (dict_casecmp(entries[i].keyword, word) == 0) {
            return &entries[i];
        }
    }

    return NULL;
}
