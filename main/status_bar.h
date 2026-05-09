#pragma once

#include "lvgl.h"

void status_bar_create(void);
void status_bar_set_time_text(const char *text);
void status_bar_set_wifi_text(const char *text);
void status_bar_apply_theme(bool dark_mode);