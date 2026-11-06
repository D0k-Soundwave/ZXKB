// Simulated preview: QMK's standard effects. The formulas below are QMK's (quantum/rgb_matrix/animations/*.h,
// lib/lib8tion, quantum/color.c, master f5db962), ported to work on the key layout instead of the keyboard's LEDs.
// Copyright 2026 D0k (port); QMK contributors (effect maths), GPL-2.0-or-later
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim.h"
#include "labels.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace proto { int key_step_ms(int speed); } // main.cpp (the firmware's table)

namespace sim {
namespace {
int proto_step_ms(int speed) { int ms = proto::key_step_ms(speed); return ms > 0 ? ms : 1; }

// ---------- lib8tion (QMK lib/lib8tion, FASTLED_SCALE8_FIXED) ----------
uint8_t scale8(uint8_t i, uint8_t scale) { return (uint8_t)(((uint16_t)i * (1 + scale)) >> 8); }
uint16_t scale16by8(uint16_t i, uint8_t scale) { return (uint16_t)(((uint32_t)i * (1 + scale)) >> 8); }
uint8_t qadd8(uint8_t a, uint8_t b) { int t = a + b; return (uint8_t)(t > 255 ? 255 : t); }
uint8_t qsub8(uint8_t a, uint8_t b) { int t = a - b; return (uint8_t)(t < 0 ? 0 : t); }
uint8_t abs8(int8_t a) { return (uint8_t)(a < 0 ? -a : a); }
const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};
uint8_t sin8(uint8_t theta) {
    uint8_t offset = theta;
    if (theta & 0x40) offset = (uint8_t)255 - offset;
    offset &= 0x3F;
    uint8_t secoffset = offset & 0x0F;
    if (theta & 0x40) secoffset++;
    uint8_t section = offset >> 4;
    uint8_t b = b_m16_interleave[section * 2], m16 = b_m16_interleave[section * 2 + 1];
    uint8_t mx = (uint8_t)((m16 * secoffset) >> 4);
    int8_t  y  = (int8_t)(mx + b);
    if (theta & 0x80) y = (int8_t)-y;
    return (uint8_t)(y + 128);
}
uint8_t cos8(uint8_t theta) { return sin8((uint8_t)(theta + 64)); }
uint8_t sqrt16(uint16_t x) {
    if (x <= 1) return (uint8_t)x;
    uint8_t low = 1, hi = x > 7904 ? 255 : (uint8_t)((x >> 5) + 8), mid;
    do {
        mid = (uint8_t)((low + hi) >> 1);
        if ((uint16_t)(mid * mid) > x) hi = (uint8_t)(mid - 1);
        else {
            if (mid == 255) return 255;
            low = (uint8_t)(mid + 1);
        }
    } while (hi >= low);
    return (uint8_t)(low - 1);
}
uint8_t atan2_8(int16_t dy, int16_t dx) {
    if (dy == 0) return dx >= 0 ? 0 : 128;
    int16_t abs_y = dy > 0 ? dy : (int16_t)-dy;
    int8_t  a;
    if (dx >= 0) a = (int8_t)(32 - (32 * (dx - abs_y) / (dx + abs_y)));
    else a = (int8_t)(96 - (32 * (dx + abs_y) / (abs_y - dx)));
    if (dy < 0) return (uint8_t)-a;
    return (uint8_t)a;
}

struct Hsv { uint8_t h, s, v; };
void hsv_to_rgb(Hsv hsv, uint8_t *out) { // QMK quantum/color.c hsv_to_rgb_impl (no CIE curve)
    if (hsv.s == 0) { out[0] = out[1] = out[2] = hsv.v; return; }
    uint16_t h = hsv.h, s = hsv.s, v = hsv.v;
    uint8_t region = (uint8_t)(h * 6 / 255), remainder = (uint8_t)((h * 2 - region * 85) * 3);
    uint8_t p = (uint8_t)((v * (255 - s)) >> 8), q = (uint8_t)((v * (255 - ((s * remainder) >> 8))) >> 8),
            t = (uint8_t)((v * (255 - ((s * (255 - remainder)) >> 8))) >> 8);
    switch (region) {
        case 6:
        case 0: out[0] = (uint8_t)v; out[1] = t; out[2] = p; break;
        case 1: out[0] = q; out[1] = (uint8_t)v; out[2] = p; break;
        case 2: out[0] = p; out[1] = (uint8_t)v; out[2] = t; break;
        case 3: out[0] = p; out[1] = q; out[2] = (uint8_t)v; break;
        case 4: out[0] = t; out[1] = p; out[2] = (uint8_t)v; break;
        default: out[0] = (uint8_t)v; out[1] = p; out[2] = q; break;
    }
}

// ---------- the keys as QMK LEDs ----------
// QMK places LEDs in a 224 x 64 space with the centre at 112, 32 (k_rgb_matrix_center); key centres are scaled into it.
struct Led { int16_t x, y; bool mod; uint16_t scan; };
std::vector<Led> g_leds;
constexpr int16_t CX = 112, CY = 32;

// key presses, as QMK's g_last_hit_tracker (most recent last) [P] LED_HITS_TO_REMEMBER 8
struct Hit { int key; uint32_t ms; };
std::vector<Hit> g_hits;
constexpr size_t HITS = 8;
std::vector<uint8_t> g_heat; // typing heatmap
uint32_t g_heat_last = 0;
// random effects keep their colours between frames
std::vector<uint8_t> g_drops;
uint32_t g_drops_tick = 0xFFFFFFFF;

int16_t dx_of(size_t i) { return (int16_t)(g_leds[i].x - CX); }
int16_t dy_of(size_t i) { return (int16_t)(g_leds[i].y - CY); }

std::string canonical(const std::string &raw) { // "Solid Color" / "REACTIVE_SIMPLE" -> QMK's own names
    std::string n = raw;
    for (char &c : n) c = c == ' ' || c == '-' ? '_' : (char)toupper((unsigned char)c);
    if (n.rfind("REACTIVE", 0) == 0) n = "SOLID_" + n;
    if (n.rfind("ZX_", 0) == 0) n = n.substr(3); // ZXKB's versions of QMK's typing effects
    if (n.rfind("REACTIVE", 0) == 0) n = "SOLID_" + n;
    return n;
}

const char *const known_names[] = {
    "SOLID_COLOR", "ALPHAS_MODS", "GRADIENT_UP_DOWN", "GRADIENT_LEFT_RIGHT", "BREATHING", "BAND_SAT", "BAND_VAL",
    "BAND_PINWHEEL_SAT", "BAND_PINWHEEL_VAL", "BAND_SPIRAL_SAT", "BAND_SPIRAL_VAL", "CYCLE_ALL", "CYCLE_LEFT_RIGHT",
    "CYCLE_UP_DOWN", "RAINBOW_MOVING_CHEVRON", "CYCLE_OUT_IN", "CYCLE_OUT_IN_DUAL", "CYCLE_PINWHEEL", "CYCLE_SPIRAL",
    "DUAL_BEACON", "RAINBOW_BEACON", "RAINBOW_PINWHEELS", "FLOWER_BLOOMING", "RIVERFLOW", "HUE_BREATHING", "HUE_PENDULUM",
    "HUE_WAVE", "RAINDROPS", "JELLYBEAN_RAINDROPS", "TYPING_HEATMAP", "SOLID_REACTIVE_SIMPLE", "SOLID_REACTIVE",
    "SOLID_REACTIVE_WIDE", "SOLID_REACTIVE_MULTIWIDE", "SOLID_REACTIVE_CROSS", "SOLID_REACTIVE_MULTICROSS",
    "SOLID_REACTIVE_NEXUS", "SOLID_REACTIVE_MULTINEXUS", "SPLASH", "MULTISPLASH", "SOLID_SPLASH", "SOLID_MULTISPLASH",
    "SOLID_COLOUR",
};

} // namespace

