#include "dict_spellcheck.h"

#include "dict_data.h"
#include "storage/sd_card.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "dict_spellcheck";

/* ── Module state ─────────────────────────────────────────────────── */
static uint8_t  *s_wordlist_buf   = NULL;   /* PSRAM buffer holding all records */
static uint32_t  s_record_count   = 0;      /* Number of records in the buffer */
static bool      s_initialized    = false;

/* ── QWERTY keyboard layout optimization ──────────────────────────── */
static const char *qwert_neighbors[26][5] = {
    /* a */ {"q", "s", "z"},
    /* b */ {"v", "g", "h"},
    /* c */ {"x", "d", "f"},
    /* d */ {"s", "e", "f", "c"},
    /* e */ {"w", "r", "d"},
    /* f */ {"d", "r", "g", "c", "v"},
    /* g */ {"f", "t", "h", "b"},
    /* h */ {"g", "j", "b", "n"},
    /* i */ {"u", "o", "j"},
    /* j */ {"h", "k", "m", "i", "n"},
    /* k */ {"j", "l", "m"},
    /* l */ {"k", "p", ";"},
    /* m */ {"n", "j", "k"},
    /* n */ {"h", "j", "b", "m"},
    /* o */ {"i", "p", "l"},
    /* p */ {"o", "l", ";"},
    /* q */ {"w", "a"},
    /* r */ {"e", "t", "f"},
    /* s */ {"a", "w", "e", "d", "z"},
    /* t */ {"r", "y", "g"},
    /* u */ {"y", "i", "j"},
    /* v */ {"c", "f", "g", "b"},
    /* w */ {"q", "s", "e"},
    /* x */ {"z", "s", "c"},
    /* y */ {"t", "u", "h"},
    /* z */ {"a", "s", "x"}
};

static bool are_qwert_neighbors(char a, char b) {
    a = tolower((unsigned char)a);
    b = tolower((unsigned char)b);
    
    if (a < 'a' || a > 'z' || b < 'a' || b > 'z') {
        return false;
    }
    
    for (int i = 0; i < 5; i++) {
        if (qwert_neighbors[a - 'a'][i] && qwert_neighbors[a - 'a'][i][0] == b) {
            return true;
        }
    }
    return false;
}

/* ── Record accessor helpers ──────────────────────────────────────── */
/* wordlist.bin layout (from parse_wordlist.py):
 *   Header:  uint32_t  record_count   (4 bytes)
 *   Records: each RECORD_SIZE (19) bytes
 *     [0..15]  char[16]  word (null-padded)
 *     [16]     uint8_t   length
 *     [17..18] uint16_t  freq_rank (little-endian, 1=most common)
 */

static inline const dict_word_entry_t *record_at(uint32_t idx)
{
    return (const dict_word_entry_t *)(s_wordlist_buf + 4 + idx * RECORD_SIZE);
}

static inline const char *word_at(uint32_t idx)
{
    return record_at(idx)->keyword;
}

static inline uint8_t length_at(uint32_t idx)
{
    return record_at(idx)->length;
}

static inline uint16_t rank_at(uint32_t idx)
{
    return record_at(idx)->freq_rank;
}

/* ── Case-insensitive comparison ──────────────────────────────────── */
static int dict_strncasecmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        int ca = tolower((unsigned char)a[i]);
        int cb = tolower((unsigned char)b[i]);
        if (ca != cb || ca == '\0') {
            return ca - cb;
        }
    }
    return 0;
}

