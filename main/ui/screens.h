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
    SCREEN_ID_SEARCH = 2,
    _SCREEN_ID_LAST = 2
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *search;
    lv_obj_t *nav_bar;
    lv_obj_t *ta_search_fake;
    lv_obj_t *btn_ocr;
    lv_obj_t *ta_search;
    lv_obj_t *lst_headword;
    lv_obj_t *kb_search;
    lv_obj_t *tb_search;
    lv_obj_t *search_btn_back;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_search();
void tick_screen_search();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/