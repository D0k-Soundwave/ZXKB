// Key changes kept by ZXKB. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "keystore.h"
#include "app.h"
#include "settings.h"
#include "macros.h"
#include <cwchar>
#include <cstdlib>
#include <string>
#include <vector>

namespace keystore {
namespace {

std::wstring file() {
    std::wstring dir = settings::folder() + L"\\keymaps";
    CreateDirectoryW(dir.c_str(), nullptr);
    wchar_t name[24];
    swprintf(name, 24, L"\\%04X_%04X.ini", g_board ? g_board->vid : 0, g_board ? g_board->pid : 0);
    return dir + name;
}

std::wstring key(int layer, int row, int col) {
    wchar_t k[24];
    swprintf(k, 24, L"%d,%d,%d", layer, row, col);
    return k;
}

struct Entry { int layer, row, col; uint16_t code; };

// [keys] "layer,row,col=code (hex)"; [knob] "layer,encoder,clockwise=code"
std::vector<Entry> load(const wchar_t *section = L"keys") {
    std::vector<Entry> v;
    std::wstring f = file();
    std::vector<wchar_t> buf(32768);
    DWORD n = GetPrivateProfileSectionW(section, buf.data(), (DWORD)buf.size(), f.c_str());
    for (const wchar_t *p = buf.data(); p < buf.data() + n && *p; p += wcslen(p) + 1) {
        wchar_t *e;
        Entry x;
        x.layer = (int)wcstol(p, &e, 10);
        if (*e != L',') continue;
        x.row = (int)wcstol(e + 1, &e, 10);
        if (*e != L',') continue;
        x.col = (int)wcstol(e + 1, &e, 10);
        if (*e != L'=') continue;
        x.code = (uint16_t)wcstoul(e + 1, nullptr, 16);
        v.push_back(x);
    }
    return v;
}

} // namespace

void record(int layer, int row, int col, uint16_t code) {
    if (!g_board) return;
    wchar_t c[8];
    swprintf(c, 8, L"%04X", code);
    WritePrivateProfileStringW(L"keys", key(layer, row, col).c_str(), c, file().c_str());
}

void record_knob(int layer, int encoder, bool cw, uint16_t code) {
    if (!g_board) return;
    wchar_t c[8];
    swprintf(c, 8, L"%04X", code);
    WritePrivateProfileStringW(L"knob", key(layer, encoder, cw ? 1 : 0).c_str(), c, file().c_str());
}

void record_macros(const std::vector<std::wstring> &texts) {
    if (!g_board) return;
    std::wstring f = file();
    WritePrivateProfileStringW(L"macros", nullptr, nullptr, f.c_str());
    for (size_t m = 0; m < texts.size(); m++)
        if (!texts[m].empty()) WritePrivateProfileStringW(L"macros", (L"M" + std::to_wstring(m)).c_str(), texts[m].c_str(), f.c_str());
}

std::vector<std::wstring> kept_macros(bool *any) {
    std::vector<std::wstring> v;
    *any = false;
    std::wstring f = file();
    for (int m = 0; m < 128; m++) {
        wchar_t buf[2048] = {0};
        GetPrivateProfileStringW(L"macros", (L"M" + std::to_wstring(m)).c_str(), L"", buf, 2048, f.c_str());
        v.push_back(buf);
        if (*buf) *any = true;
    }
    return v;
}

void forget(int layer, int row, int col) {
    if (g_board) WritePrivateProfileStringW(L"keys", key(layer, row, col).c_str(), nullptr, file().c_str());
}

void forget_all() {
    if (g_board) {
        WritePrivateProfileStringW(L"keys", nullptr, nullptr, file().c_str());
        WritePrivateProfileStringW(L"knob", nullptr, nullptr, file().c_str());
        WritePrivateProfileStringW(L"macros", nullptr, nullptr, file().c_str());
    }
}

int count() {
    if (!g_board) return 0;
    bool any;
    std::vector<std::wstring> m = kept_macros(&any);
    int n = 0;
    for (const std::wstring &t : m) n += !t.empty();
    return (int)(load().size() + load(L"knob").size()) + n;
}

int reapply(int *failed) {
    int changed = 0;
    *failed = 0;
    for (const Entry &e : load()) {
        // [P] VIA get keycode 0x04 [layer, row, col] -> [.., hi, lo]; set 0x05 [layer, row, col, hi, lo]
        uint8_t q[4] = {0x04, (uint8_t)e.layer, (uint8_t)e.row, (uint8_t)e.col}, r[32];
        if (keyboard_ask(q, 4, r, 4) && (uint16_t)(r[4] << 8 | r[5]) == e.code) continue; // already so
        uint8_t s[6] = {0x05, (uint8_t)e.layer, (uint8_t)e.row, (uint8_t)e.col, (uint8_t)(e.code >> 8), (uint8_t)e.code};
        if (keyboard_ask(s, 6, r, 4)) changed++;
        else (*failed)++;
    }
    { // the macros, if the keyboard's differ from the ones ZXKB wrote
        bool any;
        std::vector<std::wstring> want = kept_macros(&any);
        std::vector<std::wstring> now;
        int size = 0;
        if (any && macros::read_all(&now, &size)) {
            want.resize(now.size());
            bool same = true; // compared by what the macros do, not how they are written ({Ctrl+C} = {Ctrl down}{c}{Ctrl up})
            for (size_t m = 0; m < now.size() && same; m++) {
                std::wstring e1, e2;
                same = macros::to_bytes(want[m], &e1) == macros::to_bytes(now[m], &e2);
            }
            if (!same) {
                std::wstring e;
                if (macros::write_all(want, &e)) changed++;
                else (*failed)++;
            }
        }
    }
    for (const Entry &e : load(L"knob")) { // row = encoder, col = clockwise
        uint8_t q[4] = {0x14, (uint8_t)e.layer, (uint8_t)e.row, (uint8_t)e.col}, r[32];
        if (keyboard_ask(q, 4, r, 4) && (uint16_t)(r[4] << 8 | r[5]) == e.code) continue;
        uint8_t s[6] = {0x15, (uint8_t)e.layer, (uint8_t)e.row, (uint8_t)e.col, (uint8_t)(e.code >> 8), (uint8_t)e.code};
        if (keyboard_ask(s, 6, r, 4)) changed++;
        else (*failed)++;
    }
    return changed;
}

void keep_keyboard_macros() {
    if (!g_board) return;
    std::vector<std::wstring> now;
    int size = 0;
    if (!macros::read_all(&now, &size)) return;
    bool any_kept, any_now = false;
    std::vector<std::wstring> kept = kept_macros(&any_kept);
    for (const std::wstring &t : now) any_now |= !t.empty();
    kept.resize(now.size());
    if ((any_now || any_kept) && kept != now) record_macros(now);
}

bool firmware_changed() {
    if (!g_board) return false;
    // the firmware this keyboard ran when last seen: ZXKB version, or VIA protocol for standard firmware
    wchar_t now[32];
    swprintf(now, 32, L"%d.%d.%d", (int)g_board->kind, g_board->kind == Board::ZXKB ? g_st.fw_version : 0, g_board->via_protocol);
    wchar_t was[32] = {0};
    GetPrivateProfileStringW(L"keyboard", L"firmware", L"", was, 32, file().c_str());
    if (!wcscmp(was, now)) return false;
    WritePrivateProfileStringW(L"keyboard", L"firmware", now, file().c_str());
    return *was != 0; // the first time seen is not a change
}

} // namespace keystore
