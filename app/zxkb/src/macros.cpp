// Macros stored on the keyboard. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
// [P] QMK macro storage (quantum/dynamic_keymap.c, quantum/send_string/send_string.c, master f5db962): the macros
// one after another, each ending in a 0 byte; inside, 0x01 0x01 k taps key k, 0x01 0x02 k holds it, 0x01 0x03 k
// lets it go, 0x01 0x04 digits waits that many ms (VIA ends the digits with '|'); other bytes are typed as ASCII.
// VIA: 0x0C macro count, 0x0D buffer size, 0x0E read [offset hi, lo, n <= 28], 0x0F write [same + bytes].
#include "macros.h"
#include "app.h"
#include "keystore.h"
#include "kbsettings.h"
#include "preview.h"
#include "labels.h"
#include "settings.h"
#include "../res/resource.h"
#include <commdlg.h>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <cwctype>

bool zxkb_board(); // main.cpp

namespace macros {
namespace {

struct KeyName { const wchar_t *name; uint8_t usage; };
const KeyName names[] = {
    {L"Enter", 0x28}, {L"Esc", 0x29}, {L"Backspace", 0x2A}, {L"Tab", 0x2B}, {L"Space", 0x2C}, {L"CapsLock", 0x39},
    {L"F1", 0x3A}, {L"F2", 0x3B}, {L"F3", 0x3C}, {L"F4", 0x3D}, {L"F5", 0x3E}, {L"F6", 0x3F}, {L"F7", 0x40}, {L"F8", 0x41},
    {L"F9", 0x42}, {L"F10", 0x43}, {L"F11", 0x44}, {L"F12", 0x45}, {L"PrtSc", 0x46}, {L"ScrollLock", 0x47}, {L"Pause", 0x48},
    {L"Insert", 0x49}, {L"Home", 0x4A}, {L"PgUp", 0x4B}, {L"Delete", 0x4C}, {L"End", 0x4D}, {L"PgDn", 0x4E},
    {L"Right", 0x4F}, {L"Left", 0x50}, {L"Down", 0x51}, {L"Up", 0x52}, {L"NumLock", 0x53}, {L"Menu", 0x65},
    {L"F13", 0x68}, {L"F14", 0x69}, {L"F15", 0x6A}, {L"F16", 0x6B}, {L"F17", 0x6C}, {L"F18", 0x6D}, {L"F19", 0x6E},
    {L"F20", 0x6F}, {L"F21", 0x70}, {L"F22", 0x71}, {L"F23", 0x72}, {L"F24", 0x73},
    {L"Mute", 0xA8}, {L"VolumeUp", 0xA9}, {L"VolumeDown", 0xAA}, {L"NextTrack", 0xAB}, {L"PrevTrack", 0xAC}, {L"PlayPause", 0xAE},
    {L"Ctrl", 0xE0}, {L"Shift", 0xE1}, {L"Alt", 0xE2}, {L"Win", 0xE3}, {L"RCtrl", 0xE4}, {L"RShift", 0xE5}, {L"AltGr", 0xE6}, {L"RWin", 0xE7},
};

// usage -> name: letters and digits as themselves ("c", "1"), the rest from the table, else "key 0xNN"
std::wstring name_of(uint8_t u) {
    if (u >= 0x04 && u <= 0x1D) return std::wstring(1, (wchar_t)(L'a' + u - 0x04));
    if (u >= 0x1E && u <= 0x26) return std::wstring(1, (wchar_t)(L'1' + u - 0x1E));
    if (u == 0x27) return L"0";
    for (const KeyName &k : names)
        if (k.usage == u) return k.name;
    wchar_t s[16];
    swprintf(s, 16, L"key 0x%02X", u);
    return s;
}

int usage_of(std::wstring n) {
    while (!n.empty() && n.front() == L' ') n.erase(0, 1);
    while (!n.empty() && n.back() == L' ') n.pop_back();
    if (n.size() == 1) {
        wchar_t c = (wchar_t)towlower(n[0]);
        if (c >= L'a' && c <= L'z') return 0x04 + (c - L'a');
        if (c >= L'1' && c <= L'9') return 0x1E + (c - L'1');
        if (c == L'0') return 0x27;
    }
    for (const KeyName &k : names)
        if (!_wcsicmp(k.name, n.c_str())) return k.usage;
    if (!_wcsnicmp(n.c_str(), L"key ", 4)) return (int)wcstol(n.c_str() + 4, nullptr, 16);
    return -1;
}

void put(std::string &out, uint8_t code, uint8_t usage) { out += '\x01'; out += (char)code; out += (char)usage; }

} // namespace

bool ranges_possible() { return zxkb_board() && g_board->protocol >= 14; }

std::string to_bytes(const std::wstring &text, std::wstring *error) {
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        wchar_t c = text[i];
        if (c == L'{' && i + 1 < text.size() && text[i + 1] == L'{') { out += '{'; i++; continue; }
        if (c == L'{') {
            size_t e = text.find(L'}', i);
            if (e == std::wstring::npos) { *error = L"A { has no closing }."; return ""; }
            std::wstring tok = text.substr(i + 1, e - i - 1);
            i = e;
            if (!_wcsnicmp(tok.c_str(), L"wait ", 5)) {
                const wchar_t *p = tok.c_str() + 5;
                int lo = _wtoi(p), hi = lo;
                if (const wchar_t *dash = wcschr(p, L'-')) hi = _wtoi(dash + 1);
                if (hi < lo) { int t = lo; lo = hi; hi = t; }
                if (lo == hi) {
                    if (lo <= 0 || lo > 60000) { *error = L"{wait N} needs N from 1 to 60000 ms."; return ""; }
                    out += "\x01\x04" + std::to_string(lo) + "|";
                } else {
                    if (lo < 0 || hi > 60000) { *error = L"{wait A-B} needs A and B from 0 to 60000 ms."; return ""; }
                    if (!ranges_possible()) {
                        *error = L"Waits that vary, such as {wait " + std::to_wstring(lo) + L"-" + std::to_wstring(hi) +
                                 L"}, need ZXKB firmware v30 on the keyboard; use a fixed wait.";
                        return "";
                    }
                    // ZXKB's own code (firmware v30 on): a wait picked at random between the two each time it plays
                    out += "\x01\x05" + std::to_string(lo) + "-" + std::to_string(hi) + "|";
                }
                continue;
            }
            bool down = false, up = false;
            std::wstring low = tok;
            for (wchar_t &ch : low) ch = (wchar_t)towlower(ch);
            if (low.size() > 5 && low.compare(low.size() - 5, 5, L" down") == 0) { down = true; tok.erase(tok.size() - 5); }
            else if (low.size() > 3 && low.compare(low.size() - 3, 3, L" up") == 0) { up = true; tok.erase(tok.size() - 3); }
            // "Ctrl+Shift+Esc": hold all but the last, tap the last, let go in reverse
            std::vector<int> keys;
            for (size_t p = 0;;) {
                size_t plus = tok.find(L'+', p);
                std::wstring part = tok.substr(p, plus == std::wstring::npos ? std::wstring::npos : plus - p);
                int u = usage_of(part);
                if (u < 0) { *error = L"Unknown key {" + part + L"}."; return ""; }
                keys.push_back(u);
                if (plus == std::wstring::npos) break;
                p = plus + 1;
            }
            if ((down || up) && keys.size() != 1) { *error = L"down / up take one key, e.g. {Ctrl down}."; return ""; }
            if (down) put(out, 2, (uint8_t)keys[0]);
            else if (up) put(out, 3, (uint8_t)keys[0]);
            else {
                for (size_t k = 0; k + 1 < keys.size(); k++) put(out, 2, (uint8_t)keys[k]);
                put(out, 1, (uint8_t)keys.back());
                for (size_t k = keys.size() - 1; k-- > 0;) put(out, 3, (uint8_t)keys[k]);
            }
            continue;
        }
        if (c == L'\r' || c == L'\n') { put(out, 1, 0x28); if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') i++; continue; }
        if (c == L'\t') { put(out, 1, 0x2B); continue; }
        if (c < 0x20 || c > 0x7E) { *error = L"Only plain ASCII characters can be typed by a macro (QMK)."; return ""; }
        out += (char)c;
    }
    return out;
}

std::wstring to_text(const std::string &b) {
    std::wstring t;
    for (size_t i = 0; i < b.size(); i++) {
        unsigned char c = (unsigned char)b[i];
        if (c == 1 && i + 2 < b.size() && b[i + 1] == 2 && (unsigned char)b[i + 2] >= 0xE0 && (unsigned char)b[i + 2] <= 0xE7) {
            // modifiers held, one key tapped, the modifiers let go in reverse: shown as {Ctrl+Shift+c}
            std::vector<uint8_t> mods;
            size_t j = i;
            while (j + 2 < b.size() && b[j] == 1 && b[j + 1] == 2 && (unsigned char)b[j + 2] >= 0xE0 && (unsigned char)b[j + 2] <= 0xE7) {
                mods.push_back((uint8_t)b[j + 2]);
                j += 3;
            }
            bool combo = j + 2 < b.size() && b[j] == 1 && b[j + 1] == 1;
            size_t k = j + 3;
            for (size_t m = mods.size(); combo && m-- > 0; k += 3)
                combo = k + 2 < b.size() && b[k] == 1 && b[k + 1] == 3 && (uint8_t)b[k + 2] == mods[m];
            if (combo) {
                std::wstring n;
                for (uint8_t m : mods) n += name_of(m) + L"+";
                t += L"{" + n + name_of((uint8_t)b[j + 2]) + L"}";
                i = k - 1;
                continue;
            }
        }
        if (c == 1 && i + 1 < b.size()) {
            unsigned char op = (unsigned char)b[++i];
            if (op == 4) { // wait: digits, then (VIA) a '|'
                int ms = 0;
                while (i + 1 < b.size() && isdigit((unsigned char)b[i + 1])) ms = ms * 10 + (b[++i] - '0');
                if (i + 1 < b.size() && b[i + 1] == '|') i++;
                t += L"{wait " + std::to_wstring(ms) + L"}";
            } else if (op == 5) { // ZXKB: a wait that varies, min '-' max '|'
                int lo = 0, hi = 0;
                while (i + 1 < b.size() && isdigit((unsigned char)b[i + 1])) lo = lo * 10 + (b[++i] - '0');
                if (i + 1 < b.size() && b[i + 1] == '-') {
                    i++;
                    while (i + 1 < b.size() && isdigit((unsigned char)b[i + 1])) hi = hi * 10 + (b[++i] - '0');
                }
                if (i + 1 < b.size() && b[i + 1] == '|') i++;
                t += L"{wait " + std::to_wstring(lo) + L"-" + std::to_wstring(hi) + L"}";
            } else if ((op == 1 || op == 2 || op == 3) && i + 1 < b.size()) {
                std::wstring n = name_of((uint8_t)b[++i]);
                t += L"{" + n + (op == 2 ? L" down" : op == 3 ? L" up" : L"") + L"}";
            }
            continue;
        }
        if (c == '{') t += L"{{";
        else if (c >= 0x20 && c <= 0x7E) t += (wchar_t)c;
    }
    return t;
}

std::wstring key_token(uint16_t sc, bool down) {
    for (int u = 0; u < 0x74; u++)
        if (labels::scan_of_usage((uint8_t)u) == sc && sc) return L"{" + name_of((uint8_t)u) + (down ? L" down}" : L" up}");
    for (int u = 0xE0; u <= 0xE7; u++)
        if (labels::scan_of_usage((uint8_t)u) == sc) return L"{" + name_of((uint8_t)u) + (down ? L" down}" : L" up}");
    return L"";
}

bool read_all(std::vector<std::wstring> *texts, int *buffer_size) {
    texts->clear();
    uint8_t r[32];
    uint8_t qc[1] = {0x0C}, qs[1] = {0x0D};
    if (!keyboard_ask(qc, 1, r, 1)) return false;
    int count = r[1];
    if (!keyboard_ask(qs, 1, r, 1)) return false;
    int size = r[1] << 8 | r[2];
    *buffer_size = size;
    std::string buf;
    for (int off = 0; off < size; off += 28) {
        uint8_t n = (uint8_t)(size - off < 28 ? size - off : 28);
        uint8_t q[4] = {0x0E, (uint8_t)(off >> 8), (uint8_t)off, n};
        if (!keyboard_ask(q, 4, r, 4)) return false;
        buf.append((const char *)r + 4, n);
    }
    size_t p = 0;
    for (int m = 0; m < count; m++) {
        size_t z = buf.find('\0', p);
        if (z == std::string::npos) z = buf.size();
        texts->push_back(p < buf.size() ? to_text(buf.substr(p, z - p)) : L"");
        p = z + 1;
    }
    return true;
}

bool write_all(const std::vector<std::wstring> &texts, std::wstring *error) {
    uint8_t r[32];
    uint8_t qs[1] = {0x0D};
    if (!keyboard_ask(qs, 1, r, 1)) { *error = L"The keyboard did not answer."; return false; }
    int size = r[1] << 8 | r[2];
    std::string buf;
    for (size_t m = 0; m < texts.size(); m++) {
        std::wstring e;
        std::string b = to_bytes(texts[m], &e);
        if (!e.empty()) { *error = L"M" + std::to_wstring(m) + L": " + e; return false; }
        buf += b;
        buf += '\0';
    }
    if ((int)buf.size() > size) {
        *error = L"The macros need " + std::to_wstring(buf.size()) + L" bytes; the keyboard has room for " + std::to_wstring(size) + L".";
        return false;
    }
    buf.resize(size, '\0'); // the rest cleared; the last byte 0 marks a complete write (dynamic_keymap.c)
    for (int off = 0; off < size; off += 28) {
        uint8_t n = (uint8_t)(size - off < 28 ? size - off : 28);
        uint8_t q[32] = {0x0F, (uint8_t)(off >> 8), (uint8_t)off, n};
        std::memcpy(q + 4, buf.data() + off, n);
        if (!keyboard_ask(q, 4 + n, r, 4)) { *error = L"The keyboard did not take the macros."; return false; }
    }
    // kept by ZXKB, to put back after a firmware update (VIA clears macros then, as it does the keymap)
    keystore::record_macros(texts);
    return true;
}

} // namespace macros

