// Macros stored on the keyboard (VIA dynamic macros), edited as readable text:
//   plain characters are typed; {Enter} taps a key; {Ctrl down} / {Ctrl up} hold and let go; {Ctrl+C} holds Ctrl,
//   taps C, lets Ctrl go; {wait 250} waits 250 ms; {wait 200-300} a different time from 200 to 300 ms each time it
//   plays (ZXKB firmware v30 on); {{ is a literal {.
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
std::string  to_bytes(const std::wstring &text, std::wstring *error, bool ranges = false); // ranges: allowed whatever the firmware
std::wstring to_text(const std::string &bytes);
bool read_all(std::vector<std::wstring> *texts, int *buffer_size); // from the keyboard
bool ranges_possible(); // the keyboard plays waits that vary, {wait 180-220} (ZXKB firmware v30 on)
bool write_all(const std::vector<std::wstring> &texts, std::wstring *error); // to the keyboard (and kept by ZXKB)
std::wstring key_token(uint16_t scancode, bool down); // a recorded key press or release as text ("{Ctrl down}")
} // namespace macros

void macros_dialog(HWND owner); // the Macros window (not modal)
HWND macros_window();
bool macros_recording(); // keys go to the recording, not to the window's buttons
void macros_recorded_on_keyboard(int macro); // read that macro again (the window copy, when it is open)
