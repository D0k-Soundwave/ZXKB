// The Keyboard settings window. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "kbsettings.h"
#include "app.h"
#include "protocol.h"
#include "../res/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <cstring>
#include <string>

bool zxkb_board(); // main.cpp

namespace {

// The 13 setting bytes, as the firmware keeps them (zxkb.h ZXKB_CMD_SETTINGS)
struct Set {
    uint8_t b[proto::SETTINGS_BYTES] = {0};
    uint8_t &game() { return b[0]; }
    uint8_t &socd() { return b[1]; }
    uint8_t &debounce() { return b[2]; }
    uint8_t &idle() { return b[3]; }
    uint8_t *layer() { return &b[4]; } // on, r, g, b
    uint8_t &nkro() { return b[8]; }
    uint8_t *macro_modes() { return &b[9]; }
    int macro_mode(int m) { return (b[9 + m / 4] >> (m % 4 * 2)) & 3; }
    void set_macro_mode(int m, int mode) {
        uint8_t &x = b[9 + m / 4];
        x = (uint8_t)((x & ~(3 << (m % 4 * 2))) | (mode << (m % 4 * 2)));
    }
};

HWND g_dlg = nullptr;
bool g_filling = false;
bool g_kb = false;  // the keyboard has the settings (ZXKB firmware v27 on)
Set  g_before, g_now;
UINT g_delay_before = 1, g_speed_before = 31; // Windows key repeat when the window opened

// Lights off after: minutes (0 = never). [A] choices, not limits: the firmware takes 1-255.
const int idle_choices[] = {0, 1, 2, 5, 10, 15, 30, 60, 120, 240};
// [P] QMK's default debounce, 5 ms (quantum/debounce, DEBOUNCE); the slider's 1-50 ms range is [A], a range only
constexpr int DEBOUNCE_DEFAULT = 5, DEBOUNCE_MAX = 50;
constexpr int MACRO_MODES_MAX = 16; // the firmware keeps play modes for the first 16 macros (4 bytes, 2 bits each)
const wchar_t *const mode_names[3] = {L"plays once", L"repeats while held", L"on / off (press again to stop)"};

bool kb_get(Set *s) {
    uint8_t q[2] = {proto::SETTINGS, proto::SETTINGS_GET}, r[proto::REPORT];
    if (!keyboard_ask(q, 2, r, 2)) return false;
    std::memcpy(s->b, r + 2, proto::SETTINGS_BYTES);
    return true;
}

void kb_send(const Set &s) {
    uint8_t q[2 + proto::SETTINGS_BYTES] = {proto::SETTINGS, proto::SETTINGS_SET}, r[proto::REPORT];
    std::memcpy(q + 2, s.b, proto::SETTINGS_BYTES);
    keyboard_ask(q, sizeof(q), r, 2); // the reply is taken here so it is not left over
}

void kb_save() {
    uint8_t q[2] = {proto::SETTINGS, proto::SETTINGS_SAVE}, r[proto::REPORT];
    keyboard_ask(q, 2, r, 2);
}

std::wstring repeat_delay_text(int d) { return std::to_wstring(250 * (d + 1)) + L" ms"; } // [P] 0-3: ~250 ms to ~1 s
std::wstring repeat_rate_text(int s) { // [P] 0-31: ~2.5 to ~30 a second (Microsoft, SPI_SETKEYBOARDSPEED)
    int tenths = 25 + s * (300 - 25) / 31; // [C] straight line between the published ends
    return std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10) + L" a second";
}

int selected_macro() {
    int m = (int)SendDlgItemMessageW(g_dlg, IDC_KS_MACRO, CB_GETCURSEL, 0, 0);
    return m < 0 ? 0 : m;
}

