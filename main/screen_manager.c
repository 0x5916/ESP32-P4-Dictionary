#include "screen_manager.h"

#include "esp_lv_adapter.h"

void screen_navigate(lv_obj_t * screen, screen_anim_t anim) {
    lv_screen_load_anim_t lv_anim;
    switch (anim) {
        case SCREEN_ANIM_LEFT:  lv_anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;  break;
        case SCREEN_ANIM_RIGHT: lv_anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT; break;
        case SCREEN_ANIM_FADE:  lv_anim = LV_SCR_LOAD_ANIM_FADE_IN;    break;
        default:                lv_anim = LV_SCR_LOAD_ANIM_NONE;       break;
    }
    lv_screen_load_anim(screen, lv_anim, 200, 0, false);
}
