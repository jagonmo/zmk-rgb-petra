#pragma once

#include <zephyr/kernel.h>

/* Commands issued from the &rgb_petra behavior (param1). Mirrors the stock
 * RGB_* command set so the same keymap bindings feel familiar. */
#define RGB_PETRA_CMD_TOGGLE   0  /* on/off */
#define RGB_PETRA_CMD_ON       1
#define RGB_PETRA_CMD_OFF      2
#define RGB_PETRA_CMD_EFF_NEXT 3  /* cycle to next effect */
#define RGB_PETRA_CMD_EFF_PREV 4
#define RGB_PETRA_CMD_HUE_UP   5
#define RGB_PETRA_CMD_HUE_DN   6
#define RGB_PETRA_CMD_BRT_UP   7
#define RGB_PETRA_CMD_BRT_DN   8

/* Effects. Keep REACTIVE_* after the static ones; NUM must stay last. */
enum rgb_petra_effect {
    RGB_PETRA_EFF_SOLID = 0,
    RGB_PETRA_EFF_BREATHE,
    RGB_PETRA_EFF_SPECTRUM,
    RGB_PETRA_EFF_SWIRL,
    RGB_PETRA_EFF_REACTIVE_FLASH,
    RGB_PETRA_EFF_REACTIVE_TRAIL,
    RGB_PETRA_EFF_LAYER_COLOR,
    RGB_PETRA_EFF_NUM,
};

/* Entry point used by the behavior driver. */
int rgb_petra_command(uint8_t cmd, uint8_t param);
