// The Profiles by program window (tray copy): which profile each program gets while it is in front.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps.h"
#include "app.h"
#include "settings.h"
#include "../res/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <cwctype>
#include <set>
#include <string>
#include <vector>

namespace apps {
namespace {

std::vector<std::wstring> g_rows; // programs in the list, in order

void fill_list(HWND dlg) {
    HWND l = GetDlgItem(dlg, IDC_AP_LIST);
    ListView_DeleteAllItems(l);
    g_rows = settings::keys(L"apps");
    for (size_t i = 0; i < g_rows.size(); i++) {
        LVITEMW it = {};
        it.mask    = LVIF_TEXT;
        it.iItem   = (int)i;
        it.pszText = const_cast<wchar_t *>(g_rows[i].c_str());
        ListView_InsertItem(l, &it);
        std::wstring p = profile_for(g_rows[i]);
        Profile      dummy;
        if (!settings::load_profile(p, &dummy)) p += L" (no longer saved)";
        ListView_SetItemText(l, (int)i, 1, const_cast<wchar_t *>(p.c_str()));
    }
    EnableWindow(GetDlgItem(dlg, IDC_AP_REMOVE), ListView_GetNextItem(l, -1, LVNI_SELECTED) >= 0);
}

BOOL CALLBACK add_running(HWND w, LPARAM lp) { // programs with a visible window of their own
    if (!IsWindowVisible(w) || GetWindow(w, GW_OWNER) || GetWindowTextLengthW(w) == 0) return TRUE;
    std::wstring exe = exe_of(w);
    if (!exe.empty() && exe != L"zxkb.exe") reinterpret_cast<std::set<std::wstring> *>(lp)->insert(exe);
    return TRUE;
}

std::wstring typed_exe(HWND dlg) { // "C:\Games\Game.exe" or "game" -> "game.exe"
    wchar_t buf[MAX_PATH] = {0};
    GetDlgItemTextW(dlg, IDC_AP_EXE, buf, MAX_PATH);
    std::wstring e = buf;
    size_t slash = e.find_last_of(L"\\/");
    if (slash != std::wstring::npos) e = e.substr(slash + 1);
    while (!e.empty() && iswspace(e.back())) e.pop_back();
    while (!e.empty() && iswspace(e.front())) e.erase(0, 1);
    for (wchar_t &c : e) c = (wchar_t)towlower(c);
    if (e.find(L'=') != std::wstring::npos || e.find(L'[') != std::wstring::npos) return L""; // cannot be an .ini key
    if (!e.empty() && e.find(L'.') == std::wstring::npos) e += L".exe";
    return e;
}

void browse(HWND dlg) {
    wchar_t file[MAX_PATH] = {0};
    OPENFILENAMEW of = {};
    of.lStructSize = sizeof(of);
    of.hwndOwner   = dlg;
    of.lpstrFilter = L"Programs (*.exe)\0*.exe\0";
    of.lpstrFile   = file;
    of.nMaxFile    = MAX_PATH;
    of.Flags       = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&of)) SetDlgItemTextW(dlg, IDC_AP_EXE, file);
}

INT_PTR CALLBACK proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG: {
            SetDlgItemTextW(dlg, IDC_AP_INFO,
                            L"While a program here is in front, ZXKB uses its profile; when another program comes to the "
                            L"front, your lighting from before comes back. A program's profile is not saved to the "
                            L"keyboard. Works with any keyboard while ZXKB runs.");
            HWND l = GetDlgItem(dlg, IDC_AP_LIST);
            ListView_SetExtendedListViewStyle(l, LVS_EX_FULLROWSELECT);
            RECT r;
            GetClientRect(l, &r);
            LVCOLUMNW c = {};
            c.mask    = LVCF_TEXT | LVCF_WIDTH;
            c.cx      = (r.right - r.left) / 2;
            c.pszText = const_cast<wchar_t *>(L"Program");
            ListView_InsertColumn(l, 0, &c);
            c.cx      = (r.right - r.left) - c.cx - GetSystemMetrics(SM_CXVSCROLL);
            c.pszText = const_cast<wchar_t *>(L"Profile");
            ListView_InsertColumn(l, 1, &c);
            std::set<std::wstring> running;
            EnumWindows(add_running, (LPARAM)&running);
            for (const std::wstring &e : running) SendDlgItemMessageW(dlg, IDC_AP_EXE, CB_ADDSTRING, 0, (LPARAM)e.c_str());
            for (const std::wstring &p : settings::profile_names())
                SendDlgItemMessageW(dlg, IDC_AP_PROFILE, CB_ADDSTRING, 0, (LPARAM)p.c_str());
            SendDlgItemMessageW(dlg, IDC_AP_PROFILE, CB_SETCURSEL, 0, 0);
            fill_list(dlg);
            return TRUE;
        }
        case WM_NOTIFY: {
            const NMHDR *n = reinterpret_cast<const NMHDR *>(lp);
            if (n->idFrom == IDC_AP_LIST && n->code == LVN_ITEMCHANGED) { // a row chosen: its program and profile below
                int i = ListView_GetNextItem(n->hwndFrom, -1, LVNI_SELECTED);
                EnableWindow(GetDlgItem(dlg, IDC_AP_REMOVE), i >= 0);
                if (i >= 0 && i < (int)g_rows.size()) {
                    SetDlgItemTextW(dlg, IDC_AP_EXE, g_rows[i].c_str());
                    int p = (int)SendDlgItemMessageW(dlg, IDC_AP_PROFILE, CB_FINDSTRINGEXACT, (WPARAM)-1,
                                                     (LPARAM)profile_for(g_rows[i]).c_str());
                    if (p >= 0) SendDlgItemMessageW(dlg, IDC_AP_PROFILE, CB_SETCURSEL, p, 0);
                }
            }
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_AP_BROWSE: browse(dlg); return TRUE;
                case IDC_AP_SET: {
                    std::wstring exe = typed_exe(dlg);
                    int          p   = (int)SendDlgItemMessageW(dlg, IDC_AP_PROFILE, CB_GETCURSEL, 0, 0);
                    wchar_t      name[260] = {0};
                    if (p >= 0) SendDlgItemMessageW(dlg, IDC_AP_PROFILE, CB_GETLBTEXT, p, (LPARAM)name);
                    if (exe.empty() || !name[0]) {
                        MessageBoxW(dlg, L"Choose or type a program (for example game.exe) and a profile.", L"ZXKB", MB_OK | MB_ICONINFORMATION);
                        return TRUE;
                    }
                    settings::set(L"apps", exe.c_str(), name);
                    fill_list(dlg);
                    start();
                    return TRUE;
                }
                case IDC_AP_REMOVE: {
                    int i = ListView_GetNextItem(GetDlgItem(dlg, IDC_AP_LIST), -1, LVNI_SELECTED);
                    if (i >= 0 && i < (int)g_rows.size()) settings::remove(L"apps", g_rows[i].c_str());
                    fill_list(dlg);
                    start();
                    return TRUE;
                }
                case IDOK:
                case IDCANCEL:
                    EndDialog(dlg, IDOK);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

} // namespace

void dialog(HWND owner) {
    if (settings::profile_names().empty()) {
        MessageBoxW(owner, L"Save a profile first: tray menu, Profiles, Save current setup as...", L"ZXKB", MB_OK | MB_ICONINFORMATION);
        return;
    }
    DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_APPS), owner, proc, 0);
}

} // namespace apps
