#pragma once

#include "lvgl.h"

void ui_overlays_init(void);
void ui_overlays_bind_textarea(lv_obj_t *textarea);
void ui_overlays_bind_search_back_button(lv_obj_t *button);
void ui_overlays_bind_search_open_button(lv_obj_t *button);
void ui_overlays_bind_navigation_button(lv_obj_t *button, lv_obj_t *target_screen);
void ui_overlays_set_time_text(const char *text);
void ui_overlays_set_wifi_text(const char *text);
