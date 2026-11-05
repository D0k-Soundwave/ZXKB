// Key labels from QMK key codes in the chosen Windows keyboard layout. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "labels.h"
#include <cwchar>
#include <cwctype>

namespace labels {
namespace {

// [P] USB HID usage (keyboard page, 0x00-0x73) -> PC scan code set 1; 0xE0xx = extended. From the USB HID Usage
// Tables (keyboard page) and Microsoft's "Keyboard scan code specification" (not re-read this session).
const uint16_t usage_to_scan[0x74] = {
    0, 0, 0, 0,
    0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, // A-P
    0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,                                     // Q-Z
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,                                     // 1-0
    0x1C, 0x01, 0x0E, 0x0F, 0x39, 0x0C, 0x0D, 0x1A, 0x1B, 0x2B, 0x2B, 0x27, 0x28, 0x29, 0x33, 0x34, // Enter .. .
    0x35, 0x3A,                                                                                     // / Caps
    0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58,                         // F1-F12
    0xE037, 0x46, 0x45, 0xE052, 0xE047, 0xE049, 0xE053, 0xE04F, 0xE051,                             // PrtSc .. PgDn
    0xE04D, 0xE04B, 0xE050, 0xE048,                                                                 // arrows
    0xE045, 0xE035, 0x37, 0x4A, 0x4E, 0xE01C, 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49, // keypad
    0x52, 0x53, 0x56, 0xE05D, 0, 0,                                                                 // KP0 KP. ISO\ Menu
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x76,                         // F13-F24
};
const uint16_t mod_scan[8] = {0x1D, 0x2A, 0x38, 0xE05B, 0xE01D, 0x36, 0xE038, 0xE05C}; // LCtrl LShift LAlt LWin R...

// Keys with no character: Windows' own (localised) name for the scan code, shortened where it is long
std::wstring key_name(uint16_t sc) {
    wchar_t buf[64] = {0};
    LONG    lp = (LONG)((sc & 0xFF) << 16) | ((sc & 0xE000) == 0xE000 ? 1 << 24 : 0);
    if (!GetKeyNameTextW(lp, buf, 64)) return L"";
    std::wstring s = buf;
    struct { const wchar_t *from, *to; } shorter[] = {
        {L"Backspace", L"Bksp"}, {L"Caps Lock", L"Caps"}, {L"Page Up", L"PgUp"}, {L"Page Down", L"PgDn"},
        {L"Print Screen", L"PrtSc"}, {L"Prnt Scrn", L"PrtSc"}, {L"Scroll Lock", L"ScrLk"}, {L"Num Lock", L"Num"},
        {L"Insert", L"Ins"}, {L"Delete", L"Del"}, {L"Application", L"Menu"}, {L"Right Shift", L"Shift"},
        {L"Right Ctrl", L"Ctrl"}, {L"Right Alt", L"AltGr"}, {L"Left Windows", L"Win"}, {L"Right Windows", L"Win"},
    };
    for (auto &r : shorter)
        if (s == r.from) return r.to;
    if (s.size() > 1) for (size_t i = 1; i < s.size(); i++) s[i] = (wchar_t)towlower(s[i]); // "ENTER" -> "Enter"
    return s;
}

// The character a scan code types in a layout (with or without Shift), "" if none
std::wstring char_of(uint16_t sc, HKL layout, bool shift) {
    UINT vk = MapVirtualKeyExW((sc & 0xFF) | ((sc & 0xE000) == 0xE000 ? 0xE000 : 0), MAPVK_VSC_TO_VK_EX, layout);
    if (!vk) return L"";
    BYTE state[256] = {0};
    if (shift) state[VK_SHIFT] = state[VK_LSHIFT] = 0x80;
    wchar_t buf[8] = {0};
    int     n = ToUnicodeEx(vk, sc & 0xFF, state, buf, 8, 4, layout); // 4: do not change the keyboard state
    if (n < 0) { // a dead key (accent): clear it and show the accent
        wchar_t tmp[8];
        ToUnicodeEx(vk, sc & 0xFF, state, tmp, 8, 4, layout);
        n = 1;
    }
    if (n <= 0 || buf[0] < 0x20 || buf[0] == 0x7F) return L"";
    return std::wstring(buf, n);
}

Label basic(uint16_t usage, HKL layout) {
    Label l;
    uint16_t sc = 0;
    if (usage < 0x74) sc = usage_to_scan[usage];
    else if (usage >= 0xE0 && usage <= 0xE7) sc = mod_scan[usage - 0xE0];
    if (!sc) {
        switch (usage) {
            case 0xA8: l.main = L"Mute"; break;
            case 0xA9: l.main = L"Vol+"; break;
            case 0xAA: l.main = L"Vol-"; break;
            case 0xAB: l.main = L"Next"; break;
            case 0xAC: l.main = L"Prev"; break;
            case 0xAD: l.main = L"Stop"; break;
            case 0xAE: l.main = L"Play"; break;
            case 0xBD: l.main = L"Scrn+"; break; // the screen's brightness (the keyboard's is "Bri+")
            case 0xBE: l.main = L"Scrn-"; break;
            default: if (usage >= 0xCD && usage <= 0xDF) l.main = L"Mouse";
        }
        return l;
    }
    bool typing = usage >= 0x04 && usage <= 0x38 && usage != 0x28 && usage != 0x29 && usage != 0x2A && usage != 0x2B && usage != 0x2C;
    if (typing || usage == 0x64) { // character keys: as Windows types them in this layout
        std::wstring lo = char_of(sc, layout, false), up = char_of(sc, layout, true);
        if (!lo.empty()) {
            bool letter = lo.size() == 1 && up.size() == 1 && (wchar_t)towupper(lo[0]) == up[0] && lo[0] != up[0];
            l.main = letter ? up : lo;
            if (!letter && up != lo) l.sub = up;
            return l;
        }
    }
    if (usage == 0x2C) { l.main = L""; return l; } // space bar: blank, as on most caps
    l.main = key_name(sc);
    if (usage >= 0xE0) { // modifiers: short, the same on both sides
        static const wchar_t *mods[8] = {L"Ctrl", L"Shift", L"Alt", L"Win", L"Ctrl", L"Shift", L"AltGr", L"Win"};
        l.main = mods[usage - 0xE0];
    }
    return l;
}

} // namespace

HKL current_layout() { return GetKeyboardLayout(0); }

uint16_t scan_of_usage(uint8_t usage) {
    if (usage < 0x74) return usage_to_scan[usage];
    if (usage >= 0xE0 && usage <= 0xE7) return mod_scan[usage - 0xE0];
    return 0;
}

std::vector<HKL> installed() {
    int n = GetKeyboardLayoutList(0, nullptr);
    std::vector<HKL> list(n > 0 ? n : 0);
    if (n > 0) GetKeyboardLayoutList(n, list.data());
    return list;
}

// The layout's name is in the registry under its layout ID (KLID). An HKL's high word is the layout: the language
// again for a language's standard layout ("00000809"), 0xFnnn for a variant or a custom layout, found by its
// "Layout Id" nnn (M 2026-11-03: D0k's 0xF0C1 -> a0010809 "US - Spectrum (GBP on AltGr+4)").
std::wstring layout_name(HKL layout) {
    UINT_PTR v = (UINT_PTR)layout;
    unsigned lang = (unsigned)(v & 0xFFFF), dev = (unsigned)(v >> 16 & 0xFFFF);
    const wchar_t *root = L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts";
    wchar_t klid[16] = {0};
    if ((dev & 0xF000) == 0xF000) { // a variant or custom layout: look it up by its layout ID
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, root, 0, KEY_READ, &k) == ERROR_SUCCESS) {
            wchar_t name[16];
            for (DWORD i = 0;; i++) {
                DWORD len = 16;
                if (RegEnumKeyExW(k, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
                wchar_t id[16] = {0};
                DWORD   size = sizeof(id);
                if (RegGetValueW(k, name, L"Layout Id", RRF_RT_REG_SZ, nullptr, id, &size) == ERROR_SUCCESS &&
                    wcstoul(id, nullptr, 16) == (dev & 0x0FFF)) {
                    wcscpy_s(klid, name);
                    break;
                }
            }
            RegCloseKey(k);
        }
    } else {
        swprintf(klid, 16, L"%04X%04X", dev == lang ? 0 : dev, lang);
    }
    wchar_t text[128] = {0};
    DWORD   size = sizeof(text);
    if (*klid && RegGetValueW(HKEY_LOCAL_MACHINE, (std::wstring(root) + L"\\" + klid).c_str(), L"Layout Text", RRF_RT_REG_SZ,
                              nullptr, text, &size) == ERROR_SUCCESS)
        return text;
    wchar_t loc[LOCALE_NAME_MAX_LENGTH] = {0}, disp[128] = L"Windows keyboard layout";
    if (LCIDToLocaleName(MAKELCID(lang, SORT_DEFAULT), loc, LOCALE_NAME_MAX_LENGTH, 0)) GetLocaleInfoEx(loc, LOCALE_SLOCALIZEDDISPLAYNAME, disp, 128);
    return disp;
}

Label for_keycode(uint16_t kc, HKL layout, const std::vector<std::wstring> &custom) {
    Label l;
    if (kc <= 0x00FF) return basic((uint8_t)kc, layout);
    if (kc <= 0x1FFF) return basic(kc & 0xFF, layout);                 // with modifiers (e.g. Ctrl + key)
    if (kc >= 0x2000 && kc <= 0x3FFF) return basic(kc & 0xFF, layout); // mod-tap: the tapped key
    if (kc >= 0x4000 && kc <= 0x4FFF) return basic(kc & 0xFF, layout); // layer-tap: the tapped key
    // layers: MO (hold), TO (go to), DF (default), TG (toggle), OSL (next key only), with the layer number
    if (kc >= 0x5220 && kc <= 0x523F) { l.main = L"Fn"; l.sub = std::to_wstring(kc & 0x1F); return l; }
    if (kc >= 0x5200 && kc <= 0x521F) { l.main = L"TO " + std::to_wstring(kc & 0x1F); return l; }
    if (kc >= 0x5240 && kc <= 0x525F) { l.main = L"DF " + std::to_wstring(kc & 0x1F); return l; }
    if (kc >= 0x5260 && kc <= 0x527F) { l.main = L"TG " + std::to_wstring(kc & 0x1F); return l; }
    if (kc >= 0x5280 && kc <= 0x529F) { l.main = L"OSL " + std::to_wstring(kc & 0x1F); return l; }
    if (kc >= 0x7700 && kc <= 0x777F) { l.main = L"M" + std::to_wstring(kc - 0x7700); return l; } // macro
    if (kc >= 0x7840 && kc <= 0x784C) { // RGB matrix keys
        static const wchar_t *rgb[13] = {L"RGB on", L"RGB off", L"RGB", L"Mode+", L"Mode-", L"Hue+", L"Hue-", L"Sat+", L"Sat-",
                                         L"Bri+", L"Bri-", L"Spd+", L"Spd-"};
        l.main = rgb[kc - 0x7840];
        return l;
    }
    if (kc >= 0x7800 && kc <= 0x78FF) { l.main = L"Light"; return l; }
    if (kc >= 0x77C0 && kc <= 0x77C9) { l.main = L"Hot"; l.sub = std::to_wstring((kc - 0x77C0 + 1) % 10); return l; } // ZXKB hotkeys
    if (kc == 0x77CA) { l.main = L"VU"; return l; }
    if (kc == 0x7013) { l.main = L"NKRO"; return l; }
    if (kc == 0x700B) { l.main = L"WinLk"; return l; }
    if (kc == 0x7C00) { l.main = L"Boot"; return l; }
    if (kc >= 0x7E00 && kc <= 0x7E3F) { // the keyboard's own key codes
        size_t i = kc - 0x7E00;
        if (i < custom.size()) l.main = custom[i];
        return l;
    }
    return l;
}

} // namespace labels