void show() {
    g_filling = true;
    HWND d = g_dlg;
    auto check = [d](int id, bool on) { CheckDlgButton(d, id, on ? BST_CHECKED : BST_UNCHECKED); };
    check(IDC_KS_GAME, g_now.game() & 0x80);
    check(IDC_KS_ALTTAB, g_now.game() & 1);
    check(IDC_KS_ALTF4, g_now.game() & 2);
    check(IDC_KS_WIN, g_now.game() & 4);
    check(IDC_KS_SOCD_WASD, g_now.socd() & 1);
    check(IDC_KS_SOCD_ARROWS, g_now.socd() & 2);
    int db = g_now.debounce() ? g_now.debounce() : DEBOUNCE_DEFAULT;
    SendDlgItemMessageW(d, IDC_KS_DEBOUNCE, TBM_SETPOS, TRUE, db);
    SetDlgItemTextW(d, IDC_KS_DEBOUNCE_VAL, (std::to_wstring(db) + L" ms" + (db == DEBOUNCE_DEFAULT ? L" (usual)" : L"")).c_str());
    int idle_sel = -1;
    for (int i = 0; i < (int)(sizeof(idle_choices) / sizeof(idle_choices[0])); i++)
        if (idle_choices[i] == g_now.idle()) idle_sel = i;
    if (idle_sel < 0) { // a value set some other way: show it as it is
        std::wstring t = std::to_wstring(g_now.idle()) + L" minutes";
        SendDlgItemMessageW(d, IDC_KS_IDLE, CB_SETCURSEL, -1, 0);
        SetDlgItemTextW(d, IDC_KS_IDLE, t.c_str());
    } else {
        SendDlgItemMessageW(d, IDC_KS_IDLE, CB_SETCURSEL, idle_sel, 0);
    }
    check(IDC_KS_LAYER, g_now.layer()[0]);
    check(IDC_KS_NKRO, g_now.nkro());
    int macros = macro_count() < MACRO_MODES_MAX ? macro_count() : MACRO_MODES_MAX;
    if (macros > 0) SendDlgItemMessageW(d, IDC_KS_MACRO_MODE, CB_SETCURSEL, g_now.macro_mode(selected_macro()), 0);
    int kb_ids[] = {IDC_KS_GAME, IDC_KS_ALTTAB, IDC_KS_ALTF4, IDC_KS_WIN, IDC_KS_SOCD_WASD, IDC_KS_SOCD_ARROWS, IDC_KS_DEBOUNCE,
                    IDC_KS_IDLE, IDC_KS_LAYER, IDC_KS_NKRO};
    for (int id : kb_ids) EnableWindow(GetDlgItem(d, id), g_kb);
    EnableWindow(GetDlgItem(d, IDC_KS_LAYER_C), g_kb && g_now.layer()[0]);
    EnableWindow(GetDlgItem(d, IDC_KS_MACRO), g_kb && macros > 0);
    EnableWindow(GetDlgItem(d, IDC_KS_MACRO_MODE), g_kb && macros > 0);
    InvalidateRect(GetDlgItem(d, IDC_KS_LAYER_C), nullptr, FALSE);

    UINT delay = 1, speed = 31;
    SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delay, 0);
    SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &speed, 0);
    SendDlgItemMessageW(d, IDC_KS_DELAY, TBM_SETPOS, TRUE, delay);
    SendDlgItemMessageW(d, IDC_KS_RATE, TBM_SETPOS, TRUE, speed);
    SetDlgItemTextW(d, IDC_KS_DELAY_VAL, repeat_delay_text(delay).c_str());
    SetDlgItemTextW(d, IDC_KS_RATE_VAL, repeat_rate_text(speed).c_str());
    g_filling = false;
}

void changed() {
    if (g_kb) kb_send(g_now);
    show();
}

void pick_layer_colour() {
    static COLORREF custom[16];
    uint8_t *c = g_now.layer() + 1;
    CHOOSECOLORW cc = {sizeof(cc)};
    cc.hwndOwner    = g_dlg;
    cc.rgbResult    = RGB(c[0], c[1], c[2]);
    cc.lpCustColors = custom;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) return;
    c[0] = GetRValue(cc.rgbResult);
    c[1] = GetGValue(cc.rgbResult);
    c[2] = GetBValue(cc.rgbResult);
    changed();
}

