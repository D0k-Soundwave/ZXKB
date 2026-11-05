/* D0k's Keychron Q3 ISO knob keymap: KAM Wraith ZX Spectrum caps
 * Layer 0/1 = Spectrum mode (side switch on Mac), layer 2/3 = Windows mode.
 * Lighting effects, the VU meter, PC direct colour, SignalRGB and the Fn + number hotkey slots come from
 * the ZXKB community module (modules/d0k/zxkb). This keymap adds the Q3/Spectrum extras: flashing
 * magenta/white Caps Lock light, automatic Spectrum lighting in Spectrum mode, one-key EXTEND MODE
 * and LOAD "", and the LED driver colour balance (SNLED27351 current tune).
 * Copyright 2026 D0k
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include QMK_KEYBOARD_H
#include "raw_hid.h"
#include "zxkb.h"

enum layers { SPEC_BASE, SPEC_FN, WIN_BASE, WIN_FN };

enum custom_keycodes { ZX_LOAD = QK_USER_0 };

#define ZX_EXT  C(KC_LSFT)   /* Caps Shift + Symbol Shift = EXTENDED MODE on the MiSTer core */

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [SPEC_BASE] = LAYOUT_tkl_f13_iso(
        KC_ESC,   KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,     KC_MUTE,  KC_F11,   KC_F12,   KC_F9,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,     KC_BSPC,  S(KC_1),  S(KC_2),  S(KC_3),
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,              S(KC_4),  S(KC_9),  S(KC_0),
        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,  KC_NUHS,    KC_ENT,
        KC_LSFT,  S(KC_1),  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,              KC_RCTL,            KC_UP,
        KC_LCTL,  ZX_EXT,   KC_LALT,                                KC_SPC,                                 KC_RALT,  ZX_LOAD,  MO(SPEC_FN), KC_RCTL, KC_LEFT,  KC_DOWN,  KC_RGHT),

    [SPEC_FN] = LAYOUT_tkl_f13_iso(
        _______,  KC_BRID,  KC_BRIU,  KC_NO,    KC_NO,    RM_VALD,  RM_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,    RM_TOGG,  _______,  _______,  RM_NEXT,
        _______,  ZK_1,     ZK_2,     ZK_3,     ZK_4,     ZK_5,     ZK_6,     ZK_7,     ZK_8,     ZK_9,     ZK_0,     _______,  _______,    _______,  _______,  _______,  _______,
        RM_TOGG,  RM_NEXT,  RM_VALU,  RM_HUEU,  RM_SATU,  RM_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,  _______,  _______,
        _______,  RM_PREV, RM_VALD,  RM_HUED,  RM_SATD,  RM_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,
        _______,  _______,  _______,  _______,  _______,  ZK_VU,    _______,  NK_TOGG,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  GU_TOGG,  _______,                                _______,                                _______,  GU_TOGG,  _______,    _______,  _______,  _______,  _______),

    [WIN_BASE] = LAYOUT_tkl_f13_iso(
        KC_ESC,   KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,     KC_MUTE,  KC_PSCR,  KC_SCRL,  KC_PAUS,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,     KC_BSPC,  KC_INS,   KC_HOME,  KC_PGUP,
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,              KC_DEL,   KC_END,   KC_PGDN,
        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,  KC_NUHS,    KC_ENT,
        KC_LSFT,  KC_NUBS,  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,              KC_RSFT,            KC_UP,
        KC_LCTL,  KC_LWIN,  KC_LALT,                                KC_SPC,                                 KC_RALT,  KC_RWIN,  MO(WIN_FN), KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT),

    [WIN_FN] = LAYOUT_tkl_f13_iso(
        _______,  KC_BRID,  KC_BRIU,  KC_NO,    KC_NO,    RM_VALD,  RM_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,    RM_TOGG,  _______,  _______,  RM_NEXT,
        _______,  ZK_1,     ZK_2,     ZK_3,     ZK_4,     ZK_5,     ZK_6,     ZK_7,     ZK_8,     ZK_9,     ZK_0,     _______,  _______,    _______,  _______,  _______,  _______,
        RM_TOGG,  RM_NEXT,  RM_VALU,  RM_HUEU,  RM_SATU,  RM_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,  _______,  _______,
        _______,  RM_PREV, RM_VALD,  RM_HUED,  RM_SATD,  RM_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,
        _______,  _______,  _______,  _______,  _______,  ZK_VU,    _______,  NK_TOGG,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  ZK_GAME,  _______,                                _______,                                _______,  ZK_GAME,  _______,    _______,  _______,  _______,  _______),
};

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [SPEC_BASE] = { ENCODER_CCW_CW(KC_LEFT, KC_RGHT) },
    [SPEC_FN]   = { ENCODER_CCW_CW(RM_VALD, RM_VALU) },
    [WIN_BASE]  = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [WIN_FN]    = { ENCODER_CCW_CW(RM_VALD, RM_VALU) },
};
#endif

