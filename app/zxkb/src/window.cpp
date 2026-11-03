// The ZXKB window: effects and their settings in one place, next to the tray. Standard Windows controls only;
// created when opened and destroyed (with its font and brushes) when closed.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include "app.h"
#include "effects.h"
#include "vumath.h"
#include "preview.h"
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
    IDC_NOW = 100, IDC_LIST, IDC_ON, IDC_FAV, IDC_VERSION, IDC_PICK, IDC_PREVIEW,
    IDC_SLIDER = 200, // + slider index
    IDC_VALUE  = 220, // + slider index
    IDC_LABEL  = 240, // + slider index
    IDC_SWATCH = 300, // + colour index
};
enum Slider { BRIGHTNESS, SPEED, SIZE, INTENSITY, SLIDERS };
const wchar_t *slider_names[SLIDERS] = {L"Brightness", L"Speed", L"Size", L"Intensity"};
constexpr int VU_ITEM = 1000; // list item data: effect number, or VU_ITEM + VuStyle
constexpr int VU_GROUP = 100; // list group for the VU styles

// Layout in device-independent pixels (scaled for the screen's DPI). [A] sizes chosen to fit the controls:
// the preview across the top, the effect list on the left, the controls on the right.
constexpr int W = 680, MARGIN = 12, ROW = 30, LIST_W = 300, LIST_H = 226, SWATCH = 24, GUTTER = 16;
constexpr UINT TIMER_MIRROR = 1;
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
int content_height() { return MARGIN + 26 + preview_h() + 10 + LIST_H + 10 + 18 + MARGIN; }

void layout() {
    int y = MARGIN, inner = W - 2 * MARGIN;
    MoveWindow(item(IDC_NOW), px(MARGIN), px(y), px(inner), px(20), FALSE);
    y += 26;
    MoveWindow(item(IDC_PREVIEW), px(MARGIN), px(y), px(inner), px(preview_h()), FALSE);
    y += preview_h() + 10;
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
    for (int c = 0; c < 7; c++) MoveWindow(item(IDC_SWATCH + c), px(x + c * (SWATCH + 6)), px(y), px(SWATCH), px(SWATCH), FALSE);
    MoveWindow(item(IDC_PICK), px(x + 7 * (SWATCH + 6) + 6), px(y), px(cw - 7 * (SWATCH + 6) - 6), px(SWATCH), FALSE);
    MoveWindow(item(IDC_VERSION), px(MARGIN), px(top + LIST_H + 10), px(inner), px(18), FALSE);
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
    add_group(list, VU_GROUP, L"VU meter");
    for (int gr = effects::SOLID; gr < effects::HIDDEN; gr++) add_group(list, gr, effects::group_names[gr]);
    const int order[3] = {VU_MUSIC, VU_BANDS, VU_ABC};
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
    set_slider(SPEED, 0, 255, g_st.speed, fx, pct(g_st.speed, 255));
    // size: 128 = the effect's normal size, shown as 100%
    set_slider(SIZE, TUNE_MIN, 255, g_tune.size, tuned && (g_tune.flags & 1), pct(g_tune.size, 128));
    set_slider(INTENSITY, TUNE_MIN, 255, g_tune.intensity, tuned && (g_tune.flags & 2), pct(g_tune.intensity, 255));

    for (int c = 0; c < 7; c++) {
        EnableWindow(item(IDC_SWATCH + c), fx);
        InvalidateRect(item(IDC_SWATCH + c), nullptr, FALSE);
    }
    EnableWindow(item(IDC_PICK), fx);

    std::wstring ver = L"ZXKB " ZXKB_W(ZXKB_VERSION_STR);
    if (!on) ver += L" - keyboard not connected";
    else if (g_st.fw_version) ver += L", keyboard firmware v" + std::to_wstring(g_st.fw_version);
    SetWindowTextW(item(IDC_VERSION), ver.c_str());
    g_updating = false;
}

// QMK hue and saturation (0-255) from a picked RGB colour; brightness stays with the Brightness slider
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

void on_slider(int s) {
    int pos = (int)SendMessageW(item(IDC_SLIDER + s), TBM_GETPOS, 0, 0);
    switch (s) {
        case BRIGHTNESS: set_brightness(pos); break;
        case SPEED: set_speed(pos); break;
        case SIZE: set_tune(pos, g_tune.intensity); break;
        case INTENSITY: set_tune(g_tune.size, pos); break;
    }
    refresh();
}

void draw_swatch(const DRAWITEMSTRUCT *d) {
    int c = (int)d->CtlID - IDC_SWATCH;
    bool chosen = IsWindowEnabled(d->hwndItem) && g_st.sat == colours[c].sat && (colours[c].sat == 0 || g_st.hue == colours[c].hue);
    HBRUSH bg = GetSysColorBrush(COLOR_WINDOW);
    FillRect(d->hDC, &d->rcItem, bg);
    RECT r = d->rcItem;
    InflateRect(&r, -px(3), -px(3));
    HBRUSH fill = CreateSolidBrush(IsWindowEnabled(d->hwndItem) ? colours[c].swatch : GetSysColor(COLOR_BTNFACE));
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
            if (g_updating) return 0;
            if (id == IDC_ON) {
                set_effect(Button_GetCheck(item(IDC_ON)) == BST_CHECKED ? (g_st.mode ? g_st.mode : 1) : 0);
                refresh();
            } else if (id == IDC_FAV) {
                set_favourite(current_name(), Button_GetCheck(item(IDC_FAV)) == BST_CHECKED);
            } else if (id == IDC_PICK) {
                pick_colour();
                refresh();
            } else if (id >= IDC_SWATCH && id < IDC_SWATCH + 7) {
                set_colour(colours[id - IDC_SWATCH].hue, colours[id - IDC_SWATCH].sat);
                refresh();
            }
            return 0;
        }
        case WM_DRAWITEM:
            draw_swatch(reinterpret_cast<DRAWITEMSTRUCT *>(lp));
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
        case WM_CLOSE:
            window_close();
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
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
    for (int c = 0; c < 7; c++) make(L"BUTTON", colours[c].name, WS_TABSTOP | BS_OWNERDRAW, IDC_SWATCH + c);
    make(L"BUTTON", L"More colours...", WS_TABSTOP, IDC_PICK);
    make(L"STATIC", L"", SS_LEFTNOWORDWRAP, IDC_VERSION);

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
    if (g_listed != g_board) { // a keyboard connected or went away
        fill_list();
        layout(); // the preview's shape depends on the board
        mirror_request(true);
    }
    refresh();
}

void window_close() {
    if (!g_win) return;
    mirror_request(false); // the keyboard stops sending colours
    DestroyWindow(g_win); // destroys every control with it
    g_win = nullptr;
    if (g_font) { DeleteObject(g_font); g_font = nullptr; }
    // Give the memory the window used back to Windows (the heap otherwise keeps freed blocks for reuse)
    HeapCompact(GetProcessHeap(), 0);
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    if (g_window_mode) PostMessageW(g_hwnd, WM_CLOSE, 0, 0); // the window copy ends with its window
}
