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

// SignalRGB commands used for ZXKB direct mode
constexpr uint8_t DIRECT_STREAM  = 0x24; // [0x24, first, count (max 9), count x r,g,b]
constexpr uint8_t DIRECT_ENABLE  = 0x25;
constexpr uint8_t DIRECT_DISABLE = 0x26;

// Standard VIA lighting commands (QMK quantum/via.h)
constexpr uint8_t VIA_SET   = 0x07; // [0x07, channel, value id, data...]
constexpr uint8_t VIA_GET   = 0x08;
constexpr uint8_t VIA_SAVE  = 0x09; // [0x09, channel]
constexpr uint8_t VIA_RGB_MATRIX = 3;
constexpr uint8_t VIA_BRIGHTNESS = 1;
constexpr uint8_t VIA_EFFECT     = 2; // 0 = lighting off, else effect number
constexpr uint8_t VIA_SPEED      = 3;
constexpr uint8_t VIA_COLOUR     = 4; // hue, sat
} // namespace proto
