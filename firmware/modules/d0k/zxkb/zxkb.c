// ZXKB community module: lighting control from the ZXKB Windows app.
//   - PC lighting overlays: audio VU meter, and direct per-LED colour streaming
//   - SignalRGB protocol (0x21-0x28, protocol 1.0.6) so SignalRGB can drive the board;
//     compatible with SRGBmods' QMK_Community_Module (GPL-2.0-or-later), written afresh here
//   - Fn + number hotkey slots reported to the app
//   - the board reports its effect names and LED positions, so the app works on any board
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include QMK_KEYBOARD_H
#include <string.h>
#include "raw_hid.h"
#include "version.h"
#include "zxkb.h"

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 3);

#ifndef RGB_MATRIX_ENABLE
#    error "ZXKB needs RGB_MATRIX_ENABLE"
#endif

uint8_t  zxkb_vu_levels[16];
uint32_t zxkb_vu_last_update;
uint8_t  zxkb_direct[RGB_MATRIX_LED_COUNT][3];
uint8_t  zxkb_rip_led[ZXKB_RIPPLES] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t  zxkb_rip_col[ZXKB_RIPPLES];
uint32_t zxkb_rip_time[ZXKB_RIPPLES];
static uint8_t zxkb_rip_next, zxkb_rip_colour;

static bool     vu_allowed = true; /* ZK_VU (Fn + V) turns the VU meter on/off on the keyboard */
static bool     vu_requested;      /* the app wants the VU meter */
static bool     direct_requested;  /* the app or SignalRGB wants direct colour */
static uint32_t direct_last;
static uint8_t  overlay    = ZXKB_OVERLAY_NONE;
static uint8_t  saved_mode = RGB_MATRIX_SOLID_COLOR;

__attribute__((weak)) bool zxkb_overlay_allowed_user(uint8_t o) {
    return true;
}
__attribute__((weak)) bool zxkb_command_user(uint8_t *data, uint8_t length) {
    return false;
}

/* ---------- PC overlays ---------- */
static uint8_t overlay_mode(uint8_t o) {
    return o == ZXKB_OVERLAY_VU ? RGB_MATRIX_COMMUNITY_MODULE_VU_METER : RGB_MATRIX_COMMUNITY_MODULE_PC_DIRECT;
}

static bool is_overlay_mode(uint8_t mode) {
    return mode == RGB_MATRIX_COMMUNITY_MODULE_VU_METER || mode == RGB_MATRIX_COMMUNITY_MODULE_PC_DIRECT;
}

static void overlay_set(uint8_t o) {
    if (o == overlay) return;
    if (overlay == ZXKB_OVERLAY_NONE) {
        saved_mode = rgb_matrix_get_mode();
        /* QMK saves the whole lighting state (current effect included) whenever any setting changes, so a
         * brightness change during an overlay would make it the start-up effect. Remember the real one. */
        uint8_t stored = 0;
        eeconfig_read_zxkb_datablock(&stored, 0, 1);
        if (stored != saved_mode) eeconfig_update_zxkb_datablock(&saved_mode, 0, 1);
    }
    rgb_matrix_mode_noeeprom(o == ZXKB_OVERLAY_NONE ? saved_mode : overlay_mode(o));
    overlay = o;
}

void keyboard_post_init_zxkb(void) {
    keyboard_post_init_zxkb_kb();
    if (is_overlay_mode(rgb_matrix_get_mode())) {
        uint8_t stored = 0;
        if (eeconfig_is_zxkb_datablock_valid()) eeconfig_read_zxkb_datablock(&stored, 0, 1);
        rgb_matrix_mode(stored >= 1 && stored < RGB_MATRIX_EFFECT_MAX && !is_overlay_mode(stored) ? stored : RGB_MATRIX_SOLID_COLOR);
    }
}

void zxkb_overlay_clear(void) {
    overlay_set(ZXKB_OVERLAY_NONE);
}
uint8_t zxkb_overlay(void) {
    return overlay;
}

