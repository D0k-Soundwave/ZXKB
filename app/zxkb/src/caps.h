// The Caps Lock light window: effect, colours, speed, brightness and keys of the light shown while Caps Lock is on.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>

void caps_dialog(HWND owner); // modal; changes show live on the keyboard, Cancel puts back what was there
