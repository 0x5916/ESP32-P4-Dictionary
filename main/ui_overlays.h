#pragma once

#include "lvgl.h"

void ui_overlays_init(void);
void ui_overlays_bind_textarea(lv_obj_t *textarea);
void ui_overlays_bind_search_open_button(lv_obj_t *button);
void ui_overlays_bind_navigation_button(lv_obj_t *button, lv_obj_t *target_screen);
void ui_overlays_set_time_text(const char *text);
void ui_overlays_set_wifi_text(const char *text);
void ui_overlays_apply_theme(bool dark_mode);
void ui_overlays_bind_swipe_back(lv_obj_t *screen, lv_obj_t *target_screen);

/**
 * @brief Open the Definition screen for a word.
 *        Navigates to the screen and triggers an HTTP lookup.
 *
 * @param word  Null-terminated word to look up
 */
void ui_overlays_open_definition(const char *word);