/* ── Binary search: find first record whose word >= prefix ────────── */
static uint32_t find_lower_bound(const char *prefix, size_t prefix_len)
{
    uint32_t lo = 0, hi = s_record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const char *w = word_at(mid);
        int cmp = dict_strncasecmp(w, prefix, prefix_len);
        if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/* ── QWERTY-aware Levenshtein distance (limited to MAX_EDIT_DIST) ──── */
static int levenshtein_dist(const char *a, uint8_t alen,
                            const char *b, uint8_t blen)
{
    /* Optimization: if length difference > MAX_EDIT_DIST, skip */
    int diff = (int)alen - (int)blen;
    if (diff > MAX_EDIT_DIST || diff < -MAX_EDIT_DIST) {
        return MAX_EDIT_DIST + 1;
    }

    /* Full 2-row DP (MAX_WORD_LEN=16, so this is tiny) */
    uint8_t prev[MAX_WORD_LEN + 1];
    uint8_t curr[MAX_WORD_LEN + 1];

    for (uint8_t j = 0; j <= blen; ++j) {
        prev[j] = j;
    }
 
    for (uint8_t i = 1; i <= alen; ++i) {
        curr[0] = i;
        uint8_t min_val = i;

        for (uint8_t j = 1; j <= blen; ++j) {
            /* QWERTY-aware cost calculation */
            uint8_t cost;
            char ca = tolower((unsigned char)a[i - 1]);
            char cb = tolower((unsigned char)b[j - 1]);
            
            if (ca == cb) {
                cost = 0;
            } else if (are_qwert_neighbors(ca, cb)) {
                /* Lower cost for adjacent keys on QWERTY keyboard */
                cost = 1;
            } else {
                cost = 2; /* Higher cost for non-adjacent key errors */
            }
            
            uint8_t del = prev[j] + 1;
            uint8_t ins = curr[j - 1] + 1;
            uint8_t sub = prev[j - 1] + cost;
            curr[j] = del < ins ? (del < sub ? del : sub)
                                : (ins < sub ? ins : sub);
            if (curr[j] < min_val) min_val = curr[j];
        }

        /* Early termination: minimum possible distance > MAX_EDIT_DIST */
        if (min_val > MAX_EDIT_DIST) {
            return MAX_EDIT_DIST + 1;
         }

        /* Swap rows */
        memcpy(prev, curr, sizeof(uint8_t) * (blen + 1));
    }

    return prev[blen] > MAX_EDIT_DIST ? (MAX_EDIT_DIST + 1) : prev[blen];
}

/* ── Insertion sort for suggestion ranking ─────────────────────────── */
static void sort_suggestions(dict_suggestion_t *suggs, size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        dict_suggestion_t key = suggs[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && (suggs[j].score > key.score ||
               (suggs[j].score == key.score &&
                strcasecmp(suggs[j].word, key.word) > 0))) {
            suggs[j + 1] = suggs[j];
            --j;
        }
        suggs[j + 1] = key;
    }
}

/* ── Insertion sort for rank-index pairs ───────────────────────────── */
typedef struct { uint16_t rank; uint32_t idx; } rank_entry_t;

/* Static pool to avoid large stack allocation when called from LVGL timer.
 * Only one search runs at a time (single-threaded LVGL), so static is safe. */
#define PREFIX_POOL_SIZE 50
static rank_entry_t s_prefix_pool[PREFIX_POOL_SIZE];

static void sort_by_rank(rank_entry_t *entries, size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        rank_entry_t key = entries[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && entries[j].rank > key.rank) {
            entries[j + 1] = entries[j];
            --j;
        }
        entries[j + 1] = key;
    }
}

/* ══════════════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════════════ */

esp_err_t dict_spellcheck_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[INIT] Loading wordlist from SD card");

    /* Ensure SD card is mounted */
    if (!sd_card_is_mounted()) {
        esp_err_t err = sd_card_mount();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(err));
            return err;
        }
    }

    /* Open wordlist.bin */
    const char *path = "/sdcard/wordlist.bin";
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    /* Read header (4 bytes: record count) */
    uint32_t count = 0;
    if (fread(&count, sizeof(uint32_t), 1, f) != 1) {
        ESP_LOGE(TAG, "Failed to read wordlist header");
        fclose(f);
        return ESP_FAIL;
    }

    if (count == 0 || count > 500000) {
        ESP_LOGE(TAG, "Invalid record count: %u", count);
        fclose(f);
        return ESP_FAIL;
    }

    size_t data_size = (size_t)count * RECORD_SIZE;
    ESP_LOGI(TAG, "Wordlist: %u records, %zu bytes data", count, data_size);

    /* Allocate PSRAM buffer: header (4 bytes) + all records */
    size_t buf_size = 4 + data_size;
    s_wordlist_buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!s_wordlist_buf) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes in PSRAM", buf_size);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    /* Write header into buffer */
    memcpy(s_wordlist_buf, &count, sizeof(uint32_t));

    /* Read all records */
    size_t read_count = fread(s_wordlist_buf + 4, RECORD_SIZE, count, f);
    fclose(f);

    if (read_count != count) {
        ESP_LOGE(TAG, "Read %zu of %u records", read_count, count);
        free(s_wordlist_buf);
        s_wordlist_buf = NULL;
        return ESP_FAIL;
    }

    s_record_count = count;
    s_initialized = true;

    ESP_LOGI(TAG, "[INIT] Wordlist loaded: %u words in PSRAM", count);
    return ESP_OK;
}

bool dict_spellcheck_is_ready(void)
{
    return s_initialized && s_wordlist_buf != NULL;
}