void set_keys(const std::vector<KeyRect> &keys, const std::vector<uint16_t> &keymap, int cols) {
    g_leds.clear();
    g_hits.clear();
    if (keys.empty()) return;
    int minx = INT32_MAX, maxx = INT32_MIN, miny = INT32_MAX, maxy = INT32_MIN;
    for (const KeyRect &k : keys) {
        int cx = k.x + k.w / 2, cy = k.y + k.h / 2;
        minx = std::min(minx, cx); maxx = std::max(maxx, cx); miny = std::min(miny, cy); maxy = std::max(maxy, cy);
    }
    for (const KeyRect &k : keys) {
        Led l;
        l.x = (int16_t)(maxx > minx ? (k.x + k.w / 2 - minx) * 224 / (maxx - minx) : 112);
        l.y = (int16_t)(maxy > miny ? (k.y + k.h / 2 - miny) * 64 / (maxy - miny) : 32);
        uint16_t kc = 0;
        size_t at = (size_t)(k.row * cols + k.col);
        if (k.row >= 0 && k.col >= 0 && at < keymap.size()) kc = keymap[at];
        uint8_t usage = kc <= 0xFF ? (uint8_t)kc : (uint8_t)(kc & 0xFF);
        // modifiers for "Letters and modifiers": anything but letters, numbers and the space bar ([A], QMK uses
        // the keyboard's own LED flags, which standard firmware does not report)
        l.mod  = !(usage >= 0x04 && usage <= 0x27) && usage != 0x2C;
        l.scan = labels::scan_of_usage(usage);
        g_leds.push_back(l);
    }
    g_heat.assign(g_leds.size(), 0);
    g_drops.assign(g_leds.size() * 3, 0);
    g_drops_tick = 0xFFFFFFFF;
}

