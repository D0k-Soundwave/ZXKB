// Live preview: the keyboard drawn from its published layout, with the chosen keycap set's legends and colours or
// labels from what each key does in the Windows keyboard language, lit with the colours the keyboard reports (ZXKB
// firmware). Without a layout, plain keys at the LED positions the keyboard reports. Direct2D and DirectWrite resources live
// only while the preview exists.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "preview.h"
#include "app.h"
#include "labels.h"
#include "keycaps.h"
#include "sim.h"
#include "settings.h"
#include <d2d1.h>
#include <dwrite.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <vector>

namespace {

struct BoardKey {
    int            x, y, w, h;   // hundredths of a key
    int            led;          // -1 = no LED
    int            shape;        // 0 key, 1 ISO Enter, 2 knob
    unsigned       top, side, main_rgb, sub_rgb;
    const wchar_t *main, *sub, *keyword;
};
constexpr float  GAP  = 0.06f;            // [A] gap between caps, in keys, where the light shows
constexpr float  GAMMA = 1 / 2.2f;        // [A] LED brightness is linear; a screen needs gamma to look alike

// Direct2D and DirectWrite are loaded when the preview opens and unloaded when it closes, so nothing of them
// stays in ZXKB afterwards (M 2026-11-02: linked normally, 3.4 MB of their data stayed after closing).
HMODULE                g_d2d_dll = nullptr, g_dw_dll = nullptr;
ID2D1Factory          *g_d2d = nullptr;
IDWriteFactory        *g_dw = nullptr;
ID2D1HwndRenderTarget *g_rt = nullptr;
ID2D1RenderTarget     *g_cur = nullptr;   // where the drawing helpers draw: the window, or the cached caps
ID2D1SolidColorBrush  *g_brush = nullptr;  // one brush, recoloured for each shape
// The caps and legends do not change while the window is open, so they are drawn once into this picture (clear
// in the gaps); each frame only paints the lights and puts the picture over them. Drawing all the legends every
// frame cost 36% of one core (M 2026-11-03, 30 frames/s, software drawing).
ID2D1BitmapRenderTarget *g_caps_rt = nullptr;
ID2D1Bitmap           *g_caps = nullptr;
const Board           *g_caps_board = nullptr; // the keyboard and size the picture was made for
int                    g_labels_stamp = 0, g_caps_stamp = -1; // labels changed since the picture was made
struct KeyText { std::wstring main, sub, keyword; };
std::vector<struct BoardKey> g_keys_cache;    // the keys as last laid out (rebuilt with the caps picture)
std::vector<bool>      g_sel;                 // selected keys (index into g_keys_cache)
float                  g_ox = 0, g_oy = 0, g_unit = 0; // where the keys were drawn
std::vector<KeyText>   g_text;                // the legends the drawn keys point at
std::vector<KeyRect>   g_set;                 // the chosen keycap set (empty: labels from the keyboard language)
std::wstring           g_set_name;
D2D1_SIZE_U            g_caps_size = {};
IDWriteTextFormat     *g_fmt[5] = {};     // big char, word, sub, keyword, small
float                  g_fmt_unit = 0;    // the key size the formats were made for
HWND                   g_hwnd_preview = nullptr;
uint8_t                g_rgb[256 * 3];

template <class T> void release(T *&p) { if (p) { p->Release(); p = nullptr; } }


D2D1_COLOR_F hex(unsigned v, float a = 1) {
    return D2D1::ColorF(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, a);
}

int g_lit = 0; // how many LED colours g_rgb holds: the keyboard's LEDs, or (simulated) one per key
bool g_sim = false; // the preview is simulated (standard firmware: the keyboard cannot report its LEDs)
constexpr UINT TIMER_SIM = 7;
constexpr UINT SIM_FRAME_MS = 33; // [C] about 30 frames a second, as the LED mirror (zxkb.h)

D2D1_COLOR_F led_colour(int led) {
    if (led < 0 || !g_board || led >= g_lit) return D2D1::ColorF(0, 0, 0, 0);
    float r = g_rgb[led * 3] / 255.f, g = g_rgb[led * 3 + 1] / 255.f, b = g_rgb[led * 3 + 2] / 255.f;
    return D2D1::ColorF(std::pow(r, GAMMA), std::pow(g, GAMMA), std::pow(b, GAMMA), 1);
}

void make_formats(float unit) {
    for (auto &f : g_fmt) release(f);
    const float sizes[5] = {unit * 0.30f, unit * 0.16f, unit * 0.15f, unit * 0.13f, unit * 0.11f};
    const DWRITE_FONT_WEIGHT weights[5] = {DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_WEIGHT_BOLD,
                                           DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_WEIGHT_SEMI_BOLD};
    for (int i = 0; i < 5; i++) {
        g_dw->CreateTextFormat(L"Segoe UI", nullptr, weights[i], DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sizes[i], L"en-gb", &g_fmt[i]);
        g_fmt[i]->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        g_fmt[i]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
    g_fmt_unit = unit;
}

void text(const wchar_t *s, IDWriteTextFormat *f, DWRITE_TEXT_ALIGNMENT align, D2D1_RECT_F r, unsigned rgb) {
    if (!s || !*s) return;
    g_brush->SetColor(hex(rgb));
    f->SetTextAlignment(align);
    g_cur->DrawText(s, (UINT32)wcslen(s), f, r, g_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void fill(const D2D1_ROUNDED_RECT &r, D2D1_COLOR_F c) {
    g_brush->SetColor(c);
    g_cur->FillRoundedRectangle(r, g_brush);
}

// A key's light: it shows in the gap round the cap. Plain rectangles: the cap covers everything but the gap.
void draw_light(const BoardKey &k, float ox, float oy, float unit) {
    D2D1_COLOR_F light = led_colour(k.led);
    if (light.a <= 0 || k.shape == 2) return;
    float x = ox + k.x / 100.f * unit, y = oy + k.y / 100.f * unit, w = k.w / 100.f * unit, h = k.h / 100.f * unit;
    g_brush->SetColor(light);
    if (k.shape == 1) { // ISO Enter: 1.5 keys wide on the top row, 1.25 below
        g_cur->FillRectangle(D2D1::RectF(x, y, x + w, y + unit), g_brush);
        g_cur->FillRectangle(D2D1::RectF(x + 0.25f * unit, y + unit, x + w, y + h), g_brush);
    } else {
        g_cur->FillRectangle(D2D1::RectF(x, y, x + w, y + h), g_brush);
    }
}

// A key's cap (side, top) and legends
void draw_cap(const BoardKey &k, float ox, float oy, float unit) {
    float x = ox + k.x / 100.f * unit, y = oy + k.y / 100.f * unit, w = k.w / 100.f * unit, h = k.h / 100.f * unit;
    float gap = GAP * unit, rad = unit * 0.08f;
    if (k.shape == 2) { // the knob
        g_brush->SetColor(hex(0x5A6068));
        D2D1_ELLIPSE e = D2D1::Ellipse(D2D1::Point2F(x + w / 2, y + h / 2), w * 0.42f, h * 0.42f);
        g_cur->DrawEllipse(e, g_brush, unit * 0.04f);
        text(k.main, g_fmt[4], DWRITE_TEXT_ALIGNMENT_CENTER, D2D1::RectF(x, y + h / 2 - unit * 0.08f, x + w, y + h), k.main_rgb);
        return;
    }
    auto shape = [&](float in, D2D1_COLOR_F c) {
        if (k.shape == 1) { // ISO Enter: 1.5 keys wide on the top row, 1.25 below
            fill(D2D1::RoundedRect(D2D1::RectF(x + in, y + in, x + w - in, y + unit - in + rad), rad, rad), c);
            fill(D2D1::RoundedRect(D2D1::RectF(x + 0.25f * unit + in, y + in, x + w - in, y + h - in), rad, rad), c);
        } else {
            fill(D2D1::RoundedRect(D2D1::RectF(x + in, y + in, x + w - in, y + h - in), rad, rad), c);
        }
    };
    shape(gap, hex(k.side));                                           // cap side
    float lip = unit * 0.07f;                                          // cap top, inset from the side
    if (k.shape == 1) {
        fill(D2D1::RoundedRect(D2D1::RectF(x + gap + lip, y + gap + lip * 0.5f, x + w - gap - lip, y + unit - gap), rad, rad), hex(k.top));
        fill(D2D1::RoundedRect(D2D1::RectF(x + 0.25f * unit + gap + lip, y + gap + lip * 0.5f, x + w - gap - lip, y + h - gap - lip * 1.5f), rad, rad), hex(k.top));
    } else {
        fill(D2D1::RoundedRect(D2D1::RectF(x + gap + lip, y + gap + lip * 0.5f, x + w - gap - lip, y + h - gap - lip * 1.5f), rad, rad), hex(k.top));
    }

    // legends, as on the Wraith caps: a single character big at top left (shifted symbol red at top right),
    // a word centred (second legend below it); the BASIC keyword small at the bottom
    float l = x + gap + lip * 1.4f, r = x + w - gap - lip * 1.4f, t = y + gap + lip * 0.6f, b = y + h - gap - lip * 1.6f;
    if (k.shape == 1) { l = x + 0.25f * unit + gap + lip; t = y + h / 2 - unit * 0.2f; }
    bool single = wcslen(k.main) == 1;
    if (single) {
        text(k.main, g_fmt[0], DWRITE_TEXT_ALIGNMENT_LEADING, D2D1::RectF(l, t - unit * 0.05f, r, b), k.main_rgb);
        text(k.sub, g_fmt[2], DWRITE_TEXT_ALIGNMENT_TRAILING, D2D1::RectF(l, t + unit * 0.05f, r, b), k.sub_rgb);
    } else {
        text(k.main, g_fmt[1], DWRITE_TEXT_ALIGNMENT_CENTER, D2D1::RectF(l - lip, t + unit * 0.04f, r + lip, b), k.main_rgb);
        const wchar_t *second = *k.sub ? k.sub : k.keyword;
        unsigned       second_rgb = *k.sub ? k.sub_rgb : 0xF2F5F8;
        text(second, g_fmt[3], DWRITE_TEXT_ALIGNMENT_CENTER, D2D1::RectF(l - lip, t + unit * 0.24f, r + lip, b), second_rgb);
    }
    if (single && *k.keyword)
        text(k.keyword, g_fmt[4], DWRITE_TEXT_ALIGNMENT_CENTER, D2D1::RectF(l - lip, b - unit * 0.17f, r + lip, b + lip), 0xF2F5F8);
}

// Size of the drawing in keys: the keyboard's own layout, or its LED space (224 x 64) as 15 x 5 keys
void board_size(float *bw, float *bh) {
    *bw = 15.f;
    *bh = 5.f;
    if (!g_board || g_board->keys.empty()) return;
    int w, h;
    kle::bounds(g_board->keys, &w, &h);
    if (w > 0 && h > 0) { *bw = w / 100.f; *bh = h / 100.f; }
}

unsigned darker(unsigned c) { // a cap's side when the set gives only its top colour
    return ((c >> 16 & 255) * 4 / 5) << 16 | ((c >> 8 & 255) * 4 / 5) << 8 | (c & 255) * 4 / 5;
}

std::vector<BoardKey> board_keys() { // the keys to draw for the connected keyboard
    std::vector<BoardKey> keys;
    if (g_board->keys.empty()) { // no layout: keys at the LED positions the keyboard reported
        for (int i = 0; i < g_board->led_count && i * 3 + 1 < (int)g_board->leds.size(); i++)
            keys.push_back({(int)(g_board->leds[i * 3] / 224.f * 1400), (int)(g_board->leds[i * 3 + 1] / 64.f * 400), 100, 100, i, 0,
                            0x5F7690, 0x4A5E75, 0xF2F5F8, 0xFF5A4A, L"", L"", L""});
        return keys;
    }
    // the chosen keycap set, if any
    // the choice: "" labels in the layout Windows is using, "layout:<HKL hex>" labels in that layout, or a keycap set
    std::wstring want = keycaps::chosen(g_board->vid, g_board->pid);
    HKL hkl = labels::current_layout();
    if (want.rfind(L"layout:", 0) == 0) {
        HKL h = (HKL)(UINT_PTR)wcstoull(want.c_str() + 7, nullptr, 16);
        for (HKL i : labels::installed())
            if (i == h) hkl = h; // only if still installed
        want.clear();
    }
    if (want != g_set_name) {
        g_set.clear();
        if (!want.empty()) keycaps::load(want, &g_set);
        g_set_name = want;
    }
    const std::vector<KeyRect> &bk = g_board->keys;
    g_text.assign(bk.size(), KeyText());
    for (size_t i = 0; i < bk.size(); i++) {
        const KeyRect &r = bk[i];
        unsigned top = 0x5F7690, side = 0x4A5E75, main_rgb = 0xF2F5F8, sub_rgb = 0xC8D0D8;
        KeyText &t = g_text[i];
        bool knob = r.knob;
        const KeyRect *cap = g_set.empty() ? nullptr : keycaps::match(g_set, r, bk);
        if (cap) { // legends as the set has them: big character at label 0 (shifted at 2), or a word at 4; 7 below
            const std::vector<std::string> &l = cap->labels;
            if (!l[0].empty()) {
                t.main = settings::widen(l[0]);
                int si = !l[2].empty() ? 2 : 1;
                t.sub = settings::widen(l[si]);
                t.keyword = settings::widen(l[7]);
                if (cap->text_rgb[0] >= 0) main_rgb = cap->text_rgb[0];
                if (cap->text_rgb[si] >= 0) sub_rgb = cap->text_rgb[si];
            } else {
                t.main = settings::widen(l[4]);
                t.sub = settings::widen(l[7]);
                if (cap->text_rgb[4] >= 0) main_rgb = cap->text_rgb[4];
                if (cap->text_rgb[7] >= 0) sub_rgb = cap->text_rgb[7];
            }
            if (cap->cap_rgb >= 0) { top = cap->cap_rgb; side = cap->side_rgb >= 0 ? cap->side_rgb : darker(top); }
            knob |= cap->knob;
        } else { // labels from what the key does, in the keyboard language in use
            size_t at = (size_t)(r.row * g_board->cols + r.col);
            if (r.row >= 0 && r.col >= 0 && at < g_board->keymap.size()) {
                labels::Label lb = labels::for_keycode(g_board->keymap[at], hkl, g_board->custom_names);
                t.main = lb.main;
                t.sub = lb.sub;
            }
        }
        // the LED under the key (ZXKB firmware reports which), so the key lights as on the keyboard; simulated: the key
        int led = g_sim ? (int)i : -1;
        size_t m = (size_t)(r.row * g_board->map_cols + r.col);
        if (r.row >= 0 && r.col >= 0 && m < g_board->led_map.size() && g_board->led_map[m] != 255) led = g_board->led_map[m];
        BoardKey k = {r.x, r.y, r.w, r.h, led, knob ? 2 : 0, top, side, main_rgb, sub_rgb, t.main.c_str(), t.sub.c_str(), t.keyword.c_str()};
        if (r.w2) { // ISO Enter: drawn as the usual two-part shape over both rectangles
            int x0 = std::min(r.x, r.x2), x1 = std::max(r.x + r.w, r.x2 + r.w2);
            int y0 = std::min(r.y, r.y2), y1 = std::max(r.y + r.h, r.y2 + r.h2);
            k.x = x0; k.y = y0; k.w = x1 - x0; k.h = y1 - y0; k.shape = 1;
        }
        keys.push_back(k);
    }
    return keys;
}

void drop_caps() {
    release(g_caps);
    release(g_caps_rt);
    g_caps_board = nullptr;
}

void render() {
    if (!g_d2d || !g_dw) return; // the drawing libraries could not be loaded
    RECT rc;
    GetClientRect(g_hwnd_preview, &rc);
    // [M] 2026-11-02, window open: software drawing 9.2 MB own RAM / 15.9 MB reserved; graphics card
    // 15.3 MB / 62.2 MB (it loads the graphics driver into ZXKB). CPU while animating: to be measured.
    if (!g_rt) {
        if (FAILED(g_d2d->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                                                     D2D1::PixelFormat(), 96.f, 96.f),
                                                 D2D1::HwndRenderTargetProperties(g_hwnd_preview, D2D1::SizeU(rc.right, rc.bottom)), &g_rt)))
            return;
        g_rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &g_brush);
    }
    float W = (float)rc.right, H = (float)rc.bottom;
    D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
    std::vector<BoardKey> &keys = g_keys_cache;
    float unit = 0, ox = 0, oy = 0;
    if (!g_board) keys.clear();
    if (g_board) {
        float bw, bh;
        board_size(&bw, &bh);
        unit = std::fmin((W - 8) / bw, (H - 8) / bh);
        ox = (W - bw * unit) / 2;
        oy = (H - bh * unit) / 2;
        if (g_caps_board != g_board || g_caps_size.width != size.width || g_caps_size.height != size.height || g_caps_stamp != g_labels_stamp) {
            drop_caps();
            // the key list (with labels, keycaps and LEDs) only changes with the board, size or labels, not per frame
            size_t before = keys.size();
            g_sim = g_board->kind == Board::VIA && !g_board->keys.empty();
            if (g_sim) sim::set_keys(g_board->keys, g_board->keymap, g_board->cols);
            // frames only for a simulated preview; a ZXKB keyboard sends its own (the LED mirror)
            if (g_sim) SetTimer(g_hwnd_preview, TIMER_SIM, SIM_FRAME_MS, nullptr);
            else KillTimer(g_hwnd_preview, TIMER_SIM);
            keys = board_keys();
            if (keys.size() != before) g_sel.assign(keys.size(), false);
            if (!g_fmt[0] || std::fabs(unit - g_fmt_unit) > 0.5f) make_formats(unit);
            if (SUCCEEDED(g_rt->CreateCompatibleRenderTarget(D2D1::SizeF(W, H), &g_caps_rt))) {
                g_cur = g_caps_rt;
                g_caps_rt->BeginDraw();
                g_caps_rt->Clear(D2D1::ColorF(0, 0, 0, 0));
                for (const BoardKey &k : keys) draw_cap(k, ox, oy, unit);
                g_caps_rt->EndDraw();
                g_caps_rt->GetBitmap(&g_caps);
                g_caps_board = g_board;
                g_caps_size  = size;
                g_caps_stamp = g_labels_stamp;
                for (auto &f : g_fmt) release(f); // the text is in the picture now
            }
        }
    }
    g_cur = g_rt;
    g_rt->BeginDraw();
    g_rt->Clear(hex(0x0B0D10));
    if (g_board && g_caps) {
        g_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        for (const BoardKey &k : keys) draw_light(k, ox, oy, unit);
        g_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        g_rt->DrawBitmap(g_caps, D2D1::RectF(0, 0, W, H), 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        // selected keys: a white outline with a dark edge, readable over any light
        for (size_t i = 0; i < keys.size() && i < g_sel.size(); i++) {
            if (!g_sel[i]) continue;
            const BoardKey &k = keys[i];
            // on the cap, inside the gap where the key's light shows, so the light stays visible (D0k, 2026-11-03)
            float in = GAP * unit + 3.f;
            D2D1_ROUNDED_RECT r = D2D1::RoundedRect(D2D1::RectF(ox + k.x / 100.f * unit + in, oy + k.y / 100.f * unit + in,
                                                                ox + (k.x + k.w) / 100.f * unit - in, oy + (k.y + k.h) / 100.f * unit - in),
                                                    unit * 0.08f, unit * 0.08f);
            g_brush->SetColor(hex(0x000000));
            g_rt->DrawRoundedRectangle(r, g_brush, 4.f);
            g_brush->SetColor(hex(0xFFFFFF));
            g_rt->DrawRoundedRectangle(r, g_brush, 2.f);
        }
    }
    g_ox = ox; g_oy = oy; g_unit = unit;
    if (g_rt->EndDraw() == D2DERR_RECREATE_TARGET) {
        drop_caps();
        release(g_brush);
        release(g_rt);
        for (auto &f : g_fmt) release(f);
    }
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            render();
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER: // simulated preview: the next frame of the effect
            if (wp == TIMER_SIM) preview_simulate();
            return 0;
        case WM_INPUT: { // key presses for the simulated typing effects (only while the preview exists)
            RAWINPUT ri;
            UINT size = sizeof(ri);
            if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
                ri.header.dwType == RIM_TYPEKEYBOARD && !(ri.data.keyboard.Flags & RI_KEY_BREAK))
                sim::key_hit((uint16_t)(ri.data.keyboard.MakeCode | (ri.data.keyboard.Flags & RI_KEY_E0 ? 0xE000 : 0)));
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_LBUTTONDOWN: { // select the key under the pointer (Ctrl: add or remove it)
            float x = (float)(short)LOWORD(lp), y = (float)(short)HIWORD(lp);
            int hit = -1;
            for (size_t i = 0; i < g_keys_cache.size() && g_unit > 0; i++) {
                const BoardKey &k = g_keys_cache[i];
                if (x >= g_ox + k.x / 100.f * g_unit && x < g_ox + (k.x + k.w) / 100.f * g_unit &&
                    y >= g_oy + k.y / 100.f * g_unit && y < g_oy + (k.y + k.h) / 100.f * g_unit) hit = (int)i;
            }
            if (g_sel.size() != g_keys_cache.size()) g_sel.assign(g_keys_cache.size(), false);
            if (wp & MK_CONTROL) {
                if (hit >= 0) g_sel[hit] = !g_sel[hit];
            } else {
                g_sel.assign(g_keys_cache.size(), false);
                if (hit >= 0) g_sel[hit] = true;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), PREVIEW_SELECTION), (LPARAM)hwnd);
            return 0;
        }
        case WM_SIZE:
            if (g_rt) g_rt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp))); // the caps picture is redrawn for the new size
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_DESTROY: {
            RAWINPUTDEVICE rid = {0x01, 0x06, RIDEV_REMOVE, nullptr};
            RegisterRawInputDevices(&rid, 1, sizeof(rid));
            KillTimer(hwnd, TIMER_SIM);
        }
            drop_caps();
            g_keys_cache.clear();
            g_sel.clear();
            g_text.clear();
            g_set.clear();
            g_set_name.clear();
            for (auto &f : g_fmt) release(f);
            release(g_brush);
            release(g_rt);
            release(g_dw);
            release(g_d2d);
            if (g_dw_dll) { FreeLibrary(g_dw_dll); g_dw_dll = nullptr; }
            if (g_d2d_dll) { FreeLibrary(g_d2d_dll); g_d2d_dll = nullptr; }
            g_hwnd_preview = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