/* ---------- One-key LOAD "" for the Spectrum (60 ms per key so the ROM's keyboard scan sees it) ---------- */
static void zx_tap(uint16_t kc) {
    register_code16(kc);
    wait_ms(60);
    unregister_code16(kc);
    wait_ms(60);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode == ZX_LOAD) {
        if (record->event.pressed) {
            zx_tap(KC_J);       /* LOAD keyword */
            zx_tap(KC_QUOT);    /* core maps ' to Symbol Shift + P = " */
            zx_tap(KC_QUOT);
            zx_tap(KC_ENT);
        }
        return false;
    }
    return true;
}

/* ---------- Board commands, passed on by the ZXKB module ---------- */
#define HW_CMD_SET    0xD3   /* [0xD3, red, green, blue] LED driver current limit per colour (live, replies) */
#define HW_CMD_SAVE   0xD4   /* [0xD4] save the current limits to the keyboard (replies) */
#define HW_CMD_GET    0xD5   /* [0xD5] -> [0xD5, red, green, blue] */

/* ---------- Hardware colour balance: SNLED27351 current tune per channel ----------
 * Keychron ships red at 0x44 and green/blue at 0x9D, which starves red (no real white or yellow).
 * Tune register i drives CB(i+1); on this board CB1/4/7/10 = green, CB2/5/8/11 = blue, CB3/6/9/12 = red. */
#define HW_TUNE_MIN 0x10
#define HW_TUNE_MAX 0xC0
#define HW_MAGIC    0xA5
static uint8_t hw_tune[3] = { 192, 72, 58 };   /* red, green, blue: D0k's calibration 2026-11-02 (Keychron shipped 68 / 157 / 157) */

static void hw_tune_apply(void) {
    for (uint8_t drv = 0; drv < SNLED27351_DRIVER_COUNT; drv++) {
        snled27351_select_page(drv, SNLED27351_COMMAND_CURRENT_TUNE);
        for (uint8_t i = 0; i < SNLED27351_LED_CURRENT_TUNE_LENGTH; i++) {
            uint8_t ch = i % 3;   /* 0 = green, 1 = blue, 2 = red */
            snled27351_write_register(drv, i, ch == 0 ? hw_tune[1] : ch == 1 ? hw_tune[2] : hw_tune[0]);
        }
    }
}
static void hw_tune_save(void) {
    eeconfig_update_user((uint32_t)HW_MAGIC << 24 | (uint32_t)hw_tune[0] << 16 | (uint32_t)hw_tune[1] << 8 | hw_tune[2]);
}
void keyboard_post_init_user(void) {
    uint32_t v = eeconfig_read_user();
    if ((v >> 24) == HW_MAGIC) { hw_tune[0] = v >> 16; hw_tune[1] = v >> 8; hw_tune[2] = v; }
    hw_tune_apply();
}
static uint8_t clamp_tune(uint8_t v) { return v < HW_TUNE_MIN ? HW_TUNE_MIN : v > HW_TUNE_MAX ? HW_TUNE_MAX : v; }

