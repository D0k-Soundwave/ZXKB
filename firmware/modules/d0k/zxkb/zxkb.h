// ZXKB community module: lighting control from the ZXKB Windows app.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define ZXKB_PROTOCOL_VERSION 2 /* 2: status has "VU possible" and is also sent unasked on changes */
#define ZXKB_REPORT_SIZE      32 /* QMK raw HID report size (RAW_EPSIZE) */

/* Raw HID commands (32-byte reports). 0xD0-0xD9 are ZXKB's own; 0x21-0x28 are the SignalRGB protocol. */
#define ZXKB_CMD_VU_LEVELS 0xD0 /* [0xD0, 16 x level 0-255], no reply */
#define ZXKB_CMD_VU_MODE   0xD1 /* [0xD1, 1 = VU on, 0 = VU off], no reply */
#define ZXKB_CMD_FX_SET    0xD2 /* [0xD2, Spectrum effect 1-6] (saved), no reply. Kept for the old ZX VU app */
/* 0xD3-0xD5 are board-specific and handled by zxkb_command_user() (e.g. LED driver current tune) */
#define ZXKB_CMD_SLOT      0xD8 /* keyboard -> app: [0xD8, slot 1-10] when Fn + number is pressed */
#define ZXKB_CMD_INFO      0xD9 /* [0xD9, sub, ...] queries, always replies (see zxkb.c) */
#define ZXKB_INFO_STATUS   0x00
#define ZXKB_INFO_NAME     0x01
#define ZXKB_INFO_LEDS     0x02
#define ZXKB_CMD_TUNE      0xDA /* size and intensity of the current effect, always replies:
                                 * [0xDA, 0x00] get, [0xDA, 0x01, size, intensity] set (live), [0xDA, 0x02] save
                                 * -> [0xDA, sub, mode, flags (1 = size, 2 = intensity apply), size, intensity] */
#define ZXKB_TUNE_GET      0x00
#define ZXKB_TUNE_SET      0x01
#define ZXKB_TUNE_SAVE     0x02
#define ZXKB_TUNE_SIZE      0x01
#define ZXKB_TUNE_INTENSITY 0x02

/* Size and intensity: 128 = QMK's own size, 255 = full intensity. Both kept within 16-255. */
#define ZXKB_TUNE_DEFAULT_SIZE      128
#define ZXKB_TUNE_DEFAULT_INTENSITY 255
#define ZXKB_TUNE_MIN               16
#define ZXKB_TUNABLES               14 /* effects with their own size/intensity (see zxkb.c) */

/* For effects: the current effect's size and intensity */
uint8_t zxkb_size(void);
uint8_t zxkb_intensity(void);

/* "PC lighting" overlays: shown instead of the keyboard's own effect while the PC keeps sending. */
enum zxkb_overlay { ZXKB_OVERLAY_NONE = 0, ZXKB_OVERLAY_VU = 1, ZXKB_OVERLAY_DIRECT = 2 };

/* Seconds without data from the PC before an overlay gives way to the keyboard's own effect. */
#ifndef ZXKB_PC_TIMEOUT_MS
#    define ZXKB_PC_TIMEOUT_MS 5000
#endif

/* The board's firmware version (set it in the keymap's config.h), reported in the status. 0 = not set. */
#ifndef ZXKB_FIRMWARE_VERSION
#    define ZXKB_FIRMWARE_VERSION 0
#endif

/* SignalRGB device identifier (its plugin matches on these). 0,0,0 unless the keymap sets them. */
#ifndef ZXKB_SRGB_ID_1
#    define ZXKB_SRGB_ID_1 0
#endif
#ifndef ZXKB_SRGB_ID_2
#    define ZXKB_SRGB_ID_2 0
#endif
#ifndef ZXKB_SRGB_ID_3
#    define ZXKB_SRGB_ID_3 0
#endif

/* Shared with the effects in rgb_matrix_module.inc */
extern uint8_t  zxkb_vu_levels[16];
extern uint32_t zxkb_vu_last_update;
extern uint8_t  zxkb_direct[][3];
#define ZXKB_RIPPLES 8
extern uint8_t  zxkb_rip_led[ZXKB_RIPPLES];
extern uint8_t  zxkb_rip_col[ZXKB_RIPPLES];
extern uint32_t zxkb_rip_time[ZXKB_RIPPLES];

/* Leave any PC overlay and go back to the keyboard's own effect. */
void zxkb_overlay_clear(void);
uint8_t zxkb_overlay(void);

/* Handle one raw HID report. Returns true if it was a ZXKB/SignalRGB command (reply already sent).
 * Called automatically unless the keyboard defines ZXKB_NO_RAW_HID_HOOK (then call it yourself). */
bool zxkb_raw_hid_receive(uint8_t *data, uint8_t length);

/* Keymap hooks (weak, optional) */
bool zxkb_overlay_allowed_user(uint8_t overlay);           /* default: always allowed */
bool zxkb_command_user(uint8_t *data, uint8_t length);     /* board-specific commands; return true if handled */
