#pragma once

#include "lvgl.h"

void navigate_to_screen_cb(lv_event_t *event);
void swipe_back_cb(lv_event_t *event);
void navigation_bind_button(lv_obj_t *button, lv_obj_t *target_screen);
void navigation_bind_swipe_back(lv_obj_t *screen, lv_obj_t *target_screen);