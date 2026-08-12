/*
 * Petra per-key reactive RGB
 *
 * Replaces the stock ZMK RGB underglow with:
 *   - Animated base patterns (solid, breathe, spectrum, swirl)
 *   - Per-key reactive effects (flash, trail, layer color)
 *
 * The physical serpentine LED order is resolved through key_to_led[] in
 * key_led_map.h. Key events only mark state; the animation work handler is
 * the sole writer to the LED strip, so we never touch SPI from the event
 * callback.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/keymap.h>

#include <zephyr/drivers/regulator.h>

#include <zmk_vfx_petra/rgb_petra.h>
#include <zmk_vfx_petra/key_led_map.h>

LOG_MODULE_REGISTER(rgb_petra, CONFIG_ZMK_LOG_LEVEL);

#define STRIP_CHOSEN DT_CHOSEN(zmk_underglow)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_CHOSEN, chain_length)

BUILD_ASSERT(STRIP_NUM_PIXELS >= RGB_PETRA_NUM_KEYS,
             "LED chain shorter than the number of mapped keys");

static const struct device *led_strip = DEVICE_DT_GET(STRIP_CHOSEN);

static struct led_rgb pixels[STRIP_NUM_PIXELS];

/* Per-LED reactive intensity (0-255), decayed each frame. */
static uint8_t reactive[STRIP_NUM_PIXELS];

/* Physical column (0-13) and row (0-5) of each LED in the chain, derived
 * from the serpentine wiring. Used by the flag (horizontal swirl) effect. */
static const uint8_t led_col[RGB_PETRA_NUM_KEYS] = {
    9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10,
    9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 11, 10,
    9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 11, 10,
    9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3,
    6,
};

static const uint8_t led_row[RGB_PETRA_NUM_KEYS] = {
    5, 5, 5, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5,
    5,
};

#define RGB_PETRA_COLS 14
#define RGB_PETRA_ROWS 6

struct rgb_petra_state {
    bool on;
    enum rgb_petra_effect effect;
    uint16_t hue;   /* 0-359 */
    uint8_t sat;    /* 0-100 */
    uint8_t brt;    /* 0-100 */
    uint16_t phase; /* animation counter */
};

static struct rgb_petra_state state = {
    .on = IS_ENABLED(CONFIG_RGB_PETRA_START_ON),
    .effect = RGB_PETRA_EFF_SOLID,
    .hue = 0,
    .sat = 100,
    .brt = CONFIG_RGB_PETRA_BRT_START,
};

/* ---- HSB -> RGB (integer math, h:0-359 s:0-100 b:0-100) ---- */
static struct led_rgb hsb_to_rgb(uint16_t h, uint8_t s, uint8_t b) {
    uint32_t rp = 0, gp = 0, bp = 0;

    uint8_t sector = (h / 60) % 6;
    uint32_t f = h % 60;             /* 0-59 */
    uint32_t v = b;                  /* 0-100 */
    uint32_t p = v * (100 - s) / 100;
    uint32_t q = v * (100 - (s * f) / 60) / 100;
    uint32_t t = v * (100 - (s * (60 - f)) / 60) / 100;

    switch (sector) {
    case 0: rp = v; gp = t; bp = p; break;
    case 1: rp = q; gp = v; bp = p; break;
    case 2: rp = p; gp = v; bp = t; break;
    case 3: rp = p; gp = q; bp = v; break;
    case 4: rp = t; gp = p; bp = v; break;
    case 5: rp = v; gp = p; bp = q; break;
    }

    struct led_rgb out = {
        .r = (uint8_t)(rp * 255 / 100),
        .g = (uint8_t)(gp * 255 / 100),
        .b = (uint8_t)(bp * 255 / 100),
    };
    return out;
}

static inline struct led_rgb scale(struct led_rgb c, uint8_t factor /*0-255*/) {
    c.r = (uint16_t)c.r * factor / 255;
    c.g = (uint16_t)c.g * factor / 255;
    c.b = (uint16_t)c.b * factor / 255;
    return c;
}

/* ---- Base pattern renderers: fill pixels[] (physical order) ---- */

static void render_solid(void) {
    struct led_rgb c = hsb_to_rgb(state.hue, state.sat, state.brt);
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = c;
    }
}

