// Key layout backups and the check after a firmware update. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "layoutcheck.h"
#include "app.h"
#include "keycodes.h"
#include "keystore.h"
#include "macros.h"
#include "settings.h"
#include "protocol.h"
#include "../res/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <cstring>
#include <ctime>
#include <vector>

bool zxkb_board(); // main.cpp

namespace layoutcheck {
namespace {

// Everything a key layout holds: keys by layer * rows * cols + row * cols + col, knob turns by
// (layer * encoders + encoder) * 2 + clockwise, and the macros as text
struct Snapshot {
    int layers = 0, rows = 0, cols = 0, encoders = 0, firmware = 0;
    unsigned usb = 0;
    std::vector<uint16_t> keys, knob;
    std::vector<std::wstring> macros;
    bool has_macros = false;
};

int encoder_count() {
    int n = 0;
    for (const auto &k : g_board->keys)
        if (k.knob && k.encoder + 1 > n) n = k.encoder + 1;
    return n;
}

bool read(Snapshot *s) {
    if (!possible()) return false;
    s->layers   = layer_count();
    s->rows     = g_board->rows;
    s->cols     = g_board->cols;
    s->encoders = encoder_count();
    s->firmware = zxkb_board() ? g_st.fw_version : 0;
    s->usb      = (unsigned)g_board->vid << 16 | g_board->pid;
    int bytes   = s->layers * s->rows * s->cols * 2;
    std::vector<uint8_t> buf;
    uint8_t r[proto::REPORT];
    for (int off = 0; off < bytes; off += 28) { // VIA keymap buffer: 28 bytes a report
        uint8_t n = (uint8_t)(bytes - off < 28 ? bytes - off : 28);
        uint8_t q[4] = {proto::VIA_KEYMAP_BUFFER, (uint8_t)(off >> 8), (uint8_t)off, n};
        if (!keyboard_ask(q, 4, r, 4)) return false;
        buf.insert(buf.end(), r + 4, r + 4 + n);
    }
    s->keys.resize(bytes / 2);
    for (size_t i = 0; i < s->keys.size(); i++) s->keys[i] = (uint16_t)(buf[i * 2] << 8 | buf[i * 2 + 1]);
    s->knob.assign(s->layers * s->encoders * 2, 0);
    for (int l = 0; l < s->layers; l++)
        for (int e = 0; e < s->encoders; e++)
            for (int cw = 0; cw < 2; cw++) {
                uint8_t q[4] = {0x14, (uint8_t)l, (uint8_t)e, (uint8_t)cw};
                if (!keyboard_ask(q, 4, r, 4)) return false;
                s->knob[(l * s->encoders + e) * 2 + cw] = (uint16_t)(r[4] << 8 | r[5]);
            }
    int size = 0;
    s->has_macros = macro_count() > 0 && macros::read_all(&s->macros, &size);
    return true;
}

std::wstring hex(uint16_t c) { wchar_t b[8]; swprintf(b, 8, L"%04X", c); return b; }

bool save(const Snapshot &s, const std::wstring &path, const std::wstring &why) {
    std::wstring t = L"[backup]\r\n";
    wchar_t line[128];
    swprintf(line, 128, L"usb=%04X:%04X\r\nfirmware=%d\r\nlayers=%d\r\nrows=%d\r\ncols=%d\r\nencoders=%d\r\n", s.usb >> 16, s.usb & 0xFFFF,
             s.firmware, s.layers, s.rows, s.cols, s.encoders);
    t += line;
    t += L"why=" + why + L"\r\n[keys]\r\n";
    for (int l = 0; l < s.layers; l++) {
        t += std::to_wstring(l) + L"=";
        for (int i = 0; i < s.rows * s.cols; i++) t += (i ? L" " : L"") + hex(s.keys[l * s.rows * s.cols + i]);
        t += L"\r\n";
    }
    t += L"[knob]\r\n";
    for (size_t i = 0; i < s.knob.size(); i++) t += std::to_wstring(i) + L"=" + hex(s.knob[i]) + L"\r\n";
    t += L"[macros]\r\nread=" + std::wstring(s.has_macros ? L"1" : L"0") + L"\r\n";
    for (size_t m = 0; m < s.macros.size(); m++) t += L"M" + std::to_wstring(m) + L"=" + s.macros[m] + L"\r\n";
    std::string utf8 = settings::narrow(t);
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    bool ok = WriteFile(f, utf8.data(), (DWORD)utf8.size(), &w, nullptr) && w == utf8.size();
    CloseHandle(f);
    return ok;
}

bool load(const std::wstring &path, Snapshot *s) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    std::string data;
    char  buf[4096];
    DWORD n;
    while (ReadFile(f, buf, sizeof(buf), &n, nullptr) && n) data.append(buf, n);
    CloseHandle(f);
    std::wstring text = settings::widen(data), section;
    for (size_t p = 0; p < text.size();) {
        size_t e = text.find(L'\n', p);
        if (e == std::wstring::npos) e = text.size();
        std::wstring line = text.substr(p, e - p);
        p = e + 1;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty()) continue;
        if (line[0] == L'[') { section = line; continue; }
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = line.substr(0, eq), v = line.substr(eq + 1);
        if (section == L"[backup]") {
            if (k == L"usb") s->usb = (unsigned)wcstoul(v.c_str(), nullptr, 16) << 16 | (unsigned)wcstoul(v.c_str() + 5, nullptr, 16);
            else if (k == L"firmware") s->firmware = _wtoi(v.c_str());
            else if (k == L"layers") s->layers = _wtoi(v.c_str());
            else if (k == L"rows") s->rows = _wtoi(v.c_str());
            else if (k == L"cols") s->cols = _wtoi(v.c_str());
            else if (k == L"encoders") s->encoders = _wtoi(v.c_str());
        } else if (section == L"[keys]") {
            int l = _wtoi(k.c_str());
            if (s->keys.empty()) s->keys.assign(s->layers * s->rows * s->cols, 0);
            const wchar_t *c = v.c_str();
            for (int i = 0; i < s->rows * s->cols && *c; i++) {
                wchar_t *end;
                unsigned code = (unsigned)wcstoul(c, &end, 16);
                if (end == c) break;
                if (l >= 0 && l < s->layers) s->keys[l * s->rows * s->cols + i] = (uint16_t)code;
                c = end;
            }
        } else if (section == L"[knob]") {
            int i = _wtoi(k.c_str());
            if (s->knob.empty()) s->knob.assign(s->layers * s->encoders * 2, 0);
            if (i >= 0 && i < (int)s->knob.size()) s->knob[i] = (uint16_t)wcstoul(v.c_str(), nullptr, 16);
        } else if (section == L"[macros]") {
            if (k == L"read") s->has_macros = v == L"1";
            else if (k.size() > 1 && k[0] == L'M') {
                size_t m = (size_t)_wtoi(k.c_str() + 1);
                if (m < 256) {
                    if (s->macros.size() <= m) s->macros.resize(m + 1);
                    s->macros[m] = v;
                }
            }
        }
    }
    return s->layers > 0 && s->rows > 0 && s->cols > 0;
}

