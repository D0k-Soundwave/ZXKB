// The ZXKB window: effects and their settings in one place, next to the tray. Standard Windows controls only;
// created when opened and destroyed (with its font and brushes) when closed.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include "app.h"
#include "effects.h"
#include "vumath.h"
#include "preview.h"
#include "flash.h"
#include "caps.h"
#include "boards.h"
#include "keycaps.h"
#include "protocol.h"
#include "labels.h"
#include "../res/resource.h"
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string>

#define ZXKB_W2(s) L##s
#define ZXKB_W(s)  ZXKB_W2(s)

namespace {

enum : int {
    IDC_NOW = 100, IDC_LIST, IDC_ON, IDC_FAV, IDC_VERSION, IDC_PICK, IDC_PREVIEW, IDC_FIRMWARE, IDC_LOOKUP, IDC_KEYCAPS,
    IDC_KEYS_INFO, IDC_KEYS_ALL, IDC_KEYS_NONE, IDC_KEYS_EFFECT, IDC_KEYS_OFF, IDC_KEYS_COLOUR,
    IDC_CAPS_BTN, IDC_KEY_FX_LABEL, IDC_KEY_FX, IDC_KEY_SPEED_LABEL, IDC_KEY_SPEED, IDC_KEY_COLOURS_LABEL, IDC_KEY_C1, IDC_KEY_C2,
    IDC_SLIDER = 200, // + slider index
    IDC_VALUE  = 220, // + slider index
    IDC_LABEL  = 240, // + slider index
    IDC_SWATCH = 300, // + colour index
};
enum Slider { BRIGHTNESS, SPEED, SIZE, INTENSITY, SLIDERS };
// The seven Spectrum colours, then black (BLACK): for keys only, as an effect's colour has no black (that is brightness 0)
constexpr int BLACK = 7, SWATCHES = 8;
const wchar_t *slider_names[SLIDERS] = {L"Brightness", L"Speed", L"Size", L"Intensity"};
constexpr int VU_ITEM = 1000; // list item data: effect number, or VU_ITEM + VuStyle
constexpr int VU_GROUP = 100; // list group for the VU styles

// Layout in device-independent pixels (scaled for the screen's DPI). [A] sizes chosen to fit the controls:
// the preview across the top, the effect list on the left, the controls on the right.
constexpr int W = 680, MARGIN = 12, ROW = 30, LIST_W = 300, LIST_H = 226, SWATCH = 24, GUTTER = 16, KEYS_ROW = 70; // two rows: selection, then the keys' effect
constexpr UINT TIMER_MIRROR = 1;
constexpr UINT WM_LOOKED_UP = WM_APP + 120; // wParam 1 = found; lParam: error text (new std::wstring) or 0
void look_up();
void fill_keycaps();
void refresh_keys();
bool keys_mode() { return per_key_possible() && preview_selected_count() > 0; } // the colours go to the selected keys
int  g_key_target = 1; // which colour of the selected keys the colour boxes set: 1 first, 2 second
void key_colour(uint8_t r, uint8_t g, uint8_t b) { // a colour picked for the selected keys
    edit_keys(preview_selected_leds(), g_key_target == 2 ? KEY_SECOND : KEY_FIRST, 0, r, g, b);
}
const wchar_t *const key_fx_names[5] = {L"Steady", L"Flash", L"Breathe", L"Colour cycle", L"Twinkle"};
constexpr UINT MIRROR_KEEPALIVE_MS = 1000; // [C] half the keyboard's 2 s keep-alive, so the colours never stop

HWND  g_win = nullptr;
HFONT g_font = nullptr;
int   g_dpi = 96;
bool  g_updating = false; // true while the window sets its own controls (so it does not act on them)
Board *g_listed = nullptr; // the keyboard whose effects are in the list

int  px(int dip) { return MulDiv(dip, g_dpi, 96); }
HWND item(int id) { return GetDlgItem(g_win, id); }

void set_font() {
    if (g_font) DeleteObject(g_font);
    g_font = CreateFontW(-MulDiv(9, g_dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    for (HWND c = GetWindow(g_win, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
}

int preview_h() { return (int)((W - 2 * MARGIN) * preview_aspect()); }
int content_height() { return MARGIN + 26 + preview_h() + 6 + KEYS_ROW + LIST_H + 10 + 26 + MARGIN; }

void layout() {
    int y = MARGIN, inner = W - 2 * MARGIN;
    MoveWindow(item(IDC_NOW), px(MARGIN), px(y), px(inner - 230), px(20), FALSE);
    MoveWindow(item(IDC_LOOKUP), px(W - MARGIN - 220), px(y - 3), px(220), px(24), FALSE);
    MoveWindow(item(IDC_KEYCAPS), px(W - MARGIN - 260), px(y - 3), px(260), px(200), FALSE);
    y += 26;
    MoveWindow(item(IDC_PREVIEW), px(MARGIN), px(y), px(inner), px(preview_h()), FALSE);
    y += preview_h() + 6;
    { // per-key colours: what is selected, then the buttons, right to left
        const int bw[4] = {62, 96, 50, 80}; // Off, Follow effect, Clear, Select all
        const int ids[4] = {IDC_KEYS_OFF, IDC_KEYS_EFFECT, IDC_KEYS_NONE, IDC_KEYS_ALL};
        int bx = W - MARGIN;
        for (int i = 0; i < 4; i++) {
            bx -= bw[i];
            MoveWindow(item(ids[i]), px(bx), px(y), px(bw[i]), px(26), FALSE);
            bx -= i == 1 ? 14 : 6; // a wider gap between the colour buttons and the selection buttons
        }
        MoveWindow(item(IDC_KEYS_INFO), px(MARGIN), px(y + 4), px(bx - MARGIN), px(20), FALSE);
        // second row: the selected keys' effect, its speed, and which of their two colours the colour boxes set
        int r2 = y + 34;
        MoveWindow(item(IDC_KEY_FX_LABEL), px(MARGIN), px(r2 + 4), px(70), px(20), FALSE);
        MoveWindow(item(IDC_KEY_FX), px(MARGIN + 72), px(r2), px(150), px(200), FALSE);
        MoveWindow(item(IDC_KEY_COLOURS_LABEL), px(W - MARGIN - 196), px(r2 + 4), px(110), px(20), FALSE);
        MoveWindow(item(IDC_KEY_C1), px(W - MARGIN - 82), px(r2), px(38), px(26), FALSE);
        MoveWindow(item(IDC_KEY_C2), px(W - MARGIN - 38), px(r2), px(38), px(26), FALSE);
    }
    y += KEYS_ROW;
    int top = y;
    HWND list = item(IDC_LIST);
    MoveWindow(list, px(MARGIN), px(y), px(LIST_W), px(LIST_H), FALSE);
    ListView_SetColumnWidth(list, 0, px(LIST_W) - GetSystemMetricsForDpi(SM_CXVSCROLL, g_dpi) - px(4));

    int x = MARGIN + LIST_W + GUTTER, cw = W - MARGIN - x; // the controls column
    MoveWindow(item(IDC_ON), px(x), px(y), px(140), px(24), FALSE);
    MoveWindow(item(IDC_FAV), px(x + 150), px(y), px(cw - 150), px(24), FALSE);
    y += 32;
    for (int s = 0; s < SLIDERS; s++) {
        MoveWindow(item(IDC_LABEL + s), px(x), px(y + 5), px(72), px(20), FALSE);
        MoveWindow(item(IDC_SLIDER + s), px(x + 72), px(y), px(cw - 72 - 46), px(26), FALSE);
        MoveWindow(item(IDC_VALUE + s), px(W - MARGIN - 44), px(y + 5), px(44), px(20), FALSE);
        y += ROW;
    }
    y += 8;
    for (int c = 0; c < SWATCHES; c++) MoveWindow(item(IDC_SWATCH + c), px(x + c * (SWATCH + 6)), px(y), px(SWATCH), px(SWATCH), FALSE);
    MoveWindow(item(IDC_PICK), px(x + SWATCHES * (SWATCH + 6) + 4), px(y), px(cw - SWATCHES * (SWATCH + 6) - 4), px(SWATCH), FALSE);
    MoveWindow(item(IDC_VERSION), px(MARGIN), px(top + LIST_H + 14), px(inner - 260), px(18), FALSE);
    MoveWindow(item(IDC_FIRMWARE), px(W - MARGIN - 120), px(top + LIST_H + 10), px(120), px(26), FALSE);
    MoveWindow(item(IDC_CAPS_BTN), px(W - MARGIN - 250), px(top + LIST_H + 10), px(124), px(26), FALSE);
    InvalidateRect(g_win, nullptr, TRUE);
}

void add_group(HWND list, int id, const wchar_t *name) {
    LVGROUP g = {sizeof(g)};
    g.mask     = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
    g.pszHeader = const_cast<wchar_t *>(name);
    g.iGroupId = id;
    g.stateMask = g.state = LVGS_COLLAPSIBLE;
    ListView_InsertGroup(list, -1, &g);
}

void add_item(HWND list, int group, int data, const std::wstring &text) {
    LVITEMW it = {};
    it.mask     = LVIF_TEXT | LVIF_PARAM | LVIF_GROUPID;
    it.iItem    = ListView_GetItemCount(list);
    it.pszText  = const_cast<wchar_t *>(text.c_str());
    it.lParam   = data;
    it.iGroupId = group;
    ListView_InsertItem(list, &it);
}

// Fill the effect list from what the keyboard reported (rebuilt when a keyboard connects)
void fill_list() {
    HWND list = item(IDC_LIST);
    ListView_DeleteAllItems(list);
    ListView_RemoveAllGroups(list);
    g_listed = g_board;
    if (!g_board) return;
    add_group(list, effects::SPECTRUM, effects::group_names[effects::SPECTRUM]);
    if (vu_mode()) add_group(list, VU_GROUP, L"VU meter");
    for (int gr = effects::SOLID; gr < effects::HIDDEN; gr++) add_group(list, gr, effects::group_names[gr]);
    const int order[3] = {VU_MUSIC, VU_BANDS, VU_ABC};
    if (vu_mode())
        for (int s : order) add_item(list, VU_GROUP, VU_ITEM + s, vu_label(s));
    for (size_t i = 0; i < g_board->names.size(); i++) {
        effects::Group gr = effects::group(g_board->names[i], g_board->kinds[i]);
        if (gr == effects::HIDDEN || replaced(i)) continue;
        add_item(list, gr, (int)i + 1, effects::friendly(g_board->names[i]));
    }
}

void set_slider(int s, int lo, int hi, int pos, bool enabled, const std::wstring &value) {
    HWND tb = item(IDC_SLIDER + s);
    if (GetCapture() != tb) { // never move a slider under the user's mouse
        SendMessageW(tb, TBM_SETRANGEMIN, FALSE, lo);
        SendMessageW(tb, TBM_SETRANGEMAX, FALSE, hi);
        SendMessageW(tb, TBM_SETPOS, TRUE, pos);
        // every value reachable: arrow keys move 1, a click on the bar 1/32 of the range (default 1/5)
        SendMessageW(tb, TBM_SETLINESIZE, 0, 1);
        SendMessageW(tb, TBM_SETPAGESIZE, 0, (hi - lo) / 32 > 1 ? (hi - lo) / 32 : 1);
    }
    EnableWindow(tb, enabled);
    EnableWindow(item(IDC_LABEL + s), enabled);
    SetWindowTextW(item(IDC_VALUE + s), enabled ? value.c_str() : L"");
}

std::wstring pct(int v, int of) { return std::to_wstring(of ? (v * 100 + of / 2) / of : 0) + L"%"; }

void refresh() {
    if (!g_win) return;
    g_updating = true;
    bool on = g_board != nullptr;
    SetWindowTextW(item(IDC_NOW), (L"Now: " + current_effect_text()).c_str());

    // select the current effect in the list
    HWND list = item(IDC_LIST);
    int want = !on || !g_st.enabled ? -1 : vu_showing() ? VU_ITEM + g_vu_style : g_st.mode;
    int n = ListView_GetItemCount(list), found = -1;
    for (int i = 0; i < n; i++) {
        LVITEMW it = {};
        it.mask  = LVIF_PARAM;
        it.iItem = i;
        ListView_GetItem(list, &it);
        if ((int)it.lParam == want) found = i;
    }
    if (found >= 0) {
        ListView_SetItemState(list, found, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(list, found, FALSE);
    } else {
        ListView_SetItemState(list, -1, 0, LVIS_SELECTED);
    }
    EnableWindow(list, on);

    Button_SetCheck(item(IDC_ON), on && g_st.enabled ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(item(IDC_ON), on);
    std::string cur = current_name();
    Button_SetCheck(item(IDC_FAV), !cur.empty() && is_favourite(cur) ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(item(IDC_FAV), on && !cur.empty());

    bool fx = on && g_st.enabled && !vu_showing();
    bool tuned = fx && g_tune.mode == g_st.mode;
    set_slider(BRIGHTNESS, 0, g_st.max_val, g_st.val, on, pct(g_st.val, g_st.max_val));
    // with keys selected, Speed is theirs (0-15, D0k 2026-11-03: the effect's speed did nothing visible for them)
    if (keys_mode()) {
        std::vector<int> sel = preview_selected_leds();
        const uint8_t *k = sel.empty() ? nullptr : key_state(sel[0]);
        int ks = k && k[0] ? k[1] : proto::KEY_SPEED_DEFAULT;
        int ms = proto::key_step_ms(ks);
        set_slider(SPEED, 0, 255, ks, true, ms >= 1000 ? std::to_wstring(ms / 1000) + L"." + std::to_wstring(ms % 1000 / 100) + L" s"
                                                      : std::to_wstring(ms) + L" ms");
        SetWindowTextW(item(IDC_LABEL + SPEED), L"Key speed");
    } else {
        set_slider(SPEED, 0, 255, g_st.speed, fx, pct(g_st.speed, 255));
        SetWindowTextW(item(IDC_LABEL + SPEED), L"Speed");
    }
    // size: 128 = the effect's normal size, shown as 100%
    set_slider(SIZE, SIZE_MIN, 255, g_tune.size, tuned && (g_tune.flags & 1), pct(g_tune.size, 128));
    set_slider(INTENSITY, INTENSITY_MIN, 255, g_tune.intensity, tuned && (g_tune.flags & 2), pct(g_tune.intensity, 255));

    // with keys selected in the preview the colours below colour those keys; otherwise the effect (D0k, 2026-11-03)
    bool keys = keys_mode();
    for (int c = 0; c < SWATCHES; c++) {
        EnableWindow(item(IDC_SWATCH + c), c == BLACK ? keys : fx || keys); // black: keys only (an effect has no black)
        InvalidateRect(item(IDC_SWATCH + c), nullptr, FALSE);
    }
    EnableWindow(item(IDC_PICK), fx || keys);

    std::wstring ver = L"ZXKB " ZXKB_W(ZXKB_VERSION_STR);
    if (!on) ver += L" - keyboard not connected";
    else if (g_board->kind == Board::VIA) ver += L", " + (g_board->def_name.empty() ? g_board->product : g_board->def_name) + L" (standard VIA firmware)";
    else if (g_st.fw_version) ver += L", keyboard firmware v" + std::to_wstring(g_st.fw_version);
    // a keyboard ZXKB has no definition for yet: offer to look it up; once it has a layout, the keycaps choice
    bool known = on && !g_board->keys.empty();
    ShowWindow(item(IDC_LOOKUP), on && !boards::has_definition(*g_board) ? SW_SHOW : SW_HIDE);
    ShowWindow(item(IDC_KEYCAPS), known ? SW_SHOW : SW_HIDE);
    if (known) fill_keycaps();
    refresh_keys();
    SetWindowTextW(item(IDC_VERSION), ver.c_str());
    g_updating = false;
}

// QMK hue and saturation (0-255) from a picked RGB colour; brightness stays with the Brightness slider
// Keycaps: labels from the keyboard language (default), a keycap set, or import one
// Items: follow Windows, each installed layout, each keycap set, import. g_choices holds what each item stores.
std::vector<std::wstring> g_choices;
void fill_keycaps() {
    HWND cb = item(IDC_KEYCAPS);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);
    g_choices.clear();
    std::wstring now = keycaps::chosen(g_board->vid, g_board->pid);
    auto add = [&](const std::wstring &text, const std::wstring &value) {
        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)text.c_str());
        g_choices.push_back(value);
    };
    add(L"Key labels: as Windows types (" + labels::layout_name(labels::current_layout()) + L")", L"");
    for (HKL h : labels::installed()) {
        wchar_t v[32];
        swprintf(v, 32, L"layout:%llX", (unsigned long long)(UINT_PTR)h);
        add(L"Key labels: " + labels::layout_name(h), v);
    }
    for (const std::wstring &set : keycaps::list()) add(L"Keycaps: " + set, set);
    int sel = 0;
    for (size_t i = 0; i < g_choices.size(); i++)
        if (g_choices[i] == now) sel = (int)i;
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)L"Import a keycap set (KLE file)...");
    SendMessageW(cb, CB_SETCURSEL, sel, 0);
}

void on_keycaps() {
    int sel = (int)SendMessageW(item(IDC_KEYCAPS), CB_GETCURSEL, 0, 0);
    if (sel < 0 || !g_board) return;
    std::wstring name;
    if (sel < (int)g_choices.size()) name = g_choices[sel];
    else { // import
        wchar_t path[MAX_PATH] = {0};
        OPENFILENAMEW of = {sizeof(of)};
        of.hwndOwner   = g_win;
        of.lpstrFilter = L"Keyboard layout (KLE, *.json)\0*.json\0";
        of.lpstrFile   = path;
        of.nMaxFile    = MAX_PATH;
        of.Flags       = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        std::wstring error;
        if (!GetOpenFileNameW(&of)) { fill_keycaps(); return; }
        if (!keycaps::import(path, &name, &error)) {
            MessageBoxW(g_win, error.c_str(), L"ZXKB", MB_OK | MB_ICONINFORMATION);
            fill_keycaps();
            return;
        }
    }
    keycaps::choose(g_board->vid, g_board->pid, name);
    fill_keycaps();
    preview_relabel();
}

void rgb_to_hue_sat(COLORREF c, uint8_t *hue, uint8_t *sat) {
    int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
    int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx - mn;
    *sat = mx ? (uint8_t)(d * 255 / mx) : 0;
    if (!d) { *hue = 0; return; }
    int h; // 0-1535 (six sectors of 256)
    if (mx == r) h = (256 * (g - b) / d + 1536) % 1536;
    else if (mx == g) h = 512 + 256 * (b - r) / d;
    else h = 1024 + 256 * (r - g) / d;
    *hue = (uint8_t)(h / 6);
}

void pick_colour() {
    static COLORREF custom[16];
    static COLORREF last = RGB(204, 0, 0);
    CHOOSECOLORW cc = {sizeof(cc)};
    cc.hwndOwner    = g_win;
    cc.rgbResult    = last;
    cc.lpCustColors = custom;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) return;
    last = cc.rgbResult;
    uint8_t h, s;
    rgb_to_hue_sat(cc.rgbResult, &h, &s);
    set_colour(h, s);
}

// Per-key colours for the keys selected in the preview
void refresh_keys() {
    bool can = per_key_possible();
    int  n   = preview_selected_count();
    std::wstring t = !g_board ? L""
                   : !can ? L"Key colours need ZXKB firmware v22"
                   : n == 0 ? L"Click keys to colour them (Ctrl: more; empty spot: none)"
                   : std::to_wstring(n) + (n == 1 ? L" key: colours and Key speed below apply to it"
                                                    : L" keys: colours and Key speed below apply to them");
    SetWindowTextW(item(IDC_KEYS_INFO), t.c_str());
    EnableWindow(item(IDC_KEYS_ALL), can);
    EnableWindow(item(IDC_KEYS_NONE), can);
    EnableWindow(item(IDC_KEYS_EFFECT), can && n > 0);
    EnableWindow(item(IDC_KEYS_OFF), can && n > 0);
    // the first selected key's effect, speed and colours (changes go to every selected key)
    std::vector<int> sel = preview_selected_leds();
    const uint8_t *k = sel.empty() ? nullptr : key_state(sel[0]);
    bool on = can && n > 0;
    int fx = k ? k[0] : 0;
    g_updating = true;
    SendMessageW(item(IDC_KEY_FX), CB_SETCURSEL, fx >= proto::KEY_STEADY && fx <= proto::KEY_TWINKLE ? fx - 1 : 0, 0);
    g_updating = false;
    for (int id : {IDC_KEY_FX_LABEL, IDC_KEY_FX, IDC_KEY_COLOURS_LABEL, IDC_KEY_C1, IDC_KEY_C2})
        EnableWindow(item(id), on);
    // the second colour only matters for effects that use it
    bool two = on && (fx == proto::KEY_FLASH || fx == proto::KEY_BREATHE || fx == proto::KEY_TWINKLE);
    EnableWindow(item(IDC_KEY_C2), two);
    if (!two) g_key_target = 1;
    InvalidateRect(item(IDC_KEY_C1), nullptr, FALSE);
    InvalidateRect(item(IDC_KEY_C2), nullptr, FALSE);
}

// The two colour buttons show the first selected key's colours; the framed one is what the colour boxes set
void draw_key_colour(const DRAWITEMSTRUCT *d) {
    int which = d->CtlID == IDC_KEY_C2 ? 2 : 1;
    std::vector<int> sel = preview_selected_leds();
    const uint8_t *k = sel.empty() ? nullptr : key_state(sel[0]);
    COLORREF c = k ? RGB(k[which == 2 ? 5 : 2], k[which == 2 ? 6 : 3], k[which == 2 ? 7 : 4]) : GetSysColor(COLOR_BTNFACE);
    bool enabled = IsWindowEnabled(d->hwndItem) != 0;
    FillRect(d->hDC, &d->rcItem, GetSysColorBrush(COLOR_WINDOW));
    RECT r = d->rcItem;
    InflateRect(&r, -px(4), -px(4));
    HBRUSH fill = CreateSolidBrush(enabled ? c : GetSysColor(COLOR_BTNFACE));
    FillRect(d->hDC, &r, fill);
    DeleteObject(fill);
    HBRUSH frame = CreateSolidBrush(GetSysColor(enabled && which == g_key_target ? COLOR_HIGHLIGHT : COLOR_BTNSHADOW));
    RECT o = d->rcItem;
    for (int i = 0; i < (enabled && which == g_key_target ? 3 : 1); i++) { FrameRect(d->hDC, &o, frame); InflateRect(&o, -1, -1); }
    DeleteObject(frame);
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, (GetRValue(c) + GetGValue(c) + GetBValue(c)) > 380 ? RGB(0, 0, 0) : RGB(255, 255, 255));
    RECT t = r;
    DrawTextW(d->hDC, which == 2 ? L"2" : L"1", 1, &t, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void pick_key_colour() {
    // the Spectrum colours (as calibrated) offered first among the picker's own colours
    static COLORREF custom[16];
    static bool     seeded = false;
    static COLORREF last   = RGB(204, 0, 0);
    if (!seeded) {
        for (int c = 0; c < 7; c++) custom[c] = colours[c].swatch;
        seeded = true;
    }
    CHOOSECOLORW cc = {sizeof(cc)};
    cc.hwndOwner    = g_win;
    cc.rgbResult    = last;
    cc.lpCustColors = custom;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) return;
    last = cc.rgbResult;
    key_colour(GetRValue(last), GetGValue(last), GetBValue(last));
}

void on_slider(int s) {
    int pos = (int)SendMessageW(item(IDC_SLIDER + s), TBM_GETPOS, 0, 0);
    switch (s) {
        case BRIGHTNESS: set_brightness(pos); break;
        case SPEED:
            if (keys_mode()) edit_keys(preview_selected_leds(), KEY_SPEED, pos);
            else set_speed(pos);
            break;
        case SIZE: set_tune(pos, g_tune.intensity); break;
        case INTENSITY: set_tune(g_tune.size, pos); break;
    }
    refresh();
}

void draw_swatch(const DRAWITEMSTRUCT *d) {
    int c = (int)d->CtlID - IDC_SWATCH;
    bool chosen = c != BLACK && IsWindowEnabled(d->hwndItem) && !keys_mode() && g_st.sat == colours[c].sat && (colours[c].sat == 0 || g_st.hue == colours[c].hue);
    HBRUSH bg = GetSysColorBrush(COLOR_WINDOW);
    FillRect(d->hDC, &d->rcItem, bg);
    RECT r = d->rcItem;
    InflateRect(&r, -px(3), -px(3));
    HBRUSH fill = CreateSolidBrush(!IsWindowEnabled(d->hwndItem) ? GetSysColor(COLOR_BTNFACE) : c == BLACK ? RGB(0, 0, 0) : colours[c].swatch);
    FillRect(d->hDC, &r, fill);
    DeleteObject(fill);
    if (chosen || (d->itemState & ODS_FOCUS)) {
        HBRUSH frame = CreateSolidBrush(GetSysColor(COLOR_WINDOWTEXT));
        RECT o = d->rcItem;
        FrameRect(d->hDC, &o, frame);
        InflateRect(&o, -1, -1);
        FrameRect(d->hDC, &o, frame);
        DeleteObject(frame);
    }
}

void place_by_tray() {
    RECT wr = {0, 0, px(W), px(content_height())};
    AdjustWindowRectExForDpi(&wr, GetWindowLongW(g_win, GWL_STYLE), FALSE, GetWindowLongW(g_win, GWL_EXSTYLE), g_dpi);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    NOTIFYICONIDENTIFIER id = {sizeof(id)};
    id.hWnd = tray_window();
    id.uID  = 1;
    RECT icon;
    POINT at;
    if (SUCCEEDED(Shell_NotifyIconGetRect(&id, &icon))) at = {icon.right, icon.top};
    else GetCursorPos(&at);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromPoint(at, MONITOR_DEFAULTTONEAREST), &mi);
    const RECT &wa = mi.rcWork; // the screen minus the taskbar
    int gap = px(8);
    int x = at.x - w, y = at.y - h - gap;
    if (x < wa.left + gap) x = wa.left + gap;
    if (x + w > wa.right - gap) x = wa.right - gap - w;
    if (y < wa.top + gap) y = wa.top + gap;
    if (y + h > wa.bottom - gap) y = wa.bottom - gap - h;
    SetWindowPos(g_win, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NOTIFY: {
            auto *nm = reinterpret_cast<NMHDR *>(lp);
            if (nm->idFrom == IDC_LIST && nm->code == LVN_ITEMCHANGED && !g_updating) {
                auto *lv = reinterpret_cast<NMLISTVIEW *>(lp);
                if ((lv->uNewState & LVIS_SELECTED) && !(lv->uOldState & LVIS_SELECTED)) {
                    int data = (int)lv->lParam;
                    if (data >= VU_ITEM) select_vu(data - VU_ITEM);
                    else set_effect(data);
                    refresh();
                }
            }
            return 0;
        }
        case WM_HSCROLL:
            if (!g_updating)
                for (int s = 0; s < SLIDERS; s++)
                    if ((HWND)lp == item(IDC_SLIDER + s)) on_slider(s);
            return 0;
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDCANCEL) { window_close(); return 0; }
            if (id == IDC_FIRMWARE) { flash_dialog(g_win); return 0; }
            if (id == IDC_CAPS_BTN) { caps_dialog(g_win); return 0; }
            if (id == IDC_LOOKUP) { look_up(); return 0; }
            if (id == IDC_KEYCAPS && HIWORD(wp) == CBN_SELCHANGE) { on_keycaps(); return 0; }
            if (id == IDC_PREVIEW && HIWORD(wp) == PREVIEW_SELECTION) { refresh(); caps_selection_changed(); return 0; }
            if (id == IDC_KEYS_ALL) { preview_select_all(); refresh(); caps_selection_changed(); return 0; }
            if (id == IDC_KEYS_NONE) { // Clear: every key off (black), whatever is selected (D0k, 2026-11-03)
                std::vector<int> all;
                for (int i = 0; g_board && i < g_board->led_count; i++) all.push_back(i);
                edit_keys(all, KEY_FIRST, 0, 0, 0, 0);
                edit_keys(all, KEY_EFFECT, proto::KEY_STEADY);
                refresh();
                return 0;
            }
            if (id == IDC_KEYS_EFFECT) { edit_keys(preview_selected_leds(), KEY_FOLLOW, 0); refresh(); return 0; }
            if (id == IDC_KEYS_OFF) {
                edit_keys(preview_selected_leds(), KEY_FIRST, 0, 0, 0, 0);
                edit_keys(preview_selected_leds(), KEY_EFFECT, proto::KEY_STEADY);
                refresh();
                return 0;
            }
            if (id == IDC_KEY_FX && HIWORD(wp) == CBN_SELCHANGE && !g_updating) {
                int sel = (int)SendMessageW(item(IDC_KEY_FX), CB_GETCURSEL, 0, 0);
                if (sel >= 0) edit_keys(preview_selected_leds(), KEY_EFFECT, sel + 1);
                refresh();
                return 0;
            }
            if (id == IDC_KEY_C1 || id == IDC_KEY_C2) { g_key_target = id == IDC_KEY_C2 ? 2 : 1; refresh(); return 0; }
            if (g_updating) return 0;
            if (id == IDC_ON) {
                set_effect(Button_GetCheck(item(IDC_ON)) == BST_CHECKED ? (g_st.mode ? g_st.mode : 1) : 0);
                refresh();
            } else if (id == IDC_FAV) {
                set_favourite(current_name(), Button_GetCheck(item(IDC_FAV)) == BST_CHECKED);
            } else if (id == IDC_PICK) {
                if (keys_mode()) pick_key_colour();
                else pick_colour();
                refresh();
            } else if (id == IDC_SWATCH + BLACK) {
                if (keys_mode()) key_colour(0, 0, 0);
                refresh();
            } else if (id >= IDC_SWATCH && id < IDC_SWATCH + 7) {
                const Colour &c = colours[id - IDC_SWATCH];
                if (keys_mode()) key_colour(GetRValue(c.swatch), GetGValue(c.swatch), GetBValue(c.swatch));
                else set_colour(c.hue, c.sat);
                refresh();
            }
            return 0;
        }
        case WM_DRAWITEM:
            if (wp == IDC_KEY_C1 || wp == IDC_KEY_C2) draw_key_colour(reinterpret_cast<DRAWITEMSTRUCT *>(lp));
            else draw_swatch(reinterpret_cast<DRAWITEMSTRUCT *>(lp));
            return TRUE;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            SetBkColor((HDC)wp, GetSysColor(COLOR_WINDOW));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        case WM_DPICHANGED: {
            g_dpi = HIWORD(wp);
            set_font();
            layout();
            const RECT *r = reinterpret_cast<const RECT *>(lp);
            RECT wr = {0, 0, px(W), px(content_height())};
            AdjustWindowRectExForDpi(&wr, GetWindowLongW(hwnd, GWL_STYLE), FALSE, GetWindowLongW(hwnd, GWL_EXSTYLE), g_dpi);
            SetWindowPos(hwnd, nullptr, r->left, r->top, wr.right - wr.left, wr.bottom - wr.top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_TIMER:
            if (wp == TIMER_MIRROR) mirror_request(true);
            return 0;
        case WM_INPUTLANGCHANGE: // the keyboard language changed: the labels follow it
            preview_relabel();
            if (g_board && !g_board->keys.empty()) fill_keycaps();
            break;
        case WM_LOOKED_UP:
            EnableWindow(item(IDC_LOOKUP), TRUE);
            SetWindowTextW(item(IDC_LOOKUP), L"Look up this keyboard online...");
            if (wp && g_board) {
                boards::apply_definition(g_board);
                settings_changed(); // the tray copy reads it too
                request_keymap();   // labels for the preview
                fill_list();
                layout();
                refresh();
            } else if (lp) {
                auto *e = reinterpret_cast<std::wstring *>(lp);
                MessageBoxW(hwnd, (L"The lookup did not work: " + *e).c_str(), L"ZXKB", MB_OK | MB_ICONINFORMATION);
                delete e;
            }
            return 0;
        case WM_CLOSE:
            window_close();
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

DWORD WINAPI lookup_thread(void *p) {
    uint32_t     id = (uint32_t)(UINT_PTR)p;
    std::wstring error;
    bool ok = boards::lookup_online((uint16_t)(id >> 16), (uint16_t)id, &error);
    if (g_win) PostMessageW(g_win, WM_LOOKED_UP, ok, ok ? 0 : (LPARAM) new std::wstring(error));
    return 0;
}

void look_up() {
    if (!g_board) return;
    std::wstring ask = L"ZXKB does not know this keyboard's effects and layout yet.\n\nLook them up online? ZXKB downloads the "
                       L"keyboard's public VIA definition from " + boards::definition_source() +
                       L" by its USB ID (about 10 KB), once, and keeps it. Nothing else is sent.";
    if (MessageBoxW(g_win, ask.c_str(), L"ZXKB", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    EnableWindow(item(IDC_LOOKUP), FALSE);
    SetWindowTextW(item(IDC_LOOKUP), L"Looking up...");
    UINT_PTR id = ((UINT_PTR)g_board->vid << 16) | g_board->pid;
    if (HANDLE t = CreateThread(nullptr, 64 * 1024, lookup_thread, (void *)id, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr)) CloseHandle(t);
}

// The effect list ignores typed letters: a list normally jumps to the item starting with the letter, which would
// change the keyboard's effect while someone types to try a typing effect (D0k, 2026-11-03). Arrows still work.
LRESULT CALLBACK no_type_ahead(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_CHAR) return 0;
    return DefSubclassProc(h, msg, wp, lp);
}

HWND make(const wchar_t *cls, const wchar_t *text, DWORD style, int id) {
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, g_win, (HMENU)(INT_PTR)id, g_inst, nullptr);
}

} // namespace

HWND window_handle() { return g_win; }

void window_show() {
    if (g_win) {
        ShowWindow(g_win, SW_RESTORE);
        SetForegroundWindow(g_win);
        return;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc   = proc;
        wc.hInstance     = g_inst;
        wc.lpszClassName = L"ZXKBWindow";
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        LoadIconMetric(g_inst, MAKEINTRESOURCEW(IDI_ON), LIM_LARGE, &wc.hIcon);
        LoadIconMetric(g_inst, MAKEINTRESOURCEW(IDI_ON), LIM_SMALL, &wc.hIconSm);
        RegisterClassExW(&wc);
        registered = true;
    }
    g_win = CreateWindowExW(WS_EX_CONTROLPARENT, L"ZXKBWindow", L"ZXKB", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, nullptr, nullptr, g_inst, nullptr);
    g_dpi = (int)GetDpiForWindow(g_win);

    make(L"STATIC", L"", SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS, IDC_NOW);
    preview_create(g_win, IDC_PREVIEW);
    HWND list = make(WC_LISTVIEWW, L"", WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER, IDC_LIST);
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    SetWindowSubclass(list, no_type_ahead, 1, 0);
    LVCOLUMNW col = {};
    col.mask = LVCF_WIDTH;
    col.cx   = 100;
    ListView_InsertColumn(list, 0, &col);
    ListView_EnableGroupView(list, TRUE);
    make(L"BUTTON", L"Lighting on", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_ON);
    make(L"BUTTON", L"Favourite", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_FAV);
    for (int s = 0; s < SLIDERS; s++) {
        make(L"STATIC", slider_names[s], 0, IDC_LABEL + s);
        make(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, IDC_SLIDER + s);
        make(L"STATIC", L"", SS_RIGHT, IDC_VALUE + s);
    }
    for (int c = 0; c < SWATCHES; c++) make(L"BUTTON", c == BLACK ? L"Black" : colours[c].name, WS_TABSTOP | BS_OWNERDRAW, IDC_SWATCH + c);
    make(L"BUTTON", L"More colours...", WS_TABSTOP, IDC_PICK);
    make(L"STATIC", L"", SS_LEFTNOWORDWRAP, IDC_VERSION);
    make(L"BUTTON", L"Firmware...", WS_TABSTOP, IDC_FIRMWARE);
    make(L"BUTTON", L"Caps Lock light...", WS_TABSTOP, IDC_CAPS_BTN);
    make(L"BUTTON", L"Look up this keyboard online...", WS_TABSTOP, IDC_LOOKUP);
    make(WC_COMBOBOXW, L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, IDC_KEYCAPS);
    make(L"STATIC", L"", SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS, IDC_KEYS_INFO);
    make(L"BUTTON", L"Select all", WS_TABSTOP, IDC_KEYS_ALL);
    make(L"BUTTON", L"Clear", WS_TABSTOP, IDC_KEYS_NONE);
    make(L"BUTTON", L"Follow effect", WS_TABSTOP, IDC_KEYS_EFFECT);
    make(L"BUTTON", L"Off", WS_TABSTOP, IDC_KEYS_OFF);
    make(L"STATIC", L"Key effect", 0, IDC_KEY_FX_LABEL);
    HWND kfx = make(WC_COMBOBOXW, L"", WS_TABSTOP | CBS_DROPDOWNLIST, IDC_KEY_FX);
    for (const wchar_t *n : key_fx_names) SendMessageW(kfx, CB_ADDSTRING, 0, (LPARAM)n);
    make(L"STATIC", L"Colour boxes set", SS_RIGHT, IDC_KEY_COLOURS_LABEL);
    make(L"BUTTON", L"1", WS_TABSTOP | BS_OWNERDRAW, IDC_KEY_C1);
    make(L"BUTTON", L"2", WS_TABSTOP | BS_OWNERDRAW, IDC_KEY_C2);

    set_font();
    layout();
    fill_list();
    refresh_status(); // the keyboard may have changed from its own keys
    refresh();
    place_by_tray();
    mirror_request(true); // the keyboard starts sending its colours for the preview
    SetTimer(g_win, TIMER_MIRROR, MIRROR_KEEPALIVE_MS, nullptr);
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
    SetFocus(list);
}

void window_refresh() {
    if (!g_win) return;
    flash_refresh();
    if (g_listed != g_board) { // a keyboard connected or went away
        fill_list();
        layout(); // the preview's shape depends on the board
        mirror_request(true);
    }
    refresh();
}

void window_close() {
    if (!g_win || flash_busy()) return; // never close while firmware is being written
    mirror_request(false); // the keyboard stops sending colours
    DestroyWindow(g_win); // destroys every control with it
    g_win = nullptr;
    if (g_font) { DeleteObject(g_font); g_font = nullptr; }
    // Give the memory the window used back to Windows (the heap otherwise keeps freed blocks for reuse)
    HeapCompact(GetProcessHeap(), 0);
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    if (g_window_mode) PostMessageW(g_hwnd, WM_CLOSE, 0, 0); // the window copy ends with its window
}
