// ZXKB community module settings
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/* EEPROM: the keyboard's own effect (kept while a PC overlay shows), size and intensity for each tunable effect,
 * then per-key colours: one "set" bit per LED and r, g, b per LED.
 * [C] 1 + 2 x ZXKB_TUNABLES (14) + (LEDs + 7) / 8 + 3 x LEDs; Q3 (88 LEDs): 29 + 11 + 264 = 304 bytes.
 * Version 2 added size and intensity, version 3 per-key colours. */
#define EECONFIG_MODULE_ZXKB_DATA_SIZE    (1 + 2 * 14 + (RGB_MATRIX_LED_COUNT + 7) / 8 + 3 * RGB_MATRIX_LED_COUNT)
#define EECONFIG_MODULE_ZXKB_DATA_VERSION 3
