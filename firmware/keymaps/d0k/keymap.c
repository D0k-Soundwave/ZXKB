/* D0k's Keychron Q3 ISO knob keymap: KAM Wraith ZX Spectrum caps
 * Layer 0/1 = Spectrum mode (side switch on Mac), layer 2/3 = Windows mode.
 * Extras: flashing magenta/white Caps Lock light, Spectrum effects (stripes, tape loading, VU, border flash,
 *         attribute clash, typing ripple, logo sweep) selectable from the ZX VU tray app,
 * automatic Spectrum lighting in Spectrum mode, one-key EXTEND MODE and LOAD "".
 */
#include QMK_KEYBOARD_H
#include "raw_hid.h"

enum layers { SPEC_BASE, SPEC_FN, WIN_BASE, WIN_FN };

enum custom_keycodes { ZX_LOAD = QK_USER_0, VU_TOGG };

static bool vu_allowed = true;   /* Fn + V turns the VU meter on/off */
static void vu_apply(void);

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
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,
        RM_TOGG,  RM_NEXT,  RM_VALU,  RM_HUEU,  RM_SATU,  RM_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,  _______,  _______,
        _______,  RM_PREV, RM_VALD,  RM_HUED,  RM_SATD,  RM_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,
        _______,  _______,  _______,  _______,  _______,  VU_TOGG,  _______,  NK_TOGG,  _______,  _______,  _______,  _______,              _______,            _______,
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
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,
        RM_TOGG,  RM_NEXT,  RM_VALU,  RM_HUEU,  RM_SATU,  RM_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,  _______,  _______,
        _______,  RM_PREV, RM_VALD,  RM_HUED,  RM_SATD,  RM_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,
        _______,  _______,  _______,  _______,  _______,  VU_TOGG,  _______,  NK_TOGG,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  GU_TOGG,  _______,                                _______,                                _______,  GU_TOGG,  _______,    _______,  _______,  _______,  _______),
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

/* Typing ripple: remember the last 8 key presses, colour cycling red, yellow, green, cyan */
#define ZX_RIPPLES 8
uint8_t  zx_rip_led[ZX_RIPPLES]  = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t  zx_rip_col[ZX_RIPPLES];
uint32_t zx_rip_time[ZX_RIPPLES];
static uint8_t zx_rip_next = 0, zx_rip_colour = 0;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed && record->event.type == KEY_EVENT) {
        uint8_t led = g_led_config.matrix_co[record->event.key.row][record->event.key.col];
        if (led != NO_LED) {
            zx_rip_led[zx_rip_next]  = led;
            zx_rip_col[zx_rip_next]  = zx_rip_colour;
            zx_rip_time[zx_rip_next] = g_rgb_timer;
            zx_rip_next   = (zx_rip_next + 1) % ZX_RIPPLES;
            zx_rip_colour = (zx_rip_colour + 1) % 4;
        }
    }
    if (keycode == VU_TOGG) {
        if (record->event.pressed) { vu_allowed = !vu_allowed; vu_apply(); }
        return false;
    }
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

/* ---------- Audio VU meter data from the PC helper (raw HID command 0xD0) ---------- */
uint8_t  vu_levels[16];
uint32_t vu_last_update = 0;
static bool     vu_active     = false;
static bool     vu_requested  = false;   /* the PC helper wants VU mode */
static uint8_t  vu_saved_mode = RGB_MATRIX_SOLID_COLOR;

#define VU_CMD_LEVELS 0xD0   /* [0xD0, 16 x level 0-255] */
#define VU_CMD_MODE   0xD1   /* [0xD1, 1 = VU on, 0 = VU off] */
#define FX_CMD_SET    0xD2   /* [0xD2, effect 1-6] pick a Spectrum lighting effect (saved) */
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

static const uint8_t fx_modes[] = {
    RGB_MATRIX_CUSTOM_SPECTRUM_STRIPES, RGB_MATRIX_CUSTOM_TAPE_LOADING, RGB_MATRIX_CUSTOM_BORDER_FLASH,
    RGB_MATRIX_CUSTOM_ATTR_CLASH, RGB_MATRIX_CUSTOM_TYPING_RIPPLE, RGB_MATRIX_CUSTOM_LOGO_SWEEP,
};

bool via_command_kb(uint8_t *data, uint8_t length) {
    if (data[0] == VU_CMD_LEVELS) {
        for (uint8_t b = 0; b < 16; b++) vu_levels[b] = data[1 + b];
        vu_last_update = timer_read32();
        return true;   /* no reply needed */
    }
    if (data[0] == VU_CMD_MODE) {
        vu_requested = data[1] != 0;
        vu_last_update = timer_read32();
        vu_apply();
        return true;
    }
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
    if (data[0] == FX_CMD_SET) {
        uint8_t n = data[1];
        if (n >= 1 && n <= sizeof(fx_modes)) {
            vu_requested = false;          /* picking an effect switches the VU meter off */
            vu_active    = false;
            if (!rgb_matrix_is_enabled()) rgb_matrix_enable();
            rgb_matrix_mode(fx_modes[n - 1]);
        }
        return true;
    }
    return false;
}

static void vu_apply(void) {
    bool want = vu_allowed && vu_requested && get_highest_layer(default_layer_state) == WIN_BASE
                && timer_elapsed32(vu_last_update) < 5000;
    if (want && !vu_active) {
        vu_saved_mode = rgb_matrix_get_mode();
        rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_VU_METER);
        vu_active = true;
    } else if (!want && vu_active) {
        rgb_matrix_mode_noeeprom(vu_saved_mode);
        vu_active = false;
    }
}

/* ---------- Automatic Spectrum lighting when the side switch selects Spectrum mode ---------- */
static uint8_t last_default = 0xFF;
static uint8_t spec_saved_mode = RGB_MATRIX_SOLID_COLOR;

void housekeeping_task_user(void) {
    uint8_t def = get_highest_layer(default_layer_state);
    if (def != last_default && rgb_matrix_is_enabled()) {
        if (def == SPEC_BASE) {
            if (vu_active) { rgb_matrix_mode_noeeprom(vu_saved_mode); vu_active = false; }
            spec_saved_mode = rgb_matrix_get_mode();
            rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_SPECTRUM_STRIPES);
        } else if (last_default == SPEC_BASE) {
            rgb_matrix_mode_noeeprom(spec_saved_mode);
        }
        last_default = def;
    }
    /* drop out of VU mode if the PC helper stops sending for 5 s */
    if (vu_active && timer_elapsed32(vu_last_update) > 5000) vu_apply();
}

/* ---------- Caps Lock light: flashing magenta/white (normal effect when off) ---------- */
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (host_keyboard_led_state().caps_lock) {
        /* FLASH attribute: swap magenta and white every 320 ms, like the Spectrum's own FLASH */
        if ((g_rgb_timer / 320) & 1) RGB_MATRIX_INDICATOR_SET_COLOR(CAPS_LOCK_LED_INDEX, 195, 195, 195);
        else                         RGB_MATRIX_INDICATOR_SET_COLOR(CAPS_LOCK_LED_INDEX, 200, 0, 200);
    }
    return false;   /* skip the board's default white Caps Lock light */
}
