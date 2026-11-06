// Tests ZXKB's input hook logic without touching the desktop: key events are fed straight to the hook procedure and
// the keys it would send are recorded instead of sent. Build: g++ -O1 -o hook_test.exe test/hook_test.cpp -luser32
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <cstdio>
#include <string>

static std::string g_sent; // "+A" down, "-A" up
static UINT fake_send(UINT n, LPINPUT in, int) {
    for (UINT i = 0; i < n; i++) {
        g_sent += (in[i].ki.dwFlags & KEYEVENTF_KEYUP) ? '-' : '+';
        g_sent += (char)in[i].ki.wVk;
        g_sent += ' ';
    }
    return n;
}
#define SendInput fake_send
#define SetWindowsHookExW(a, b, c, d) ((HHOOK)1)
#define CallNextHookEx(a, b, c, d) 0
#include "../src/inputhook.cpp"
#undef SendInput

using namespace inputhook;
static int  g_fail = 0;
static std::string g_passed; // keys the hook let through

static void key(DWORD vk, bool down, bool alt = false) {
    KBDLLHOOKSTRUCT k = {};
    k.vkCode = vk;
    k.flags  = alt ? LLKHF_ALTDOWN : 0;
    LRESULT r = proc(HC_ACTION, down ? WM_KEYDOWN : WM_KEYUP, (LPARAM)&k);
    if (!r) { g_passed += down ? '+' : '-'; g_passed += (char)vk; g_passed += ' '; }
}
static void expect(const char *what, const std::string &passed, const std::string &sent) {
    bool ok = g_passed == passed && g_sent == sent;
    std::printf("%s %s: passed '%s' sent '%s'\n", ok ? "ok  " : "FAIL", what, g_passed.c_str(), g_sent.c_str());
    if (!ok) { std::printf("     wanted passed '%s' sent '%s'\n", passed.c_str(), sent.c_str()); g_fail++; }
    g_passed.clear();
    g_sent.clear();
}
static void set(bool game, uint8_t socd) {
    g_game = game;
    g_block = 7;
    g_socd = socd;
    for (int p = 0; p < 4; p++) { held[p][0] = held[p][1] = false; active[p] = -1; }
}

int main() {
    set(false, 0);
    key('A', true); key('D', true); key('D', false); key('A', false);
    expect("SOCD off: all keys pass", "+A +D -D -A ", "");

    set(false, 1);
    key('A', true); key('D', true); key('D', false); key('A', false);
    expect("SOCD: A, then D wins, D let go: A back", "+A +D -A ", "-A -D +A ");

    key('A', true); key('D', true); key('A', false); key('D', false);
    expect("SOCD: A, then D wins, A let go first", "+A +D -D ", "-A ");

    key('A', true); key('D', true); key('A', true); key('D', true);
    expect("SOCD: key repeat only for the winner", "+A +D +D ", "-A ");
    key('D', false); key('A', false);
    g_passed.clear(); g_sent.clear();

    key(VK_LEFT, true); key(VK_RIGHT, true);
    expect("SOCD off for arrows when only A/D W/S chosen", "+% +' ", "");
    key(VK_LEFT, false); key(VK_RIGHT, false);
    g_passed.clear(); g_sent.clear();

    set(true, 0);
    key(VK_MENU, true, true); key(VK_TAB, true, true); key(VK_TAB, false, true); key(VK_MENU, false, true);
    expect("Game mode: Alt+Tab blocked (Alt passes)", "+\x12 -\x12 ", "");
    key(VK_MENU, true, true); key(VK_F4, true, true); key(VK_F4, false, true); key(VK_MENU, false, true);
    expect("Game mode: Alt+F4 blocked", "+\x12 -\x12 ", "");
    key(VK_LWIN, true); key(VK_LWIN, false); key(VK_RWIN, true); key(VK_RWIN, false);
    expect("Game mode: Windows keys blocked", "", "");
    key(VK_TAB, true); key(VK_TAB, false); key(VK_F4, true); key(VK_F4, false);
    expect("Game mode: Tab and F4 alone still work", "+\t -\t +s -s ", "");

    set(false, 0);
    key(VK_LWIN, true); key(VK_LWIN, false);
    expect("Game mode off: Windows key works", "+[ -[ ", "");

    std::printf(g_fail ? "%d failed\n" : "all passed\n", g_fail);
    return g_fail;
}
