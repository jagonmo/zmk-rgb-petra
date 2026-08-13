/*
 * Petra per-key RGB - core.
 *
 * Owns the shared global state (state, pixels, reactive, geometry tables),
 * the HSB helper, the animation tick, the ext-power regulator, the behavior
 * command entry, and the ZMK event listeners.
 *
 * Effect rendering lives in per-family files (effects_base.c,
 * effects_animated.c, effects_reactive.c, effects_backlog.c). The tick
 * dispatches to the right family by enum range.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/keymap.h>

#if IS_ENABLED(CONFIG_RGB_PETRA_CAPS_INDICATOR)
#include <zmk/hid_indicators.h>
#endif

#include <zmk_vfx_petra/rgb_petra.h>
#include <zmk_vfx_petra/key_led_map.h>
#include <zmk_vfx_petra/effects.h>

LOG_MODULE_REGISTER(rgb_petra, CONFIG_ZMK_LOG_LEVEL);

#define STRIP_CHOSEN DT_CHOSEN(zmk_underglow)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_CHOSEN, chain_length)

BUILD_ASSERT(STRIP_NUM_PIXELS >= RGB_PETRA_NUM_KEYS,
             "LED chain shorter than the number of mapped keys");

static const struct device *led_strip = DEVICE_DT_GET(STRIP_CHOSEN);

/* ---- Shared globals (declared extern in effects.h) ---- */
struct led_rgb pixels[RGB_PETRA_NUM_KEYS];
uint8_t reactive[RGB_PETRA_NUM_KEYS];

struct rgb_petra_state state = {
    .on = IS_ENABLED(CONFIG_RGB_PETRA_START_ON),
    .effect = RGB_PETRA_EFF_SOLID,
    .hue = 0,
    .sat = 100,
    .brt = CONFIG_RGB_PETRA_BRT_START,
    .speed = 4,
};

/* Physical column (0-13) and row (0-5) of each LED, from serpentine wiring. */
const uint8_t led_col[RGB_PETRA_NUM_KEYS] = {
    9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10,
    9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 11, 10,
    9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 11, 10,
    9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3,
    6,
};

const uint8_t led_row[RGB_PETRA_NUM_KEYS] = {
    5, 5, 5, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5,
    5,
};

/* ---- Shared helpers ---- */
struct led_rgb rgbp_hsb(uint16_t h, uint8_t s, uint8_t b) {
    uint32_t rp = 0, gp = 0, bp = 0;
    uint8_t sector = (h / 60) % 6;
    uint32_t f = h % 60;
    uint32_t v = b;
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

struct led_rgb rgbp_scale(struct led_rgb c, uint8_t factor) {
    c.r = (uint16_t)c.r * factor / 255;
    c.g = (uint16_t)c.g * factor / 255;
    c.b = (uint16_t)c.b * factor / 255;
    return c;
}

/* ---- Reactive decay ---- */
static void decay_reactive(void) {
    for (int i = 0; i < RGB_PETRA_NUM_KEYS; i++) {
        if (reactive[i] > CONFIG_RGB_PETRA_REACTIVE_DECAY) {
            reactive[i] -= CONFIG_RGB_PETRA_REACTIVE_DECAY;
        } else {
            reactive[i] = 0;
        }
    }
}

/* ---- Caps-lock indicator overlay ---- */
#if IS_ENABLED(CONFIG_RGB_PETRA_CAPS_INDICATOR)
static void caps_overlay(void) {
    zmk_hid_indicators_t ind = zmk_hid_indicators_get_current_profile();
    /* Caps lock = bit 1 (0x02) per USB HID spec. */
    if (!(ind & 0x02)) {
        return;
    }
    /* blink: on for ~half a second, off for ~half a second (at ~30fps) */
    bool blink_on = (state.phase / 15) & 1;
    struct led_rgb c = blink_on ? (struct led_rgb){0, 255, 0}
                                : (struct led_rgb){0, 0, 0};
    if (CONFIG_RGB_PETRA_CAPS_LED < STRIP_NUM_PIXELS) {
        pixels[CONFIG_RGB_PETRA_CAPS_LED] = c;
    }
}
#else
static void caps_overlay(void) {}
#endif

/* ---- Dispatch by enum range ---- */
static void render_current(void) {
    enum rgb_petra_effect e = state.effect;

    if (e >= RGB_PETRA_EFF_FLAG) {
        rgbp_render_backlog(e);
    } else if (e >= RGB_PETRA_EFF_TYPING_HEATMAP) {
        rgbp_render_reactive(e);
    } else if (e >= RGB_PETRA_EFF_RAINDROPS) {
        rgbp_render_animated(e);
    } else {
        rgbp_render_base(e);
    }
}

/* ---- Animation tick ---- */
static void rgb_petra_tick(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(rgb_petra_work, rgb_petra_tick);

static void rgb_petra_tick(struct k_work *work) {
    if (!state.on) {
        return;
    }

    render_current();
    decay_reactive();
    caps_overlay();

    int err = led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
    if (err < 0) {
        LOG_ERR("led_strip_update_rgb failed: %d", err);
    }

    state.phase += state.speed;
    k_work_reschedule(&rgb_petra_work, K_MSEC(CONFIG_RGB_PETRA_TICK_MS));
}

static void clear_strip(void) {
    memset(pixels, 0, sizeof(pixels));
    led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
}

/* ---- ext-power regulator ---- */
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
        state.effect = (state.effect + 1) % RGB_PETRA_EFF_ACTIVE_END;
        break;
    case RGB_PETRA_CMD_EFF_PREV:
        state.effect = (state.effect + RGB_PETRA_EFF_ACTIVE_END - 1)
                       % RGB_PETRA_EFF_ACTIVE_END;
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
    case RGB_PETRA_CMD_SPD_UP:
        if (state.speed < 10) state.speed++;
        break;
    case RGB_PETRA_CMD_SPD_DN:
        if (state.speed > 1) state.speed--;
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
        if (led < RGB_PETRA_NUM_KEYS) {
            reactive[led] = 255;
            rgbp_reactive_note_press(led);
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
    if (!device_is_ready(led_strip)) {
        LOG_ERR("LED strip device NOT ready");
        return -ENODEV;
    }
    if (state.on) {
        start_anim();
    }
    return 0;
}

SYS_INIT(rgb_petra_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
