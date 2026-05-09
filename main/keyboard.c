#include "keyboard.h"
#include "custom_keyboard.h"
#include "ui/ui.h"
#include "ui_helpers.h"
#include "screen_manager.h"
#include "bsp/display.h"
#include "esp_log.h"

#define SEARCH_HEADWORD_LIST_COMPACT_HEIGHT 295
#define SEARCH_HEADWORD_LIST_EXPANDED_HEIGHT 605

static const char *TAG = "keyboard";

static void set_search_headword_list_height(lv_coord_t height)
{
    if (objects.search_headword_lst) {
        lv_obj_set_height(objects.search_headword_lst, height);
    }
}

void keyboard_hide(void)
{
    if (!objects.search_kb) {
        return;
    }

    lv_obj_t *textarea = lv_keyboard_get_textarea(objects.search_kb);
    if (textarea) {
        ui_obj_set_state_if(textarea, LV_STATE_FOCUSED, false);
        ui_obj_set_state_if(textarea, LV_STATE_CHECKED, false);
        lv_obj_clear_flag(textarea, LV_OBJ_FLAG_STATE_TRICKLE);
    }

    lv_keyboard_set_textarea(objects.search_kb, NULL);
    ui_obj_set_hidden(objects.search_kb, true);
    set_search_headword_list_height(SEARCH_HEADWORD_LIST_EXPANDED_HEIGHT);
}

void keyboard_show(lv_obj_t *textarea)
{
    if (!objects.search_kb || !textarea) {
        return;
    }

    lv_obj_add_state(textarea, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(objects.search_kb, textarea);
    ui_obj_set_hidden(objects.search_kb, false);
    lv_obj_move_foreground(objects.search_kb);
    set_search_headword_list_height(SEARCH_HEADWORD_LIST_COMPACT_HEIGHT);
}

void keyboard_create(void)
{
    if (!objects.search_kb) {
        ESP_LOGW(TAG, "Generated keyboard object is missing");
        return;
    }

    apply_custom_keyboard_layout(objects.search_kb);
    lv_obj_add_event_cb(objects.search_kb, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(objects.search_kb, keyboard_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_move_background(objects.search_kb);
}

void keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        keyboard_hide();
    }
}

void textarea_focus_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *textarea = lv_event_get_target(event);
    keyboard_show(textarea);
}