size_t dict_spellcheck_prefix_search(const char *prefix,
                                     char results[][MAX_WORD_LEN],
                                     size_t max_results)
{
    if (!s_initialized || !prefix || !results || max_results == 0) {
        return 0;
    }

    size_t prefix_len = strlen(prefix);
    if (prefix_len == 0) {
        return 0;
    }

    /* Find the first record >= prefix */
    uint32_t start = find_lower_bound(prefix, prefix_len);

    /* Collect prefix-matching words into the static pool (avoids large stack alloc) */
    size_t found = 0;
    for (uint32_t i = start; i < s_record_count && found < PREFIX_POOL_SIZE; ++i) {
        const char *w = word_at(i);
        if (dict_strncasecmp(w, prefix, prefix_len) != 0) {
            break;
        }
        s_prefix_pool[found].rank = rank_at(i);
        s_prefix_pool[found].idx  = i;
        ++found;
    }

    if (found == 0) {
        return 0;
    }

    /* Sort by frequency rank (lower rank = more common = first) */
    sort_by_rank(s_prefix_pool, found);

    /* Copy top-N results */
    size_t out = (found < max_results) ? found : max_results;
    for (size_t i = 0; i < out; ++i) {
        const char *w = word_at(s_prefix_pool[i].idx);
        uint8_t wlen = length_at(s_prefix_pool[i].idx);
        size_t copy_len = (wlen < MAX_WORD_LEN) ? wlen : (MAX_WORD_LEN - 1);
        memcpy(results[i], w, copy_len);
        results[i][copy_len] = '\0';
    }

    return out;
}

size_t dict_spellcheck_suggest(const char *word,
                               char suggestions[][MAX_WORD_LEN],
                               size_t max_suggestions)
{
    if (!s_initialized || !word || !suggestions || max_suggestions == 0) {
        return 0;
    }

    size_t wlen = strlen(word);
    if (wlen == 0) {
        return 0;
    }

    /* First try prefix search – if we have matches, return those */
    size_t prefix_count = dict_spellcheck_prefix_search(word, suggestions, max_suggestions);
    if (prefix_count > 0) {
        ESP_LOGI(TAG, "Prefix search found %zu suggestions for '%s'", prefix_count, word);
        return prefix_count;
    }

    /* No prefix match – fall back to edit distance spell correction.
     * Scan a window around where the word would be inserted alphabetically.
     * Window size: ±512 records to catch most typos, including QWERTY errors. */
    dict_suggestion_t best[MAX_SUGGESTIONS];
    size_t best_count = 0;

    uint32_t center = find_lower_bound(word, wlen);
    int32_t win = 512; /* Increased window size to capture QWERTY errors */

    int32_t start_idx = (int32_t)center - win;
    if (start_idx < 0) start_idx = 0;
    uint32_t end_idx = center + (uint32_t)win;
    if (end_idx > s_record_count) end_idx = s_record_count;

    char input_lower[MAX_WORD_LEN];
    for (size_t i = 0; i < wlen && i < MAX_WORD_LEN - 1; ++i) {
        input_lower[i] = tolower((unsigned char)word[i]);
    }
    input_lower[wlen < MAX_WORD_LEN ? wlen : MAX_WORD_LEN - 1] = '\0';
    uint8_t input_len = (uint8_t)strlen(input_lower);

    for (uint32_t i = (uint32_t)start_idx; i < end_idx; ++i) {
        uint8_t dlen = length_at(i);
        int diff = (int)dlen - (int)input_len;
        if (diff > MAX_EDIT_DIST || diff < -MAX_EDIT_DIST) {
            continue;
        }

        int dist = levenshtein_dist(input_lower, input_len,
                                    word_at(i), dlen);
        if (dist > MAX_EDIT_DIST) {
            continue;
        }

        /* Score = edit_distance * 65536 + freq_rank (prioritize closer match) */
        int score = dist * 65536 + rank_at(i);

        /* Insert into best[] if it qualifies */
        if (best_count < max_suggestions) {
            const char *w = word_at(i);
            size_t copy_len = (dlen < MAX_WORD_LEN) ? dlen : (MAX_WORD_LEN - 1);
            memcpy(best[best_count].word, w, copy_len);
            best[best_count].word[copy_len] = '\0';
            best[best_count].score = score;
            ++best_count;
        } else {
            /* Find the worst entry */
            int worst_idx = 0;
            for (size_t k = 1; k < best_count; ++k) {
                if (best[k].score > best[worst_idx].score) {
                    worst_idx = (int)k;
                }
            }
            if (score < best[worst_idx].score) {
                const char *w = word_at(i);
                size_t copy_len = (dlen < MAX_WORD_LEN) ? dlen : (MAX_WORD_LEN - 1);
                memcpy(best[worst_idx].word, w, copy_len);
                best[worst_idx].word[copy_len] = '\0';
                best[worst_idx].score = score;
            }
        }
    }

    if (best_count == 0) {
        return 0;
    }

    /* Sort by score (ascending) */
    sort_suggestions(best, best_count);

    /* Copy to output */
    for (size_t i = 0; i < best_count; ++i) {
        size_t slen = strlen(best[i].word);
        if (slen >= MAX_WORD_LEN) slen = MAX_WORD_LEN - 1;
        memcpy(suggestions[i], best[i].word, slen);
        suggestions[i][slen] = '\0';
    }

    return best_count;
}

void dict_spellcheck_deinit(void)
{
    if (s_wordlist_buf) {
        free(s_wordlist_buf);
        s_wordlist_buf = NULL;
    }
    s_record_count = 0;
    s_initialized = false;
    ESP_LOGI(TAG, "[DEINIT] Spellcheck engine released");
}