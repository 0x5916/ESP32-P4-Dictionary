#include "definition_ui.h"
#include "ui/ui.h"
#include "screen_manager.h"
#include "settings_service.h"
#include "dict_lookup.h"
#include "keyboard.h"
#include "esp_log.h"
#include <ctype.h>

static const char *TAG = "definition_ui";

/* Font generated with lv_font_conv: provides `my_ipa_font` */
extern const lv_font_t my_ipa_font;

static void definition_scroll_event_cb(lv_event_t *e)
{
    lv_obj_t *cont = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_SCROLL || code == LV_EVENT_SCROLL_END) {
        /* Force all child labels to redraw to prevent color artifacts across line breaks */
        lv_obj_t *child = lv_obj_get_child(cont, 0);
        while (child != NULL) {
            lv_obj_invalidate(child);
            child = lv_obj_get_child(cont, lv_obj_get_index(child) + 1);
        }
    }
}

static void definition_add_pos_ipa_header(const dict_pos_group_t *group, lv_color_t text_color,
                                          lv_color_t pron_label_color)
{
    if (!group || !objects.definition_cont || group->pos[0] == '\0') {
        return;
    }

    lv_obj_t *header = lv_obj_create(objects.definition_cont);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 8, 0);
    lv_obj_set_style_margin_left(header, 8, 0);
    lv_obj_set_style_margin_top(header, 6, 0);
    lv_obj_set_style_margin_bottom(header, 2, 0);

    lv_obj_t *pos_lbl = lv_label_create(header);
    lv_label_set_text(pos_lbl, group->pos);
    lv_obj_set_style_text_font(pos_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(pos_lbl, text_color, 0);

    if (group->pron_count > 0) {
        lv_obj_t *ipa_title = lv_label_create(header);
        lv_label_set_text(ipa_title, "IPA:");
        lv_obj_set_style_text_font(ipa_title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(ipa_title, pron_label_color, 0);

        for (uint8_t i = 0; i < group->pron_count; ++i) {
            const dict_pronunciation_t *pr = &group->pronunciations[i];
            if (pr->ipa[0] == '\0') {
                continue;
            }

            lv_obj_t *ipa_lbl = lv_label_create(header);
            lv_label_set_text(ipa_lbl, pr->ipa);
            lv_obj_set_style_text_font(ipa_lbl, &my_ipa_font, 0);
            lv_obj_set_style_text_color(ipa_lbl, text_color, 0);

            if (pr->dialect[0] != '\0') {
                lv_obj_t *dialect_lbl = lv_label_create(header);
                lv_label_set_text_fmt(dialect_lbl, "(%s)", pr->dialect);
                lv_obj_set_style_text_font(dialect_lbl, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(dialect_lbl, pron_label_color, 0);
            }
        }
    }
}

void definition_ui_populate(const dict_entry_t *entry, bool success)
{
    if (!objects.definition_cont) {
        ESP_LOGW(TAG, "definition_cont is NULL");
        return;
    }

    /* Clear any previous content */
    lv_obj_clean(objects.definition_cont);

    /* Enable scrolling on the container to handle long content */
    lv_obj_add_flag(objects.definition_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_snap_y(objects.definition_cont, LV_SCROLL_SNAP_NONE);

    /* Use flex column layout so items flow naturally without manual y-offset */
    lv_obj_set_flex_flow(objects.definition_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(objects.definition_cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(objects.definition_cont, 4, 0);
    lv_obj_set_style_pad_column(objects.definition_cont, 0, 0);

    /* Add scroll event handler to fix recolor artifacts across line breaks */
    lv_obj_add_event_cb(objects.definition_cont, definition_scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(objects.definition_cont, definition_scroll_event_cb, LV_EVENT_SCROLL_END, NULL);

    bool dark_mode = false;
    settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &dark_mode);
    lv_color_t text_color = dark_mode ? lv_color_white() : lv_color_black();
    lv_color_t subtle_color = dark_mode
        ? lv_palette_lighten(LV_PALETTE_GREY, 2)
        : lv_palette_darken(LV_PALETTE_GREY, 1);
    lv_color_t zh_color = lv_palette_main(LV_PALETTE_ORANGE);
    lv_color_t pron_label_color = lv_palette_main(LV_PALETTE_TEAL);

    /* Pre-compute hex string for subtle_color (used by dim_brackets recolor).
     * Hardcoded to avoid lv_color_to_u32() portability issues (ARGB vs RGB). */
    const char *hex_dim = dark_mode ? "B0B0B0" : "757575";

    if (!success || !entry || entry->pos_count == 0) {
        lv_obj_t *lbl = lv_label_create(objects.definition_cont);
        lv_label_set_text(lbl, "No definition found.\nCheck your network connection.");
        lv_obj_set_style_text_color(lbl, subtle_color, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_margin_top(lbl, 30, 0);
        return;
    }

    /* ── Word headword ──────────────────────────────────────────── */
    lv_obj_t *word_lbl = lv_label_create(objects.definition_cont);
    lv_label_set_text(word_lbl, entry->keyword);
    lv_obj_set_style_text_font(word_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(word_lbl, text_color, 0);
    lv_obj_set_width(word_lbl, lv_pct(100));
    lv_obj_set_style_margin_bottom(word_lbl, 8, 0);

    /* ── Senses grouped by PoS ──────────────────────────────────── */
    bool show_zh = true;
    settings_get_bool(SETTINGS_KEY_SHOW_ZH, true, &show_zh);

    for (uint8_t g = 0; g < entry->pos_count; ++g) {
        const dict_pos_group_t *group = &entry->pos_groups[g];
        if (group->sense_count == 0) continue;

        definition_ui_add_pos_ipa_header(group, text_color, pron_label_color);

        /* ── Separator ────────────────────────────────────────────────── */
        lv_obj_t *sep = lv_obj_create(objects.definition_cont);
        lv_obj_remove_style_all(sep);
        lv_obj_set_size(sep, lv_pct(100), 1);
        lv_obj_set_style_bg_opa(sep, LV_OPA_20, 0);
        lv_obj_set_style_bg_color(sep, text_color, 0);
        lv_obj_set_style_margin_top(sep, 8, 0);
        lv_obj_set_style_margin_bottom(sep, 8, 0);

        /* ── Senses for this PoS ──────────────────────────────────────── */
        for (uint8_t i = 0; i < group->sense_count; ++i) {
            const dict_sense_t *s = &group->senses[i];

            /* Definition row with number */
            lv_obj_t *row = lv_obj_create(objects.definition_cont);
            lv_obj_remove_style_all(row);
            lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                                  LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_column(row, 10, 0);
            lv_obj_set_style_margin_left(row, 8, 0);
            lv_obj_set_style_margin_top(row, 6, 0);

            /* Number label (e.g., "1.", "2.") */
            lv_obj_t *num_lbl = lv_label_create(row);
            char num_buf[8];
            snprintf(num_buf, sizeof(num_buf), "%d.", (int)(i + 1));
            lv_label_set_text(num_lbl, num_buf);
            lv_obj_set_style_text_font(num_lbl, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(num_lbl, text_color, 0);

            /* Definition - show full text with normal brackets (no dimming) */
            if (s->definition[0] != '\0') {
                lv_obj_t *def_lbl = lv_label_create(row);
                lv_obj_remove_style_all(def_lbl);
                char def_buf[1024];
                definition_ui_dim_brackets(def_buf, sizeof(def_buf), s->definition, hex_dim);
                lv_label_set_recolor(def_lbl, true);
                lv_label_set_text(def_lbl, def_buf);
                lv_label_set_long_mode(def_lbl, LV_LABEL_LONG_WRAP);
                lv_obj_set_width(def_lbl, LV_PCT(100));
                lv_obj_set_flex_grow(def_lbl, 1);
                lv_obj_set_style_text_font(def_lbl, &lv_font_montserrat_20, 0);
                lv_obj_set_style_text_color(def_lbl, text_color, 0);
                lv_obj_clear_flag(def_lbl, LV_OBJ_FLAG_SCROLLABLE);
                /* Enable scroll chain to allow proper event handling */
                lv_obj_add_flag(def_lbl, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            }

            /* Chinese translation */
            if (show_zh && s->definition_zh[0] != '\0') {
                lv_obj_t *zh_lbl = lv_label_create(objects.definition_cont);
                lv_label_set_text(zh_lbl, s->definition_zh);
                lv_obj_set_style_text_font(zh_lbl, &lv_font_montserrat_20, 0);
                lv_obj_set_style_text_color(zh_lbl, zh_color, 0);
                lv_obj_set_width(zh_lbl, lv_pct(100));
                lv_obj_set_style_margin_left(zh_lbl, 40, 0);
                lv_obj_set_style_margin_top(zh_lbl, 2, 0);
            }

            /* Synonyms and Antonyms */
            bool show_synonyms_antonyms = true;
            settings_get_bool(SETTINGS_KEY_SYNONYMS_ANTONYMS, true, &show_synonyms_antonyms);
            
            if (show_synonyms_antonyms && (s->synonyms[0] != '\0' || s->antonyms[0] != '\0')) {
                lv_obj_t *syn_ant_row = lv_obj_create(objects.definition_cont);
                lv_obj_remove_style_all(syn_ant_row);
                lv_obj_set_size(syn_ant_row, lv_pct(100), LV_SIZE_CONTENT);
                lv_obj_set_flex_flow(syn_ant_row, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(syn_ant_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
                lv_obj_set_style_pad_column(syn_ant_row, 10, 0);
                lv_obj_set_style_margin_left(syn_ant_row, 40, 0);
                lv_obj_set_style_margin_top(syn_ant_row, 4, 0);
                lv_obj_set_style_margin_bottom(syn_ant_row, 8, 0);
                
                if (s->synonyms[0] != '\0') {
                    lv_obj_t *syn_col = lv_obj_create(syn_ant_row);
                    lv_obj_remove_style_all(syn_col);
                    lv_obj_set_size(syn_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_flex_flow(syn_col, LV_FLEX_FLOW_COLUMN);
                    lv_obj_set_flex_align(syn_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
                    lv_obj_set_style_pad_row(syn_col, 2, 0);
                    
                    lv_obj_t *syn_title = lv_label_create(syn_col);
                    lv_label_set_text(syn_title, "Synonyms:");
                    lv_obj_set_style_text_font(syn_title, &lv_font_montserrat_18, 0);
                    lv_obj_set_style_text_color(syn_title, lv_palette_lighten(LV_PALETTE_BLUE, 2), 0);
                    
                    lv_obj_t *syn_item = lv_label_create(syn_col);
                    lv_label_set_text(syn_item, s->synonyms);
                    lv_obj_set_style_text_font(syn_item, &lv_font_montserrat_18, 0);
                    lv_obj_set_style_text_color(syn_item, subtle_color, 0);
                    lv_obj_set_style_margin_top(syn_item, 2, 0);
                }
                
                if (s->antonyms[0] != '\0') {
                    lv_obj_t *ant_col = lv_obj_create(syn_ant_row);
                    lv_obj_remove_style_all(ant_col);
                    lv_obj_set_size(ant_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_flex_flow(ant_col, LV_FLEX_FLOW_COLUMN);
                    lv_obj_set_flex_align(ant_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
                    lv_obj_set_style_pad_row(ant_col, 2, 0);
                    lv_obj_set_style_margin_left(ant_col, 20, 0);
                    
                    lv_obj_t *ant_title = lv_label_create(ant_col);
                    lv_label_set_text(ant_title, "Antonyms:");
                    lv_obj_set_style_text_font(ant_title, &lv_font_montserrat_18, 0);
                    lv_obj_set_style_text_color(ant_title, lv_palette_lighten(LV_PALETTE_RED, 2), 0);
                    
                    lv_obj_t *ant_item = lv_label_create(ant_col);
                    lv_label_set_text(ant_item, s->antonyms);
                    lv_obj_set_style_text_font(ant_item, &lv_font_montserrat_18, 0);
                    lv_obj_set_style_text_color(ant_item, subtle_color, 0);
                    lv_obj_set_style_margin_top(ant_item, 2, 0);
                }
            }

            /* Example - dimmed, with bracket dimming via recolor */
            if (s->example[0] != '\0') {
                lv_obj_t *ex_lbl = lv_label_create(objects.definition_cont);
                char ex_src[260];
                snprintf(ex_src, sizeof(ex_src), "\"%s\"", s->example);
                char ex_buf[520];
                definition_ui_dim_brackets(ex_buf, sizeof(ex_buf), ex_src, hex_dim);
                lv_label_set_text(ex_lbl, ex_buf);
                lv_label_set_recolor(ex_lbl, true);
                lv_obj_set_style_text_font(ex_lbl, &lv_font_montserrat_18, 0);
                /* Dim examples with reduced opacity */
                lv_obj_set_style_text_opa(ex_lbl, LV_OPA_60, 0);
                lv_obj_set_style_text_color(ex_lbl, subtle_color, 0);
                lv_obj_set_width(ex_lbl, lv_pct(100));
                lv_obj_set_style_margin_left(ex_lbl, 40, 0);
                lv_obj_set_style_margin_top(ex_lbl, 2, 0);
                lv_obj_set_style_margin_bottom(ex_lbl, 10, 0);
            }
        }
    }
}

void definition_ui_dim_brackets(char *buf, size_t buf_sz, const char *src, const char *hex_dim)
{
    size_t si = 0, di = 0;
    while (src[si] && di < buf_sz - 1) {
        if (src[si] == '(') {
            /* Start dim colour, then include opening bracket as literal text */
            di += snprintf(buf + di, buf_sz - di, "#%s (", hex_dim);
            si++;
            /* Copy until matching ')' (or end) */
            while (src[si] && src[si] != ')' && di < buf_sz - 1) {
                buf[di++] = src[si++];
            }
            if (src[si] == ')') {
                /* Close the recolor span after the bracketed content */
                di += snprintf(buf + di, buf_sz - di, ")#");
                si++;
            }
        } else {
            buf[di++] = src[si++];
        }
    }
    buf[di] = '\0';
}

void definition_ui_add_pos_ipa_header(const dict_pos_group_t *group, lv_color_t text_color,
                                      lv_color_t pron_label_color)
{
    definition_add_pos_ipa_header(group, text_color, pron_label_color);
}

static void definition_lookup_result_cb(const dict_entry_t *entry, bool success)
{
    int total_senses = 0;
    if (entry) {
        for (int i = 0; i < entry->pos_count; i++) {
            total_senses += entry->pos_groups[i].sense_count;
        }
    }
    ESP_LOGI(TAG, "Lookup result: success=%d, total_senses=%d", success, total_senses);
    definition_ui_populate(entry, success);
}

void definition_ui_lookup_result_cb(const dict_entry_t *entry, bool success)
{
    definition_lookup_result_cb(entry, success);
}

void definition_ui_open(const char *word)
{
    if (!word || *word == '\0') return;

    /* Navigate to definition screen */
    if (objects.definition) {
        keyboard_hide();
        screen_navigate(objects.definition, SCREEN_ANIM_LEFT);
    }

    /* Show loading state */
    if (objects.definition_cont) {
        lv_obj_clean(objects.definition_cont);
        bool dark_mode = false;
        settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &dark_mode);
        lv_obj_t *lbl = lv_label_create(objects.definition_cont);
        lv_label_set_text(lbl, "Loading...");
        lv_obj_set_style_text_color(lbl,
            dark_mode ? lv_color_white() : lv_palette_darken(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 40);
    }

    /* Trigger HTTP lookup */
    dict_lookup_word(word);
}