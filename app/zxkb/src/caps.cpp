// The Caps Lock light window. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "caps.h"
#include "app.h"
#include "preview.h"
#include "protocol.h"
#include "../res/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <string>

namespace {

CapsLight g_before, g_now; // what the keyboard had when the window opened, and what is shown now
HWND      g_dlg = nullptr;
bool      g_filling = false;
const wchar_t *const fx_names[4] = {L"Off", L"Steady", L"Flash", L"Breathe"};
const uint8_t fx_values[4] = {proto::KEY_NONE, proto::KEY_STEADY, proto::KEY_FLASH, proto::KEY_BREATHE};

int key_count(const CapsLight &c) {
    int n = 0;
    for (uint8_t b : c.keys)
        for (int i = 0; i < 8; i++) n += (b >> i) & 1;
    return n;
}

void show() {
    g_filling = true;
    int sel = 0;
    for (int i = 0; i < 4; i++)
        if (fx_values[i] == g_now.fx) sel = i;
    SendDlgItemMessageW(g_dlg, IDC_CAPS_FX, CB_SETCURSEL, sel, 0);
    SendDlgItemMessageW(g_dlg, IDC_CAPS_SPEED, TBM_SETPOS, TRUE, g_now.speed);
    int ms = proto::key_step_ms(g_now.speed);
    std::wstring t = ms >= 1000 ? std::to_wstring(ms / 1000) + L"." + std::to_wstring(ms % 1000 / 100) + L" s" : std::to_wstring(ms) + L" ms";
    SetDlgItemTextW(g_dlg, IDC_CAPS_SPEED_VAL, t.c_str());
    CheckDlgButton(g_dlg, IDC_CAPS_FULL, g_now.full ? BST_CHECKED : BST_UNCHECKED);
    int n = key_count(g_now);
    SetDlgItemTextW(g_dlg, IDC_CAPS_KEYS, (L"Lights " + std::to_wstring(n) + (n == 1 ? L" key" : L" keys")).c_str());
    bool moving = g_now.fx == proto::KEY_FLASH || g_now.fx == proto::KEY_BREATHE;
    EnableWindow(GetDlgItem(g_dlg, IDC_CAPS_SPEED), moving);
    EnableWindow(GetDlgItem(g_dlg, IDC_CAPS_C2), moving);
    EnableWindow(GetDlgItem(g_dlg, IDC_CAPS_USE_SEL), preview_selected_count() > 0);
    EnableWindow(GetDlgItem(g_dlg, IDC_CAPS_ONLY), caps_lock_led() >= 0);
    EnableWindow(GetDlgItem(g_dlg, IDC_CAPS_START), caps_start_keys_possible());
    InvalidateRect(GetDlgItem(g_dlg, IDC_CAPS_C1), nullptr, FALSE);
    InvalidateRect(GetDlgItem(g_dlg, IDC_CAPS_C2), nullptr, FALSE);
    g_filling = false;
}

void send() { set_caps_light(g_now); }

void pick(uint8_t *rgb) {
    static COLORREF custom[16];
    static bool     seeded = false;
    if (!seeded) { // the Spectrum colours (as calibrated) first among the picker's own colours
        for (int c = 0; c < 7; c++) custom[c] = colours[c].swatch;
        custom[7] = RGB(255, 255, 255);
        custom[8] = RGB(255, 0, 255);
        seeded = true;
    }
    CHOOSECOLORW cc = {sizeof(cc)};
    cc.hwndOwner    = g_dlg;
    cc.rgbResult    = RGB(rgb[0], rgb[1], rgb[2]);
    cc.lpCustColors = custom;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) return;
    rgb[0] = GetRValue(cc.rgbResult);
    rgb[1] = GetGValue(cc.rgbResult);
    rgb[2] = GetBValue(cc.rgbResult);
    send();
    show();
}

