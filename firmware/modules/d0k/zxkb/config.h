// ZXKB community module settings
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/* EEPROM: the keyboard's own effect (kept while a PC overlay shows), size and intensity for each tunable effect,
 * per key: effect, speed (0-255), first colour (r, g, b), second colour (r, g, b); the Caps Lock light: effect,
 * speed, two colours, full-brightness flag, one bit per LED for its keys; then 32 spare bytes, so later settings
 * can be added without resetting the others.
 * [C] 1 + 2 x 14 + 8 x LEDs + 9 + (LEDs + 7) / 8 + 32; Q3 (88 LEDs): 29 + 704 + 9 + 11 + 32 = 785 bytes.
 * Version 2 size and intensity, 3 per-key colours, 4 per-key effects, 5 a byte per key speed, 6 Caps Lock light. */
#define EECONFIG_MODULE_ZXKB_DATA_SIZE    (1 + 2 * 14 + 8 * RGB_MATRIX_LED_COUNT + 9 + (RGB_MATRIX_LED_COUNT + 7) / 8 + 32)
#define EECONFIG_MODULE_ZXKB_DATA_VERSION 6
