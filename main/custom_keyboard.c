#include "custom_keyboard.h"
#include "ui.h"
#include "ui_helpers.h"
#include "esp_log.h"
#include <stddef.h>

#define CUSTOM_KB_CTRL_KEY(width)    (LV_BTNMATRIX_CTRL_POPOVER | (width))
#define CUSTOM_KB_CTRL_ACTION(width) (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG | LV_BTNMATRIX_CTRL_CHECKED | (width))
#define CUSTOM_KB_CTRL_HIDDEN(width) (LV_BTNMATRIX_CTRL_HIDDEN | (width))

static const char *TAG = "custom_keyboard";

static const char *custom_kb_map_lower[] = {
"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
" ", "a", "s", "d", "f", "g", "h", "j", "k", "l", " ", "\n",
LV_SYMBOL_UP, "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
"1#", " ", LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t custom_kb_ctrl_lower[] = {
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_HIDDEN(1),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_HIDDEN(1),
CUSTOM_KB_CTRL_ACTION(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_ACTION(4),
CUSTOM_KB_CTRL_ACTION(4), 12, CUSTOM_KB_CTRL_ACTION(4)
};

static const char *custom_kb_map_upper[] = {
"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
" ", "A", "S", "D", "F", "G", "H", "J", "K", "L", " ", "\n",
LV_SYMBOL_DOWN, "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
"1#", " ", LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t custom_kb_ctrl_upper[] = {
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_HIDDEN(1),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_HIDDEN(1),
CUSTOM_KB_CTRL_ACTION(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_ACTION(4),
CUSTOM_KB_CTRL_ACTION(4), 12, CUSTOM_KB_CTRL_ACTION(4)
};

static void custom_keyboard_event_cb(lv_event_t *event)
{
    if (!ui_event_is(event, LV_EVENT_VALUE_CHANGED)) {
        return;
    }

    lv_obj_t *keyboard = lv_event_get_current_target(event);
    if (keyboard == NULL) {
        return;
    }

    uint32_t btn_id = lv_buttonmatrix_get_selected_button(keyboard);
    if (btn_id == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }

    const char *txt = lv_buttonmatrix_get_button_text(keyboard, btn_id);
    if (txt == NULL) {
        return;
    }

    if (lv_strcmp(txt, LV_SYMBOL_UP) == 0) {
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER);
        return;
    }

    if (lv_strcmp(txt, LV_SYMBOL_DOWN) == 0) {
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
        return;
    }

    lv_keyboard_def_event_cb(event);
}

void apply_custom_keyboard_layout(lv_obj_t *keyboard)
{
    if (keyboard == NULL) {
        ESP_LOGW(TAG, "Keyboard object is null; skipping custom keyboard layout");
        return;
    }
    lv_obj_remove_event_cb(keyboard, lv_keyboard_def_event_cb);
    lv_obj_add_event_cb(keyboard, custom_keyboard_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, custom_kb_map_lower, custom_kb_ctrl_lower);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, custom_kb_map_upper, custom_kb_ctrl_upper);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
}
