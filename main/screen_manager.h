#pragma once
#include "lvgl.h"

typedef enum {
    SCREEN_ANIM_NONE  = 0,
    SCREEN_ANIM_LEFT,       // slide in from right
    SCREEN_ANIM_RIGHT,      // slide in from left
    SCREEN_ANIM_FADE,
} screen_anim_t;

// The ONE function you call everywhere
void screen_navigate(lv_obj_t * screen, screen_anim_t anim);