static void render_breathe(void) {
    /* triangle wave 0..255..0 over ~256 frames */
    uint8_t p = state.phase & 0xFF;
    uint8_t tri = (state.phase & 0x100) ? (255 - p) : p;
    uint8_t brt = (uint32_t)state.brt * tri / 255;
    struct led_rgb c = hsb_to_rgb(state.hue, state.sat, brt);
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = c;
    }
}

static void render_spectrum(void) {
    uint16_t h = (state.hue + state.phase) % 360;
    struct led_rgb c = hsb_to_rgb(h, state.sat, state.brt);
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = c;
    }
}

static void render_swirl(void) {
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        uint16_t h = (state.hue + state.phase + (i * 360 / STRIP_NUM_PIXELS)) % 360;
        pixels[i] = hsb_to_rgb(h, state.sat, state.brt);
    }
}

/* Flag: full rainbow spread across the width (by column), travelling left to
 * right, with a slight per-row skew so the wave undulates like a flag. */
static void render_flag(void) {
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        /* one full rainbow across the 14 columns */
        uint16_t col_hue = led_col[i] * 360 / RGB_PETRA_COLS;
        /* gentle diagonal skew: each row down shifts the wave a bit */
        uint16_t row_skew = led_row[i] * 18;
        uint16_t h = (state.hue + state.phase + col_hue + row_skew) % 360;
        pixels[i] = hsb_to_rgb(h, state.sat, state.brt);
    }
}

/* ---- Reactive renderers ---- */

static void render_reactive_flash(void) {
    /* dim base, keys light up to full on press then decay */
    struct led_rgb base = hsb_to_rgb(state.hue, state.sat, state.brt / 6);
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        if (reactive[i]) {
            struct led_rgb hot = hsb_to_rgb(state.hue, state.sat, state.brt);
            pixels[i] = scale(hot, reactive[i]);
        } else {
            pixels[i] = base;
        }
    }
}

static void render_reactive_trail(void) {
    /* like flash but hue shifts with intensity for a trail feel */
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        if (reactive[i]) {
            uint16_t h = (state.hue + (255 - reactive[i])) % 360;
            struct led_rgb hot = hsb_to_rgb(h, state.sat, state.brt);
            pixels[i] = scale(hot, reactive[i]);
        } else {
            pixels[i] = (struct led_rgb){0, 0, 0};
        }
    }
}

static void render_layer_color(void) {
    /* base hue derived from the highest active layer */
    uint8_t layer = zmk_keymap_highest_layer_active();
    uint16_t h = (state.hue + layer * 40) % 360;
    struct led_rgb base = hsb_to_rgb(h, state.sat, state.brt);
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = base;
        if (reactive[i]) {
            struct led_rgb hot = hsb_to_rgb((h + 180) % 360, state.sat, state.brt);
            pixels[i] = scale(hot, reactive[i]);
        }
    }
}

static void decay_reactive(void) {
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        if (reactive[i] > CONFIG_RGB_PETRA_REACTIVE_DECAY) {
            reactive[i] -= CONFIG_RGB_PETRA_REACTIVE_DECAY;
        } else {
            reactive[i] = 0;
        }
    }
}

/* ---- Animation tick ---- */

