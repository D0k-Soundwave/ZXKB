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

static const bool vu_allowed = true; /* kept for the status report; ZK_VU now switches to/from the VU effect */
static bool     vu_requested;      /* the app wants the VU meter */
static bool     direct_requested;  /* the app or SignalRGB wants direct colour */
static uint32_t direct_last;
static uint8_t  overlay    = ZXKB_OVERLAY_NONE;
static uint8_t  mirror[RGB_MATRIX_LED_COUNT][3]; /* the colours last sent to the LEDs */
static bool     mirroring;
static uint32_t mirror_until, mirror_last;
static uint8_t  mirror_next;

void zxkb_mirror_set(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index >= RGB_MATRIX_LED_COUNT) return;
    mirror[index][0] = r;
    mirror[index][1] = g;
    mirror[index][2] = b;
}
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

/* Effects that only make sense while the PC drives them, so never the start-up effect. The VU meter is a normal
 * effect (picked and saved like any other); it shows dark bars while nothing sends it levels. */
static bool is_overlay_mode(uint8_t mode) {
    return mode == RGB_MATRIX_COMMUNITY_MODULE_PC_DIRECT;
}

/* ---------- Saved settings (module EEPROM datablock) ---------- */
typedef struct PACKED {
    uint8_t home_mode;                    /* the keyboard's own effect, kept while an overlay shows */
    uint8_t size[ZXKB_TUNABLES];          /* per tunable effect, see tunables[] */
    uint8_t intensity[ZXKB_TUNABLES];
} zxkb_config_t;
_Static_assert(sizeof(zxkb_config_t) == EECONFIG_MODULE_ZXKB_DATA_SIZE, "update EECONFIG_MODULE_ZXKB_DATA_SIZE in config.h");
static zxkb_config_t cfg;

/* Effects with their own size and intensity, and which of the two apply */
#define SZ ZXKB_TUNE_SIZE
#define IN ZXKB_TUNE_INTENSITY
static const struct { uint8_t mode, flags; } tunables[] = {
    {RGB_MATRIX_COMMUNITY_MODULE_TYPING_RIPPLE, SZ | IN},
#ifdef RGB_MATRIX_KEYREACTIVE_ENABLED
    {RGB_MATRIX_COMMUNITY_MODULE_KEY_EXPLOSION, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_REACTIVE_SIMPLE, IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_REACTIVE, IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_REACTIVE_WIDE, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_REACTIVE_MULTIWIDE, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_REACTIVE_CROSS, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_REACTIVE_MULTICROSS, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_REACTIVE_NEXUS, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_REACTIVE_MULTINEXUS, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_SPLASH, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_MULTISPLASH, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_SOLID_SPLASH, SZ | IN},
    {RGB_MATRIX_COMMUNITY_MODULE_ZX_SOLID_MULTISPLASH, SZ | IN},
#endif
};
#undef SZ
#undef IN
_Static_assert(sizeof(tunables) / sizeof(tunables[0]) <= ZXKB_TUNABLES, "raise ZXKB_TUNABLES and the EEPROM size");

void eeconfig_init_zxkb_datablock(void) { /* first start, or the saved layout changed: defaults */
    zxkb_config_t def;
    def.home_mode = RGB_MATRIX_SOLID_COLOR;
    for (uint8_t i = 0; i < ZXKB_TUNABLES; i++) {
        def.size[i]      = ZXKB_TUNE_DEFAULT_SIZE;
        def.intensity[i] = ZXKB_TUNE_DEFAULT_INTENSITY;
    }
    eeconfig_update_zxkb_datablock(&def, 0, sizeof(def));
}

static int8_t tunable_index(uint8_t mode) {
    for (uint8_t i = 0; i < sizeof(tunables) / sizeof(tunables[0]); i++)
        if (tunables[i].mode == mode) return i;
    return -1;
}
static uint8_t own_mode(void) { /* the effect the keyboard shows when no overlay is on */
    return overlay != ZXKB_OVERLAY_NONE ? saved_mode : rgb_matrix_get_mode();
}
uint8_t zxkb_size(void) {
    int8_t t = tunable_index(rgb_matrix_get_mode());
    return t < 0 ? ZXKB_TUNE_DEFAULT_SIZE : cfg.size[t];
}
uint8_t zxkb_intensity(void) {
    int8_t t = tunable_index(rgb_matrix_get_mode());
    return t < 0 ? ZXKB_TUNE_DEFAULT_INTENSITY : cfg.intensity[t];
}

static void overlay_set(uint8_t o) {
    if (o == overlay) return;
    if (overlay == ZXKB_OVERLAY_NONE) {
        saved_mode = rgb_matrix_get_mode();
        /* QMK saves the whole lighting state (current effect included) whenever any setting changes, so a
         * brightness change during an overlay would make it the start-up effect. Remember the real one. */
        if (cfg.home_mode != saved_mode) {
            cfg.home_mode = saved_mode;
            eeconfig_update_zxkb_datablock(&cfg.home_mode, 0, 1);
        }
    }
    rgb_matrix_mode_noeeprom(o == ZXKB_OVERLAY_NONE ? saved_mode : overlay_mode(o));
    overlay = o;
}

void keyboard_post_init_zxkb(void) {
    keyboard_post_init_zxkb_kb();
    eeconfig_read_zxkb_datablock(&cfg, 0, sizeof(cfg));
    for (uint8_t i = 0; i < ZXKB_TUNABLES; i++) {
        if (cfg.size[i] < ZXKB_TUNE_MIN) cfg.size[i] = ZXKB_TUNE_DEFAULT_SIZE;
        if (cfg.intensity[i] < ZXKB_TUNE_MIN) cfg.intensity[i] = ZXKB_TUNE_DEFAULT_INTENSITY;
    }
    if (is_overlay_mode(rgb_matrix_get_mode())) {
        uint8_t m = cfg.home_mode;
        rgb_matrix_mode(m >= 1 && m < RGB_MATRIX_EFFECT_MAX && !is_overlay_mode(m) ? m : RGB_MATRIX_SOLID_COLOR);
    }
}

