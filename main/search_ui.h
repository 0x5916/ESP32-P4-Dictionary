#pragma once

#include "lvgl.h"

void search_ui_open_screen_cb(lv_event_t *event);
void search_ui_textarea_value_changed_cb(lv_event_t *event);
void search_ui_update_suggestion_list(const char *text);
void search_ui_suggest_list_btn_cb(lv_event_t *event);
void search_ui_suggest_timer_cb(lv_timer_t *timer);
void search_ui_create_suggest_timer(void);
void search_ui_bind_textarea(lv_obj_t *textarea);
void search_ui_bind_search_open_button(lv_obj_t *button);