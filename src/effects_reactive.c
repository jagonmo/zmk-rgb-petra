/*
 * Tanda 3 - Reactive (key-triggered) effects. STUB.
 * Implemented in a later batch. Uses the shared reactive[] intensity buffer
 * for a basic flash so the enum is satisfied and the firmware builds/links.
 */

#include <zmk_vfx_petra/effects.h>

void rgbp_render_reactive(enum rgb_petra_effect eff) {
    ARG_UNUSED(eff);
    /* placeholder: dim base + reactive flash on pressed keys */
    struct led_rgb base = rgbp_hsb(state.hue, state.sat, state.brt / 6);
    struct led_rgb hot = rgbp_hsb(state.hue, state.sat, state.brt);
    for (int i = 0; i < RGB_PETRA_NUM_KEYS; i++) {
        pixels[i] = reactive[i] ? rgbp_scale(hot, reactive[i]) : base;
    }
}