/* Pick the overlay that should show now. Direct colour wins over the VU meter. */
static void overlay_update(void) {
    uint8_t want = ZXKB_OVERLAY_NONE;
    if (direct_requested && timer_elapsed32(direct_last) < ZXKB_PC_TIMEOUT_MS && zxkb_overlay_allowed_user(ZXKB_OVERLAY_DIRECT)) {
        want = ZXKB_OVERLAY_DIRECT;
    } else if (vu_allowed && vu_requested && timer_elapsed32(zxkb_vu_last_update) < ZXKB_PC_TIMEOUT_MS && zxkb_overlay_allowed_user(ZXKB_OVERLAY_VU)) {
        want = ZXKB_OVERLAY_VU;
    }
    overlay_set(want);
}

void housekeeping_task_zxkb(void) {
    housekeeping_task_zxkb_kb();
    if (overlay != ZXKB_OVERLAY_NONE && rgb_matrix_get_mode() != overlay_mode(overlay)) {
        /* the effect was changed on the keyboard: it wins until the app asks again */
        overlay          = ZXKB_OVERLAY_NONE;
        vu_requested     = false;
        direct_requested = false;
    }
    if (overlay != ZXKB_OVERLAY_NONE || vu_requested || direct_requested) overlay_update();
}

/* ---------- Keys: hotkey slots, VU toggle, typing ripple history ---------- */
bool process_record_zxkb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_zxkb_kb(keycode, record)) return false;

    if (record->event.pressed && record->event.type == KEY_EVENT) {
        uint8_t led = g_led_config.matrix_co[record->event.key.row][record->event.key.col];
        if (led != NO_LED) {
            zxkb_rip_led[zxkb_rip_next]  = led;
            zxkb_rip_col[zxkb_rip_next]  = zxkb_rip_colour;
            zxkb_rip_time[zxkb_rip_next] = g_rgb_timer;
            zxkb_rip_next                = (zxkb_rip_next + 1) % ZXKB_RIPPLES;
            zxkb_rip_colour              = (zxkb_rip_colour + 1) % 4;
        }
    }
    if (keycode >= ZXKB_SLOT_1 && keycode <= ZXKB_SLOT_10) {
        if (record->event.pressed) {
            uint8_t msg[ZXKB_REPORT_SIZE] = {ZXKB_CMD_SLOT, (uint8_t)(keycode - ZXKB_SLOT_1 + 1)};
            raw_hid_send(msg, sizeof(msg));
        }
        return false;
    }
    if (keycode == ZXKB_VU_TOGGLE) {
        if (record->event.pressed) {
            vu_allowed = !vu_allowed;
            overlay_update();
        }
        return false;
    }
    return true;
}

/* ---------- Effect names, generated from the same lists QMK builds its effect numbers from ---------- */
#define RGB_MATRIX_EFFECT(name, ...) #name "\0"
static const char names_core[] =
#include "rgb_matrix_effects.inc"
    "";
#ifdef COMMUNITY_MODULES_ENABLE
static const char names_module[] =
#    include "rgb_matrix_community_modules.inc"
    "";
#endif
#if defined(RGB_MATRIX_CUSTOM_KB) || defined(RGB_MATRIX_CUSTOM_USER)
static const char names_custom[] =
#    ifdef RGB_MATRIX_CUSTOM_KB
#        include "rgb_matrix_kb.inc"
#    endif
#    ifdef RGB_MATRIX_CUSTOM_USER
#        include "rgb_matrix_user.inc"
#    endif
    "";
#endif
#undef RGB_MATRIX_EFFECT

/* Find name number n (0-based) in a list of NUL-separated names; *n is reduced by the names skipped. */
static const char *find_name(const char *list, size_t size, uint8_t *n) {
    const char *p = list, *end = list + size - 1; /* last byte is the string's own NUL */
    while (p < end) {
        if (*n == 0) return p;
        p += strlen(p) + 1;
        (*n)--;
    }
    return NULL;
}