void draw(const DRAWITEMSTRUCT *di) {
    const uint8_t *c = g_now.layer() + 1;
    bool enabled = IsWindowEnabled(di->hwndItem) != 0;
    FillRect(di->hDC, &di->rcItem, GetSysColorBrush(COLOR_BTNFACE));
    RECT r = di->rcItem;
    DrawEdge(di->hDC, &r, (di->itemState & ODS_SELECTED) ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT | BF_ADJUST);
    InflateRect(&r, -3, -3);
    HBRUSH b = CreateSolidBrush(enabled ? RGB(c[0], c[1], c[2]) : GetSysColor(COLOR_BTNFACE));
    FillRect(di->hDC, &r, b);
    DeleteObject(b);
    if (di->itemState & ODS_FOCUS) DrawFocusRect(di->hDC, &di->rcItem);
}

void set_repeat(UINT delay, UINT speed, bool keep) {
    UINT flags = SPIF_SENDCHANGE | (keep ? SPIF_UPDATEINIFILE : 0);
    SystemParametersInfoW(SPI_SETKEYBOARDDELAY, delay, nullptr, flags);
    SystemParametersInfoW(SPI_SETKEYBOARDSPEED, speed, nullptr, flags);
}

INT_PTR CALLBACK proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            g_dlg = nullptr;
            return TRUE;
        case WM_INITDIALOG: {
            g_dlg = dlg;
            g_kb  = kbsettings_possible() && kb_get(&g_now);
            if (g_kb && !(g_now.game() & 7)) g_now.game() |= 7; // none chosen blocks all three (firmware v28): show it so
            g_before = g_now;
            SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &g_delay_before, 0);
            SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &g_speed_before, 0);
            SendDlgItemMessageW(dlg, IDC_KS_DEBOUNCE, TBM_SETRANGE, TRUE, MAKELPARAM(1, DEBOUNCE_MAX));
            SendDlgItemMessageW(dlg, IDC_KS_DELAY, TBM_SETRANGE, TRUE, MAKELPARAM(0, 3));
            SendDlgItemMessageW(dlg, IDC_KS_RATE, TBM_SETRANGE, TRUE, MAKELPARAM(0, 31));
            for (int m : idle_choices) {
                std::wstring t = m == 0 ? L"never" : m == 1 ? L"1 minute" : std::to_wstring(m) + L" minutes";
                SendDlgItemMessageW(dlg, IDC_KS_IDLE, CB_ADDSTRING, 0, (LPARAM)t.c_str());
            }
            int macros = macro_count() < MACRO_MODES_MAX ? macro_count() : MACRO_MODES_MAX;
            for (int m = 0; m < macros; m++)
                SendDlgItemMessageW(dlg, IDC_KS_MACRO, CB_ADDSTRING, 0, (LPARAM)(L"Macro " + std::to_wstring(m)).c_str());
            SendDlgItemMessageW(dlg, IDC_KS_MACRO, CB_SETCURSEL, 0, 0);
            for (const wchar_t *n : mode_names) SendDlgItemMessageW(dlg, IDC_KS_MACRO_MODE, CB_ADDSTRING, 0, (LPARAM)n);
            SetDlgItemTextW(dlg, IDC_KS_INFO,
                            g_kb ? L"Changes show at once. OK keeps them (on the keyboard and in Windows); Cancel puts back what was there."
                                 : L"The keyboard settings need ZXKB firmware v27 or later. Windows' key repeat works with any keyboard.");
            show();
            return TRUE;
        }
        case WM_DRAWITEM:
            draw(reinterpret_cast<const DRAWITEMSTRUCT *>(lp));
            return TRUE;
        case WM_HSCROLL: {
            if (g_filling) return TRUE;
            HWND h = (HWND)lp;
            if (h == GetDlgItem(dlg, IDC_KS_DEBOUNCE)) {
                int v = (int)SendMessageW(h, TBM_GETPOS, 0, 0);
                if (v != (g_now.debounce() ? g_now.debounce() : DEBOUNCE_DEFAULT)) {
                    g_now.debounce() = (uint8_t)(v == DEBOUNCE_DEFAULT ? 0 : v);
                    changed();
                }
            } else if (h == GetDlgItem(dlg, IDC_KS_DELAY) || h == GetDlgItem(dlg, IDC_KS_RATE)) {
                set_repeat((UINT)SendDlgItemMessageW(dlg, IDC_KS_DELAY, TBM_GETPOS, 0, 0),
                           (UINT)SendDlgItemMessageW(dlg, IDC_KS_RATE, TBM_GETPOS, 0, 0), false);
                show();
            }
            return TRUE;
        }
        case WM_COMMAND: {
            if (g_filling) return TRUE;
            int  id = LOWORD(wp);
            auto bit = [dlg, id](uint8_t &field, uint8_t mask) {
                if (IsDlgButtonChecked(dlg, id) == BST_CHECKED) field |= mask;
                else field &= (uint8_t)~mask;
                changed();
            };
            switch (id) {
                case IDC_KS_GAME: bit(g_now.game(), 0x80); return TRUE;
                case IDC_KS_ALTTAB: case IDC_KS_ALTF4: case IDC_KS_WIN: { // at least one stays ticked (none = all three)
                    uint8_t mask = id == IDC_KS_ALTTAB ? 1 : id == IDC_KS_ALTF4 ? 2 : 4;
                    if ((g_now.game() & 7) == mask && IsDlgButtonChecked(dlg, id) != BST_CHECKED) { show(); return TRUE; }
                    bit(g_now.game(), mask);
                    return TRUE;
                }
                case IDC_KS_SOCD_WASD: bit(g_now.socd(), 1); return TRUE;
                case IDC_KS_SOCD_ARROWS: bit(g_now.socd(), 2); return TRUE;
                case IDC_KS_LAYER: { // a first switch-on gets white, so it shows
                    uint8_t *c = g_now.layer() + 1;
                    if (!c[0] && !c[1] && !c[2]) c[0] = c[1] = c[2] = 255;
                    bit(g_now.layer()[0], 1);
                    return TRUE;
                }
                case IDC_KS_NKRO: bit(g_now.nkro(), 1); return TRUE;
                case IDC_KS_LAYER_C: pick_layer_colour(); return TRUE;
                case IDC_KS_IDLE:
                    if (HIWORD(wp) == CBN_SELCHANGE) {
                        int s = (int)SendDlgItemMessageW(dlg, IDC_KS_IDLE, CB_GETCURSEL, 0, 0);
                        if (s >= 0) { g_now.idle() = (uint8_t)idle_choices[s]; changed(); }
                    }
                    return TRUE;
                case IDC_KS_MACRO:
                    if (HIWORD(wp) == CBN_SELCHANGE) show();
                    return TRUE;
                case IDC_KS_MACRO_MODE:
                    if (HIWORD(wp) == CBN_SELCHANGE) {
                        int s = (int)SendDlgItemMessageW(dlg, IDC_KS_MACRO_MODE, CB_GETCURSEL, 0, 0);
                        if (s >= 0 && s < 3) { g_now.set_macro_mode(selected_macro(), s); changed(); }
                    }
                    return TRUE;
                case IDOK: {
                    if (g_kb) kb_save();
                    UINT delay = 1, speed = 31;
                    SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delay, 0);
                    SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &speed, 0);
                    if (delay != g_delay_before || speed != g_speed_before) set_repeat(delay, speed, true);
                    DestroyWindow(dlg);
                    return TRUE;
                }
                case IDCANCEL:
                    if (g_kb && std::memcmp(g_now.b, g_before.b, sizeof(g_now.b)) != 0) kb_send(g_before);
                    set_repeat(g_delay_before, g_speed_before, false);
                    DestroyWindow(dlg);
                    return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

} // namespace

bool kbsettings_possible() { return zxkb_board() && g_board->protocol >= 12; }

void kbsettings_dialog(HWND owner) {
    if (g_dlg) { SetForegroundWindow(g_dlg); return; }
    g_dlg = CreateDialogParamW(g_inst, MAKEINTRESOURCEW(IDD_KBSETTINGS), owner, proc, 0);
    if (g_dlg) ShowWindow(g_dlg, SW_SHOW);
}

HWND kbsettings_window() { return g_dlg; }