// ---------- the Macros window ----------
namespace {
HWND g_mdlg = nullptr;
std::vector<std::wstring> g_texts;
int  g_size = 0, g_sel = 0;
bool g_recording = false, g_filling = false;
DWORD g_last_key = 0;

int used_bytes() {
    size_t n = 0;
    for (const std::wstring &t : g_texts) {
        std::wstring e;
        n += macros::to_bytes(t, &e).size() + 1;
    }
    return (int)n;
}

void fill_list() {
    HWND l = GetDlgItem(g_mdlg, IDC_MC_LIST);
    SendMessageW(l, LB_RESETCONTENT, 0, 0);
    for (size_t m = 0; m < g_texts.size(); m++) {
        std::wstring t = L"M" + std::to_wstring(m) + L":  " + (g_texts[m].empty() ? L"(empty)" : g_texts[m]);
        SendMessageW(l, LB_ADDSTRING, 0, (LPARAM)t.c_str());
    }
    SendMessageW(l, LB_SETCURSEL, g_sel, 0);
    int used = used_bytes();
    SetDlgItemTextW(g_mdlg, IDC_MC_INFO, (std::to_wstring(g_texts.size()) + L" macros on the keyboard, " + std::to_wstring(used) + L" of " +
                                          std::to_wstring(g_size) + L" bytes used. Give them to keys in Change keys (Macros).").c_str());
}

void show_sel() {
    g_filling = true;
    SetDlgItemTextW(g_mdlg, IDC_MC_EDIT, g_sel < (int)g_texts.size() ? g_texts[g_sel].c_str() : L"");
    g_filling = false;
}

void record(bool on) {
    g_recording = on;
    SetDlgItemTextW(g_mdlg, IDC_MC_RECORD, on ? L"Stop recording" : L"Record");
    // while recording, key presses come as raw input to this window (the preview gets them back after)
    RAWINPUTDEVICE rid = {0x01, 0x06, 0, on ? g_mdlg : nullptr};
    if (on) RegisterRawInputDevices(&rid, 1, sizeof(rid));
    else preview_input_back();
    g_last_key = GetTickCount();
    if (on) SetFocus(g_mdlg); // keys must not type into the boxes or press the buttons
}

INT_PTR CALLBACK mproc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG: {
            g_mdlg = dlg;
            if (!macros::read_all(&g_texts, &g_size)) SetDlgItemTextW(dlg, IDC_MC_INFO, L"The keyboard's macros could not be read.");
            g_sel = 0;
            fill_list();
            show_sel();
            CheckDlgButton(dlg, IDC_MC_WAITS, BST_CHECKED);
            SetDlgItemInt(dlg, IDC_MC_VARY, 0, FALSE);
            EnableWindow(GetDlgItem(dlg, IDC_MC_VARY), macros::ranges_possible());
            SetDlgItemTextW(dlg, IDC_MC_VARY_NOTE, macros::ranges_possible() ? L"ms each time it plays (0 = exact)"
                                                                             : L"(waits that vary need ZXKB firmware v30)");
            { // recording on the keyboard itself, if its firmware can
                int  rec = 0;
                bool kb  = kb_record_macro(&rec);
                HWND cb  = GetDlgItem(dlg, IDC_MC_KB_SLOT);
                for (size_t m = 0; m < g_texts.size(); m++)
                    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)(L"M" + std::to_wstring(m)).c_str());
                SendMessageW(cb, CB_SETCURSEL, kb && rec < (int)g_texts.size() ? rec : 0, 0);
                EnableWindow(cb, kb);
                SetDlgItemTextW(dlg, IDC_MC_KB_LABEL, kb ? L"Recording on the keyboard: the record key (Fn+M as delivered) starts and stops it, into"
                                                          : L"This keyboard cannot record macros by itself (that needs ZXKB firmware v29): use Record above.");
                ShowWindow(cb, kb ? SW_SHOW : SW_HIDE);
            }
            return TRUE;
        }
        case WM_INPUT: {
            if (!g_recording) break;
            RAWINPUT ri;
            UINT size = sizeof(ri);
            if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1 || ri.header.dwType != RIM_TYPEKEYBOARD) break;
            bool down = !(ri.data.keyboard.Flags & RI_KEY_BREAK);
            uint16_t sc = (uint16_t)(ri.data.keyboard.MakeCode | (ri.data.keyboard.Flags & RI_KEY_E0 ? 0xE000 : 0));
            std::wstring tok = macros::key_token(sc, down);
            if (tok.empty() || g_sel >= (int)g_texts.size()) break;
            DWORD now = GetTickCount(), gap = now - g_last_key;
            g_last_key = now;
            // [A] waits shorter than 30 ms are typing rhythm, not intended pauses; kept to 10 ms steps
            if (IsDlgButtonChecked(dlg, IDC_MC_WAITS) == BST_CHECKED && gap >= 30 && g_texts[g_sel].size()) {
                int ms = (int)(gap / 10 * 10), vary = macros::ranges_possible() ? (int)GetDlgItemInt(dlg, IDC_MC_VARY, nullptr, FALSE) : 0;
                g_texts[g_sel] += vary > 0 ? L"{wait " + std::to_wstring(ms > vary ? ms - vary : 0) + L"-" + std::to_wstring(ms + vary) + L"}"
                                           : L"{wait " + std::to_wstring(ms) + L"}";
            }
            g_texts[g_sel] += tok;
            show_sel();
            fill_list();
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_MC_LIST:
                    if (HIWORD(wp) == LBN_SELCHANGE) {
                        g_sel = (int)SendDlgItemMessageW(dlg, IDC_MC_LIST, LB_GETCURSEL, 0, 0);
                        show_sel();
                    }
                    return TRUE;
                case IDC_MC_EDIT:
                    if (HIWORD(wp) == EN_CHANGE && !g_filling && g_sel < (int)g_texts.size()) {
                        wchar_t buf[2048] = {0};
                        GetDlgItemTextW(dlg, IDC_MC_EDIT, buf, 2048);
                        g_texts[g_sel] = buf;
                        int keep = g_sel;
                        fill_list();
                        g_sel = keep;
                    }
                    return TRUE;
                case IDC_MC_RECORD: record(!g_recording); return TRUE;
                case IDC_MC_KB_SLOT:
                    if (HIWORD(wp) == CBN_SELCHANGE) {
                        int m = (int)SendDlgItemMessageW(dlg, IDC_MC_KB_SLOT, CB_GETCURSEL, 0, 0);
                        if (m >= 0 && !kb_set_record_macro(m))
                            MessageBoxW(dlg, L"The keyboard did not take the change.", L"ZXKB", MB_OK | MB_ICONWARNING);
                    }
                    return TRUE;
                case IDC_MC_CLEARONE:
                    if (g_sel < (int)g_texts.size()) { g_texts[g_sel].clear(); show_sel(); fill_list(); }
                    return TRUE;
                case IDC_MC_SAVE: {
                    if (g_recording) record(false);
                    std::wstring e;
                    if (macros::write_all(g_texts, &e)) MessageBoxW(dlg, L"The macros are on the keyboard.", L"ZXKB", MB_OK | MB_ICONINFORMATION);
                    else MessageBoxW(dlg, e.c_str(), L"ZXKB", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                case IDC_MC_EXPORT: case IDC_MC_IMPORT: {
                    bool save = LOWORD(wp) == IDC_MC_EXPORT;
                    wchar_t path[MAX_PATH] = L"ZXKB macros.txt";
                    OPENFILENAMEW of = {sizeof(of)};
                    of.hwndOwner = dlg; of.lpstrFilter = L"Text (*.txt)\0*.txt\0"; of.lpstrFile = path; of.nMaxFile = MAX_PATH;
                    of.lpstrDefExt = L"txt"; of.Flags = save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST;
                    if (!(save ? GetSaveFileNameW(&of) : GetOpenFileNameW(&of))) return TRUE;
                    if (save) { // one macro a line, "M0: text" (UTF-8)
                        std::string out;
                        for (size_t m = 0; m < g_texts.size(); m++) out += "M" + std::to_string(m) + ": " + settings::narrow(g_texts[m]) + "\r\n";
                        HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
                        DWORD n;
                        if (f != INVALID_HANDLE_VALUE) { WriteFile(f, out.data(), (DWORD)out.size(), &n, nullptr); CloseHandle(f); }
                    } else {
                        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
                        if (f == INVALID_HANDLE_VALUE) return TRUE;
                        DWORD sz = GetFileSize(f, nullptr), n = 0;
                        std::string in(sz < 1 << 20 ? sz : 0, '\0');
                        ReadFile(f, &in[0], (DWORD)in.size(), &n, nullptr);
                        CloseHandle(f);
                        for (size_t p = 0; p < in.size();) {
                            size_t e = in.find('\n', p);
                            std::string line = in.substr(p, e == std::string::npos ? std::string::npos : e - p);
                            if (!line.empty() && line.back() == '\r') line.pop_back();
                            if (line.size() > 1 && line[0] == 'M') {
                                size_t c = line.find(':');
                                int m = atoi(line.c_str() + 1);
                                if (c != std::string::npos && m >= 0 && m < (int)g_texts.size())
                                    g_texts[m] = settings::widen(line.substr(c + 1 + (c + 1 < line.size() && line[c + 1] == ' ')));
                            }
                            if (e == std::string::npos) break;
                            p = e + 1;
                        }
                        show_sel();
                        fill_list();
                        MessageBoxW(dlg, L"Imported. Press Save to keyboard to put them on the keyboard.", L"ZXKB", MB_OK | MB_ICONINFORMATION);
                    }
                    return TRUE;
                }
                case IDCANCEL: case IDOK:
                    if (g_recording) record(false);
                    DestroyWindow(dlg);
                    return TRUE;
            }
            break;
        case WM_DESTROY:
            g_mdlg = nullptr;
            return TRUE;
    }
    return FALSE;
}
} // namespace

void macros_dialog(HWND owner) {
    if (!g_board || !g_board->via_protocol || macro_count() <= 0) {
        MessageBoxW(owner, L"This keyboard does not keep macros (no VIA macros).", L"ZXKB", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (g_mdlg) { SetForegroundWindow(g_mdlg); return; }
    g_mdlg = CreateDialogParamW(g_inst, MAKEINTRESOURCEW(IDD_MACROS), owner, mproc, 0);
    if (g_mdlg) ShowWindow(g_mdlg, SW_SHOW);
}

HWND macros_window() { return g_mdlg; }
bool macros_recording() { return g_mdlg && g_recording; }

void macros_recorded_on_keyboard(int macro) {
    if (!g_mdlg) return;
    std::vector<std::wstring> now;
    int size = 0;
    if (!macros::read_all(&now, &size) || macro < 0 || macro >= (int)now.size() || macro >= (int)g_texts.size()) return;
    g_texts[macro] = now[macro]; // only that one: changes not yet saved in the window stay
    fill_list();
    if (g_sel == macro) show_sel();
}
