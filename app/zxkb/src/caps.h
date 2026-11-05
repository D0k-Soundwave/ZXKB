// The Caps Lock light window: effect, colours, speed, brightness and keys of the light shown while Caps Lock is on.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>

void caps_dialog(HWND owner); // not modal; changes show live on the keyboard, Cancel puts back what was there
HWND caps_window();           // the open Caps Lock window (for keyboard navigation), or null
void caps_selection_changed(); // keys were selected in the preview