// ---------- the check window ----------
enum Kind { KEY, KNOB, MACRO };
struct Diff {
    Kind         kind;
    int          layer, a, b; // key: row, col; knob: encoder, clockwise; macro: number
    uint16_t     yours, now;
    std::wstring yours_text, now_text;
    bool         keep; // put yours back
};

Snapshot          g_backup, g_now;
std::vector<Diff> g_diffs;
std::wstring      g_file;
bool              g_after_update = false;
HWND              g_dlg = nullptr;

std::wstring name(uint16_t code) { return keycodes::describe(code, GetKeyboardLayout(0), g_board->custom_names); }

std::wstring where(const Diff &d) {
    if (d.kind == MACRO) return L"Macro M" + std::to_wstring(d.a);
    if (d.kind == KNOB) return L"Layer " + std::to_wstring(d.layer) + L", knob " + (d.b ? L"turn right" : L"turn left");
    // the key by what it does on the base layer in use (ZXKB firmware reports it; else layer 0), named through the
    // Windows language layout. Never by keycap legends: those are a personal display choice (D0k, 2026-11-06)
    int base_layer = zxkb_board() && g_board->protocol >= 11 && g_st.base_layer < g_now.layers ? g_st.base_layer : 0;
    uint16_t base = g_now.keys[(base_layer * g_now.rows + d.a) * g_now.cols + d.b];
    return L"Layer " + std::to_wstring(d.layer) + L", key " + name(base) + L" (" + std::to_wstring(d.a) + L"," + std::to_wstring(d.b) + L")";
}