static void rgb_petra_tick(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(rgb_petra_work, rgb_petra_tick);

static void rgb_petra_tick(struct k_work *work) {
    if (!state.on) {
        return;
    }

    switch (state.effect) {
    case RGB_PETRA_EFF_SOLID:          render_solid(); break;
    case RGB_PETRA_EFF_BREATHE:        render_breathe(); break;
    case RGB_PETRA_EFF_SPECTRUM:       render_spectrum(); break;
    case RGB_PETRA_EFF_SWIRL:          render_swirl(); break;
    case RGB_PETRA_EFF_FLAG:           render_flag(); break;
    case RGB_PETRA_EFF_REACTIVE_FLASH: render_reactive_flash(); break;
    case RGB_PETRA_EFF_REACTIVE_TRAIL: render_reactive_trail(); break;
    case RGB_PETRA_EFF_LAYER_COLOR:    render_layer_color(); break;
    default: render_solid(); break;
    }

    decay_reactive();

    int err = led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
    if (err < 0) {
        LOG_ERR("led_strip_update_rgb failed: %d", err);
    } else if (state.phase == 0) {
        LOG_INF("first frame written to strip OK");
    }

    state.phase++;
    k_work_reschedule(&rgb_petra_work, K_MSEC(CONFIG_RGB_PETRA_TICK_MS));
}

static void clear_strip(void) {
    memset(pixels, 0, sizeof(pixels));
    led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
}

#if DT_NODE_EXISTS(DT_NODELABEL(petra_led_pwr))
static const struct device *led_pwr = DEVICE_DT_GET(DT_NODELABEL(petra_led_pwr));

static void ext_power_set(bool on) {
    if (led_pwr == NULL || !device_is_ready(led_pwr)) {
        return;
    }
    if (on) {
        regulator_enable(led_pwr);
    } else {
        regulator_disable(led_pwr);
    }
}
#else
static void ext_power_set(bool on) { ARG_UNUSED(on); }
#endif

static void start_anim(void) {
    ext_power_set(true);
    k_work_reschedule(&rgb_petra_work, K_NO_WAIT);
}

static void stop_anim(void) {
    k_work_cancel_delayable(&rgb_petra_work);
    clear_strip();
    ext_power_set(false);
}

/* ---- Behavior command entry ---- */

int rgb_petra_command(uint8_t cmd, uint8_t param) {
    ARG_UNUSED(param);
    LOG_INF("rgb_petra_command cmd=%d (on=%d effect=%d)", cmd, state.on, state.effect);

    switch (cmd) {
    case RGB_PETRA_CMD_TOGGLE:
        state.on = !state.on;
        break;
    case RGB_PETRA_CMD_ON:
        state.on = true;
        break;
    case RGB_PETRA_CMD_OFF:
        state.on = false;
        break;
    case RGB_PETRA_CMD_EFF_NEXT:
        state.effect = (state.effect + 1) % RGB_PETRA_EFF_NUM;
        break;
    case RGB_PETRA_CMD_EFF_PREV:
        state.effect = (state.effect + RGB_PETRA_EFF_NUM - 1) % RGB_PETRA_EFF_NUM;
        break;
    case RGB_PETRA_CMD_HUE_UP:
        state.hue = (state.hue + CONFIG_RGB_PETRA_HUE_STEP) % 360;
        break;
    case RGB_PETRA_CMD_HUE_DN:
        state.hue = (state.hue + 360 - CONFIG_RGB_PETRA_HUE_STEP) % 360;
        break;
    case RGB_PETRA_CMD_BRT_UP:
        state.brt = MIN(100, state.brt + CONFIG_RGB_PETRA_BRT_STEP);
        break;
    case RGB_PETRA_CMD_BRT_DN:
        state.brt = (state.brt > CONFIG_RGB_PETRA_BRT_STEP)
                        ? state.brt - CONFIG_RGB_PETRA_BRT_STEP
                        : 0;
        break;
    default:
        return -ENOTSUP;
    }

    if (state.on) {
        start_anim();
    } else {
        stop_anim();
    }
    return 0;
}

/* ---- Key event listener: mark reactive intensity only ---- */

static int rgb_petra_key_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (ev->position < RGB_PETRA_NUM_KEYS) {
        uint8_t led = key_to_led[ev->position];
        if (led < STRIP_NUM_PIXELS) {
            reactive[led] = 255;
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_petra_keys, rgb_petra_key_listener);
ZMK_SUBSCRIPTION(rgb_petra_keys, zmk_position_state_changed);

/* ---- Idle handling ---- */

#if IS_ENABLED(CONFIG_RGB_PETRA_AUTO_OFF_IDLE)
static int rgb_petra_activity_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (ev->state == ZMK_ACTIVITY_ACTIVE) {
        if (state.on) {
            start_anim();
        }
    } else {
        stop_anim();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_petra_activity, rgb_petra_activity_listener);
ZMK_SUBSCRIPTION(rgb_petra_activity, zmk_activity_state_changed);
#endif

/* ---- Init ---- */

static int rgb_petra_init(void) {
    LOG_INF("rgb_petra init: start_on=%d effect=%d strip=%p",
            state.on, state.effect, (void *)led_strip);

    if (!device_is_ready(led_strip)) {
        LOG_ERR("LED strip device NOT ready");
        return -ENODEV;
    }
    LOG_INF("LED strip ready, %d pixels", STRIP_NUM_PIXELS);

    if (state.on) {
        start_anim();
        LOG_INF("animation started");
    }
    return 0;
}

SYS_INIT(rgb_petra_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);