// The Change keys window. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "remap.h"
#include "app.h"
#include "keycodes.h"
#include "labels.h"
#include "preview.h"
#include "keystore.h"
#include "../res/resource.h"
#include <commctrl.h>
#include <string>
#include <vector>

namespace {

HWND g_dlg = nullptr;
std::vector<keycodes::Choice> g_choices;
int  g_knob_part = 0; // knob selected: 0 press, 1 turn left, 2 turn right

int knob_key() { // the first selected key, if it is a knob
    std::vector<int> keys = preview_selected_keys();
    return !keys.empty() && g_board && g_board->keys[keys[0]].encoder >= 0 ? keys[0] : -1;
}

void fill() {
    HWND list = GetDlgItem(g_dlg, IDC_RM_LIST);
    ListView_DeleteAllItems(list);
    ListView_RemoveAllGroups(list);
    ListView_EnableGroupView(list, TRUE);
    const std::vector<std::wstring> &groups = keycodes::group_names();
    for (size_t g = 0; g < groups.size(); g++) {
        LVGROUP grp = {sizeof(grp)};
        grp.mask      = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
        grp.pszHeader = const_cast<wchar_t *>(groups[g].c_str());
        grp.iGroupId  = (int)g;
        grp.stateMask = grp.state = LVGS_COLLAPSIBLE;
        ListView_InsertGroup(list, -1, &grp);
    }
    g_choices = keycodes::choices(labels::current_layout(), layer_count(), macro_count(), g_board ? g_board->custom_names : std::vector<std::wstring>(),
                                  g_board && g_board->kind == Board::ZXKB);
    for (size_t i = 0; i < g_choices.size(); i++) {
        LVITEMW it = {};
        it.mask     = LVIF_TEXT | LVIF_PARAM | LVIF_GROUPID;
        it.iItem    = (int)i;
        it.pszText  = const_cast<wchar_t *>(g_choices[i].name.c_str());
        it.lParam   = (LPARAM)i;
        it.iGroupId = g_choices[i].group;
        ListView_InsertItem(list, &it);
    }
}

void show() {
    if (!g_dlg) return;
    std::vector<int> keys = preview_selected_keys();
    std::wstring t = L"Layer " + std::to_wstring(g_board ? g_board->shown_layer : 0) + L": ";
    // changing a layer the keyboard is not typing from now: say so (D0k set Y on layer 0 while typing from layer 2)
    if (g_board && g_board->kind == Board::ZXKB && g_board->protocol >= 11 && g_board->shown_layer != g_st.base_layer &&
        g_board->shown_layer != g_st.top_layer)
        t = L"Layer " + std::to_wstring(g_board->shown_layer) + L" (the keyboard types from layer " + std::to_wstring(g_st.base_layer) +
            L" now): ";
    if (keys.empty()) t += L"select keys in the preview (Ctrl+click for more), then choose what they do.";
    else {
        t += std::to_wstring(keys.size()) + (keys.size() == 1 ? L" key selected" : L" keys selected");
        uint16_t kc;
        HKL hkl = labels::current_layout();
        int knob = knob_key();
        if (knob >= 0) { // the knob: what pressing and turning it do
            uint16_t l, r;
            t = L"Layer " + std::to_wstring(g_board->shown_layer) + L", the knob: press ";
            t += key_function(knob, &kc) ? keycodes::describe(kc, hkl, g_board->custom_names) : L"?";
            t += L"; left " + (knob_function(knob, false, &l) ? keycodes::describe(l, hkl, g_board->custom_names) : std::wstring(L"?"));
            t += L"; right " + (knob_function(knob, true, &r) ? keycodes::describe(r, hkl, g_board->custom_names) : std::wstring(L"?"));
        } else if (key_function(keys[0], &kc)) t += L"; now: " + keycodes::describe(kc, hkl, g_board->custom_names);
    }
    bool knob = knob_key() >= 0;
    for (int id : {IDC_RM_PRESS, IDC_RM_LEFT, IDC_RM_RIGHT}) ShowWindow(GetDlgItem(g_dlg, id), knob ? SW_SHOW : SW_HIDE);
    CheckRadioButton(g_dlg, IDC_RM_PRESS, IDC_RM_RIGHT, IDC_RM_PRESS + g_knob_part);
    SetDlgItemTextW(g_dlg, IDC_RM_INFO, t.c_str());
    EnableWindow(GetDlgItem(g_dlg, IDC_RM_ASSIGN), !keys.empty());
    int kept = keystore::count();
    SetDlgItemTextW(g_dlg, IDC_RM_KEPT, (std::to_wstring(kept) + (kept == 1 ? L" key change" : L" key changes") +
                                        L" kept by ZXKB, put back after firmware updates").c_str());
    EnableWindow(GetDlgItem(g_dlg, IDC_RM_REAPPLY), kept > 0);
    EnableWindow(GetDlgItem(g_dlg, IDC_RM_FORGET), kept > 0);
}

void assign() {
    HWND list = GetDlgItem(g_dlg, IDC_RM_LIST);
    int sel = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    if (sel < 0) return;
    LVITEMW it = {};
    it.mask  = LVIF_PARAM;
    it.iItem = sel;
    ListView_GetItem(list, &it);
    if ((size_t)it.lParam >= g_choices.size()) return;
    std::vector<int> keys = preview_selected_keys();
    if (keys.empty()) { // D0k 2026-11-05: with nothing selected this used to say the keyboard refused the change
        MessageBoxW(g_dlg, L"First select the keys to change: click them in the preview (Ctrl+click for more).", L"ZXKB",
                    MB_OK | MB_ICONINFORMATION);
        return;
    }
    int done = 0;
    uint16_t code = g_choices[(size_t)it.lParam].code;
    for (int k : keys) {
        bool turn = g_knob_part && g_board->keys[k].encoder >= 0;
        done += turn ? set_knob_function(k, g_knob_part == 2, code) : set_key_function(k, code);
    }
    if (done < (int)keys.size())
        MessageBoxW(g_dlg, (L"The keyboard did not take the change for " + std::to_wstring(keys.size() - done) + L" of " +
                            std::to_wstring(keys.size()) + L" keys.").c_str(), L"ZXKB", MB_OK | MB_ICONWARNING);
    preview_relabel();
    show();
}

INT_PTR CALLBACK proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG: {
            g_dlg = dlg;
            HWND list = GetDlgItem(dlg, IDC_RM_LIST);
            ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            RECT r;
            GetClientRect(list, &r);
            LVCOLUMNW col = {};
            col.mask = LVCF_WIDTH;
            col.cx   = r.right - GetSystemMetrics(SM_CXVSCROLL) - 4;
            ListView_InsertColumn(list, 0, &col);
            fill();
            show();
            return TRUE;
        }
        case WM_NOTIFY: {
            auto *nm = reinterpret_cast<NMHDR *>(lp);
            if (nm->idFrom == IDC_RM_LIST && nm->code == NM_DBLCLK) assign();
            return TRUE;
        }
        case WM_ACTIVATE:
            if (LOWORD(wp) != WA_INACTIVE) show();
            return FALSE;
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_RM_ASSIGN) { assign(); return TRUE; }
            if (LOWORD(wp) >= IDC_RM_PRESS && LOWORD(wp) <= IDC_RM_RIGHT) { g_knob_part = LOWORD(wp) - IDC_RM_PRESS; show(); return TRUE; }
            if (LOWORD(wp) == IDC_RM_REAPPLY) {
                int failed = 0, n = keystore::reapply(&failed);
                std::wstring t = std::to_wstring(n) + (n == 1 ? L" key put back" : L" keys put back") +
                                 (failed ? L", " + std::to_wstring(failed) + L" could not be." : L" (the others were already so).");
                MessageBoxW(dlg, t.c_str(), L"ZXKB", MB_OK | (failed ? MB_ICONWARNING : MB_ICONINFORMATION));
                show_layer(g_board ? g_board->shown_layer : 0); // read the layer again for the labels
                show();
                return TRUE;
            }
            if (LOWORD(wp) == IDC_RM_FORGET) {
                if (MessageBoxW(dlg, L"Forget the key changes ZXKB keeps? The keys stay as they are now; ZXKB just stops putting "
                                     L"them back after firmware updates.", L"ZXKB", MB_YESNO | MB_ICONQUESTION) == IDYES)
                    keystore::forget_all();
                show();
                return TRUE;
            }
            if (LOWORD(wp) == IDCANCEL || LOWORD(wp) == IDOK) { DestroyWindow(dlg); return TRUE; }
            break;
        case WM_DESTROY:
            g_dlg = nullptr;
            return TRUE;
    }
    return FALSE;
}

} // namespace

void remap_dialog(HWND owner) {
    if (!g_board || !g_board->via_protocol || g_board->keys.empty()) {
        MessageBoxW(owner, L"Changing keys needs a keyboard with VIA whose layout ZXKB knows (look it up first).", L"ZXKB", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (g_dlg) { SetForegroundWindow(g_dlg); return; }
    g_dlg = CreateDialogParamW(g_inst, MAKEINTRESOURCEW(IDD_REMAP), owner, proc, 0);
    if (g_dlg) ShowWindow(g_dlg, SW_SHOW);
}

void remap_refresh() { show(); }

HWND remap_window() { return g_dlg; }