std::wstring macro_text(const std::wstring &t) { return t.empty() ? L"(empty)" : t.size() > 40 ? t.substr(0, 40) + L"..." : t; }

void find_diffs() {
    g_diffs.clear();
    int layers = g_backup.layers < g_now.layers ? g_backup.layers : g_now.layers;
    bool same_matrix = g_backup.rows == g_now.rows && g_backup.cols == g_now.cols;
    for (int l = 0; same_matrix && l < layers; l++)
        for (int r = 0; r < g_now.rows; r++)
            for (int c = 0; c < g_now.cols; c++) {
                int i = l * g_now.rows * g_now.cols + r * g_now.cols + c;
                if (i >= (int)g_backup.keys.size() || g_backup.keys[i] == g_now.keys[i]) continue;
                Diff d{KEY, l, r, c, g_backup.keys[i], g_now.keys[i], name(g_backup.keys[i]), name(g_now.keys[i]),
                       keystore::is_kept(L"keys", l, r, c)};
                g_diffs.push_back(d);
            }
    int encs = g_backup.encoders < g_now.encoders ? g_backup.encoders : g_now.encoders;
    for (int l = 0; l < layers; l++)
        for (int e = 0; e < encs; e++)
            for (int cw = 0; cw < 2; cw++) {
                int ib = (l * g_backup.encoders + e) * 2 + cw, in = (l * g_now.encoders + e) * 2 + cw;
                if (ib >= (int)g_backup.knob.size() || in >= (int)g_now.knob.size() || g_backup.knob[ib] == g_now.knob[in]) continue;
                Diff d{KNOB, l, e, cw, g_backup.knob[ib], g_now.knob[in], name(g_backup.knob[ib]), name(g_now.knob[in]),
                       keystore::is_kept(L"knob", l, e, cw)};
                g_diffs.push_back(d);
            }
    if (g_backup.has_macros && g_now.has_macros)
        for (size_t m = 0; m < g_now.macros.size(); m++) {
            std::wstring y = m < g_backup.macros.size() ? g_backup.macros[m] : L"";
            if (y == g_now.macros[m]) continue;
            // macros are always the user's own: yours put back unless the backup's is empty
            Diff d{MACRO, 0, (int)m, 0, 0, 0, macro_text(y), macro_text(g_now.macros[m]), !y.empty()};
            g_diffs.push_back(d);
        }
}

void fill() {
    HWND lv = GetDlgItem(g_dlg, IDC_LC_LIST);
    ListView_DeleteAllItems(lv);
    for (size_t i = 0; i < g_diffs.size(); i++) {
        std::wstring w = where(g_diffs[i]);
        LVITEMW it = {};
        it.mask    = LVIF_TEXT;
        it.iItem   = (int)i;
        it.pszText = const_cast<wchar_t *>(w.c_str());
        ListView_InsertItem(lv, &it);
        ListView_SetItemText(lv, (int)i, 1, const_cast<wchar_t *>(g_diffs[i].yours_text.c_str()));
        ListView_SetItemText(lv, (int)i, 2, const_cast<wchar_t *>(g_diffs[i].now_text.c_str()));
        ListView_SetCheckState(lv, (int)i, g_diffs[i].keep);
    }
    std::wstring info;
    if (g_diffs.empty()) {
        info = g_after_update ? L"Checked: the keyboard's key layout, knob and macros match the copy taken before the update. Nothing to put back."
                              : L"The keyboard's key layout, knob and macros match this backup.";
    } else {
        info = std::to_wstring(g_diffs.size()) + (g_diffs.size() == 1 ? L" difference" : L" differences") +
               L" between the copy (yours) and the keyboard now. Ticked: yours is put back. Unticked: the keyboard keeps what it has "
               L"(after an update, the new firmware's own keys). Keys you changed in ZXKB and your macros start ticked.";
    }
    SetDlgItemTextW(g_dlg, IDC_LC_INFO, info.c_str());
    EnableWindow(GetDlgItem(g_dlg, IDC_LC_APPLY), !g_diffs.empty());
    SetDlgItemTextW(g_dlg, IDCANCEL, g_diffs.empty() ? L"Close" : L"Keep the keyboard as it is");
}

