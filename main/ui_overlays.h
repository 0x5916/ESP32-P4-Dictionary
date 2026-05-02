#pragma once

#include "lvgl.h"

void ui_overlays_init(void);
void ui_overlays_bind_textarea(lv_obj_t *textarea);
void ui_overlays_set_time_text(const char *text);
void ui_overlays_set_wifi_text(const char *text);
