// Keycap sets. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "keycaps.h"
#include "json.h"
#include "settings.h"
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cwchar>

namespace keycaps {
namespace {

constexpr DWORD MAX_SET = 2 * 1024 * 1024; // [A] KLE files are tens of KB
constexpr int   MATCH_DIST    = 60;              // [A] hundredths of a key: a set key this close to a key's centre covers it

std::wstring exe_folder() {
    wchar_t p[MAX_PATH];
    DWORD   n = GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s(p, n);
    return s.substr(0, s.find_last_of(L'\\')) + L"\\keycaps";
}

std::wstring user_folder() {
    std::wstring f = settings::folder() + L"\\keycaps";
    CreateDirectoryW(f.c_str(), nullptr);
    return f;
}

bool read(const std::wstring &path, std::string *out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(f, nullptr), n = 0;
    bool  ok   = size > 0 && size <= MAX_SET;
    if (ok) {
        out->resize(size);
        ok = ReadFile(f, &(*out)[0], size, &n, nullptr) && n == size;
    }
    CloseHandle(f);
    return ok;
}

bool parse(const std::string &text, std::vector<KeyRect> *keys) {
    Json j = Json::parse(text, true); // KLE's own download writes keys without quotes
    if (!j.is(Json::ARR)) return false;
    *keys = kle::from_kle(j);
    return !keys->empty();
}

std::wstring id(uint16_t vid, uint16_t pid) {
    wchar_t s[16];
    swprintf(s, 16, L"%04X_%04X", vid, pid);
    return s;
}

void centre(const KeyRect &k, int ox, int oy, int *cx, int *cy) {
    *cx = k.x - ox + k.w / 2;
    *cy = k.y - oy + k.h / 2;
}

} // namespace

std::vector<std::wstring> list() {
    std::vector<std::wstring> names;
    for (const std::wstring &dir : {user_folder(), exe_folder()}) {
        WIN32_FIND_DATAW fd;
        HANDLE f = FindFirstFileW((dir + L"\\*.json").c_str(), &fd);
        if (f == INVALID_HANDLE_VALUE) continue;
        do {
            std::wstring n = fd.cFileName;
            n = n.substr(0, n.size() - 5);
            if (std::find(names.begin(), names.end(), n) == names.end()) names.push_back(n);
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool load(const std::wstring &name, std::vector<KeyRect> *keys) {
    std::string text;
    for (const std::wstring &dir : {user_folder(), exe_folder()})
        if (read(dir + L"\\" + name + L".json", &text)) return parse(text, keys);
    return false;
}

std::wstring chosen(uint16_t vid, uint16_t pid) { return settings::get(L"keycaps", id(vid, pid).c_str()); }

void choose(uint16_t vid, uint16_t pid, const std::wstring &name) { settings::set(L"keycaps", id(vid, pid).c_str(), name); }

bool import(const std::wstring &path, std::wstring *name, std::wstring *error) {
    std::string text;
    std::vector<KeyRect> keys;
    if (!read(path, &text)) { *error = L"The file could not be read."; return false; }
    if (!parse(text, &keys)) { *error = L"This is not a KLE keyboard layout file (keyboard-layout-editor.com, Download JSON)."; return false; }
    std::wstring file = path.substr(path.find_last_of(L'\\') + 1);
    if (file.size() > 5 && _wcsicmp(file.c_str() + file.size() - 5, L".json") == 0) file = file.substr(0, file.size() - 5);
    *name = file;
    std::wstring dest = user_folder() + L"\\" + file + L".json";
    if (!CopyFileW(path.c_str(), dest.c_str(), FALSE)) { *error = L"The file could not be copied to ZXKB's keycap sets."; return false; }
    return true;
}

const KeyRect *match(const std::vector<KeyRect> &set, const KeyRect &key, const std::vector<KeyRect> &board) {
    bool has_matrix = false;
    for (const KeyRect &s : set) has_matrix |= s.row >= 0;
    if (has_matrix) {
        for (const KeyRect &s : set)
            if (s.row == key.row && s.col == key.col) return &s;
        return nullptr;
    }
    // no matrix positions: by position, both layouts measured from their top-left key
    int bx = INT32_MAX, by = INT32_MAX, sx = INT32_MAX, sy = INT32_MAX;
    for (const KeyRect &k : board) { bx = std::min(bx, k.x); by = std::min(by, k.y); }
    for (const KeyRect &k : set) { sx = std::min(sx, k.x); sy = std::min(sy, k.y); }
    int kx, ky;
    centre(key, bx, by, &kx, &ky);
    const KeyRect *best = nullptr;
    long best_d = (long)MATCH_DIST * MATCH_DIST;
    for (const KeyRect &s : set) {
        int cx, cy;
        centre(s, sx, sy, &cx, &cy);
        long d = (long)(cx - kx) * (cx - kx) + (long)(cy - ky) * (cy - ky);
        if (d <= best_d) { best_d = d; best = &s; }
    }
    return best;
}

} // namespace keycaps
