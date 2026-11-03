// ZXKB community module settings
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/* EEPROM: the keyboard's own effect (kept while a PC overlay shows), size and intensity for each tunable effect,
 * then per key: effect and speed (one byte), first colour (r, g, b), second colour (r, g, b).
 * [C] 1 + 2 x ZXKB_TUNABLES (14) + 7 x LEDs; Q3 (88 LEDs): 29 + 616 = 645 bytes.
 * Version 2 added size and intensity, 3 per-key colours, 4 per-key effects. */
#define EECONFIG_MODULE_ZXKB_DATA_SIZE    (1 + 2 * 14 + 7 * RGB_MATRIX_LED_COUNT)
#define EECONFIG_MODULE_ZXKB_DATA_VERSION 4
