// key_led_map.h
// Mapeo manual: key position (indice del arreglo) -> indice fisico del LED en la cadena WS2812
// Orden de key position = orden de aparicion en flatcat.keymap (fila por fila, izquierda a derecha).

#pragma once
#include <zephyr/kernel.h>

#define RGB_PETRA_NUM_KEYS 79

static const uint8_t key_to_led[RGB_PETRA_NUM_KEYS] = {
    // --- Fila 0: ESC F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12 PSCRN ---
    [ 0] = 22,   // ESC
    [ 1] = 21,   // F1
    [ 2] = 20,   // F2
    [ 3] = 19,   // F3
    [ 4] = 18,   // F4
    [ 5] = 17,   // F5
    [ 6] = 16,   // F6
    [ 7] = 15,   // F7
    [ 8] = 14,   // F8
    [ 9] = 13,   // F9
    [10] = 12,   // F10
    [11] = 11,   // F11
    [12] = 10,   // F12
    [13] = 9,    // PSCRN

    // --- Fila 1: GRAVE N1 N2 N3 N4 N5 N6 N7 N8 N9 N0 MINUS EQUAL DEL ---
    [14] = 23,   // GRAVE
    [15] = 24,   // N1
    [16] = 25,   // N2
    [17] = 26,   // N3
    [18] = 27,   // N4
    [19] = 28,   // N5
    [20] = 29,   // N6
    [21] = 30,   // N7
    [22] = 31,   // N8
    [23] = 32,   // N9
    [24] = 33,   // N0
    [25] = 34,   // MINUS
    [26] = 35,   // EQUAL
    [27] = 8,    // DEL

    // --- Fila 2: TAB Q W E R T Y U I O P LBKT RBKT BKSP ---
    [28] = 48,   // TAB
    [29] = 47,   // Q
    [30] = 46,   // W
    [31] = 45,   // E
    [32] = 44,   // R
    [33] = 43,   // T
    [34] = 42,   // Y
    [35] = 41,   // U
    [36] = 40,   // I
    [37] = 39,   // O
    [38] = 38,   // P
    [39] = 37,   // LBKT
    [40] = 36,   // RBKT
    [41] = 7,    // BKSP

    // --- Fila 3: CLCK A S D F G H J K L SEMI SQT BSLH ENTER ---
    [42] = 49,   // CLCK
    [43] = 50,   // A
    [44] = 51,   // S
    [45] = 52,   // D
    [46] = 53,   // F
    [47] = 54,   // G
    [48] = 55,   // H
    [49] = 56,   // J
    [50] = 57,   // K
    [51] = 58,   // L
    [52] = 59,   // SEMI
    [53] = 60,   // SQT
    [54] = 61,   // BSLH
    [55] = 6,    // ENTER

    // --- Fila 4: LSHFT NUHS Z X C V B N M COMMA DOT FSLH UP RSHFT ---
    [56] = 74,   // LSHFT
    [57] = 73,   // NUHS
    [58] = 72,   // Z
    [59] = 71,   // X
    [60] = 70,   // C
    [61] = 69,   // V
    [62] = 68,   // B
    [63] = 67,   // N
    [64] = 66,   // M
    [65] = 65,   // COMMA
    [66] = 64,   // DOT
    [67] = 63,   // FSLH
    [68] = 62,   // UP
    [69] = 5,    // RSHFT

    // --- Fila 5: LCTRL LGUI LALT SPACE RALT LEFT DOWN RIGHT LOWER ---
    [70] = 75,   // LCTRL
    [71] = 76,   // LGUI
    [72] = 77,   // LALT
    [73] = 78,   // SPACE
    [74] = 0,    // RALT
    [75] = 1,    // LEFT
    [76] = 2,    // DOWN
    [77] = 3,    // RIGHT
    [78] = 4,    // LOWER
};
