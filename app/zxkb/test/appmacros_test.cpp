// Tests ZXKB's macro player without touching the desktop: the key events it would send are recorded, not sent.
// Build (QMK MSYS): g++ -O1 -I src -o appmacros_test.exe test/appmacros_test.cpp -luser32
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>

static std::string g_sent;
static UINT fake_send(UINT n, LPINPUT in, int) {
    for (UINT i = 0; i < n; i++) {
        char b[16];
        std::snprintf(b, sizeof(b), "%c%02X ", (in[i].ki.dwFlags & KEYEVENTF_KEYUP) ? '-' : '+', in[i].ki.wScan);
        g_sent += b;
    }
    return n;
}
#define SendInput fake_send
#define Sleep(ms) ((void)0)

// stand-ins for what the player needs from the rest of ZXKB
#include "app.h"
#include "fnkeys.h"
#include "keystore.h"
#include "labels.h"
#include "layoutcheck.h"
#include "macros.h"
Board *g_board = nullptr;
bool keyboard_ask(const uint8_t *, int, uint8_t *, int) { return false; }
void settings_changed() {}
namespace fnkeys { bool active() { return false; } }
namespace labels { uint16_t scan_of_usage(uint8_t u) { return u; } } // the usage itself, to read the result
namespace layoutcheck { bool read_keymap(std::vector<uint16_t> *, int *) { return false; } }
namespace keystore {
std::wstring value(const wchar_t *, const wchar_t *) { return L""; }
void set_value(const wchar_t *, const wchar_t *, const wchar_t *) {}
std::wstring kept_macro(int) { return L""; }
void record(int, int, int, uint16_t) {}
}
namespace macros { std::string to_bytes(const std::wstring &, std::wstring *, bool) { return ""; } }
#include "../src/appmacros.cpp"
#undef SendInput

using namespace appmacros;
static int g_fail = 0;
static void expect(const char *what, const std::string &bytes, const std::string &want) {
    g_sent.clear();
    g_stop = false;
    play(bytes);
    bool ok = g_sent == want;
    std::printf("%s %s: '%s'\n", ok ? "ok  " : "FAIL", what, g_sent.c_str());
    if (!ok) { std::printf("     wanted '%s'\n", want.c_str()); g_fail++; }
}

int main() {
    expect("plain text, US positions (a = usage 04)", "ab", "+04 -04 +05 -05 ");
    expect("capital: Shift (E1) held", "A", "+E1 +04 -04 -E1 ");
    expect("! is Shift + 1 (1E)", "!", "+E1 +1E -1E -E1 ");
    expect("tap code (Enter 28)", std::string("\x01\x01\x28", 3), "+28 -28 ");
    expect("down / up codes (Ctrl E0, C 06)", std::string("\x01\x02\xE0\x01\x01\x06\x01\x03\xE0", 9), "+E0 +06 -06 -E0 ");
    expect("fixed wait then key", std::string("\x01\x04") + "5|" + "a", "+04 -04 ");
    expect("wait that varies, then key", std::string("\x01\x05") + "1-3|" + "a", "+04 -04 ");
    for (int i = 0; i < 256; i++) g_down[i] = false;

    // a macro stopped part-way leaves nothing held: the player lets go of what it pressed
    g_sent.clear();
    send_usage(0xE1, true);
    for (int u = 0; u < 256; u++) if (g_down[u]) send_usage((uint8_t)u, false);
    bool ok = g_sent == "+E1 -E1 ";
    std::printf("%s stopped part-way: held keys let go: '%s'\n", ok ? "ok  " : "FAIL", g_sent.c_str());
    g_fail += !ok;

    std::printf(g_fail ? "%d failed\n" : "all passed\n", g_fail);
    return g_fail;
}
