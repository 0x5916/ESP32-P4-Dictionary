#include "settings_ui.h"

#include "screen_manager.h"
#include "settings_service.h"
#include "ui_overlays.h"
#include "ui/ui.h"

static bool settings_ui_is_dark_mode(void)
{
    bool enabled = false;
    settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &enabled);
    return enabled;
}

static lv_color_t settings_title_color(bool dark)
{
    return dark ? lv_palette_lighten(LV_PALETTE_BLUE, 2) : lv_palette_main(LV_PALETTE_BLUE);
}

static lv_color_t settings_row_bg_color(bool dark)
{
    return dark ? lv_palette_darken(LV_PALETTE_GREY, 4) : lv_color_white();
}

static lv_color_t settings_row_border_color(bool dark)
{
    return dark ? lv_palette_darken(LV_PALETTE_GREY, 2) : lv_palette_lighten(LV_PALETTE_GREY, 2);
}

static lv_color_t settings_row_pressed_bg_color(bool dark)
{
    return dark ? lv_palette_darken(LV_PALETTE_BLUE, 3) : lv_palette_lighten(LV_PALETTE_BLUE, 4);
}

static lv_color_t settings_row_pressed_border_color(bool dark)
{
    return dark ? lv_palette_main(LV_PALETTE_BLUE) : lv_palette_main(LV_PALETTE_BLUE);
}

static lv_color_t settings_subtitle_color(bool dark)
{
    return dark ? lv_palette_lighten(LV_PALETTE_GREY, 2) : lv_palette_darken(LV_PALETTE_GREY, 1);
}

void settings_ui_apply_theme(bool dark_mode)
{
    lv_display_t *dispp = lv_display_get_default();
    if (!dispp) {
        return;
    }

    lv_theme_t *theme = lv_theme_default_init(
        dispp,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        dark_mode,
        LV_FONT_DEFAULT
    );
    lv_display_set_theme(dispp, theme);
}

static void settings_rebuild_async(void *user_data)
{
    lv_obj_t *parent = (lv_obj_t *)user_data;
    if (parent) {
        settings_ui_build(parent);
    }
}

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

lv_obj_t *settings_create_section_title(lv_obj_t *parent, const char *title) {
    bool dark_mode = settings_ui_is_dark_mode();
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, title);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, settings_title_color(dark_mode), 0);
    lv_obj_set_style_pad_top(label, 8, 0);
    lv_obj_set_style_pad_bottom(label, 2, 0);
    return label;
}

static lv_obj_t *settings_create_row_base(lv_obj_t *parent) {
    bool dark_mode = settings_ui_is_dark_mode();
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 16, 0);
    lv_obj_set_style_pad_column(row, 14, 0);
    lv_obj_set_style_pad_row(row, 6, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, settings_row_border_color(dark_mode), 0);
    lv_obj_set_style_bg_color(row, settings_row_bg_color(dark_mode), 0);
    lv_obj_set_style_bg_color(row, settings_row_pressed_bg_color(dark_mode), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(row, settings_row_pressed_border_color(dark_mode), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(row, 2, LV_STATE_PRESSED);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

static lv_obj_t *settings_create_text_block(lv_obj_t *parent, const char *title, const char *subtitle) {
    bool dark_mode = settings_ui_is_dark_mode();
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_add_flag(col, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, 0);

    lv_obj_t *title_label = lv_label_create(col);
    lv_label_set_text(title_label, title);
    lv_obj_set_width(title_label, lv_pct(100));
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);

    if (subtitle && subtitle[0] != '\0') {
        lv_obj_t *subtitle_label = lv_label_create(col);
        lv_label_set_text(subtitle_label, subtitle);
        lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(subtitle_label, lv_pct(100));
        lv_obj_set_style_text_font(subtitle_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(subtitle_label, settings_subtitle_color(dark_mode), 0);
    }

    return col;
}

static void settings_toggle_switch_from_row(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *sw = (lv_obj_t *)lv_event_get_user_data(event);
    if (!sw) {
        return;
    }

    if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
        lv_obj_clear_state(sw, LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    lv_obj_send_event(sw, LV_EVENT_VALUE_CHANGED, NULL);
}

lv_obj_t *settings_create_switch_row(lv_obj_t *parent,
                                            const char *title,
                                            const char *subtitle,
                                            bool checked,
                                            lv_event_cb_t cb,
                                            void *user_data) {
    lv_obj_t *row = settings_create_row_base(parent);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    settings_create_text_block(row, title, subtitle);

    lv_obj_t *sw = lv_switch_create(row);
    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    if (cb) {
        lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    lv_obj_add_event_cb(row, settings_toggle_switch_from_row, LV_EVENT_CLICKED, sw);
    return sw;
}

lv_obj_t *settings_create_action_row(lv_obj_t *parent,
                                            const char *title,
                                            const char *subtitle,
                                            const char *button_text,
                                            lv_event_cb_t cb,
                                            void *user_data) {
    lv_obj_t *row = settings_create_row_base(parent);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    settings_create_text_block(row, title, subtitle);

    if (cb) {
        lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, user_data);
    }

    const char *indicator_text = button_text;
    if ((!indicator_text || indicator_text[0] == '\0') && cb) {
        indicator_text = ">";
    }

    if (indicator_text && indicator_text[0] != '\0') {
        lv_obj_t *btn = lv_btn_create(row);
        lv_obj_set_height(btn, 40);
        lv_obj_set_style_pad_left(btn, 14, 0);
        lv_obj_set_style_pad_right(btn, 14, 0);
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, indicator_text);
        lv_obj_center(label);

        if (cb) {
            lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
        }
    }
    return row;
}

lv_obj_t *settings_create_dropdown_row(lv_obj_t *parent,
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

static void settings_dark_mode_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    lv_obj_t *sw = lv_event_get_target(event);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    settings_set_bool(SETTINGS_KEY_DARK_MODE, enabled);
    settings_ui_apply_theme(enabled);
    ui_overlays_apply_theme(enabled);
    lv_async_call(settings_rebuild_async, objects.settings_cont);
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
                               NULL,
                               settings_open_wifi_cb,
                               NULL);

    settings_create_section_title(parent, "Appearance");
    bool dark_mode_enabled = false;
    settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &dark_mode_enabled);
    settings_create_switch_row(parent,
                               "Dark mode",
                               "Reduce glare in low light",
                               dark_mode_enabled,
                               settings_dark_mode_cb,
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
