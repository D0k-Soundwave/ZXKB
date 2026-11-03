// Live preview: the keyboard drawn with its keycaps (D0k's Q3 with KAM Wraith caps, or plain keys from the LED
// positions on other boards), lit with the colours the keyboard reports. Direct2D and DirectWrite resources live
// only while the preview exists.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "preview.h"
#include "app.h"
#include <d2d1.h>
#include <dwrite.h>
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
const BoardKey q3_keys[] = {
#include "board_q3.inc"
};
constexpr int    Q3_W = 1925, Q3_H = 625; // board size in hundredths of a key
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
D2D1_SIZE_U            g_caps_size = {};
IDWriteTextFormat     *g_fmt[5] = {};     // big char, word, sub, keyword, small
float                  g_fmt_unit = 0;    // the key size the formats were made for
HWND                   g_hwnd_preview = nullptr;
uint8_t                g_rgb[256 * 3];

template <class T> void release(T *&p) { if (p) { p->Release(); p = nullptr; } }

bool is_q3() {
    return g_board && g_board->led_count == 88 && g_board->product.find(L"q3") != std::wstring::npos;
}

D2D1_COLOR_F hex(unsigned v, float a = 1) {
    return D2D1::ColorF(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, a);
}

D2D1_COLOR_F led_colour(int led) {
    if (led < 0 || !g_board || led >= g_board->led_count) return D2D1::ColorF(0, 0, 0, 0);
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

std::vector<BoardKey> board_keys(bool *q3) { // the keys to draw for the connected keyboard
    std::vector<BoardKey> keys;
    *q3 = is_q3();
    if (*q3) keys.assign(q3_keys, q3_keys + sizeof(q3_keys) / sizeof(q3_keys[0]));
    else // keys at the LED positions the keyboard reported
        for (int i = 0; i < g_board->led_count && i * 3 + 1 < (int)g_board->leds.size(); i++)
            keys.push_back({(int)(g_board->leds[i * 3] / 224.f * 1400), (int)(g_board->leds[i * 3 + 1] / 64.f * 400), 100, 100, i, 0,
                            0x5F7690, 0x4A5E75, 0xF2F5F8, 0xFF5A4A, L"", L"", L""});
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
    bool q3 = false;
    std::vector<BoardKey> keys;
    float unit = 0, ox = 0, oy = 0;
    if (g_board) {
        keys = board_keys(&q3);
        float bw = q3 ? Q3_W / 100.f : 15.f, bh = q3 ? Q3_H / 100.f : 5.f; // other boards: 224 x 64 LED space as 15 x 5 keys
        unit = std::fmin((W - 8) / bw, (H - 8) / bh);
        ox = (W - bw * unit) / 2;
        oy = (H - bh * unit) / 2;
        if (g_caps_board != g_board || g_caps_size.width != size.width || g_caps_size.height != size.height) {
            drop_caps();
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
    }
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
        case WM_SIZE:
            if (g_rt) g_rt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp))); // the caps picture is redrawn for the new size
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_DESTROY:
            drop_caps();
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
    return g_hwnd_preview;
}

// Redraw only when a colour changed: a still effect then costs nothing
void preview_update() {
    if (!g_hwnd_preview || !g_board || !g_board->led_count) return;
    uint8_t now[sizeof(g_rgb)];
    int n = g_board->led_count; // at most 255, so it fits g_rgb
    keyboard_mirror(now, n);
    if (std::memcmp(now, g_rgb, n * 3) == 0) return;
    std::memcpy(g_rgb, now, n * 3);
    InvalidateRect(g_hwnd_preview, nullptr, FALSE);
}

double preview_aspect() {
    return is_q3() ? (double)Q3_H / Q3_W : 5.0 / 15.0;
}
