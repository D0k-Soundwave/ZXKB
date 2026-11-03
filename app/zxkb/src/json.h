// Minimal JSON reader for keyboard definitions (VIA, QMK, KLE). Read-only, no exceptions; bad input gives a null value.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <vector>

struct Json {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
    double                                    num = 0;
    std::string                               str; // UTF-8
    std::vector<Json>                         items;
    std::vector<std::pair<std::string, Json>> members;

    const Json &operator[](const char *key) const;  // null value if missing
    const Json &operator[](size_t i) const;         // null value if out of range
    const Json &operator[](int i) const { return (*this)[(size_t)(i < 0 ? 0 : i)]; } // so j[0] is not ambiguous
    size_t       size() const { return type == ARR ? items.size() : type == OBJ ? members.size() : 0; }
    bool         is(Type t) const { return type == t; }
    int          to_int(int def = 0) const;          // numbers, and strings like "0x3434" or "12"
    std::string  to_str() const { return type == STR ? str : std::string(); }

    static Json parse(const std::string &text, bool loose = false); // loose: KLE's unquoted keys
};
