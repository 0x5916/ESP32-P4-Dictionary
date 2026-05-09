#include "navigation.h"
#include "ui/ui.h"
#include "ui_helpers.h"
#include "screen_manager.h"
#include "keyboard.h"
#include "esp_log.h"

static const char *TAG = "navigation";

void navigate_to_screen_cb(lv_event_t *event)
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

void swipe_back_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_GESTURE)) return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;

    if (lv_indev_get_gesture_dir(indev) != LV_DIR_RIGHT) return;

    // After acting on the gesture, tell LVGL to ignore further
    // input events until the user lifts their finger
    lv_indev_wait_release(indev);

    lv_obj_t *screen = lv_event_get_target(event);
    if (screen == objects.search) keyboard_hide();

    lv_obj_t *target_screen = (lv_obj_t *)lv_event_get_user_data(event);
    if (!target_screen) return;

    screen_navigate(target_screen, SCREEN_ANIM_RIGHT);
}

void navigation_bind_button(lv_obj_t *button, lv_obj_t *target_screen)
{
    if (!button || !target_screen) {
        return;
    }

    lv_obj_add_event_cb(button, navigate_to_screen_cb, LV_EVENT_CLICKED, target_screen);
}

void navigation_bind_swipe_back(lv_obj_t *screen, lv_obj_t *target_screen)
{
    if (!screen || !target_screen) {
        return;
    }

    lv_obj_add_event_cb(screen, swipe_back_cb, LV_EVENT_GESTURE, target_screen);
}