// Live preview: the keyboard drawn with its keycaps, lit with the colours the keyboard reports (LED mirror).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>

HWND preview_create(HWND parent, int id); // child control; its drawing resources go when it is destroyed
void preview_update();                    // new LED colours arrived: redraw
double preview_aspect();                  // height / width of the drawing for the connected keyboard
