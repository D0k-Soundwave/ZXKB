// Macros stored on the keyboard (VIA dynamic macros), edited as readable text:
//   plain characters are typed; {Enter} taps a key; {Ctrl down} / {Ctrl up} hold and let go; {Ctrl+C} holds Ctrl,
//   taps C, lets Ctrl go; {wait 250} waits 250 ms; {{ is a literal {.
// QMK types plain characters with US key positions (quantum/send_string: ascii_to_keycode), whatever the PC's layout.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace macros {
// QMK's macro bytes <-> the text form above
std::string  to_bytes(const std::wstring &text, std::wstring *error);
std::wstring to_text(const std::string &bytes);
bool read_all(std::vector<std::wstring> *texts, int *buffer_size); // from the keyboard
bool write_all(const std::vector<std::wstring> &texts, std::wstring *error); // to the keyboard (and kept by ZXKB)
std::wstring key_token(uint16_t scancode, bool down); // a recorded key press or release as text ("{Ctrl down}")
} // namespace macros

void macros_dialog(HWND owner); // the Macros window (not modal)
HWND macros_window();
bool macros_recording(); // keys go to the recording, not to the window's buttons
