#pragma once

#include "lvgl.h"

void settings_ui_build(lv_obj_t *parent);
void settings_ui_apply_theme(bool dark_mode);
lv_obj_t *settings_create_section_title(lv_obj_t *parent, const char *title);
lv_obj_t *settings_create_switch_row(
	lv_obj_t *parent,
	const char *title,
	const char *subtitle,
	bool checked,
	lv_event_cb_t cb,
	void *user_data
);
lv_obj_t *settings_create_action_row(
	lv_obj_t *parent,
	const char *title,
	const char *subtitle,
	const char *button_text,
	lv_event_cb_t cb,
	void *user_data
);
lv_obj_t *settings_create_dropdown_row(
	lv_obj_t *parent,
	const char *title,
	const char *subtitle,
	const char *options,
	uint16_t selected,
	lv_event_cb_t cb,
	void *user_data
);