void set_leds(const std::vector<uint8_t> &xyflags, const std::vector<uint16_t> &scan) {
    g_leds.clear();
    g_hits.clear();
    for (size_t i = 0; i + 2 < xyflags.size(); i += 3) {
        Led l;
        l.x    = xyflags[i];
        l.y    = xyflags[i + 1];
        l.mod  = (xyflags[i + 2] & 0x01) != 0; // [P] QMK LED_FLAG_MODIFIER 0x01
        l.scan = i / 3 < scan.size() ? scan[i / 3] : 0;
        g_leds.push_back(l);
    }
    g_heat.assign(g_leds.size(), 0);
    g_drops.assign(g_leds.size() * 3, 0);
    g_drops_tick = 0xFFFFFFFF;
}

void hsv(uint8_t h, uint8_t s, uint8_t v, uint8_t *rgb) { hsv_to_rgb({h, s, v}, rgb); }

// As zxkb.c rgb_matrix_indicators_advanced_zxkb (v33), with its step table worked out as proto::key_step_ms
void keys_frame(const uint8_t *key, int leds, uint8_t v, uint32_t now, uint8_t *rgb) {
    for (int i = 0; i < leds; i++) {
        const uint8_t *k = key + i * 8, *a = k + 2, *b = k + 5;
        uint8_t *out = rgb + i * 3;
        uint8_t fx = k[0];
        if (fx == 0) { out[0] = out[1] = out[2] = 0; continue; } // follows the effect: the effect is off, so dark
        uint32_t step = (uint32_t)proto_step_ms(k[1]);
        uint8_t mix = 255, c[3];
        switch (fx) {
            case 2: mix = (now / step) & 1 ? 0 : 255; break;                          // flash
            case 3: mix = sin8((uint8_t)(now * 256 / (step * 5))); break;              // breathe
            case 5: {                                                                  // twinkle
                uint8_t w = sin8((uint8_t)(now * 256 / (step * 6) + i * 97));
                mix = w > 200 ? (uint8_t)((w - 200) * 255 / 55) : 0;
                break;
            }
            case 4: {                                                                  // colour cycle
                uint8_t peak = std::max(a[0], std::max(a[1], a[2]));
                hsv_to_rgb({(uint8_t)(now * 256 / (step * 10) + i * 3), 255, peak}, c);
                for (int j = 0; j < 3; j++) out[j] = (uint8_t)(c[j] * v / 255);
                continue;
            }
            default: break;
        }
        for (int j = 0; j < 3; j++) out[j] = (uint8_t)((a[j] * mix + b[j] * (255 - mix)) / 255 * v / 255);
    }
}

bool known(const std::string &effect) {
    std::string n = canonical(effect);
    for (const char *k : known_names)
        if (n == k) return true;
    return false;
}

