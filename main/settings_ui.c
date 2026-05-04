#include "settings_ui.h"

#include "screen_manager.h"
#include "ui/ui.h"

static void style_settings_parent(lv_obj_t *parent) {
    lv_obj_set_size(parent, 470, 670);
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 10, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_radius(parent, 0, 0);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
}

static lv_obj_t *settings_create_section_title(lv_obj_t *parent, const char *title) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, title);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_pad_top(label, 8, 0);
    lv_obj_set_style_pad_bottom(label, 2, 0);
    return label;
}

static lv_obj_t *settings_create_row_base(lv_obj_t *parent) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 12, 0);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_style_pad_row(row, 4, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

static lv_obj_t *settings_create_text_block(lv_obj_t *parent, const char *title, const char *subtitle) {
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, 0);

    lv_obj_t *title_label = lv_label_create(col);
    lv_label_set_text(title_label, title);
    lv_obj_set_width(title_label, lv_pct(100));
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);

    if (subtitle && subtitle[0] != '\0') {
        lv_obj_t *subtitle_label = lv_label_create(col);
        lv_label_set_text(subtitle_label, subtitle);
        lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(subtitle_label, lv_pct(100));
        lv_obj_set_style_text_font(subtitle_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(subtitle_label, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    }

    return col;
}

static lv_obj_t *settings_create_switch_row(lv_obj_t *parent,
                                            const char *title,
                                            const char *subtitle,
                                            bool checked,
                                            lv_event_cb_t cb,
                                            void *user_data) {
    lv_obj_t *row = settings_create_row_base(parent);
    settings_create_text_block(row, title, subtitle);

    lv_obj_t *sw = lv_switch_create(row);
    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    if (cb) {
        lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    return sw;
}

static lv_obj_t *settings_create_action_row(lv_obj_t *parent,
                                            const char *title,
                                            const char *subtitle,
                                            const char *button_text,
                                            lv_event_cb_t cb,
                                            void *user_data) {
    lv_obj_t *row = settings_create_row_base(parent);
    settings_create_text_block(row, title, subtitle);

    lv_obj_t *btn = lv_btn_create(row);
    lv_obj_set_height(btn, 40);
    lv_obj_set_style_pad_left(btn, 14, 0);
    lv_obj_set_style_pad_right(btn, 14, 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, button_text);
    lv_obj_center(label);

    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    return btn;
}

static lv_obj_t *settings_create_dropdown_row(lv_obj_t *parent,
                                              const char *title,
                                              const char *subtitle,
                                              const char *options,
                                              uint16_t selected,
                                              lv_event_cb_t cb,
                                              void *user_data) {
    lv_obj_t *row = settings_create_row_base(parent);
    settings_create_text_block(row, title, subtitle);

    lv_obj_t *dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, options);
    lv_dropdown_set_selected(dd, selected);
    lv_obj_set_width(dd, 140);

    if (cb) {
        lv_obj_add_event_cb(dd, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    return dd;
}

static void settings_open_wifi_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !objects.wi_fi) {
        return;
    }

    screen_navigate(objects.wi_fi, SCREEN_ANIM_LEFT);
}

void settings_ui_build(lv_obj_t *parent) {
    if (!parent) {
        return;
    }

    lv_obj_clean(parent);
    style_settings_parent(parent);

    // settings_create_section_title(parent, "Dictionary");
    // settings_create_switch_row(parent,
    //                            "Show Chinese definitions",
    //                            "Display bilingual meanings when available",
    //                            true,
    //                            settings_show_zh_cb,
    //                            NULL);

    settings_create_section_title(parent, "Network");
    settings_create_action_row(parent,
                               "Wi-Fi settings",
                               "Open Wi-Fi screen to scan and connect",
                               "Open",
                               settings_open_wifi_cb,
                               NULL);

    // settings_create_switch_row(parent,
    //                            "Online fallback",
    //                            "Use Wi-Fi lookup when offline dictionary misses a word",
    //                            true,
    //                            settings_online_fallback_cb,
    //                            NULL);

    // settings_create_dropdown_row(parent,
    //                              "Font size",
    //                              "Adjust reading size for definition text",
    //                              "Small\nMedium\nLarge",
    //                              1,
    //                              settings_font_size_cb,
    //                              NULL);

    // settings_create_section_title(parent, "History");
    // settings_create_switch_row(parent,
    //                            "Save history",
    //                            "Keep recent searched words",
    //                            true,
    //                            settings_history_cb,
    //                            NULL);

    // settings_create_action_row(parent,
    //                            "Clear history",
    //                            "Remove all recent searches from local storage",
    //                            "Clear",
    //                            settings_clear_history_cb,
    //                            NULL);
}
