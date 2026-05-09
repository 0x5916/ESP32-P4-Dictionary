#pragma once

#include "lvgl.h"

void keyboard_create(void);
void keyboard_hide(void);
void keyboard_show(lv_obj_t *textarea);
void keyboard_event_cb(lv_event_t *event);
void textarea_focus_cb(lv_event_t *event);