/* [0xDA, sub, ...] -> [0xDA, sub, mode, flags, size, intensity] for the keyboard's own effect */
static void tune_command(uint8_t *data, uint8_t length) {
    uint8_t mode = own_mode();
    int8_t  t    = tunable_index(mode);
    if (t >= 0 && data[1] == ZXKB_TUNE_SET) {
        cfg.size[t]      = data[2] < ZXKB_TUNE_MIN ? ZXKB_TUNE_MIN : data[2];
        cfg.intensity[t] = data[3] < ZXKB_TUNE_MIN ? ZXKB_TUNE_MIN : data[3];
    } else if (data[1] == ZXKB_TUNE_SAVE) {
        eeconfig_update_zxkb_datablock(&cfg, 0, sizeof(cfg)); /* only changed bytes are written */
    }
    memset(&data[2], 0, length - 2);
    data[2] = mode;
    data[3] = t < 0 ? 0 : tunables[t].flags;
    data[4] = t < 0 ? ZXKB_TUNE_DEFAULT_SIZE : cfg.size[t];
    data[5] = t < 0 ? ZXKB_TUNE_DEFAULT_INTENSITY : cfg.intensity[t];
    raw_hid_send(data, length);
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

/* Could the VU meter show right now? (lighting on, and the keymap allows it, e.g. Windows mode) */
static bool vu_possible(void) {
    return rgb_matrix_is_enabled() && zxkb_overlay_allowed_user(ZXKB_OVERLAY_VU);
}
static void info_status(uint8_t *data, uint8_t length);

void housekeeping_task_zxkb(void) {
    housekeeping_task_zxkb_kb();
    if (overlay != ZXKB_OVERLAY_NONE && rgb_matrix_get_mode() != overlay_mode(overlay)) {
        /* the effect was changed on the keyboard: it wins until the app asks again */
        overlay          = ZXKB_OVERLAY_NONE;
        vu_requested     = false;
        direct_requested = false;
    }
    if (overlay != ZXKB_OVERLAY_NONE || vu_requested || direct_requested) overlay_update();

    /* LED mirror for the preview: one packet at a time, so the keyboard never stalls for a whole frame */
    if (mirroring) {
        if (timer_expired32(timer_read32(), mirror_until)) {
            mirroring = false;
        } else if (timer_elapsed32(mirror_last) >= ZXKB_MIRROR_PACKET_MS) {
            uint8_t msg[ZXKB_REPORT_SIZE] = {ZXKB_CMD_MIRROR, mirror_next};
            uint8_t count = RGB_MATRIX_LED_COUNT - mirror_next < 9 ? RGB_MATRIX_LED_COUNT - mirror_next : 9;
            msg[2] = count;
            memcpy(&msg[3], mirror[mirror_next], count * 3);
            raw_hid_send(msg, sizeof(msg));
            mirror_next = mirror_next + count >= RGB_MATRIX_LED_COUNT ? 0 : mirror_next + count;
            mirror_last = timer_read32();
        }
    }

    /* Tell the app at once when something it cares about changes (lighting on/off, effect, overlay, whether the
     * VU meter could show), so it can stop listening to audio while nothing would show it. */
    static uint8_t last[5];
    static bool    sent = false;
    uint8_t        now[5] = {rgb_matrix_is_enabled(), own_mode(), overlay, vu_requested, vu_possible()};
    if (!sent || memcmp(now, last, sizeof(now)) != 0) {
        memcpy(last, now, sizeof(now));
        sent = true;
        uint8_t msg[ZXKB_REPORT_SIZE] = {ZXKB_CMD_INFO, ZXKB_INFO_STATUS};
        info_status(msg, sizeof(msg));
        raw_hid_send(msg, sizeof(msg));
    }
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
    if (keycode == ZXKB_VU_TOGGLE) { /* switch between the VU meter effect and the effect before it */
        if (record->event.pressed) {
            uint8_t mode = own_mode();
            if (mode == RGB_MATRIX_COMMUNITY_MODULE_VU_METER) {
                uint8_t m = cfg.home_mode;
                rgb_matrix_mode(m >= 1 && m < RGB_MATRIX_EFFECT_MAX && !is_overlay_mode(m) && m != mode ? m : RGB_MATRIX_SOLID_COLOR);
            } else {
                if (cfg.home_mode != mode) {
                    cfg.home_mode = mode;
                    eeconfig_update_zxkb_datablock(&cfg.home_mode, 0, 1);
                }
                overlay = ZXKB_OVERLAY_NONE;
                vu_requested = direct_requested = false;
                rgb_matrix_mode(RGB_MATRIX_COMMUNITY_MODULE_VU_METER);
            }
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
 *                  max brightness, overlay, VU allowed, VU requested, VU possible, firmware version]. Also sent unasked when
 *                  lighting, effect, overlay or VU state changes. */
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
    data[15] = vu_possible();
    data[16] = ZXKB_FIRMWARE_VERSION;
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
        case ZXKB_CMD_TUNE:
            tune_command(data, length);
            return true;
        case ZXKB_CMD_MIRROR:
            mirroring = data[1] != 0;
            mirror_until = timer_read32() + ZXKB_MIRROR_KEEPALIVE_MS;
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
