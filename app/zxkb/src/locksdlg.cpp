// The Lock lights window (ZXKB window copy): the light shown while Caps Lock, Num Lock or Scroll Lock is on. Changes
// are written to zxkb.ini and the tray (which shows the lights) told at once; while this window is open the light
// shows in the ZXKB window's preview only. Cancel puts back what was there.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "locklights.h"
#include "app.h"
#include "caps.h"
#include "protocol.h"
#include "numbox.h"
#include "preview.h"
#include "../res/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <string>

bool caps_possible();
extern UINT g_locks_msg;

namespace locklights {
namespace {

Light g_before[COUNT], g_now[COUNT];
bool  g_filling = false;
const wchar_t *const fx_names[4] = {L"Off", L"Steady", L"Flash", L"Breathe"};
const uint8_t fx_values[4] = {proto::KEY_NONE, proto::KEY_STEADY, proto::KEY_FLASH, proto::KEY_BREATHE};
constexpr int OPEN_KEYS = 100; // the dialog's result: open the Caps Lock light on chosen keys next

int id(int w, int part) { return IDC_LK_ROW + 10 * w + part; }

std::wstring speed_text(int speed) {
    int ms = proto::key_step_ms(speed);
    return ms >= 1000 ? std::to_wstring(ms / 1000) + L"." + std::to_wstring(ms % 1000 / 100) + L" s" : std::to_wstring(ms) + L" ms";
}

void tell_tray() { settings_changed(); }

void send(int w) {
    set(w, g_now[w]);
    tell_tray();
}

void show(HWND dlg) {
    g_filling = true;
    for (int w = 0; w < COUNT; w++) {
        const Light &l = g_now[w];
        int sel = 0;
        for (int i = 0; i < 4; i++)
            if (fx_values[i] == l.fx) sel = i;
        SendDlgItemMessageW(dlg, id(w, 1), CB_SETCURSEL, sel, 0);
        SendDlgItemMessageW(dlg, id(w, 4), TBM_SETPOS, TRUE, l.speed);
        if (GetFocus() != GetDlgItem(dlg, id(w, 5))) SetDlgItemTextW(dlg, id(w, 5), speed_text(l.speed).c_str());
        CheckDlgButton(dlg, id(w, 6), l.full ? BST_CHECKED : BST_UNCHECKED);
        bool on = l.fx != proto::KEY_NONE, moving = l.fx == proto::KEY_FLASH || l.fx == proto::KEY_BREATHE;
        EnableWindow(GetDlgItem(dlg, id(w, 2)), on);
        EnableWindow(GetDlgItem(dlg, id(w, 3)), moving);
        EnableWindow(GetDlgItem(dlg, id(w, 4)), moving);
        EnableWindow(GetDlgItem(dlg, id(w, 5)), moving);
        EnableWindow(GetDlgItem(dlg, id(w, 6)), on);
        InvalidateRect(GetDlgItem(dlg, id(w, 2)), nullptr, FALSE);
        InvalidateRect(GetDlgItem(dlg, id(w, 3)), nullptr, FALSE);
    }
    EnableWindow(GetDlgItem(dlg, IDC_LK_KEYS), caps_possible());
    g_filling = false;
}

void speed_typed(HWND dlg, int w) {
    wchar_t typed[64] = {0};
    GetDlgItemTextW(dlg, id(w, 5), typed, 64);
    int pos = numbox::nearest(typed, speed_text(g_now[w].speed), 0, 255, speed_text);
    if (pos >= 0 && pos != g_now[w].speed) { g_now[w].speed = (uint8_t)pos; send(w); }
    SetDlgItemTextW(dlg, id(w, 5), speed_text(g_now[w].speed).c_str());
    show(dlg);
}

void pick(HWND dlg, int w, uint8_t *rgb) {
    static COLORREF custom[16];
    static bool     seeded = false;
    if (!seeded) { // the Spectrum colours (as calibrated) first among the picker's own colours
        for (int c = 0; c < 7; c++) custom[c] = colours[c].swatch;
        custom[7] = RGB(255, 255, 255);
        custom[8] = RGB(255, 0, 255);
        custom[9] = RGB(0, 0, 0);
        seeded = true;
    }
    CHOOSECOLORW cc = {sizeof(cc)};
    cc.hwndOwner    = dlg;
    cc.rgbResult    = RGB(rgb[0], rgb[1], rgb[2]);
    cc.lpCustColors = custom;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) return;
    rgb[0] = GetRValue(cc.rgbResult);
    rgb[1] = GetGValue(cc.rgbResult);
    rgb[2] = GetBValue(cc.rgbResult);
    send(w);
    show(dlg);
}

void draw(const DRAWITEMSTRUCT *d) {
    int w = (int)(d->CtlID - IDC_LK_ROW) / 10;
    if (w < 0 || w >= COUNT) return;
    const uint8_t *c = (d->CtlID - IDC_LK_ROW) % 10 == 3 ? g_now[w].b : g_now[w].a;
    bool enabled = IsWindowEnabled(d->hwndItem) != 0;
    FillRect(d->hDC, &d->rcItem, GetSysColorBrush(COLOR_BTNFACE));
    RECT r = d->rcItem;
    DrawEdge(d->hDC, &r, (d->itemState & ODS_SELECTED) ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT | BF_ADJUST);
    InflateRect(&r, -3, -3);
    HBRUSH b = CreateSolidBrush(enabled ? RGB(c[0], c[1], c[2]) : GetSysColor(COLOR_BTNFACE));
    FillRect(d->hDC, &r, b);
    DeleteObject(b);
    if (d->itemState & ODS_FOCUS) DrawFocusRect(d->hDC, &d->rcItem);
}

void preview(bool on) { // while this window is open the lights show in the ZXKB window's preview, not on the keyboard
    preview_locks(on);   // (D0k, 2026-11-06); the tray shows them again once the ZXKB window is closed
}

INT_PTR CALLBACK proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG: {
            reload();
            for (int w = 0; w < COUNT; w++) {
                g_before[w] = g_now[w] = get(w);
                for (const wchar_t *n : fx_names) SendDlgItemMessageW(dlg, id(w, 1), CB_ADDSTRING, 0, (LPARAM)n);
                SendDlgItemMessageW(dlg, id(w, 4), TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
                SendDlgItemMessageW(dlg, id(w, 4), TBM_SETPAGESIZE, 0, 8);
            }
            std::wstring info = L"While a lock key is on, ZXKB lights the whole keyboard in its light, then puts your lighting "
                                L"back. Works with any keyboard while ZXKB runs. Caps Lock shows over Num Lock, Num Lock over "
                                L"Scroll Lock. Flash and Breathe go between colour 1 and colour 2 (black for on / off). While this window is "
                                L"open, press a lock key to see its light in the preview.";
            SetDlgItemTextW(dlg, IDC_LK_INFO, info.c_str());
            show(dlg);
            preview(true);
            return TRUE;
        }
        case WM_DRAWITEM:
            draw(reinterpret_cast<const DRAWITEMSTRUCT *>(lp));
            return TRUE;
        case WM_HSCROLL:
            if (!g_filling)
                for (int w = 0; w < COUNT; w++)
                    if ((HWND)lp == GetDlgItem(dlg, id(w, 4))) {
                        g_now[w].speed = (uint8_t)SendDlgItemMessageW(dlg, id(w, 4), TBM_GETPOS, 0, 0);
                        send(w);
                        show(dlg);
                    }
            return TRUE;
        case WM_COMMAND: {
            if (g_filling) return TRUE;
            int c = LOWORD(wp);
            if (c >= IDC_LK_ROW && c < IDC_LK_ROW + 10 * COUNT) {
                int w = (c - IDC_LK_ROW) / 10, part = (c - IDC_LK_ROW) % 10;
                if (part == 1 && HIWORD(wp) == CBN_SELCHANGE) {
                    int sel = (int)SendDlgItemMessageW(dlg, c, CB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < 4) { g_now[w].fx = fx_values[sel]; send(w); show(dlg); }
                } else if (part == 2) pick(dlg, w, g_now[w].a);
                else if (part == 3) pick(dlg, w, g_now[w].b);
                else if (part == 5 && HIWORD(wp) == EN_KILLFOCUS) speed_typed(dlg, w);
                else if (part == 6) { g_now[w].full = IsDlgButtonChecked(dlg, c) == BST_CHECKED; send(w); }
                return TRUE;
            }
            switch (c) {
                case IDOK:
                    for (int w = 0; w < COUNT; w++)
                        if (GetFocus() == GetDlgItem(dlg, id(w, 5))) { // Enter in a speed box: take the value
                            speed_typed(dlg, w);
                            SendDlgItemMessageW(dlg, id(w, 5), EM_SETSEL, 0, -1);
                            return TRUE;
                        }
                    preview(false);
                    EndDialog(dlg, IDOK);
                    return TRUE;
                case IDC_LK_KEYS:
                    preview(false);
                    EndDialog(dlg, OPEN_KEYS);
                    return TRUE;
                case IDCANCEL: // put back what was there
                    for (int w = 0; w < COUNT; w++) { g_now[w] = g_before[w]; set(w, g_now[w]); }
                    tell_tray();
                    preview(false);
                    EndDialog(dlg, IDCANCEL);
                    return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

} // namespace

void dialog(HWND owner) {
    if (DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_LOCKS), owner, proc, 0) == OPEN_KEYS) caps_dialog(owner);
}

} // namespace locklights