bool put_back(std::wstring *report) {
    uint8_t r[proto::REPORT];
    int     failed = 0, done = 0;
    bool    macros_changed = false;
    std::vector<std::wstring> macros = g_now.macros;
    for (const Diff &d : g_diffs) {
        if (!d.keep) continue;
        if (d.kind == KEY) {
            uint8_t q[6] = {0x05, (uint8_t)d.layer, (uint8_t)d.a, (uint8_t)d.b, (uint8_t)(d.yours >> 8), (uint8_t)d.yours};
            if (keyboard_ask(q, 6, r, 4)) { keystore::record(d.layer, d.a, d.b, d.yours); done++; }
            else failed++;
        } else if (d.kind == KNOB) {
            uint8_t q[6] = {0x15, (uint8_t)d.layer, (uint8_t)d.a, (uint8_t)d.b, (uint8_t)(d.yours >> 8), (uint8_t)d.yours};
            if (keyboard_ask(q, 6, r, 4)) { keystore::record_knob(d.layer, d.a, d.b != 0, d.yours); done++; }
            else failed++;
        } else {
            macros[d.a] = d.a < (int)g_backup.macros.size() ? g_backup.macros[d.a] : L"";
            macros_changed = true;
        }
    }
    if (macros_changed) {
        std::wstring e;
        if (macros::write_all(macros, &e)) done++;
        else { failed++; *report += L"Macros: " + e + L"\n"; }
    }
    // read again and check: every ticked one is yours now, every unticked one as it was
    Snapshot after;
    if (!read(&after)) { *report += L"The keyboard could not be read again to check."; return false; }
    int wrong = 0;
    for (const Diff &d : g_diffs) {
        uint16_t want = d.keep ? d.yours : d.now;
        if (d.kind == KEY) wrong += after.keys[d.layer * after.rows * after.cols + d.a * after.cols + d.b] != want;
        else if (d.kind == KNOB) wrong += after.knob[(d.layer * after.encoders + d.a) * 2 + d.b] != want;
        else if (after.has_macros) {
            std::wstring w = d.keep ? (d.a < (int)g_backup.macros.size() ? g_backup.macros[d.a] : L"") : g_now.macros[d.a];
            wrong += d.a < (int)after.macros.size() && after.macros[d.a] != w;
        }
    }
    if (!wrong && !failed) *report += L"Put back and checked: the keyboard now matches your choices.";
    else *report += std::to_wstring(wrong + failed) + L" did not take. The list shows the keyboard as it is now.";
    g_now = after;
    return !wrong && !failed;
}

