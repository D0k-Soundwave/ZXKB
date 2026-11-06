// Lock lights for any keyboard (see locklights.h).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "locklights.h"
#include "app.h"
#include "protocol.h"
#include "settings.h"
#include <cmath>
#include <cstdio>
#include <string>

void via_set(uint8_t value_id, uint8_t a, uint8_t b);
void schedule_save();
extern bool g_save_pending;

namespace locklights {
namespace {

const wchar_t *const keys[COUNT]  = {L"caps", L"num", L"scroll"};
const wchar_t *const names[COUNT] = {L"Caps Lock", L"Num Lock", L"Scroll Lock"};
const int            vks[COUNT]   = {VK_CAPITAL, VK_NUMLOCK, VK_SCROLL};

Light g_lights[COUNT];
bool  g_loaded = false;
int   g_shown  = -1;              // the light on the keyboard now, -1 none
int   g_sent_fx = -1;             // the effect sent for it
int   g_sent_hue = -1, g_sent_sat = -1, g_sent_val = -1; // last colour and brightness sent (only changes are sent)

void load() {
    for (int w = 0; w < COUNT; w++) {
        Light l;
        unsigned v[9];
        std::wstring s = settings::get(L"locks", keys[w]);
        if (swscanf(s.c_str(), L"%u,%u,%u,%u,%u,%u,%u,%u,%u", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8]) == 9) {
            l.fx = (uint8_t)v[0];
            l.speed = (uint8_t)v[1];
            for (int j = 0; j < 3; j++) { l.a[j] = (uint8_t)v[2 + j]; l.b[j] = (uint8_t)v[5 + j]; }
            l.full = v[8] != 0;
        }
        if (l.fx != proto::KEY_STEADY && l.fx != proto::KEY_FLASH && l.fx != proto::KEY_BREATHE) l.fx = proto::KEY_NONE;
        g_lights[w] = l;
    }
    g_loaded = true;
    g_sent_hue = g_sent_sat = g_sent_val = -1; // shown again with the settings as they are now
}

void rgb_to_hsv(const uint8_t *c, uint8_t *h, uint8_t *s, uint8_t *v) { // QMK's 0-255 hue, saturation and value
    int r = c[0], g = c[1], b = c[2];
    int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    *v = (uint8_t)mx;
    if (mx == 0 || mx == mn) { *h = 0; *s = 0; return; }
    int d = mx - mn;
    *s = (uint8_t)(255 * d / mx);
    int hh; // 0-1535: six sixths of 256
    if (mx == r) hh = 256 * (g - b) / d;
    else if (mx == g) hh = 512 + 256 * (b - r) / d;
    else hh = 1024 + 256 * (r - g) / d;
    if (hh < 0) hh += 1536;
    *h = (uint8_t)(hh / 6);
}

// The board's "solid colour" effect, as its effect list names it ("SOLID_COLOR", "Solid Color", "Solid colour")
int solid_effect() {
    for (int m = 1; m < 256; m++) {
        std::string n = effect_name(m);
        if (n.empty()) break;
        std::string k;
        for (char ch : n) if (isalpha((unsigned char)ch)) k += (char)tolower((unsigned char)ch);
        if (k == "solidcolor" || k == "solidcolour") return m;
    }
    return 0;
}

int raw_brightness(int val) { // VIA brightness is 0-255 of the board's maximum (as set_brightness)
    int mx = g_st.max_val;
    int v = mx ? (val * 255 + mx - 1) / mx : 0;
    return v > 255 ? 255 : v;
}

} // namespace

const wchar_t *name(int which) { return which >= 0 && which < COUNT ? names[which] : L""; }

Light get(int which) {
    if (!g_loaded) load();
    return which >= 0 && which < COUNT ? g_lights[which] : Light();
}

void set(int which, const Light &l) {
    if (which < 0 || which >= COUNT) return;
    wchar_t t[80];
    swprintf(t, 80, L"%u,%u,%u,%u,%u,%u,%u,%u,%u", l.fx, l.speed, l.a[0], l.a[1], l.a[2], l.b[0], l.b[1], l.b[2], l.full ? 1 : 0);
    settings::set(L"locks", keys[which], t);
    g_lights[which] = l;
    g_sent_hue = -1; // shown again with the new settings
}

void reload() { load(); }

bool any_on() {
    if (!g_loaded) load();
    for (const Light &l : g_lights)
        if (l.fx != proto::KEY_NONE) return true;
    return false;
}

void frame(const Light &l, DWORD ms, uint8_t *hue, uint8_t *sat, uint8_t *val) {
    DWORD   step = (DWORD)proto::key_step_ms(l.speed);
    uint8_t c[3];
    if (l.fx == proto::KEY_FLASH) {
        const uint8_t *f = (ms / step) & 1 ? l.b : l.a;
        for (int j = 0; j < 3; j++) c[j] = f[j];
    } else if (l.fx == proto::KEY_BREATHE) { // a to b and back once every 5 steps, as the firmware's sin8
        DWORD   period = step * 5;
        double  x = 0.5 + 0.5 * std::sin(6.283185307179586 * (double)(ms % period) / (double)period);
        for (int j = 0; j < 3; j++) c[j] = (uint8_t)(l.a[j] * x + l.b[j] * (1 - x) + 0.5);
    } else {
        for (int j = 0; j < 3; j++) c[j] = l.a[j];
    }
    rgb_to_hsv(c, hue, sat, val);
}

int choose(const bool on[COUNT], const Light lights[COUNT]) {
    for (int w = 0; w < COUNT; w++)
        if (on[w] && lights[w].fx != proto::KEY_NONE) return w;
    return -1;
}

bool showing() { return g_shown >= 0; }

void put_back() {
    if (g_shown < 0) return;
    g_shown = -1;
    g_sent_fx = g_sent_hue = g_sent_sat = g_sent_val = -1;
    if (!g_board || !g_board->via_channel) return;
    via_set(proto::VIA_EFFECT, g_st.enabled ? g_st.mode : 0, 0);
    via_set(proto::VIA_COLOUR, g_st.hue, g_st.sat);
    via_set(proto::VIA_BRIGHTNESS, (uint8_t)raw_brightness(g_st.val), 0);
    if (g_save_pending) schedule_save(); // lighting changed meanwhile: saved now the light is gone
}

void forget() {
    g_shown = -1;
    g_sent_fx = g_sent_hue = g_sent_sat = g_sent_val = -1;
}

void tick(bool allowed) {
    if (!g_loaded) load();
    bool on[COUNT];
    for (int w = 0; w < COUNT; w++) on[w] = (GetKeyState(vks[w]) & 1) != 0;
    int want = allowed && g_board && g_board->via_channel ? choose(on, g_lights) : -1;
    int fx   = want >= 0 ? solid_effect() : 0;
    if (want < 0 || !fx) { put_back(); return; }
    if (want != g_shown) g_sent_hue = g_sent_sat = g_sent_val = -1;
    g_shown = want;
    if (g_sent_fx != fx) { via_set(proto::VIA_EFFECT, (uint8_t)fx, 0); g_sent_fx = fx; }
    const Light &l = g_lights[want];
    uint8_t h, s, v;
    frame(l, GetTickCount(), &h, &s, &v);
    int bright = raw_brightness(l.full ? g_st.max_val : g_st.val) * v / 255;
    if (h != g_sent_hue || s != g_sent_sat) { via_set(proto::VIA_COLOUR, h, s); g_sent_hue = h; g_sent_sat = s; }
    if (bright != g_sent_val) { via_set(proto::VIA_BRIGHTNESS, (uint8_t)bright, 0); g_sent_val = bright; }
}

} // namespace locklights
