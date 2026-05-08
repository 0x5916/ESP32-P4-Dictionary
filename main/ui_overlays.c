#include "ui_overlays.h"

#include "ui/ui.h"
#include "custom_keyboard.h"
#include "screen_manager.h"
#include "settings_ui.h"
#include "settings_service.h"
#include "dict_spellcheck.h"
#include "dict_lookup.h"
#include "bsp/display.h"
#include "esp_log.h"
#include "ui_helpers.h"
#include <ctype.h>

#define STATUS_BAR_HEIGHT 50
#define SEARCH_HEADWORD_LIST_COMPACT_HEIGHT 295
#define SEARCH_HEADWORD_LIST_EXPANDED_HEIGHT 605
#define SUGGEST_DEBOUNCE_MS 0

static const char *TAG = "ui_overlays";

static lv_obj_t *status_bar;
static lv_obj_t *time_label;
static lv_obj_t *wifi_label;
static lv_timer_t *suggest_timer = NULL;
static lv_obj_t  *suggest_ta    = NULL;   /* textarea being monitored */

/* Forward declarations */
static void lookup_result_cb(const dict_entry_t *entry, bool success);

static void set_search_headword_list_height(lv_coord_t height)
{
    if (objects.search_headword_lst) {
        lv_obj_set_height(objects.search_headword_lst, height);
    }
}

static void hide_search_keyboard(void)
{
    if (!objects.search_kb) {
        return;
    }

    lv_obj_t *textarea = lv_keyboard_get_textarea(objects.search_kb);
    if (textarea) {
        ui_obj_set_state_if(textarea, LV_STATE_FOCUSED, false);
        ui_obj_set_state_if(textarea, LV_STATE_CHECKED, false);
        lv_obj_clear_flag(textarea, LV_OBJ_FLAG_STATE_TRICKLE);
    }

    lv_keyboard_set_textarea(objects.search_kb, NULL);
    ui_obj_set_hidden(objects.search_kb, true);
    set_search_headword_list_height(SEARCH_HEADWORD_LIST_EXPANDED_HEIGHT);
}

static void show_search_keyboard(lv_obj_t *textarea)
{
    if (!objects.search_kb || !textarea) {
        return;
    }

    lv_obj_add_state(textarea, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(objects.search_kb, textarea);
    ui_obj_set_hidden(objects.search_kb, false);
    lv_obj_move_foreground(objects.search_kb);
    set_search_headword_list_height(SEARCH_HEADWORD_LIST_COMPACT_HEIGHT);
}

static void keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        hide_search_keyboard();
    }
}

static void textarea_focus_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *textarea = lv_event_get_target(event);
    show_search_keyboard(textarea);
}

static void open_search_screen_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED) || !objects.search) {
        return;
    }

    screen_navigate(objects.search, SCREEN_ANIM_LEFT);
    show_search_keyboard(objects.search_search_ta);
}

static void navigate_to_screen_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_CLICKED)) {
        return;
    }

    lv_obj_t *target_screen = (lv_obj_t *)lv_event_get_user_data(event);
    if (!target_screen) {
        ESP_LOGW(TAG, "navigate_to_screen_cb: target screen is NULL");
        return;
    }

    screen_navigate(target_screen, SCREEN_ANIM_LEFT);
}

static void swipe_back_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_GESTURE)) return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;

    if (lv_indev_get_gesture_dir(indev) != LV_DIR_RIGHT) return;

    // After acting on the gesture, tell LVGL to ignore further
    // input events until the user lifts their finger
    lv_indev_wait_release(indev);

    lv_obj_t *screen = lv_event_get_target(event);
    if (screen == objects.search) hide_search_keyboard();

    lv_obj_t *target_screen = (lv_obj_t *)lv_event_get_user_data(event);
    if (!target_screen) return;

    screen_navigate(target_screen, SCREEN_ANIM_RIGHT);
}

/* ── Spell-check suggestion helpers ───────────────────────────────── */

static void suggest_list_btn_cb(lv_event_t *event)
{
    lv_obj_t *btn = lv_event_get_target(event);
    if (!btn || !suggest_ta) return;

    /* lv_list_add_btn creates a label child; the text is on the button itself */
    const char *text = lv_list_get_btn_text(objects.search_headword_lst, btn);
    if (text) {
        lv_textarea_set_text(suggest_ta, text);
        lv_textarea_set_cursor_pos(suggest_ta, LV_TEXTAREA_CURSOR_LAST);
        /* Navigate to definition screen and look up the word */
        ui_overlays_open_definition(text);
    }
}

