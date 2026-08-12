/*
 * Tanda 2 - Animated (RNG / stateful) effects. STUB.
 * Implemented in a later batch. For now each falls back to a plain fill so the
 * enum is satisfied and the firmware builds/links.
 */

#include <zmk_vfx_petra/effects.h>

/* xorshift8 PRNG, shared via effects.h. */
static uint8_t rng_state = 0xA3;
uint8_t rgbp_rand8(void) {
    rng_state ^= rng_state << 1;
    rng_state ^= rng_state >> 1;
    rng_state ^= rng_state << 2;
    return rng_state;
}

void rgbp_render_animated(enum rgb_petra_effect eff) {
    ARG_UNUSED(eff);
    /* placeholder: solid current color until implemented */
    struct led_rgb c = rgbp_hsb(state.hue, state.sat, state.brt);
    for (int i = 0; i < RGB_PETRA_NUM_KEYS; i++) {
        pixels[i] = c;
    }
}
