// Live preview: the keyboard drawn with its keycaps, lit with the colours the keyboard reports (LED mirror).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <vector>

HWND preview_create(HWND parent, int id); // child control; its drawing resources go when it is destroyed
void preview_update();                    // new LED colours arrived: redraw
double preview_aspect();
void preview_relabel(); // the keymap or the keyboard language changed: redraw the key labels
void preview_simulate(); // work out the next frame of the effect (timer: standard keyboards, a draft, lock lights)
// The window's draft (changes not applied yet): the preview works it out instead of showing the keyboard's own LEDs.
// keys: show the keys' own colours (the regular layer) instead of the effect (keys being coloured, or effect off)
void preview_draft(bool on, bool keys);
bool preview_drafting();
void preview_locks(bool on); // the Lock lights window is open: the lock light that is on shows over the preview
void preview_input_back(); // key presses (raw input) to the preview again, after the Macros window recorded
// Key selection: click a key, Ctrl+click to add or remove one. The parent gets WM_COMMAND (id, PREVIEW_SELECTION).
constexpr WORD PREVIEW_SELECTION = 1;
std::vector<int> preview_selected_leds();   // LED numbers of the selected keys
std::vector<int> preview_selected_keys();   // the selected keys as indices into g_board->keys
int  preview_selected_count();
void preview_select_all();
void preview_select_none();                  // height / width of the drawing for the connected keyboard
