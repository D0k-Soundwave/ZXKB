// Number boxes beside sliders. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "numbox.h"
#include <cmath>
#include <cwctype>

namespace numbox {
namespace {

// The number in a text, and whether it is in seconds ("1.5 s") rather than anything else ("ms", "%", none)
bool number_of(const std::wstring &t, double *v, int *unit) { // unit: 0 none, 1 ms, 2 s, 3 other
    size_t i = 0;
    while (i < t.size() && !iswdigit(t[i]) && t[i] != L'.') i++;
    if (i == t.size()) return false;
    size_t j = i;
    while (j < t.size() && (iswdigit(t[j]) || t[j] == L'.' || t[j] == L',')) j++;
    std::wstring n = t.substr(i, j - i);
    for (wchar_t &c : n) if (c == L',') c = L'.';
    *v = wcstod(n.c_str(), nullptr);
    std::wstring rest = t.substr(j);
    size_t k = 0;
    while (k < rest.size() && rest[k] == L' ') k++;
    rest = rest.substr(k);
    if (rest.empty()) *unit = 0;
    else if (rest.compare(0, 2, L"ms") == 0) *unit = 1;
    else if (rest[0] == L's' && (rest.size() == 1 || !iswalpha(rest[1]))) *unit = 2;
    else *unit = 3;
    return true;
}

double in_ms(double v, int unit) { return unit == 2 ? v * 1000 : v; }

} // namespace

int nearest(const std::wstring &typed, const std::wstring &shown, int lo, int hi, const Format &fmt) {
    double want;
    int    unit;
    if (!number_of(typed, &want, &unit)) return -1;
    if (unit == 0) { // no unit typed: the unit the box showed
        double dummy;
        if (!number_of(shown, &dummy, &unit)) unit = 0;
    }
    want = in_ms(want, unit);
    int    best = -1;
    double best_d = 0;
    for (int p = lo; p <= hi; p++) {
        double v;
        int    u;
        if (!number_of(fmt(p), &v, &u)) continue;
        double d = std::fabs(in_ms(v, u) - want);
        if (best < 0 || d < best_d) { best = p; best_d = d; }
    }
    return best;
}

} // namespace numbox