bool zxkb_command_user(uint8_t *data, uint8_t length) {
    if (data[0] == HW_CMD_SET) {
        for (uint8_t c = 0; c < 3; c++) hw_tune[c] = clamp_tune(data[1 + c]);
        hw_tune_apply();
        data[1] = hw_tune[0]; data[2] = hw_tune[1]; data[3] = hw_tune[2];
        raw_hid_send(data, length);
        return true;
    }
    if (data[0] == HW_CMD_SAVE || data[0] == HW_CMD_GET) {
        if (data[0] == HW_CMD_SAVE) hw_tune_save();
        data[1] = hw_tune[0]; data[2] = hw_tune[1]; data[3] = hw_tune[2];
        raw_hid_send(data, length);
        return true;
    }
    return false;
}

/* PC lighting (VU meter, direct colour) only in Windows mode, as before */
bool zxkb_overlay_allowed_user(uint8_t overlay) {
    return get_highest_layer(default_layer_state) == WIN_BASE;
}

/* ---------- Automatic Spectrum lighting when the side switch selects Spectrum mode ---------- */
static uint8_t last_default = 0xFF;
static uint8_t spec_saved_mode = RGB_MATRIX_SOLID_COLOR;

void housekeeping_task_user(void) {
    uint8_t def = get_highest_layer(default_layer_state);
    if (def != last_default && rgb_matrix_is_enabled()) {
        if (def == SPEC_BASE) {
            zxkb_overlay_clear();
            spec_saved_mode = rgb_matrix_get_mode();
            rgb_matrix_mode_noeeprom(RGB_MATRIX_COMMUNITY_MODULE_SPECTRUM_STRIPES);
        } else if (last_default == SPEC_BASE) {
            rgb_matrix_mode_noeeprom(spec_saved_mode);
        }
        last_default = def;
    }
}

/* ---------- Caps Lock light ----------
 * Drawn by the ZXKB module and set from the app (effect, colours, speed, brightness, keys). On first start it flashes
 * magenta / white at 320 ms, full brightness, on Tab, Q, Caps Lock, A, Caps Shift and \ (D0k 2026-11-05). LED numbers
 * from QMK's Q3 ISO knob layout (keyboard.json rgb_matrix): Tab 33 (2,0), Q 34 (2,1), Caps Lock 49 (3,0), A 50 (3,1),
 * Left Shift / Caps Shift 63 (4,0), ISO \ 64 (4,1). */
void zxkb_caps_keys_user(uint8_t *bits) {
    static const uint8_t leds[] = {33, 34, CAPS_LOCK_LED_INDEX, 50, 63, 64};
    for (uint8_t k = 0; k < sizeof(leds); k++) bits[leds[k] / 8] |= 1 << (leds[k] % 8);
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    return false;   /* skip the board's default white Caps Lock light (the ZXKB module draws the Caps Lock light) */
}

/* ---------- LED mirror for ZXKB's live preview ----------
 * rules.mk wraps the SNLED27351 driver's set-colour calls, so every colour that reaches the LEDs (after effects,
 * brightness and the Caps Lock light) is copied for the ZXKB module, which sends it to the app on request. */
void __real_snled27351_set_color(int index, uint8_t red, uint8_t green, uint8_t blue);
void __real_snled27351_set_color_all(uint8_t red, uint8_t green, uint8_t blue);
void __wrap_snled27351_set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    zxkb_mirror_set(index, red, green, blue);
    __real_snled27351_set_color(index, red, green, blue);
}
void __wrap_snled27351_set_color_all(uint8_t red, uint8_t green, uint8_t blue) {
    for (int i = 0; i < RGB_MATRIX_LED_COUNT; i++) zxkb_mirror_set(i, red, green, blue);
    __real_snled27351_set_color_all(red, green, blue);
}
