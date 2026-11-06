// Tests the lock lights' choice and colours without a keyboard or the desktop: what would be sent is recorded.
// Build (QMK MSYS): g++ -O1 -I src -o locklights_test.exe test/locklights_test.cpp -luser32
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <cstdio>
#include <string>
#include "app.h"
#include "protocol.h"
#include "settings.h"

Board *g_board = nullptr;
Status g_st;
bool   g_save_pending = false;
static std::string g_sent;
void via_set(uint8_t id, uint8_t a, uint8_t b) { char t[32]; std::snprintf(t, 32, "%u:%u,%u ", id, a, b); g_sent += t; }
void schedule_save() { g_sent += "save "; }
uint8_t effect_off_mode() { return 0; }
std::string effect_name(int m) { return m == 1 ? "Breathing" : m == 2 ? "Solid Color" : ""; }
namespace proto { int key_step_ms(int) { return 320; } }
namespace settings {
std::wstring get(const wchar_t *, const wchar_t *, const wchar_t *def) { return def; }
void set(const wchar_t *, const wchar_t *, const std::wstring &) {}
}
static bool g_caps = false;
#define GetKeyState(vk) ((vk) == VK_CAPITAL && g_caps ? 1 : 0)
#define GetTickCount() 0
#include "../src/locklights.cpp"

using namespace locklights;
static int g_fail = 0;
static void expect(const char *what, const std::string &got, const std::string &want) {
    bool ok = got == want;
    std::printf("%s %s: '%s'\n", ok ? "ok  " : "FAIL", what, got.c_str());
    if (!ok) { std::printf("     wanted '%s'\n", want.c_str()); g_fail++; }
}

int main() {
    Light l[COUNT];
    bool on[COUNT] = {true, true, false};
    expect("all off: none shows", std::to_string(choose(on, l)), "-1");
    l[NUM].fx = proto::KEY_STEADY;
    expect("Caps (off) and Num on: Num shows", std::to_string(choose(on, l)), "1");
    l[CAPS].fx = proto::KEY_STEADY;
    expect("Caps over Num", std::to_string(choose(on, l)), "0");

    uint8_t h, s, v;
    char t[32];
    Light red; red.fx = proto::KEY_STEADY; red.a[0] = 255; red.a[1] = 0; red.a[2] = 0;
    frame(red, 0, &h, &s, &v); std::snprintf(t, 32, "%u,%u,%u", h, s, v);
    expect("steady red: hue 0, full saturation", t, "0,255,255");
    Light fl; fl.fx = proto::KEY_FLASH; fl.a[0] = 0; fl.a[1] = 0; fl.a[2] = 255; // blue / black
    frame(fl, 100, &h, &s, &v); std::snprintf(t, 32, "%u,%u,%u", h, s, v);
    expect("flash first step: blue (hue 170)", t, "170,255,255");
    frame(fl, 400, &h, &s, &v); std::snprintf(t, 32, "%u", v);
    expect("flash second step: black (dark)", t, "0");
    Light br; br.fx = proto::KEY_BREATHE; br.a[0] = br.a[1] = br.a[2] = 255;
    frame(br, 400, &h, &s, &v); std::snprintf(t, 32, "%u", v); // quarter of 1600 ms: at colour 1
    expect("breathe at a quarter: colour 1 (white, full)", t, "255");
    frame(br, 1200, &h, &s, &v); std::snprintf(t, 32, "%u", v);
    expect("breathe at three quarters: colour 2 (black)", t, "0");

    // on the keyboard: solid colour effect, colour, brightness; then the keyboard's own lighting back
    Board b; b.via_channel = 3; g_board = &b;
    g_st.mode = 1; g_st.enabled = 1; g_st.hue = 85; g_st.sat = 255; g_st.val = 128; g_st.max_val = 255;
    g_loaded = true;
    g_lights[CAPS] = red;
    g_caps = true;
    g_sent.clear(); tick(true);
    expect("Caps Lock on: solid colour, red, full brightness", g_sent, "2:2,0 4:0,255 1:255,0 ");
    g_sent.clear(); tick(true);
    expect("still on: nothing sent again", g_sent, "");
    g_caps = false; g_save_pending = true;
    g_sent.clear(); tick(true);
    expect("Caps Lock off: own effect, colour, brightness back; save", g_sent, "2:1,0 4:85,255 1:128,0 save ");
    g_caps = true;
    g_sent.clear(); tick(false);
    expect("not allowed (window open): nothing shown", g_sent, "");

    std::printf(g_fail ? "%d failed\n" : "all passed\n", g_fail);
    return g_fail;
}