HWND preview_create(HWND parent, int id) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc     = {};
        wc.lpfnWndProc   = proc;
        wc.hInstance     = g_inst;
        wc.lpszClassName = L"ZXKBPreview";
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }
    typedef HRESULT(WINAPI * d2d_create)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS *, void **);
    typedef HRESULT(WINAPI * dw_create)(DWRITE_FACTORY_TYPE, REFIID, IUnknown **);
    g_d2d_dll = LoadLibraryW(L"d2d1.dll");
    g_dw_dll  = LoadLibraryW(L"dwrite.dll");
    auto mk_d2d = g_d2d_dll ? (d2d_create)(void *)GetProcAddress(g_d2d_dll, "D2D1CreateFactory") : nullptr;
    auto mk_dw  = g_dw_dll ? (dw_create)(void *)GetProcAddress(g_dw_dll, "DWriteCreateFactory") : nullptr;
    if (mk_d2d) mk_d2d(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr, (void **)&g_d2d);
    if (mk_dw) mk_dw(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown **>(&g_dw));
    g_hwnd_preview = CreateWindowExW(0, L"ZXKBPreview", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, g_inst, nullptr);
    // key presses for simulated typing effects, from any keyboard, only while the preview is open (no hook)
    RAWINPUTDEVICE rid = {0x01, 0x06, RIDEV_INPUTSINK, g_hwnd_preview};
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
    return g_hwnd_preview;
}

