// Number boxes beside sliders: type an exact value instead of dragging (D0k, 2026-11-05). The typed value is matched
// to the nearest slider step by the same text the box shows, so every slider's own units work: "75%", "320 ms",
// "1.5 s", "12.4 a second". A number typed without a unit takes the unit the box showed.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <string>

namespace numbox {
using Format = std::function<std::wstring(int pos)>;
// The slider position from lo to hi whose text is nearest to what was typed, or -1 if no number was typed.
int nearest(const std::wstring &typed, const std::wstring &shown, int lo, int hi, const Format &fmt);
} // namespace numbox
