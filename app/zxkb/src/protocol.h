// ZXKB <-> keyboard raw HID protocol (see firmware/modules/d0k/zxkb/zxkb.h)
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace proto {
constexpr uint16_t USAGE_PAGE = 0xFF60; // QMK raw HID
constexpr uint16_t USAGE      = 0x61;
constexpr int      REPORT     = 32;     // payload bytes per report

// ZXKB commands
constexpr uint8_t VU_LEVELS = 0xD0; // [0xD0, 16 x level]
constexpr uint8_t VU_MODE   = 0xD1; // [0xD1, 0/1]
constexpr uint8_t SLOT      = 0xD8; // keyboard -> app: [0xD8, slot 1-10]
constexpr uint8_t INFO      = 0xD9; // [0xD9, sub, ...]
constexpr uint8_t INFO_STATUS = 0x00;
constexpr uint8_t INFO_NAME   = 0x01;
constexpr uint8_t INFO_LEDS   = 0x02;
constexpr uint8_t INFO_MATRIX = 0x03; // [0xD9, 0x03, first] -> [.., rows, cols, first, n, n x LED (255 none)] (protocol 5)
constexpr uint8_t MIRROR    = 0xDB; // [0xDB, 1] send LED colours for 2 s, [0xDB, 0] stop; replies [0xDB, first, n, n x rgb]
constexpr uint8_t BOOTLOADER = 0xDC; // [0xDC, 'Z', 'X', 'B', 'L'] -> [0xDC, 1], then the keyboard restarts into its bootloader
constexpr uint8_t BOOT_NONE      = 0; // status byte 17 (protocol 4): bootloader ZXKB can write firmware with
constexpr uint8_t BOOT_STM32_DFU = 1; // STM32 ROM DFU, USB 0483:DF11
constexpr uint8_t KEYS      = 0xDD; // per-key colours and effects (protocol 7): sub 0 get, 1 set, 2 save, 3 clear
constexpr uint8_t KEYS_GET = 0x00, KEYS_SET = 0x01, KEYS_SAVE = 0x02, KEYS_CLEAR = 0x03;
constexpr int     KEY_BYTES       = 8; // effect, speed 0-255, first r g b, second r g b (protocol 8)
constexpr int     KEYS_PER_PACKET = 3; // (32 - 4 header bytes) / 8
constexpr uint8_t KEY_NONE = 0, KEY_STEADY = 1, KEY_FLASH = 2, KEY_BREATHE = 3, KEY_CYCLE = 4, KEY_TWINKLE = 5;
constexpr int     KEY_SPEED_DEFAULT = 122; // 320 ms a step, the Spectrum FLASH rate (firmware zxkb.c key_step_ms)
int key_step_ms(int speed);              // 16 ms (255) to 5 s (0), as the firmware's table
constexpr uint8_t CAPS      = 0xDE; // Caps Lock light (protocol 9): sub 0 get, 1 set, 2 save; always replies
constexpr uint8_t CAPS_GET = 0x00, CAPS_SET = 0x01, CAPS_SAVE = 0x02, CAPS_START_KEYS = 0x03; // 3: protocol 10
// reply/set layout: [0xDE, sub, effect, speed, r, g, b, r2, g2, b2, full, n, n bytes of key bits (bit i = LED i)]
constexpr uint8_t SETTINGS  = 0xDF; // keyboard settings (protocol 12): sub 0 get, 1 set + 13 bytes, 2 save; always replies
constexpr uint8_t SETTINGS_GET = 0x00, SETTINGS_SET = 0x01, SETTINGS_SAVE = 0x02;
constexpr int     SETTINGS_BYTES = 14; // game, SOCD, debounce ms, idle minutes, layer light on r g b, NKRO, 4 bytes macro
                                       // modes, the macro the record key records into (protocol 13; older firmware: 13)
constexpr uint8_t RECORDED  = 0xD7; // keyboard -> app (protocol 13): [0xD7, macro, bytes lo, hi, 1 saved / 0 not]
constexpr uint8_t TUNE      = 0xDA; // [0xDA, sub, ...] size and intensity of the current effect
constexpr uint8_t TUNE_GET  = 0x00;
constexpr uint8_t TUNE_SET  = 0x01; // [0xDA, 0x01, size, intensity]
constexpr uint8_t TUNE_SAVE = 0x02;
constexpr uint8_t TUNE_HAS_SIZE      = 0x01;
constexpr uint8_t TUNE_HAS_INTENSITY = 0x02;

// SignalRGB commands used for ZXKB direct mode
constexpr uint8_t DIRECT_STREAM  = 0x24; // [0x24, first, count (max 9), count x r,g,b]
constexpr uint8_t DIRECT_ENABLE  = 0x25;
constexpr uint8_t DIRECT_DISABLE = 0x26;

// Standard VIA lighting commands (QMK quantum/via.h)
constexpr uint8_t VIA_PROTOCOL = 0x01; // [0x01] -> [0x01, version hi, lo]
constexpr uint8_t VIA_KEYMAP_BUFFER = 0x12; // [0x12, offset hi, lo, size <= 28] -> same + size bytes (keycodes, big-endian)
constexpr uint8_t VIA_SET   = 0x07; // [0x07, channel, value id, data...]
constexpr uint8_t VIA_GET   = 0x08;
constexpr uint8_t VIA_SAVE  = 0x09; // [0x09, channel]
constexpr uint8_t VIA_RGB_MATRIX = 3;
constexpr uint8_t VIA_BRIGHTNESS = 1;
constexpr uint8_t VIA_EFFECT     = 2; // 0 = lighting off, else effect number
constexpr uint8_t VIA_SPEED      = 3;
constexpr uint8_t VIA_COLOUR     = 4; // hue, sat
} // namespace proto