/* [0xD9, 0x01, mode] -> [0xD9, 0x01, mode, kind (0 QMK, 1 module, 2 board/keymap), name..., 0] */
static void info_name(uint8_t *data, uint8_t length) {
    uint8_t     n    = data[2] - 1; /* mode 1 is the first effect */
    const char *name = NULL;
    uint8_t     kind = 0;
    if (data[2] >= 1 && data[2] < RGB_MATRIX_EFFECT_MAX) {
        name = find_name(names_core, sizeof(names_core), &n);
#ifdef COMMUNITY_MODULES_ENABLE
        if (!name) { kind = 1; name = find_name(names_module, sizeof(names_module), &n); }
#endif
#if defined(RGB_MATRIX_CUSTOM_KB) || defined(RGB_MATRIX_CUSTOM_USER)
        if (!name) { kind = 2; name = find_name(names_custom, sizeof(names_custom), &n); }
#endif
    }
    memset(&data[3], 0, length - 3);
    if (name) {
        data[3] = kind;
        strncpy((char *)&data[4], name, length - 5);
    } else {
        data[3] = 0xFF;
    }
}

/* [0xD9, 0x02, first] -> [0xD9, 0x02, first, count, then count x (x, y, flags)], up to 9 LEDs */
static void info_leds(uint8_t *data, uint8_t length) {
    uint8_t first = data[2], count = 0;
    memset(&data[3], 0, length - 3);
    while (count < 9 && first + count < RGB_MATRIX_LED_COUNT) {
        uint8_t i         = first + count;
        data[4 + count * 3] = g_led_config.point[i].x;
        data[5 + count * 3] = g_led_config.point[i].y;
        data[6 + count * 3] = g_led_config.flags[i];
        count++;
    }
    data[3] = count;
}

/* [0xD9, 0x00] -> [0xD9, 0x00, protocol, LED count, effect count, mode, enabled, brightness, speed, hue, sat,
 *                  max brightness, overlay, VU allowed, VU requested] */
static void info_status(uint8_t *data, uint8_t length) {
    memset(&data[2], 0, length - 2);
    data[2]  = ZXKB_PROTOCOL_VERSION;
    data[3]  = RGB_MATRIX_LED_COUNT;
    data[4]  = RGB_MATRIX_EFFECT_MAX - 1;
    data[5]  = overlay != ZXKB_OVERLAY_NONE ? saved_mode : rgb_matrix_get_mode();
    data[6]  = rgb_matrix_is_enabled();
    data[7]  = rgb_matrix_get_val();
    data[8]  = rgb_matrix_get_speed();
    data[9]  = rgb_matrix_get_hue();
    data[10] = rgb_matrix_get_sat();
    data[11] = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
    data[12] = overlay;
    data[13] = vu_allowed;
    data[14] = vu_requested;
}

/* ---------- SignalRGB protocol ---------- */
enum {
    SRGB_GET_QMK_VERSION      = 0x21,
    SRGB_GET_PROTOCOL_VERSION = 0x22,
    SRGB_GET_UNIQUE_ID        = 0x23,
    SRGB_STREAM_RGB_DATA      = 0x24,
    SRGB_MODE_ENABLE          = 0x25,
    SRGB_MODE_DISABLE         = 0x26,
    SRGB_GET_TOTAL_LEDS       = 0x27,
    SRGB_GET_FIRMWARE_TYPE    = 0x28,
};
#define SRGB_ERROR_LED_BOUNDS 253
#define SRGB_ERROR_LED_COUNT  254
#define BCD2(b) ((((b) >> 4) & 0x0F) * 10 + ((b) & 0x0F))

static void reply3(uint8_t *data, uint8_t length, uint8_t a, uint8_t b, uint8_t c) {
    memset(&data[1], 0, length - 1);
    data[1] = a;
    data[2] = b;
    data[3] = c;
    raw_hid_send(data, length);
}

