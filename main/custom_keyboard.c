#include "custom_keyboard.h"
#include "ui.h"
#include "esp_log.h"
#include <stddef.h>

#define CUSTOM_KB_CTRL_KEY(width)    (LV_BTNMATRIX_CTRL_POPOVER | (width))
#define CUSTOM_KB_CTRL_ACTION(width) (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG | LV_BTNMATRIX_CTRL_CHECKED | (width))
#define CUSTOM_KB_CTRL_HIDDEN(width) (LV_BTNMATRIX_CTRL_HIDDEN | (width))

static const char *TAG = "custom_keyboard";

static const char *custom_kb_map_lower[] = {
"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
" ", "a", "s", "d", "f", "g", "h", "j", "k", "l", " ", "\n",
" ", "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
"ABC", " ", LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t custom_kb_ctrl_lower[] = {
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_HIDDEN(1),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_HIDDEN(1),
CUSTOM_KB_CTRL_HIDDEN(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_ACTION(4),
CUSTOM_KB_CTRL_ACTION(4), 12, CUSTOM_KB_CTRL_ACTION(4)
};

static const char *custom_kb_map_upper[] = {
"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
" ", "A", "S", "D", "F", "G", "H", "J", "K", "L", " ", "\n",
" ", "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
"abc", " ", LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t custom_kb_ctrl_upper[] = {
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_HIDDEN(1),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_HIDDEN(1),
CUSTOM_KB_CTRL_HIDDEN(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2),
CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_KEY(2), CUSTOM_KB_CTRL_ACTION(4),
CUSTOM_KB_CTRL_ACTION(4), 12, CUSTOM_KB_CTRL_ACTION(4)
};

void apply_custom_keyboard_layout(lv_obj_t *keyboard)
{
    if (keyboard == NULL) {
        ESP_LOGW(TAG, "Keyboard object is null; skipping custom keyboard layout");
        return;
    }
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, custom_kb_map_lower, custom_kb_ctrl_lower);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, custom_kb_map_upper, custom_kb_ctrl_upper);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
}
