#pragma once

#include "lvgl.h"
#include <stdbool.h>

static inline bool ui_event_is(lv_event_t *event, lv_event_code_t code)
{
    return lv_event_get_code(event) == code;
}

static inline void ui_obj_set_hidden(lv_obj_t *obj, bool hidden)
{
    if (!obj) {
        return;
    }

    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static inline void ui_obj_set_state_if(lv_obj_t *obj, lv_state_t state, bool enabled)
{
    if (!obj) {
        return;
    }

    if (enabled) {
        lv_obj_add_state(obj, state);
    } else {
        lv_obj_clear_state(obj, state);
    }
}

static inline void ui_label_set_text_if(lv_obj_t *label, const char *text)
{
    if (label && text) {
        lv_label_set_text(label, text);
    }
}

static inline void ui_bind_event(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t code, void *user_data)
{
    if (obj && cb) {
        lv_obj_add_event_cb(obj, cb, code, user_data);
    }
}
