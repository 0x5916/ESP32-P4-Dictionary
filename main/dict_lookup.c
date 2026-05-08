#include "dict_lookup.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <string.h>

static const char *TAG = "dict_lookup";

/* Free Dictionary API (freedictionaryapi.com) — no key required */
#define DICT_API_BASE    "https://freedictionaryapi.com/api/v1/entries/en/"
#define MAX_RESPONSE     (64 * 1024)   /* 64 KB response buffer in PSRAM */
#define LOOKUP_TASK_STACK 8192
#define LOOKUP_TASK_PRIO  5

/* ── Module state ─────────────────────────────────────────────────── */
static dict_entry_t              s_result;
static bool                      s_result_valid = false;
static dict_lookup_result_cb_t   s_result_cb   = NULL;
static bool                      s_busy        = false;

/* ── HTTP response accumulator ────────────────────────────────────── */
typedef struct {
    char  *buf;
    int    len;
    int    cap;
} http_resp_t;

static esp_err_t http_event_cb(esp_http_client_event_t *evt)
{
    http_resp_t *r = (http_resp_t *)evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA: {
            int space = r->cap - r->len - 1;  /* -1 for null terminator */
            if (space > 0 && evt->data_len > 0) {
                int to_copy = (evt->data_len < space) ? evt->data_len : space;
                memcpy(r->buf + r->len, evt->data, to_copy);
                r->len += to_copy;
                if (to_copy < evt->data_len) {
                    ESP_LOGW(TAG, "Response buffer full, truncated %d bytes",
                             evt->data_len - to_copy);
                }
            }
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

/* ── JSON → dict_entry_t parser ─────────────────────────────────────
 *
 * Expected response from freedictionaryapi.com:
 *   {
 *     "word": "hello",
 *     "entries": [
 *       {
 *         "language": { "code": "en", "name": "English" },
 *         "partOfSpeech": "noun",
 *         "pronunciations": [
 *           { "type": "ipa", "text": "/həˈloʊ/", "tags": [] }
 *         ],
 *         "senses": [
 *           {
 *             "definition": "...",
 *             "examples": ["..."],
 *             "translations": [
 *               { "language": { "code": "zh" }, "word": "你好" }
 *             ],
 *             "subsenses": []
 *           }
 *         ]
 *       }
 *     ]
 *   }
 * ──────────────────────────────────────────────────────────────────── */
static bool parse_response(const char *json, dict_entry_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        const char *err_ptr = cJSON_GetErrorPtr();
        if (err_ptr) {
            /* Log a snippet around the error for debugging */
            int offset = (int)(err_ptr - json);
            ESP_LOGE(TAG, "JSON parse failed near offset %d: %.40s", offset, err_ptr);
        } else {
            ESP_LOGE(TAG, "JSON parse failed (null input?)");
        }
        return false;
    }

    /* word */
    const cJSON *j_word = cJSON_GetObjectItem(root, "word");
    if (j_word && j_word->valuestring) {
        strncpy(out->keyword, j_word->valuestring, sizeof(out->keyword) - 1);
        out->keyword[sizeof(out->keyword) - 1] = '\0';
    }

    /* entries[] */
    const cJSON *entries = cJSON_GetObjectItem(root, "entries");
    if (!entries || !cJSON_IsArray(entries)) {
        ESP_LOGE(TAG, "No entries array in response");
        cJSON_Delete(root);
        return false;
    }

    char ipa_buf[64] = {0};
    bool ipa_set = false;
    out->sense_count = 0;

    cJSON *entry;
    cJSON_ArrayForEach(entry, entries) {
        if (out->sense_count >= 8) break;

        /* Only English entries */
        const cJSON *lang = cJSON_GetObjectItem(entry, "language");
        if (lang) {
            const cJSON *code = cJSON_GetObjectItem(lang, "code");
            if (code && code->valuestring &&
                strncmp(code->valuestring, "en", 2) != 0) {
                continue;
            }
        }

        /* partOfSpeech */
        char pos[16] = {0};
        const cJSON *j_pos = cJSON_GetObjectItem(entry, "partOfSpeech");
        if (j_pos && j_pos->valuestring) {
            snprintf(pos, sizeof(pos), "%s", j_pos->valuestring);
        }

        /* pronunciations — take first IPA */
        if (!ipa_set) {
            const cJSON *prons = cJSON_GetObjectItem(entry, "pronunciations");
            if (prons && cJSON_IsArray(prons)) {
                cJSON *p;
                cJSON_ArrayForEach(p, prons) {
                    const cJSON *ptype = cJSON_GetObjectItem(p, "type");
                    const cJSON *ptext = cJSON_GetObjectItem(p, "text");
                    if (ptype && ptext && ptype->valuestring &&
                        strcmp(ptype->valuestring, "ipa") == 0 &&
                        ptext->valuestring) {
                        strncpy(ipa_buf, ptext->valuestring,
                                sizeof(ipa_buf) - 1);
                        ipa_set = true;
                        break;
                    }
                }
            }
        }

        /* senses[] */
        const cJSON *senses = cJSON_GetObjectItem(entry, "senses");
        if (!senses || !cJSON_IsArray(senses)) continue;

        cJSON *sense;
        cJSON_ArrayForEach(sense, senses) {
            if (out->sense_count >= 8) break;

            dict_sense_t *s = &out->senses[out->sense_count];
            memset(s, 0, sizeof(*s));

            /* Copy IPA to every sense */
            if (ipa_set) {
                memcpy(s->ipa, ipa_buf, sizeof(s->ipa));
            }

            /* partOfSpeech */
            snprintf(s->pos, sizeof(s->pos), "%s", pos);

            /* definition */
            const cJSON *def = cJSON_GetObjectItem(sense, "definition");
            if (def && def->valuestring) {
                strncpy(s->definition, def->valuestring, sizeof(s->definition) - 1);
            }

            /* examples — take first */
            const cJSON *examples = cJSON_GetObjectItem(sense, "examples");
            if (examples && cJSON_IsArray(examples)) {
                const cJSON *ex = cJSON_GetArrayItem(examples, 0);
                if (ex && ex->valuestring) {
                    strncpy(s->example, ex->valuestring, sizeof(s->example) - 1);
                }
            }

            /* translations — find Chinese */
            const cJSON *trans = cJSON_GetObjectItem(sense, "translations");
            if (trans && cJSON_IsArray(trans)) {
                cJSON *tr;
                cJSON_ArrayForEach(tr, trans) {
                    const cJSON *tr_lang = cJSON_GetObjectItem(tr, "language");
                    if (tr_lang) {
                        const cJSON *tr_code = cJSON_GetObjectItem(tr_lang, "code");
                        if (tr_code && tr_code->valuestring &&
                            strncmp(tr_code->valuestring, "zh", 2) == 0) {
                            const cJSON *tr_word = cJSON_GetObjectItem(tr, "word");
                            if (tr_word && tr_word->valuestring) {
                                strncpy(s->definition_zh, tr_word->valuestring,
                                        sizeof(s->definition_zh) - 1);
                                break;
                            }
                        }
                    }
                }
            }

            out->sense_count++;

            /* Also parse subsenses (up to 2 per sense) */
            const cJSON *subsenses = cJSON_GetObjectItem(sense, "subsenses");
            if (subsenses && cJSON_IsArray(subsenses)) {
                cJSON *sub;
                int sub_count = 0;
                cJSON_ArrayForEach(sub, subsenses) {
                    if (out->sense_count >= 8 || sub_count >= 2) break;

                    dict_sense_t *ss = &out->senses[out->sense_count];
                    memset(ss, 0, sizeof(*ss));

                    if (ipa_set) {
                        memcpy(ss->ipa, ipa_buf, sizeof(ss->ipa));
                    }
                    snprintf(ss->pos, sizeof(ss->pos), "%s", pos);

                    const cJSON *sub_def = cJSON_GetObjectItem(sub, "definition");
                    if (sub_def && sub_def->valuestring) {
                        strncpy(ss->definition, sub_def->valuestring,
                                sizeof(ss->definition) - 1);
                    }

                    const cJSON *sub_examples = cJSON_GetObjectItem(sub, "examples");
                    if (sub_examples && cJSON_IsArray(sub_examples)) {
                        const cJSON *ex = cJSON_GetArrayItem(sub_examples, 0);
                        if (ex && ex->valuestring) {
                            strncpy(ss->example, ex->valuestring,
                                    sizeof(ss->example) - 1);
                        }
                    }

                    const cJSON *sub_trans = cJSON_GetObjectItem(sub, "translations");
                    if (sub_trans && cJSON_IsArray(sub_trans)) {
                        cJSON *tr;
                        cJSON_ArrayForEach(tr, sub_trans) {
                            const cJSON *tr_lang = cJSON_GetObjectItem(tr, "language");
                            if (tr_lang) {
                                const cJSON *tr_code = cJSON_GetObjectItem(tr_lang, "code");
                                if (tr_code && tr_code->valuestring &&
                                    strncmp(tr_code->valuestring, "zh", 2) == 0) {
                                    const cJSON *tr_word = cJSON_GetObjectItem(tr, "word");
                                    if (tr_word && tr_word->valuestring) {
                                        strncpy(ss->definition_zh, tr_word->valuestring,
                                                sizeof(ss->definition_zh) - 1);
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    out->sense_count++;
                    sub_count++;
                }
            }
        }
    }

    cJSON_Delete(root);
    return out->sense_count > 0;
}

/* ── Async dispatch to LVGL task ──────────────────────────────────── */
static void async_dispatch_cb(void *arg)
{
    (void)arg;
    if (s_result_cb) {
        s_result_cb(&s_result, s_result_valid);
    }
    s_busy = false;
}

/* ── Background lookup task ───────────────────────────────────────── */
static void lookup_task(void *pv_param)
{
    char *word = (char *)pv_param;
    ESP_LOGI(TAG, "Looking up: %s", word);

    /* Allocate response buffer in PSRAM */
    http_resp_t resp = {
        .buf = heap_caps_malloc(MAX_RESPONSE, MALLOC_CAP_SPIRAM),
        .len = 0,
        .cap = MAX_RESPONSE,
    };
    if (!resp.buf) {
        ESP_LOGE(TAG, "Failed to alloc response buffer");
        s_result_valid = false;
        free(word);
        lv_async_call(async_dispatch_cb, NULL);
        vTaskDelete(NULL);
        return;
    }

    /* Build URL: /entries/en/{word}?translations=true */
    char url[256];
    snprintf(url, sizeof(url), "%s%s?translations=true", DICT_API_BASE, word);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_cb,
        .user_data = &resp,
        .timeout_ms = 10000,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        free(resp.buf);
        s_result_valid = false;
        free(word);
        lv_async_call(async_dispatch_cb, NULL);
        vTaskDelete(NULL);
        return;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (err == ESP_OK && status == 200 && resp.len > 0) {
        resp.buf[resp.len] = '\0';
        ESP_LOGI(TAG, "HTTP OK, %d bytes", resp.len);

        memset(&s_result, 0, sizeof(s_result));
        s_result_valid = parse_response(resp.buf, &s_result);
    } else {
        ESP_LOGW(TAG, "HTTP failed: err=%s status=%d len=%d",
                 esp_err_to_name(err), status, resp.len);
        s_result_valid = false;
    }

    esp_http_client_cleanup(client);
    free(resp.buf);
    free(word);

    /* Dispatch result to LVGL task */
    lv_async_call(async_dispatch_cb, NULL);
    vTaskDelete(NULL);
}

/* ══════════════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════════════ */

esp_err_t dict_lookup_init(void)
{
    ESP_LOGI(TAG, "[INIT] dict_lookup_init()");
    s_busy = false;
    s_result_valid = false;
    s_result_cb = NULL;
    return ESP_OK;
}

void dict_lookup_set_result_cb(dict_lookup_result_cb_t cb)
{
    s_result_cb = cb;
}

void dict_lookup_word(const char *word)
{
    if (!word || *word == '\0') return;
    if (s_busy) {
        ESP_LOGW(TAG, "Lookup already in progress");
        return;
    }
    s_busy = true;

    /* Duplicate word string for the task (task frees it) */
    char *word_copy = strdup(word);
    if (!word_copy) {
        ESP_LOGE(TAG, "strdup failed");
        s_busy = false;
        return;
    }

    xTaskCreate(lookup_task, "dict_lookup", LOOKUP_TASK_STACK,
                word_copy, LOOKUP_TASK_PRIO, NULL);
}

void dict_lookup_deinit(void)
{
    /* Nothing persistent to clean up */
}