void key_hit(uint16_t scancode) {
    for (size_t i = 0; i < g_leds.size(); i++) {
        if (g_leds[i].scan != scancode || !scancode) continue;
        if (g_hits.size() == HITS) g_hits.erase(g_hits.begin());
        g_hits.push_back({(int)i, 0});
        g_hits.back().ms = 0xFFFFFFFF; // stamped at the next render
        // typing heatmap: [P] QMK defaults INCREASE_STEP 32, SPREAD 40, AREA_LIMIT 16
        g_heat[i] = qadd8(g_heat[i], 32);
        for (size_t j = 0; j < g_leds.size(); j++) {
            int dx = g_leds[i].x - g_leds[j].x, dy = g_leds[i].y - g_leds[j].y;
            uint8_t d = sqrt16((uint16_t)std::min(dx * dx + dy * dy, 65535));
            if (j == i) g_heat[j] = qadd8(g_heat[j], 32);
            else if (d <= 40) g_heat[j] = qadd8(g_heat[j], std::min<uint8_t>(qsub8(40, d), 16));
        }
        return;
    }
}

bool render(const std::string &effect, uint8_t h, uint8_t s, uint8_t v, uint8_t speed, uint32_t ms, uint8_t *rgb) {
    std::string n = canonical(effect);
    if (n == "SOLID_COLOUR") n = "SOLID_COLOR";
    if (!known(n)) return false;
    const Hsv base = {h, s, v};
    uint16_t timer = (uint16_t)ms; // QMK's g_rgb_timer counts ms in 16 bits... wraps like the keyboard's
    for (Hit &hit : g_hits)
        if (hit.ms == 0xFFFFFFFF) hit.ms = ms;
    // heatmap cooling: [P] DECREASE_DELAY_MS 25, one step each
    if (ms - g_heat_last >= 25) {
        uint32_t steps = (ms - g_heat_last) / 25;
        for (uint8_t &x : g_heat) x = qsub8(x, (uint8_t)std::min<uint32_t>(steps, 255));
        g_heat_last = ms;
    }
    uint8_t time_i  = (uint8_t)scale16by8(timer, qadd8(speed / 4, 1)); // effect_runner_i
    uint8_t time_dx = (uint8_t)scale16by8(timer, speed / 2);           // effect_runner_dx_dy(_dist)
    uint16_t time_sc = scale16by8(timer, speed / 4);                   // effect_runner_sin_cos_i
    int8_t cos_v = (int8_t)(cos8((uint8_t)time_sc) - 128), sin_v = (int8_t)(sin8((uint8_t)time_sc) - 128);

    // random effects: a new drop every few QMK ticks, as RAINDROPS / JELLYBEAN_RAINDROPS
    bool drops = n == "RAINDROPS" || n == "JELLYBEAN_RAINDROPS";
    if (drops && !g_leds.empty()) {
        uint32_t tick = scale16by8(timer, qadd8(speed, 16)) / (n == "RAINDROPS" ? 10 : 5);
        auto drop = [&](size_t i) {
            Hsv c = base;
            if (n == "RAINDROPS") { int8_t dh = (int8_t)((uint8_t)(c.h + 128) - c.h) / 4; c.h = (uint8_t)(c.h + dh * (rand() % 3)); }
            else { c.h = (uint8_t)rand(); c.s = (uint8_t)(127 + rand() % 129); }
            hsv_to_rgb(c, &g_drops[i * 3]);
        };
        if (g_drops_tick == 0xFFFFFFFF) for (size_t i = 0; i < g_leds.size(); i++) drop(i);
        else if (tick != g_drops_tick) drop((size_t)rand() % g_leds.size());
        g_drops_tick = tick;
    }

    for (size_t i = 0; i < g_leds.size(); i++) {
        Hsv c = base;
        uint8_t *out = rgb + i * 3;
        int16_t x = g_leds[i].x, y = g_leds[i].y, dx = dx_of(i), dy = dy_of(i);
        uint8_t dist = sqrt16((uint16_t)(dx * dx + dy * dy));
        if (n == "SOLID_COLOR") {
        } else if (n == "ALPHAS_MODS") {
            if (g_leds[i].mod) c.h = (uint8_t)(c.h + speed);
        } else if (n == "GRADIENT_UP_DOWN") {
            c.h = (uint8_t)(base.h + scale8(64, speed) * (y >> 4));
        } else if (n == "GRADIENT_LEFT_RIGHT") {
            c.h = (uint8_t)(base.h + (scale8(64, speed) * x >> 5));
        } else if (n == "BREATHING") {
            c.v = scale8((uint8_t)(abs8((int8_t)(sin8((uint8_t)(time_i / 2)) - 128)) * 2), c.v);
        } else if (n == "BAND_SAT" || n == "BAND_VAL") {
            int16_t t = (int16_t)((n == "BAND_SAT" ? c.s : c.v) - abs(scale8((uint8_t)x, 228) + 28 - time_i) * 8);
            if (n == "BAND_SAT") c.s = scale8((uint8_t)(t < 0 ? 0 : t), c.s);
            else c.v = scale8((uint8_t)(t < 0 ? 0 : t), c.v);
        } else if (n == "BAND_PINWHEEL_SAT") {
            c.s = scale8((uint8_t)(c.s - time_dx - atan2_8(dy, dx) * 3), c.s);
        } else if (n == "BAND_PINWHEEL_VAL") {
            c.v = scale8((uint8_t)(c.v - time_dx - atan2_8(dy, dx) * 3), c.v);
        } else if (n == "BAND_SPIRAL_SAT") {
            c.s = scale8((uint8_t)(c.s + dist - time_dx - atan2_8(dy, dx)), c.s);
        } else if (n == "BAND_SPIRAL_VAL") {
            c.v = scale8((uint8_t)(c.v + dist - time_dx - atan2_8(dy, dx)), c.v);
        } else if (n == "CYCLE_ALL") {
            c.h = time_i;
        } else if (n == "CYCLE_LEFT_RIGHT") {
            c.h = (uint8_t)(x - time_i);
        } else if (n == "CYCLE_UP_DOWN") {
            c.h = (uint8_t)(y - time_i);
        } else if (n == "RAINBOW_MOVING_CHEVRON") {
            c.h = (uint8_t)(c.h + abs8((int8_t)(y - CY)) + (x - time_i));
        } else if (n == "CYCLE_OUT_IN") {
            c.h = (uint8_t)(3 * dist / 2 + time_dx);
        } else if (n == "CYCLE_OUT_IN_DUAL") {
            int16_t ddx = (int16_t)(CX / 2 - abs8((int8_t)dx));
            c.h = (uint8_t)(3 * sqrt16((uint16_t)(ddx * ddx + dy * dy)) + time_dx);
        } else if (n == "CYCLE_PINWHEEL") {
            c.h = (uint8_t)(atan2_8(dy, dx) + time_dx);
        } else if (n == "CYCLE_SPIRAL") {
            c.h = (uint8_t)(dist - time_dx - atan2_8(dy, dx));
        } else if (n == "DUAL_BEACON") {
            c.h = (uint8_t)(c.h + ((y - CY) * cos_v + (x - CX) * sin_v) / 128);
        } else if (n == "RAINBOW_BEACON") {
            c.h = (uint8_t)(c.h + ((y - CY) * 2 * cos_v + (x - CX) * 2 * sin_v) / 128);
        } else if (n == "RAINBOW_PINWHEELS") {
            c.h = (uint8_t)(c.h + ((y - CY) * 3 * cos_v + (56 - abs8((int8_t)(x - CX))) * 3 * sin_v) / 128);
        } else if (n == "FLOWER_BLOOMING") {
            uint8_t t = (uint8_t)scale16by8(timer, qadd8(speed / 10, 1));
            c.h = (uint8_t)(y > CY ? x * 3 - y * 3 + t : x * 3 - y * 3 - t);
            hsv_to_rgb(c, out);
            if (y > CY) std::swap(out[0], out[2]); // QMK draws the lower half blue-for-red
            continue;
        } else if (n == "RIVERFLOW") {
            uint8_t t = (uint8_t)scale16by8((uint16_t)(timer + i * 315), speed / 8);
            c.v = scale8((uint8_t)(abs8((int8_t)(sin8(t) - 128)) * 2), c.v);
        } else if (n == "HUE_BREATHING") {
            c.h = (uint8_t)(c.h + scale8((uint8_t)(abs8((int8_t)(sin8((uint8_t)(time_i / 2)) - 128)) * 2), 12));
        } else if (n == "HUE_PENDULUM") {
            c.h = (uint8_t)(c.h + scale8((uint8_t)(abs8((int8_t)(sin8(time_i) + x - 128)) * 2), 12));
        } else if (n == "HUE_WAVE") {
            c.h = (uint8_t)(c.h + scale8(abs8((int8_t)(x - time_i)), 24));
        } else if (drops) {
            std::memcpy(out, &g_drops[i * 3], 3);
            continue;
        } else if (n == "TYPING_HEATMAP") {
            uint8_t val = g_heat[i];
            Hsv hh = {(uint8_t)(170 - qsub8(val, 85)), base.s, scale8((uint8_t)((qadd8(170, val) - 170) * 3), base.v)};
            hsv_to_rgb(hh, out);
            continue;
        } else if (n == "SOLID_REACTIVE_SIMPLE" || n == "SOLID_REACTIVE") { // effect_runner_reactive
            uint16_t max_tick = (uint16_t)(65535 / qadd8(speed, 1)), tick = max_tick;
            for (int j = (int)g_hits.size() - 1; j >= 0; j--)
                if (g_hits[j].key == (int)i) { tick = (uint16_t)std::min<uint32_t>(ms - g_hits[j].ms, max_tick); break; }
            uint16_t offset = scale16by8(tick, qadd8(speed, 1));
            if (offset > 255) offset = 255;
            c.h = (uint8_t)scale16by8(timer, (uint8_t)(qadd8(speed, 8) >> 4));
            if (n == "SOLID_REACTIVE_SIMPLE") c.v = scale8((uint8_t)(255 - offset), c.v);
            else c.h = (uint8_t)(c.h + scale8((uint8_t)(255 - offset), 64));
        } else { // splash family: effect_runner_reactive_splash
            bool multi = n.find("MULTI") != std::string::npos;
            size_t start = multi ? 0 : (g_hits.empty() ? 0 : g_hits.size() - 1);
            c.v = 0;
            for (size_t j = start; j < g_hits.size(); j++) {
                const Led &k = g_leds[(size_t)g_hits[j].key];
                int16_t hx = (int16_t)(x - k.x), hy = (int16_t)(y - k.y);
                uint8_t d = sqrt16((uint16_t)std::min(hx * hx + hy * hy, 65535));
                uint16_t tick = scale16by8((uint16_t)std::min<uint32_t>(ms - g_hits[j].ms, 65535), qadd8(speed, 1));
                uint16_t e;
                if (n.find("WIDE") != std::string::npos) e = (uint16_t)(tick + d * 5);
                else if (n.find("CROSS") != std::string::npos) {
                    int ax = abs(hx) * 16 > 255 ? 255 : abs(hx) * 16, ay = abs(hy) * 16 > 255 ? 255 : abs(hy) * 16;
                    e = (uint16_t)(tick + d + (ax > ay ? ay : ax));
                } else {
                    e = (uint16_t)(tick - d);
                    if (n.find("NEXUS") != std::string::npos && (d > 72 || ((hx > 8 || hx < -8) && (hy > 8 || hy < -8)))) e = 255;
                }
                if (e > 255) e = 255;
                if (n.find("NEXUS") != std::string::npos) c.h = (uint8_t)(base.h + hy / 4);
                else if (n.find("WIDE") != std::string::npos || n.find("CROSS") != std::string::npos)
                    c.h = (uint8_t)scale16by8(timer, (uint8_t)(qadd8(speed, 8) >> 4));
                else if (n == "SPLASH" || n == "MULTISPLASH") c.h = (uint8_t)(c.h + e);
                c.v = qadd8(c.v, (uint8_t)(255 - e));
            }
            c.v = scale8(c.v, base.v);
        }
        hsv_to_rgb(c, out);
    }
    return true;
}

} // namespace sim