static void update_suggestion_list(const char *text)
{
    if (!objects.search_headword_lst) return;

    /* Clear existing items */
    lv_obj_clean(objects.search_headword_lst);

    if (!text || *text == '\0') return;

    /* Skip non-alpha input (e.g. user typed a space or number) */
    for (const char *p = text; *p; ++p) {
        if (!isalpha((unsigned char)*p)) return;
    }

    if (!dict_spellcheck_is_ready()) {
        ESP_LOGD(TAG, "Spellcheck not ready, skipping suggestions");
        return;
    }

    char results[MAX_SUGGESTIONS][MAX_WORD_LEN];
    size_t count = dict_spellcheck_suggest(text, results, MAX_SUGGESTIONS);

    for (size_t i = 0; i < count; ++i) {
        lv_obj_t *btn = lv_list_add_btn(objects.search_headword_lst, NULL, results[i]);
        if (btn) {
            lv_obj_add_event_cb(btn, suggest_list_btn_cb, LV_EVENT_CLICKED, NULL);
        }
    }
}

static void suggest_timer_cb(lv_timer_t *timer)
{
    if (!suggest_ta) return;

    const char *text = lv_textarea_get_text(suggest_ta);
    if (!text) return;

    update_suggestion_list(text);

    /* Pause after firing; will be resumed on next VALUE_CHANGED */
    lv_timer_pause(timer);
}

static void textarea_value_changed_cb(lv_event_t *event)
{
    LV_UNUSED(event);

    /* Reset debounce timer: when text changes, wait SUGGEST_DEBOUNCE_MS
     * before actually performing the search. */
    if (suggest_timer) {
        lv_timer_resume(suggest_timer);
        lv_timer_reset(suggest_timer);
    }
}

static void create_suggest_timer(void)
{
    /* One-shot timer that fires after SUGGEST_DEBOUNCE_MS of inactivity */
    suggest_timer = lv_timer_create(suggest_timer_cb, SUGGEST_DEBOUNCE_MS, NULL);
    if (suggest_timer) {
        lv_timer_pause(suggest_timer);   /* start paused; resume on first edit */
    }
}

static void create_status_bar(void)
{
    lv_display_t *display = lv_display_get_default();
    if (!display) {
        ESP_LOGW(TAG, "No default display available for status bar");
        return;
    }

    status_bar = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_size(status_bar, lv_display_get_horizontal_resolution(display), STATUS_BAR_HEIGHT);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(status_bar, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    time_label = lv_label_create(status_bar);
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_20, 0);
    lv_label_set_text(time_label, "--:--");
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 12, 0);

    wifi_label = lv_label_create(status_bar);
    lv_obj_set_style_text_color(wifi_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_20, 0);
    lv_label_set_text(wifi_label, "WiFi: --");
    lv_obj_align(wifi_label, LV_ALIGN_RIGHT_MID, -12, 0);

    bool dark_mode = false;
    settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &dark_mode);
    ui_overlays_apply_theme(dark_mode);
}

static void create_keyboard(void)
{
    if (!objects.search_kb) {
        ESP_LOGW(TAG, "Generated keyboard object is missing");
        return;
    }

    apply_custom_keyboard_layout(objects.search_kb);
    lv_obj_add_event_cb(objects.search_kb, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(objects.search_kb, keyboard_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_move_background(objects.search_kb);
}

void ui_overlays_init(void)
{
    ESP_LOGI(TAG, "[INIT] ui_overlays_init() - Applying saved settings");
    
    bool dark_mode = false;
    esp_err_t err = settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &dark_mode);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[INIT] Loaded dark_mode from settings: %u", dark_mode);
    } else {
        ESP_LOGW(TAG, "[INIT] Failed to load dark_mode, using default: false");
    }
    settings_ui_apply_theme(dark_mode);
    ui_overlays_apply_theme(dark_mode);

    uint8_t brightness = 100;
    err = settings_get_u8(SETTINGS_KEY_BRIGHTNESS, 100, &brightness);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[INIT] Loaded brightness from settings: %u%%", brightness);
    } else {
        ESP_LOGW(TAG, "[INIT] Failed to load brightness, using default: 100%%");
    }
    int set_err = bsp_display_brightness_set((int)brightness);
    if (set_err != 0) {
        ESP_LOGE(TAG, "[INIT] Failed to apply brightness: %d", set_err);
    }

    bool show_zh = true;
    err = settings_get_bool(SETTINGS_KEY_SHOW_ZH, true, &show_zh);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[INIT] Loaded show_chinese_definition from settings: %u", show_zh);
    } else {
        ESP_LOGW(TAG, "[INIT] Failed to load show_chinese_definition, using default: true");
    }

    settings_ui_build(objects.settings_cont);
    create_status_bar();
    create_keyboard();
    create_suggest_timer();

    /* Initialize dictionary lookup and register result callback */
    dict_lookup_init();
    dict_lookup_set_result_cb(lookup_result_cb);
}