INT_PTR CALLBACK proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG: {
            g_dlg   = dlg;
            HWND lv = GetDlgItem(dlg, IDC_LC_LIST);
            ListView_SetExtendedListViewStyle(lv, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
            const wchar_t *heads[3] = {L"Where", L"Yours (the copy)", L"Keyboard now"};
            int widths[3] = {230, 150, 150};
            RECT rc;
            GetClientRect(lv, &rc);
            int total = rc.right - 20;
            for (int c = 0; c < 3; c++) {
                LVCOLUMNW col = {};
                col.mask    = LVCF_TEXT | LVCF_WIDTH;
                col.pszText = const_cast<wchar_t *>(heads[c]);
                col.cx      = total * widths[c] / 530;
                ListView_InsertColumn(lv, c, &col);
            }
            std::wstring f = g_file.substr(g_file.find_last_of(L'\\') + 1);
            SetDlgItemTextW(dlg, IDC_LC_FILE, (L"Copy: " + f).c_str());
            fill();
            return TRUE;
        }
        case WM_NOTIFY: {
            auto *h = reinterpret_cast<NMHDR *>(lp);
            if (h->idFrom == IDC_LC_LIST && h->code == LVN_ITEMCHANGED) {
                auto *n = reinterpret_cast<NMLISTVIEW *>(lp);
                if (n->iItem >= 0 && n->iItem < (int)g_diffs.size() && (n->uChanged & LVIF_STATE))
                    g_diffs[n->iItem].keep = ListView_GetCheckState(h->hwndFrom, n->iItem) != 0;
            }
            return FALSE;
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_LC_ALL: case IDC_LC_NONE:
                    for (size_t i = 0; i < g_diffs.size(); i++) ListView_SetCheckState(GetDlgItem(dlg, IDC_LC_LIST), (int)i, LOWORD(wp) == IDC_LC_ALL);
                    return TRUE;
                case IDC_LC_APPLY: {
                    SetCursor(LoadCursor(nullptr, IDC_WAIT));
                    std::wstring report;
                    bool ok = put_back(&report);
                    MessageBoxW(dlg, report.c_str(), L"ZXKB", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONWARNING));
                    find_diffs(); // what still differs from the copy (the ones left to the keyboard, or that did not take)
                    for (Diff &d : g_diffs) d.keep = false;
                    fill();
                    if (ok) EndDialog(dlg, 1);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(dlg, 0);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

} // namespace

std::wstring folder() {
    std::wstring d = settings::folder() + L"\\backups";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

bool possible() { return g_board && g_board->via_protocol && g_board->rows > 0 && g_board->cols > 0 && layer_count() > 0; }

std::wstring backup(const std::wstring &why, std::wstring *error) {
    Snapshot s;
    if (!read(&s)) { *error = L"The keyboard's key layout could not be read."; return L""; }
    wchar_t name[96];
    time_t  t = time(nullptr);
    tm      lt;
    localtime_s(&lt, &t);
    swprintf(name, 96, L"\\%04X_%04X %04d-%02d-%02d %02d%02d%02d v%d.ini", s.usb >> 16, s.usb & 0xFFFF, lt.tm_year + 1900, lt.tm_mon + 1,
             lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, s.firmware);
    std::wstring path = folder() + name;
    if (!save(s, path, why)) { *error = L"The backup could not be written to " + path + L"."; return L""; }
    return path;
}

void check(HWND owner, const std::wstring &file, bool after_update) {
    g_backup = Snapshot();
    g_now    = Snapshot();
    g_file   = file;
    g_after_update = after_update;
    if (!load(file, &g_backup)) { MessageBoxW(owner, L"That backup could not be read.", L"ZXKB", MB_OK | MB_ICONWARNING); return; }
    if (!g_board || g_backup.usb != ((unsigned)g_board->vid << 16 | g_board->pid)) {
        MessageBoxW(owner, L"That backup is from a different keyboard.", L"ZXKB", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!read(&g_now)) { MessageBoxW(owner, L"The keyboard's key layout could not be read.", L"ZXKB", MB_OK | MB_ICONWARNING); return; }
    find_diffs();
    DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_LAYOUTCHECK), owner, proc, 0);
    g_dlg = nullptr;
}

void choose_and_check(HWND owner) {
    wchar_t path[MAX_PATH] = {0};
    std::wstring dir = folder();
    OPENFILENAMEW of = {sizeof(of)};
    of.hwndOwner       = owner;
    of.lpstrFilter     = L"ZXKB layout backups (*.ini)\0*.ini\0";
    of.lpstrFile       = path;
    of.nMaxFile        = MAX_PATH;
    of.lpstrInitialDir = dir.c_str();
    of.lpstrTitle      = L"Put back a key layout backup";
    of.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&of)) check(owner, path, false);
}

} // namespace layoutcheck
