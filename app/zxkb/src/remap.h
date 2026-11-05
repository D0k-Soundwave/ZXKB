// The Change keys window: what the keys selected in the preview do on the layer shown. Through VIA, so it works on
// ZXKB keyboards and standard VIA keyboards alike; changes are kept by the keyboard at once.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>

void remap_dialog(HWND owner); // not modal: keys can be selected in the preview while it is open
void remap_refresh();          // the selection, the layer or the keymap changed
HWND remap_window();           // for keyboard navigation, or null
