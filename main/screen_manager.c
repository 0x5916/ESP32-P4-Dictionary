#include "screen_manager.h"

#include "esp_lv_adapter.h"
#include "esp_log.h"

static const char *TAG = "screen_manager";

void screen_navigate(lv_obj_t * screen, screen_anim_t anim) {
    const char *anim_name = "NONE";
    lv_screen_load_anim_t lv_anim;
    
    switch (anim) {
        case SCREEN_ANIM_LEFT:  
            anim_name = "LEFT";
            lv_anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;  
            break;
        case SCREEN_ANIM_RIGHT: 
            anim_name = "RIGHT";
            lv_anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT; 
            break;
        case SCREEN_ANIM_FADE:  
            anim_name = "FADE";
            lv_anim = LV_SCR_LOAD_ANIM_FADE_IN;    
            break;
        default:                
            anim_name = "NONE";
            lv_anim = LV_SCR_LOAD_ANIM_NONE;       
            break;
    }
    
    ESP_LOGD(TAG, "[NAV] screen_navigate() screen=%p anim=%s", screen, anim_name);
    lv_screen_load_anim(screen, lv_anim, 150, 0, false);
    ESP_LOGD(TAG, "[NAV] Screen load animation initiated");
}
