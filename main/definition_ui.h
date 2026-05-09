#pragma once

#include "lvgl.h"
#include "dict.h"

void definition_ui_populate(const dict_entry_t *entry, bool success);
void definition_ui_add_pos_ipa_header(const dict_pos_group_t *group, lv_color_t text_color,
                                      lv_color_t pron_label_color);
void definition_ui_lookup_result_cb(const dict_entry_t *entry, bool success);
void definition_ui_open(const char *word);