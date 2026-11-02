// ZXKB community module settings
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/* EEPROM: the keyboard's own effect (kept while a PC overlay shows), then size and intensity for each
 * tunable effect. [C] 1 + 2 x ZXKB_TUNABLES (14) = 29 bytes. Version 2 added size and intensity. */
#define EECONFIG_MODULE_ZXKB_DATA_SIZE    29
#define EECONFIG_MODULE_ZXKB_DATA_VERSION 2
