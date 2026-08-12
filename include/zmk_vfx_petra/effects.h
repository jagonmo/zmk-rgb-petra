#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>

#include <zmk_vfx_petra/key_led_map.h>

/* Physical geometry of the flatcat matrix. */
#define RGB_PETRA_COLS 14
#define RGB_PETRA_ROWS 6

/* ---- Central effect enum (QMK RGB Matrix port) ----
 * Order defines the cycle order for RGBP_EFF. Backlog effects live after
 * RGB_PETRA_EFF_ACTIVE_END and are compiled but never entered in the cycle. */
enum rgb_petra_effect {
    /* Tanda 1: base (non-reactive) */
    RGB_PETRA_EFF_SOLID = 0,
    RGB_PETRA_EFF_GRADIENT_UP_DOWN,
    RGB_PETRA_EFF_GRADIENT_LEFT_RIGHT,
    RGB_PETRA_EFF_BREATHING,
    RGB_PETRA_EFF_BAND_SAT,
    RGB_PETRA_EFF_BAND_VAL,
    RGB_PETRA_EFF_BAND_PINWHEEL_SAT,
    RGB_PETRA_EFF_BAND_PINWHEEL_VAL,
    RGB_PETRA_EFF_BAND_SPIRAL_SAT,
    RGB_PETRA_EFF_BAND_SPIRAL_VAL,
    RGB_PETRA_EFF_CYCLE_ALL,
    RGB_PETRA_EFF_CYCLE_LEFT_RIGHT,
    RGB_PETRA_EFF_CYCLE_UP_DOWN,
    RGB_PETRA_EFF_RAINBOW_MOVING_CHEVRON,
    RGB_PETRA_EFF_CYCLE_OUT_IN,
    RGB_PETRA_EFF_CYCLE_OUT_IN_DUAL,
    RGB_PETRA_EFF_CYCLE_PINWHEEL,
    RGB_PETRA_EFF_CYCLE_SPIRAL,
    RGB_PETRA_EFF_DUAL_BEACON,
    RGB_PETRA_EFF_RAINBOW_BEACON,
    RGB_PETRA_EFF_RAINBOW_PINWHEELS,

    /* Tanda 2: animated (RNG / stateful) */
    RGB_PETRA_EFF_RAINDROPS,
    RGB_PETRA_EFF_JELLYBEAN_RAINDROPS,
    RGB_PETRA_EFF_HUE_BREATHING,
    RGB_PETRA_EFF_HUE_PENDULUM,
    RGB_PETRA_EFF_HUE_WAVE,
    RGB_PETRA_EFF_PIXEL_FRACTAL,
    RGB_PETRA_EFF_PIXEL_FLOW,
    RGB_PETRA_EFF_PIXEL_RAIN,

    /* Tanda 3: reactive (key-triggered) */
    RGB_PETRA_EFF_TYPING_HEATMAP,
    RGB_PETRA_EFF_DIGITAL_RAIN,
    RGB_PETRA_EFF_SOLID_REACTIVE_SIMPLE,
    RGB_PETRA_EFF_SOLID_REACTIVE,
    RGB_PETRA_EFF_SOLID_REACTIVE_WIDE,
    RGB_PETRA_EFF_SOLID_REACTIVE_MULTIWIDE,
    RGB_PETRA_EFF_SOLID_REACTIVE_CROSS,
    RGB_PETRA_EFF_SOLID_REACTIVE_MULTICROSS,
    RGB_PETRA_EFF_SOLID_REACTIVE_NEXUS,
    RGB_PETRA_EFF_SOLID_REACTIVE_MULTINEXUS,
    RGB_PETRA_EFF_SPLASH,
    RGB_PETRA_EFF_MULTISPLASH,
    RGB_PETRA_EFF_SOLID_SPLASH,
    RGB_PETRA_EFF_SOLID_MULTISPLASH,

    /* End of the active cycle. */
    RGB_PETRA_EFF_ACTIVE_END,

    /* Backlog: compiled but not in the cycle (petra originals). */
    RGB_PETRA_EFF_FLAG = RGB_PETRA_EFF_ACTIVE_END,
    RGB_PETRA_EFF_LAYER_COLOR,

    RGB_PETRA_EFF_NUM,
};

/* ---- Shared global state (defined in rgb_petra.c) ---- */
struct rgb_petra_state {
    bool on;
    enum rgb_petra_effect effect;
    uint16_t hue;   /* 0-359 */
    uint8_t sat;    /* 0-100 */
    uint8_t brt;    /* 0-100 */
    uint16_t phase; /* animation counter, ++ each frame */
};

extern struct rgb_petra_state state;

/* Frame buffer, physical LED order (defined in rgb_petra.c). */
extern struct led_rgb pixels[RGB_PETRA_NUM_KEYS];

/* Per-LED reactive intensity 0-255, decayed each frame (defined in rgb_petra.c). */
extern uint8_t reactive[RGB_PETRA_NUM_KEYS];

/* Physical geometry tables (defined in rgb_petra.c). */
extern const uint8_t led_col[RGB_PETRA_NUM_KEYS];
extern const uint8_t led_row[RGB_PETRA_NUM_KEYS];

/* ---- Shared helpers (defined in rgb_petra.c) ---- */
struct led_rgb rgbp_hsb(uint16_t h, uint8_t s, uint8_t b);
struct led_rgb rgbp_scale(struct led_rgb c, uint8_t factor /*0-255*/);

/* Simple xorshift PRNG for animated effects (defined in effects_animated.c). */
uint8_t rgbp_rand8(void);

/* ---- Effect renderers, grouped by file ---- */
/* effects_base.c */
void rgbp_render_base(enum rgb_petra_effect eff);
/* effects_animated.c */
void rgbp_render_animated(enum rgb_petra_effect eff);
/* effects_reactive.c */
void rgbp_render_reactive(enum rgb_petra_effect eff);
/* effects_backlog.c */
void rgbp_render_backlog(enum rgb_petra_effect eff);
