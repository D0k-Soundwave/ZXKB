// Settings and profiles: plain text .ini files in %APPDATA%\ZXKB.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>

namespace settings {

static const wchar_t *RUN_KEY   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *RUN_VALUE = L"ZXKB";

std::wstring widen(const std::string &s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

std::string narrow(const std::wstring &w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring folder() {
    static std::wstring dir;
    if (dir.empty()) {
        wchar_t *appdata = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
            dir = std::wstring(appdata) + L"\\ZXKB";
            CoTaskMemFree(appdata);
        } else {
            dir = L".";
        }
        CreateDirectoryW(dir.c_str(), nullptr);
        CreateDirectoryW((dir + L"\\profiles").c_str(), nullptr);
    }
    return dir;
}

static std::wstring ini() { return folder() + L"\\zxkb.ini"; }
static std::wstring profile_path(const std::wstring &name) { return folder() + L"\\profiles\\" + name + L".ini"; }

std::wstring get(const wchar_t *section, const wchar_t *key, const wchar_t *def) {
    wchar_t buf[1024];
    GetPrivateProfileStringW(section, key, def, buf, 1024, ini().c_str());
    return buf;
}

int get_int(const wchar_t *section, const wchar_t *key, int def) {
    return (int)GetPrivateProfileIntW(section, key, def, ini().c_str());
}

void set(const wchar_t *section, const wchar_t *key, const std::wstring &value) {
    WritePrivateProfileStringW(section, key, value.c_str(), ini().c_str());
}

void set_int(const wchar_t *section, const wchar_t *key, int value) {
    set(section, key, std::to_wstring(value));
}

// ZX_VU.json is tiny and written by the old app itself: {"style": "bands"|"abc", "vu": true|false, "effect": n}
void import_zx_vu() {
    if (GetFileAttributesW(ini().c_str()) != INVALID_FILE_ATTRIBUTES) return; // not the first run
    set_int(L"vu", L"on", 1);
    set(L"vu", L"style", L"bands");
    wchar_t appdata[MAX_PATH];
    if (!GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) return;
    HANDLE f = CreateFileW((std::wstring(appdata) + L"\\ZX_VU.json").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    char  text[512] = {0};
    DWORD n = 0;
    ReadFile(f, text, sizeof(text) - 1, &n, nullptr);
    CloseHandle(f);
    std::string s(text, n);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    if (s.find("\"style\":\"abc\"") != std::string::npos) set(L"vu", L"style", L"abc");
    if (s.find("\"vu\":false") != std::string::npos) set_int(L"vu", L"on", 0);
}

std::vector<std::wstring> profile_names() {
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((folder() + L"\\profiles\\*.ini").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring n = fd.cFileName;
            names.push_back(n.substr(0, n.size() - 4));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(names.begin(), names.end(), [](const std::wstring &a, const std::wstring &b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    return names;
}

bool valid_profile_name(const std::wstring &name) {
    if (name.empty() || name.size() > 60 || name.back() == L'.' || name.back() == L' ') return false;
    return name.find_first_of(L"\\/:*?\"<>|") == std::wstring::npos;
}

bool load_profile(const std::wstring &name, Profile *p) {
    std::wstring path = profile_path(name);
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    wchar_t buf[256];
    GetPrivateProfileStringW(L"profile", L"effect", L"", buf, 256, path.c_str());
    p->effect = narrow(buf);
    p->speed  = GetPrivateProfileIntW(L"profile", L"speed", 128, path.c_str());
    p->hue    = GetPrivateProfileIntW(L"profile", L"hue", 0, path.c_str());
    p->sat    = GetPrivateProfileIntW(L"profile", L"sat", 255, path.c_str());
    p->val    = GetPrivateProfileIntW(L"profile", L"brightness", 255, path.c_str());
    p->vu     = GetPrivateProfileIntW(L"profile", L"vu", 0, path.c_str()) != 0;
    GetPrivateProfileStringW(L"profile", L"vu_style", L"bands", buf, 256, path.c_str());
    p->vu_abc = wcscmp(buf, L"abc") == 0;
    return true;
}

bool save_profile(const std::wstring &name, const Profile &p) {
    if (!valid_profile_name(name)) return false;
    std::wstring path = profile_path(name);
    auto w = [&](const wchar_t *k, const std::wstring &v) { return WritePrivateProfileStringW(L"profile", k, v.c_str(), path.c_str()); };
    return w(L"effect", widen(p.effect)) && w(L"speed", std::to_wstring(p.speed)) && w(L"hue", std::to_wstring(p.hue)) &&
           w(L"sat", std::to_wstring(p.sat)) && w(L"brightness", std::to_wstring(p.val)) && w(L"vu", p.vu ? L"1" : L"0") &&
           w(L"vu_style", p.vu_abc ? L"abc" : L"bands");
}

bool delete_profile(const std::wstring &name) {
    return DeleteFileW(profile_path(name).c_str()) != 0;
}

static std::wstring run_command() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return L"\"" + std::wstring(exe) + L"\"";
}

bool autostart() {
    wchar_t buf[MAX_PATH + 4];
    DWORD   size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return false;
    return _wcsicmp(buf, run_command().c_str()) == 0;
}

void set_autostart(bool on) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
    if (on) {
        std::wstring cmd = run_command();
        RegSetValueExW(key, RUN_VALUE, 0, REG_SZ, (const BYTE *)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, RUN_VALUE);
    }
    RegCloseKey(key);
}

} // namespace settings