static bool signalrgb_command(uint8_t *data, uint8_t length) {
    switch (data[0]) {
        case SRGB_GET_QMK_VERSION:
            reply3(data, length, BCD2(QMK_VERSION_BCD >> 24), BCD2(QMK_VERSION_BCD >> 16), BCD2(QMK_VERSION_BCD));
            return true;
        case SRGB_GET_PROTOCOL_VERSION:
            reply3(data, length, 1, 0, 6);
            return true;
        case SRGB_GET_UNIQUE_ID:
            reply3(data, length, ZXKB_SRGB_ID_1, ZXKB_SRGB_ID_2, ZXKB_SRGB_ID_3);
            return true;
        case SRGB_GET_TOTAL_LEDS:
            reply3(data, length, RGB_MATRIX_LED_COUNT, 0, 0);
            return true;
        case SRGB_GET_FIRMWARE_TYPE:
            reply3(data, length, 2, 0, 0); /* 2 = QMK mainline */
            return true;
        case SRGB_MODE_ENABLE:
            direct_requested = true;
            direct_last      = timer_read32();
            overlay_update();
            return true;
        case SRGB_MODE_DISABLE:
            direct_requested = false;
            overlay_update();
            return true;
        case SRGB_STREAM_RGB_DATA: { /* [0x24, first LED, count (max 9), count x (r, g, b)] */
            uint8_t first = data[1], count = data[2];
            if (first + count > RGB_MATRIX_LED_COUNT) {
                reply3(data, length, SRGB_ERROR_LED_BOUNDS, 0, 0);
            } else if (count >= 10) {
                reply3(data, length, SRGB_ERROR_LED_COUNT, 0, 0);
            } else {
                memcpy(zxkb_direct[first], &data[3], count * 3);
                direct_last = timer_read32();
            }
            return true;
        }
    }
    return false;
}

/* ---------- Spectrum effect picker kept for the old ZX VU app ---------- */
static const uint8_t fx_modes[] = {
    RGB_MATRIX_COMMUNITY_MODULE_SPECTRUM_STRIPES, RGB_MATRIX_COMMUNITY_MODULE_TAPE_LOADING, RGB_MATRIX_COMMUNITY_MODULE_BORDER_FLASH,
    RGB_MATRIX_COMMUNITY_MODULE_ATTR_CLASH,       RGB_MATRIX_COMMUNITY_MODULE_TYPING_RIPPLE, RGB_MATRIX_COMMUNITY_MODULE_LOGO_SWEEP,
};

/* ---------- Raw HID entry point ---------- */
bool zxkb_raw_hid_receive(uint8_t *data, uint8_t length) {
    switch (data[0]) {
        case ZXKB_CMD_VU_LEVELS:
            memcpy(zxkb_vu_levels, &data[1], 16);
            zxkb_vu_last_update = timer_read32();
            return true;
        case ZXKB_CMD_VU_MODE:
            vu_requested        = data[1] != 0;
            zxkb_vu_last_update = timer_read32();
            overlay_update();
            return true;
        case ZXKB_CMD_FX_SET: {
            uint8_t n = data[1];
            if (n >= 1 && n <= sizeof(fx_modes)) {
                vu_requested     = false; /* picking an effect switches PC lighting off */
                direct_requested = false;
                overlay          = ZXKB_OVERLAY_NONE;
                if (!rgb_matrix_is_enabled()) rgb_matrix_enable();
                rgb_matrix_mode(fx_modes[n - 1]);
            }
            return true;
        }
        case ZXKB_CMD_INFO:
            if (data[1] == ZXKB_INFO_STATUS) info_status(data, length);
            else if (data[1] == ZXKB_INFO_NAME) info_name(data, length);
            else if (data[1] == ZXKB_INFO_LEDS) info_leds(data, length);
            else memset(&data[2], 0xFF, length - 2);
            raw_hid_send(data, length);
            return true;
    }
    if (signalrgb_command(data, length)) return true;
    return zxkb_command_user(data, length);
}

#ifndef ZXKB_NO_RAW_HID_HOOK
#    ifdef VIA_ENABLE
bool via_command_kb(uint8_t *data, uint8_t length) {
    return zxkb_raw_hid_receive(data, length);
}
#    else
void raw_hid_receive(uint8_t *data, uint8_t length) {
    zxkb_raw_hid_receive(data, length);
}
#    endif
#endif
