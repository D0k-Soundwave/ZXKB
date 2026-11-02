// Settings and profiles: plain text .ini files in %APPDATA%\ZXKB.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <vector>

// What a profile holds: the whole lighting setup
struct Profile {
    std::string effect;          // effect by name, so it survives firmware rebuilds that renumber effects
    int  speed = 128, hue = 0, sat = 255, val = 255;
    int  size = 128, intensity = 255;  // for effects that have them (typing effects)
    bool vu = false;
    int  vu_style = 0;                 // VuStyle: 0 bands, 1 AY channels, 2 music
};

namespace settings {
std::wstring folder();                                  // %APPDATA%\ZXKB (created if missing)
std::wstring get(const wchar_t *section, const wchar_t *key, const wchar_t *def = L"");
int          get_int(const wchar_t *section, const wchar_t *key, int def);
void         set(const wchar_t *section, const wchar_t *key, const std::wstring &value);
void         set_int(const wchar_t *section, const wchar_t *key, int value);
void         import_zx_vu();                            // first run: take VU on/off and style from ZX_VU.json

std::vector<std::wstring> profile_names();              // sorted
bool load_profile(const std::wstring &name, Profile *p);
bool save_profile(const std::wstring &name, const Profile &p);
bool delete_profile(const std::wstring &name);
bool valid_profile_name(const std::wstring &name);

bool autostart();                                       // Startup-folder shortcut present and pointing at this exe
void set_autostart(bool on);

const wchar_t *vu_style_name(int style);   // "bands", "abc", "music"
int            vu_style_from(const std::wstring &name);

std::wstring widen(const std::string &s);
std::string  narrow(const std::wstring &s);
} // namespace settings
