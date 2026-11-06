// Tests profiles by program without the desktop: front-program changes are fed in, the profiles applied recorded.
// Build (QMK MSYS): g++ -O1 -std=c++17 -DUNICODE -D_UNICODE -I src -o apps_test.exe test/apps_test.cpp -luser32
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <cstdio>
#include <string>
#include "app.h"
#include "settings.h"

Board *g_board = nullptr;
static std::string g_log; // effects applied, in order
static std::string g_now = "RAINBOW";
Profile current_profile() { Profile p; p.effect = g_now; return p; }
void apply_profile_data(const Profile &p) { g_log += p.effect + " "; g_now = p.effect; }
namespace settings {
std::wstring get(const wchar_t *section, const wchar_t *key, const wchar_t *) {
    std::wstring k = key;
    if (std::wstring(section) != L"apps") return L"";
    return k == L"game.exe" ? L"Game" : k == L"editor.exe" ? L"Work" : k == L"gone.exe" ? L"Deleted" : L"";
}
bool load_profile(const std::wstring &name, Profile *p) {
    if (name == L"Game") { p->effect = "SOLID_RED"; return true; }
    if (name == L"Work") { p->effect = "SOLID_WHITE"; return true; }
    return false; // "Deleted": the profile file is gone
}
std::vector<std::wstring> keys(const wchar_t *) { return {L"game.exe", L"editor.exe"}; }
}
#include "../src/apps.cpp"

using namespace apps;
static int g_fail = 0;
static void expect(const char *what, const std::string &want) {
    bool ok = g_log == want;
    std::printf("%s %s: '%s'\n", ok ? "ok  " : "FAIL", what, g_log.c_str());
    if (!ok) { std::printf("     wanted '%s'\n", want.c_str()); g_fail++; }
    g_log.clear();
}

int main() {
    Board b;
    front_changed(L"game.exe");
    expect("no keyboard: nothing", "");
    g_board = &b;
    front_changed(L"notepad.exe");
    expect("program without a profile: nothing", "");
    front_changed(L"game.exe");
    expect("game in front: its profile", "SOLID_RED ");
    front_changed(L"zxkb.exe");
    expect("ZXKB's own window in front: no change", "");
    front_changed(L"game.exe");
    expect("game again: not applied twice", "");
    front_changed(L"editor.exe");
    expect("another program with a profile: that one", "SOLID_WHITE ");
    front_changed(L"notepad.exe");
    expect("program without a profile: lighting from before the first", "RAINBOW ");
    bool ok = !active();
    std::printf("%s nothing in use after\n", ok ? "ok  " : "FAIL");
    g_fail += !ok;
    front_changed(L"gone.exe");
    expect("profile no longer saved: nothing", "");
    front_changed(L"game.exe");
    forget(); // keyboard unplugged meanwhile
    g_log.clear();
    g_now = "RAINBOW";
    front_changed(L"notepad.exe");
    expect("after the keyboard was unplugged: nothing put back (it starts with its own)", "");

    std::printf(g_fail ? "%d failed\n" : "all passed\n", g_fail);
    return g_fail;
}
