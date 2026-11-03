// Firmware updates from the ZXKB window: the keyboard restarts into its bootloader on command (or the user holds
// Esc while plugging it in) and dfu-util, shipped next to ZXKB.exe, writes the chosen .bin.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>

void flash_dialog(HWND owner); // modal: pick a firmware file and install it
void flash_refresh();          // the keyboard connected or went away
bool flash_busy();             // installing: the window must not close