void ui_overlays_bind_textarea(lv_obj_t *textarea)
{
    if (!textarea) {
        return;
    }

    /* Store reference for suggestion updates */
    suggest_ta = textarea;

    lv_obj_add_event_cb(textarea, textarea_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(textarea, textarea_focus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(textarea, textarea_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void ui_overlays_bind_search_open_button(lv_obj_t *button)
{
    ui_bind_event(button, open_search_screen_cb, LV_EVENT_CLICKED, NULL);
}

void ui_overlays_bind_swipe_back(lv_obj_t *screen, lv_obj_t *target_screen)
{
    if (!screen || !target_screen) {
        return;
    }

    lv_obj_add_event_cb(screen, swipe_back_cb, LV_EVENT_GESTURE, target_screen);
}

void ui_overlays_set_time_text(const char *text)
{
    ui_label_set_text_if(time_label, text);
}

void ui_overlays_set_wifi_text(const char *text)
{
    ui_label_set_text_if(wifi_label, text);
}

void ui_overlays_apply_theme(bool dark_mode)
{
    if (!status_bar) {
        return;
    }

    lv_color_t bg_color = dark_mode
        ? lv_palette_darken(LV_PALETTE_GREY, 4)
        : lv_palette_main(LV_PALETTE_BLUE);
    lv_obj_set_style_bg_color(status_bar, bg_color, 0);
    if (time_label) {
        lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    }
    if (wifi_label) {
        lv_obj_set_style_text_color(wifi_label, lv_color_white(), 0);
    }
}

void ui_overlays_bind_navigation_button(lv_obj_t *button, lv_obj_t *target_screen)
{
    if (!button || !target_screen) {
        return;
    }

    lv_obj_add_event_cb(button, navigate_to_screen_cb, LV_EVENT_CLICKED, target_screen);
}

/* ── Definition screen ────────────────────────────────────────────── */

static void definition_populate(const dict_entry_t *entry, bool success)
{
    if (!objects.definition_cont) {
        ESP_LOGW(TAG, "definition_cont is NULL");
        return;
    }

    /* Clear any previous content */
    lv_obj_clean(objects.definition_cont);

    /* Use flex column layout so items flow naturally without manual y-offset */
    lv_obj_set_flex_flow(objects.definition_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(objects.definition_cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(objects.definition_cont, 4, 0);
    lv_obj_set_style_pad_column(objects.definition_cont, 0, 0);

    bool dark_mode = false;
    settings_get_bool(SETTINGS_KEY_DARK_MODE, false, &dark_mode);
    lv_color_t text_color = dark_mode ? lv_color_white() : lv_color_black();
    lv_color_t pos_color = lv_palette_main(LV_PALETTE_BLUE);
    lv_color_t subtle_color = dark_mode
        ? lv_palette_lighten(LV_PALETTE_GREY, 2)
        : lv_palette_darken(LV_PALETTE_GREY, 1);
    lv_color_t zh_color = lv_palette_main(LV_PALETTE_ORANGE);

    if (!success || !entry || entry->sense_count == 0) {
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
    lv_obj_set_style_text_font(word_lbl, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(word_lbl, text_color, 0);
    lv_obj_set_width(word_lbl, lv_pct(100));

    /* ── IPA (pronunciation) ────────────────────────────────────── */
    /* API already includes slashes, e.g. "/kliːn/" */
    const char *ipa = entry->senses[0].ipa;
    if (ipa && ipa[0] != '\0') {
        lv_obj_t *ipa_lbl = lv_label_create(objects.definition_cont);
        /* If IPA already starts with '/', use as-is; else wrap in slashes */
        if (ipa[0] == '/') {
            lv_label_set_text(ipa_lbl, ipa);
        } else {
            char ipa_buf[68];
            snprintf(ipa_buf, sizeof(ipa_buf), "/%s/", ipa);
            lv_label_set_text(ipa_lbl, ipa_buf);
        }
        lv_obj_set_style_text_font(ipa_lbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(ipa_lbl, subtle_color, 0);
        lv_obj_set_width(ipa_lbl, lv_pct(100));
    }

    /* ── Separator ──────────────────────────────────────────────── */
    lv_obj_t *sep = lv_obj_create(objects.definition_cont);
    lv_obj_remove_style_all(sep);
    lv_obj_set_size(sep, lv_pct(100), 1);
    lv_obj_set_style_bg_opa(sep, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(sep, text_color, 0);

    /* ── Senses ─────────────────────────────────────────────────── */
    bool show_zh = true;
    settings_get_bool(SETTINGS_KEY_SHOW_ZH, true, &show_zh);

    for (uint8_t i = 0; i < entry->sense_count; ++i) {
        const dict_sense_t *s = &entry->senses[i];

        /* POS badge + definition row */
        lv_obj_t *row = lv_obj_create(objects.definition_cont);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(row, 8, 0);
        lv_obj_set_style_margin_top(row, 6, 0);

        /* POS badge */
        if (s->pos[0] != '\0') {
            lv_obj_t *pos_lbl = lv_label_create(row);
            lv_label_set_text(pos_lbl, s->pos);
            lv_obj_set_style_text_font(pos_lbl, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(pos_lbl, pos_color, 0);
            lv_obj_set_style_bg_opa(pos_lbl, LV_OPA_20, 0);
            lv_obj_set_style_bg_color(pos_lbl, pos_color, 0);
            lv_obj_set_style_radius(pos_lbl, 4, 0);
            lv_obj_set_style_pad_left(pos_lbl, 6, 0);
            lv_obj_set_style_pad_right(pos_lbl, 6, 0);
            lv_obj_set_style_pad_top(pos_lbl, 2, 0);
            lv_obj_set_style_pad_bottom(pos_lbl, 2, 0);
        }

        /* Definition */
        if (s->definition[0] != '\0') {
            lv_obj_t *def_lbl = lv_label_create(row);
            lv_label_set_text(def_lbl, s->definition);
            lv_obj_set_style_text_font(def_lbl, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(def_lbl, text_color, 0);
            lv_obj_set_flex_grow(def_lbl, 1);
        }

        /* Chinese translation */
        if (show_zh && s->definition_zh[0] != '\0') {
            lv_obj_t *zh_lbl = lv_label_create(objects.definition_cont);
            lv_label_set_text(zh_lbl, s->definition_zh);
            lv_obj_set_style_text_font(zh_lbl, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(zh_lbl, zh_color, 0);
            lv_obj_set_width(zh_lbl, lv_pct(100));
            lv_obj_set_style_margin_left(zh_lbl, 8, 0);
        }

        /* Example */
        if (s->example[0] != '\0') {
            lv_obj_t *ex_lbl = lv_label_create(objects.definition_cont);
            char ex_buf[260];
            snprintf(ex_buf, sizeof(ex_buf), "\"%s\"", s->example);
            lv_label_set_text(ex_lbl, ex_buf);
            lv_obj_set_style_text_font(ex_lbl, &lv_font_montserrat_18, 0);
            lv_obj_set_style_text_color(ex_lbl, subtle_color, 0);
            lv_obj_set_width(ex_lbl, lv_pct(100));
            lv_obj_set_style_margin_left(ex_lbl, 8, 0);
        }
    }
}

static void lookup_result_cb(const dict_entry_t *entry, bool success)
{
    ESP_LOGI(TAG, "Lookup result: success=%d, senses=%d", success,
             entry ? entry->sense_count : 0);
    definition_populate(entry, success);
}

void ui_overlays_open_definition(const char *word)
{
    if (!word || *word == '\0') return;

    /* Navigate to definition screen */
    if (objects.definition) {
        hide_search_keyboard();
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
