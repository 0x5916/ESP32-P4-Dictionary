#include "search_ui.h"
#include "ui/ui.h"
#include "definition_ui.h"
#include "keyboard.h"
#include "navigation.h"
#include "ui_helpers.h"
#include "dict_spellcheck.h"
#include "screen_manager.h"
#include "esp_log.h"
#include <ctype.h>

#define SUGGEST_DEBOUNCE_MS 0

static const char *TAG = "search_ui";

static lv_timer_t *suggest_timer = NULL;
static lv_obj_t  *suggest_ta    = NULL;   /* textarea being monitored */

static void update_suggestion_list(const char *text);

static void suggest_list_btn_cb(lv_event_t *event)
{
    lv_obj_t *btn = lv_event_get_target(event);
    if (!btn || !suggest_ta) return;

    /* lv_list_add_btn creates a label child; the text is on the button itself */
    const char *text = lv_list_get_btn_text(objects.search_headword_lst, btn);
    if (text) {
        lv_textarea_set_text(suggest_ta, text);
        lv_textarea_set_cursor_pos(suggest_ta, LV_TEXTAREA_CURSOR_LAST);
        /* Navigate to definition screen and look up the word */
        definition_ui_open(text);
    }
}

void search_ui_update_suggestion_list(const char *text)
{
    if (!objects.search_headword_lst) return;

    /* Clear existing items */
    lv_obj_clean(objects.search_headword_lst);

    if (!text || *text == '\0') return;

    /* Skip non-alpha input (e.g. user typed a space or number) */
    for (const char *p = text; *p; ++p) {
        if (!isalpha((unsigned char)*p)) return;
    }

    if (!dict_spellcheck_is_ready()) {
        ESP_LOGD(TAG, "Spellcheck not ready, skipping suggestions");
        return;
    }

    char results[MAX_SUGGESTIONS][MAX_WORD_LEN];
    size_t count = dict_spellcheck_suggest(text, results, MAX_SUGGESTIONS);

    for (size_t i = 0; i < count; ++i) {
        lv_obj_t *btn = lv_list_add_btn(objects.search_headword_lst, NULL, results[i]);
        if (btn) {
            lv_obj_add_event_cb(btn, suggest_list_btn_cb, LV_EVENT_CLICKED, NULL);
        }
    }
}

static void suggest_timer_cb(lv_timer_t *timer)
{
    if (!suggest_ta) return;

    const char *text = lv_textarea_get_text(suggest_ta);
    if (!text) return;

    search_ui_update_suggestion_list(text);

    /* Pause after firing; will be resumed on next VALUE_CHANGED */
    lv_timer_pause(timer);
}

void search_ui_textarea_value_changed_cb(lv_event_t *event)
{
    LV_UNUSED(event);

    /* Reset debounce timer: when text changes, wait SUGGEST_DEBOUNCE_MS
     * before actually performing the search. */
    if (suggest_timer) {
        lv_timer_resume(suggest_timer);
        lv_timer_reset(suggest_timer);
    }
}

void search_ui_create_suggest_timer(void)
{
    /* One-shot timer that fires after SUGGEST_DEBOUNCE_MS of inactivity */
    suggest_timer = lv_timer_create(suggest_timer_cb, SUGGEST_DEBOUNCE_MS, NULL);
    if (suggest_timer) {
        lv_timer_pause(suggest_timer);   /* start paused; resume on first edit */
    }
}

void search_ui_suggest_timer_cb(lv_timer_t *timer)
{
    suggest_timer_cb(timer);
}

void search_ui_suggest_list_btn_cb(lv_event_t *event)
{
    suggest_list_btn_cb(event);
}

void search_ui_open_screen_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED) || !objects.search) {
        return;
    }

    screen_navigate(objects.search, SCREEN_ANIM_LEFT);
    keyboard_show(objects.search_search_ta);
}

void search_ui_bind_textarea(lv_obj_t *textarea)
{
    if (!textarea) {
        return;
    }

    /* Store reference for suggestion updates */
    suggest_ta = textarea;

    lv_obj_add_event_cb(textarea, search_ui_textarea_value_changed_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(textarea, search_ui_textarea_value_changed_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(textarea, search_ui_textarea_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void search_ui_bind_search_open_button(lv_obj_t *button)
{
    ui_bind_event(button, search_ui_open_screen_cb, LV_EVENT_CLICKED, NULL);
}