void draw(const DRAWITEMSTRUCT *d) {
    const uint8_t *c = d->CtlID == IDC_CAPS_C2 ? g_now.b : g_now.a;
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

INT_PTR CALLBACK proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            g_dlg = nullptr;
            return TRUE;
        case WM_ACTIVATE: // keys selected in the preview meanwhile: "Use the keys selected" may now apply
            if (LOWORD(wp) != WA_INACTIVE && g_dlg) show();
            return FALSE;
        case WM_INITDIALOG:
            g_dlg    = dlg;
            g_before = g_now = caps_light();
            for (const wchar_t *n : fx_names) SendDlgItemMessageW(dlg, IDC_CAPS_FX, CB_ADDSTRING, 0, (LPARAM)n);
            SendDlgItemMessageW(dlg, IDC_CAPS_SPEED, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
            SendDlgItemMessageW(dlg, IDC_CAPS_SPEED, TBM_SETPAGESIZE, 0, 8);
            show();
            return TRUE;
        case WM_DRAWITEM:
            draw(reinterpret_cast<const DRAWITEMSTRUCT *>(lp));
            return TRUE;
        case WM_HSCROLL:
            if (!g_filling && (HWND)lp == GetDlgItem(dlg, IDC_CAPS_SPEED)) {
                g_now.speed = (uint8_t)SendDlgItemMessageW(dlg, IDC_CAPS_SPEED, TBM_GETPOS, 0, 0);
                send();
                show();
            }
            return TRUE;
        case WM_COMMAND:
            if (g_filling) return TRUE;
            switch (LOWORD(wp)) {
                case IDC_CAPS_FX:
                    if (HIWORD(wp) == CBN_SELCHANGE) {
                        int sel = (int)SendDlgItemMessageW(dlg, IDC_CAPS_FX, CB_GETCURSEL, 0, 0);
                        if (sel >= 0 && sel < 4) { g_now.fx = fx_values[sel]; send(); show(); }
                    }
                    return TRUE;
                case IDC_CAPS_C1: pick(g_now.a); return TRUE;
                case IDC_CAPS_C2: pick(g_now.b); return TRUE;
                case IDC_CAPS_FULL:
                    g_now.full = IsDlgButtonChecked(dlg, IDC_CAPS_FULL) == BST_CHECKED;
                    send();
                    return TRUE;
                case IDC_CAPS_USE_SEL: { // the keys selected in the preview
                    std::vector<uint8_t> bits(g_now.keys.size(), 0);
                    for (int led : preview_selected_leds())
                        if (led / 8 < (int)bits.size()) bits[led / 8] |= (uint8_t)(1 << (led % 8));
                    g_now.keys = bits;
                    send();
                    show();
                    return TRUE;
                }
                case IDC_CAPS_START: // the keys it started on (D0k: after Caps Lock only there was no way back)
                    caps_start_keys();
                    g_now.keys = caps_light().keys;
                    show();
                    return TRUE;
                case IDC_CAPS_ONLY: {
                    int led = caps_lock_led();
                    std::vector<uint8_t> bits(g_now.keys.size(), 0);
                    if (led >= 0 && led / 8 < (int)bits.size()) bits[led / 8] |= (uint8_t)(1 << (led % 8));
                    g_now.keys = bits;
                    send();
                    show();
                    return TRUE;
                }
                case IDOK:
                    DestroyWindow(dlg);
                    return TRUE;
                case IDCANCEL: // put back what the keyboard had
                    g_now = g_before;
                    send();
                    DestroyWindow(dlg);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

} // namespace

// Not modal: keys can be selected in the preview while it is open (D0k, 2026-11-05)
void caps_dialog(HWND owner) {
    if (!caps_possible()) {
        MessageBoxW(owner, L"Setting the Caps Lock light needs ZXKB firmware v24 or later.", L"ZXKB", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (g_dlg) { SetForegroundWindow(g_dlg); return; }
    g_dlg = CreateDialogParamW(g_inst, MAKEINTRESOURCEW(IDD_CAPS), owner, proc, 0);
    if (g_dlg) ShowWindow(g_dlg, SW_SHOW);
}

HWND caps_window() { return g_dlg; }

void caps_selection_changed() { if (g_dlg) show(); }