// Simulated preview: the effect worked out for this moment; nothing to do (and no redraw) when it is not simulated
void preview_simulate() {
    if (!g_hwnd_preview || !g_board || g_board->kind != Board::VIA || g_board->keys.empty()) return;
    std::string fx = g_st.enabled && g_st.mode >= 1 && g_st.mode <= (int)g_board->names.size() ? g_board->names[g_st.mode - 1] : "";
    size_t n = std::min<size_t>(g_board->keys.size(), 256);
    uint8_t now[sizeof(g_rgb)] = {0};
    if (g_pc_effect != PCFX_NONE || !sim::known(fx) || !sim::render(fx, g_st.hue, g_st.sat, g_st.val, g_st.speed, GetTickCount(), now))
        std::memset(now, 0, n * 3); // lighting off, a ZXKB PC effect, or an effect the simulator does not have: unlit
    g_lit = (int)n;
    if (std::memcmp(now, g_rgb, n * 3) == 0) return;
    std::memcpy(g_rgb, now, n * 3);
    InvalidateRect(g_hwnd_preview, nullptr, FALSE);
}

// Redraw only when a colour changed: a still effect then costs nothing
void preview_update() {
    if (!g_hwnd_preview || !g_board || !g_board->led_count) return;
    uint8_t now[sizeof(g_rgb)];
    int n = g_board->led_count; // at most 255, so it fits g_rgb
    keyboard_mirror(now, n);
    g_lit = n;
    if (std::memcmp(now, g_rgb, n * 3) == 0) return;
    std::memcpy(g_rgb, now, n * 3);
    InvalidateRect(g_hwnd_preview, nullptr, FALSE);
}

std::vector<int> preview_selected_leds() {
    std::vector<int> leds;
    for (size_t i = 0; i < g_sel.size() && i < g_keys_cache.size(); i++)
        if (g_sel[i] && g_keys_cache[i].led >= 0) leds.push_back(g_keys_cache[i].led);
    return leds;
}
int preview_selected_count() {
    int n = 0;
    for (bool s : g_sel) n += s;
    return n;
}
void preview_select_all() {
    g_sel.assign(g_keys_cache.size(), true);
    if (g_hwnd_preview) InvalidateRect(g_hwnd_preview, nullptr, FALSE);
}
void preview_select_none() {
    g_sel.assign(g_keys_cache.size(), false);
    if (g_hwnd_preview) InvalidateRect(g_hwnd_preview, nullptr, FALSE);
}

void preview_relabel() {
    g_labels_stamp++;
    if (g_hwnd_preview) InvalidateRect(g_hwnd_preview, nullptr, FALSE);
}

double preview_aspect() {
    float bw, bh;
    board_size(&bw, &bh);
    return bh / bw;
}
