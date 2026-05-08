#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_SETTINGS = 2,
    SCREEN_ID_SEARCH = 3,
    SCREEN_ID_WI_FI = 4,
    SCREEN_ID_DEFINITION = 5,
    _SCREEN_ID_LAST = 5
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *settings;
    lv_obj_t *search;
    lv_obj_t *wi_fi;
    lv_obj_t *definition;
    lv_obj_t *main_search_fake_ta;
    lv_obj_t *main_ocr_btn;
    lv_obj_t *main_reccent_searches_lst;
    lv_obj_t *main_history_btn;
    lv_obj_t *main_bookmark_btn;
    lv_obj_t *main_settings_btn;
    lv_obj_t *settings_cont;
    lv_obj_t *search_search_ta;
    lv_obj_t *search_headword_lst;
    lv_obj_t *search_kb;
    lv_obj_t *wi_fi_state_switch;
    lv_obj_t *wi_fi_state_label;
    lv_obj_t *wi_fi_network_lst;
    lv_obj_t *wi_fi_load_spinner;
    lv_obj_t *wifi_scan_btn;
    lv_obj_t *definition_cont;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_settings();
void tick_screen_settings();

void create_screen_search();
void tick_screen_search();

void create_screen_wi_fi();
void tick_screen_wi_fi();

void create_screen_definition();
void tick_screen_definition();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/