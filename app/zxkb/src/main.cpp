// ZXKB: tray app for keyboards running the ZXKB QMK module. Effects by name, effect settings, favourites,
// brightness, profiles, Fn + number hotkey slots, and the audio VU meter.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dbt.h>
#include <objbase.h>
#include <string>
#include <vector>
#include "../res/resource.h"
#include "effects.h"
#include "hidlink.h"
#include "protocol.h"
#include "settings.h"
#include "vu.h"

namespace {

// ---------- menu command ids ----------
enum : UINT {
    ID_STYLE_BANDS = 100, ID_STYLE_ABC, ID_STYLE_MUSIC, // VU styles: ID_STYLE_BANDS + VuStyle
    ID_OFF, ID_BRIGHTER, ID_DIMMER, ID_FASTER, ID_SLOWER,
    ID_FAV_ADD, ID_FAV_REMOVE, ID_PROFILE_SAVE, ID_AUTOSTART, ID_QUIT, ID_BIGGER, ID_SMALLER, ID_STRONGER, ID_SOFTER,
    ID_EFFECT  = 1000, // + effect number (1-255)
    ID_FAV     = 1300, // + favourite index
    ID_COLOUR  = 1400, // + colour index
    ID_PLOAD   = 2000, // + profile index
    ID_PDEL    = 2500, // + profile index
    ID_SLOT_FX = 3000, // + slot (0-9): use current effect
    ID_SLOT_CLR = 3100, // + slot: clear
    ID_SLOT_PRO = 3200, // + slot * 100 + profile index
};
constexpr UINT WM_TRAY      = WM_APP + 50;
constexpr UINT TIMER_SAVE   = 1;
constexpr UINT SAVE_DELAY_MS = 3000; // write settings to the keyboard 3 s after the last change, not on every click
constexpr int  STEP         = 16;    // brightness, speed, size, intensity step: as QMK's own keys (RGB_MATRIX_VAL_STEP / SPD_STEP)
constexpr int  TUNE_MIN     = 16;    // the keyboard keeps size and intensity within 16-255
constexpr int  MAX_PROFILES = 100;   // menu id range per slot

// Spectrum BRIGHT palette as QMK hue/saturation (with D0k's calibrated colour balance the pure hues match)
struct Colour { const wchar_t *name; uint8_t hue, sat; };
const Colour colours[] = {
    {L"Red", 0, 255}, {L"Yellow", 43, 255}, {L"Green", 85, 255}, {L"Cyan", 128, 255},
    {L"Blue", 170, 255}, {L"Magenta", 213, 255}, {L"White", 0, 0},
};

HINSTANCE      g_inst;
HWND           g_hwnd;
UINT           g_taskbar_created;
NOTIFYICONDATAW g_nid = {};
HidLink        g_link;
VuMeter        g_vu(&g_link);
Board         *g_board = nullptr;  // null while no keyboard is connected
Status         g_st;               // last known keyboard state
Tune           g_tune;             // size and intensity of the current effect
bool           g_vu_on = true;
int            g_vu_style = VU_BANDS;
std::vector<std::string>  g_favs;  // effect names
std::wstring   g_slots[10];        // "effect:NAME" or "profile:NAME"

// ---------- helpers ----------
std::string effect_name(int mode) {
    return g_board && mode >= 1 && mode <= (int)g_board->names.size() ? g_board->names[mode - 1] : "";
}
int effect_number(const std::string &name) {
    if (!g_board) return 0;
    for (size_t i = 0; i < g_board->names.size(); i++)
        if (g_board->names[i] == name) return (int)i + 1;
    return 0;
}
// The VU styles are listed with the effects. As favourites and hotkeys they are stored as "VU:bands" etc.
const wchar_t *vu_label(int style) {
    return style == VU_MUSIC ? L"VU meter: music (treble, mids, bass)"
         : style == VU_ABC   ? L"VU meter: AY channels (Spectrum music)"
                             : L"VU meter: frequency bands";
}
std::string vu_name(int style) { return "VU:" + settings::narrow(settings::vu_style_name(style)); }
bool is_vu_name(const std::string &n) { return n.rfind("VU:", 0) == 0; }
int  vu_style_of(const std::string &n) { return settings::vu_style_from(settings::widen(n.substr(3))); }
std::wstring menu_name(const std::string &n) { return is_vu_name(n) ? vu_label(vu_style_of(n)) : effects::friendly(n); }

std::wstring current_effect_text() {
    if (!g_board) return L"keyboard not connected";
    if (g_vu_on) return vu_label(g_vu_style);
    if (!g_st.enabled) return L"lighting off";
    return effects::friendly(effect_name(g_st.mode));
}

void update_tray() {
    std::wstring tip = L"ZXKB - " + current_effect_text();
    lstrcpynW(g_nid.szTip, tip.c_str(), ARRAYSIZE(g_nid.szTip));
    if (g_nid.hIcon) DestroyIcon(g_nid.hIcon);
    LoadIconMetric(g_inst, MAKEINTRESOURCEW(g_board ? IDI_ON : IDI_OFF), LIM_SMALL, &g_nid.hIcon);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void send(std::initializer_list<uint8_t> bytes) {
    uint8_t buf[proto::REPORT] = {0};
    int n = 0;
    for (uint8_t b : bytes) buf[n++] = b;
    g_link.send(buf, n);
}

void via_set(uint8_t value_id, uint8_t a, uint8_t b = 0) {
    send({proto::VIA_SET, proto::VIA_RGB_MATRIX, value_id, a, b});
}

bool g_save_pending = false;
void schedule_save() { g_save_pending = true; SetTimer(g_hwnd, TIMER_SAVE, SAVE_DELAY_MS, nullptr); }

void save_now() {
    KillTimer(g_hwnd, TIMER_SAVE);
    if (g_board && g_save_pending) {
        send({proto::VIA_SAVE, proto::VIA_RGB_MATRIX});
        send({proto::TUNE, proto::TUNE_SAVE});
    }
    g_save_pending = false;
}

void save_lists() {
    std::string favs;
    for (const std::string &f : g_favs) favs += (favs.empty() ? "" : ",") + f;
    settings::set(L"favourites", L"effects", settings::widen(favs));
    for (int i = 0; i < 10; i++) settings::set(L"slots", std::to_wstring(i + 1).c_str(), g_slots[i]);
}

// ---------- actions ----------
ULONGLONG g_vu_sent = 0;                  // when ZXKB last switched the VU meter on or off on the keyboard
constexpr ULONGLONG VU_GRACE_MS = 1000;   // replies take a few ms; ignore older status about the VU meter for this long

// Listen to audio only while the VU meter could actually be seen: on, lighting on, and allowed by the keyboard
// (not switched off with Fn + V, not in Spectrum mode). Anything else costs nothing.
void update_capture() {
    if (g_board && g_vu_on && g_st.enabled && g_st.vu_possible) g_vu.start();
    else g_vu.stop();
}

void set_vu(bool on) {
    g_vu_on = on;
    settings::set_int(L"vu", L"on", on);
    if (!g_board) return;
    if (on && !g_st.enabled) { // switching lighting back on: the keyboard reports at once if the VU still cannot show
        via_set(proto::VIA_EFFECT, g_st.mode ? g_st.mode : 1);
        g_st.enabled = 1;
        g_st.vu_possible = 1;
    }
    send({proto::VU_MODE, (uint8_t)on});
    g_vu_sent = GetTickCount64();
    update_capture();
    update_tray();
}

void set_vu_style(int style) {
    g_vu_style = style;
    g_vu.set_style(style);
    settings::set(L"vu", L"style", settings::vu_style_name(style));
    update_tray();
}

void set_effect(int mode) { // 0 = lighting off
    if (!g_board) return;
    if (g_vu_on) set_vu(false); // an effect replaces the VU meter (tick VU again to bring it back), as in ZX VU
    via_set(proto::VIA_EFFECT, (uint8_t)mode);
    if (mode) g_st.mode = (uint8_t)mode;
    g_st.enabled = mode != 0;
    send({proto::TUNE, proto::TUNE_GET}); // this effect's own size and intensity
    schedule_save();
    update_tray();
}

void set_brightness(int val) {
    if (!g_board) return;
    val = val < 0 ? 0 : val > g_st.max_val ? g_st.max_val : val;
    g_st.val = (uint8_t)val;
    // VIA brightness is 0-255 scaled to the board's maximum; round up so the board lands on val
    int v = g_st.max_val ? (val * 255 + g_st.max_val - 1) / g_st.max_val : 0;
    via_set(proto::VIA_BRIGHTNESS, (uint8_t)(v > 255 ? 255 : v));
    schedule_save();
}

void set_speed(int speed) {
    if (!g_board) return;
    g_st.speed = (uint8_t)(speed < 0 ? 0 : speed > 255 ? 255 : speed);
    via_set(proto::VIA_SPEED, g_st.speed);
    schedule_save();
}

// Size and intensity belong to the current effect; the keyboard keeps them per effect
void set_tune(int size, int intensity) {
    if (!g_board || !g_tune.flags) return;
    auto clamp = [](int v) { return v < TUNE_MIN ? TUNE_MIN : v > 255 ? 255 : v; };
    g_tune.size      = (uint8_t)clamp(size);
    g_tune.intensity = (uint8_t)clamp(intensity);
    send({proto::TUNE, proto::TUNE_SET, g_tune.size, g_tune.intensity});
    schedule_save();
}

void set_colour(uint8_t hue, uint8_t sat) {
    if (!g_board) return;
    g_st.hue = hue;
    g_st.sat = sat;
    via_set(proto::VIA_COLOUR, hue, sat);
    schedule_save();
}

Profile current_profile() {
    Profile p;
    p.effect = g_st.enabled ? effect_name(g_st.mode) : "OFF";
    p.speed = g_st.speed; p.hue = g_st.hue; p.sat = g_st.sat; p.val = g_st.val;
    p.vu = g_vu_on; p.vu_style = g_vu_style;
    if (g_tune.flags && g_tune.mode == g_st.mode) { p.size = g_tune.size; p.intensity = g_tune.intensity; }
    return p;
}

void apply_profile(const std::wstring &name) {
    Profile p;
    if (!g_board || !settings::load_profile(name, &p)) return;
    if (p.effect == "OFF") set_effect(0);
    else if (int m = effect_number(p.effect)) {
        set_effect(m);
        send({proto::TUNE, proto::TUNE_SET, (uint8_t)p.size, (uint8_t)p.intensity}); // ignored by effects without them
        g_tune.size = (uint8_t)p.size; g_tune.intensity = (uint8_t)p.intensity;
    }
    set_colour((uint8_t)p.hue, (uint8_t)p.sat);
    set_speed(p.speed);
    set_brightness(p.val);
    set_vu_style(p.vu_style);
    set_vu(p.vu);
}

void select_vu(int style) {
    set_vu_style(style);
    if (!g_vu_on) set_vu(true);
}

void select_by_name(const std::string &n) { // an effect or a VU style
    if (is_vu_name(n)) select_vu(vu_style_of(n));
    else if (int m = effect_number(n)) set_effect(m);
}

void apply_slot(int slot) { // slot 1-10
    const std::wstring &s = g_slots[slot - 1];
    if (s.rfind(L"effect:", 0) == 0) {
        select_by_name(settings::narrow(s.substr(7)));
    } else if (s.rfind(L"profile:", 0) == 0) {
        apply_profile(s.substr(8));
    }
}

std::wstring slot_text(int i) {
    const std::wstring &s = g_slots[i];
    std::wstring key = L"Fn+" + std::to_wstring((i + 1) % 10) + L": ";
    if (s.rfind(L"effect:", 0) == 0) return key + menu_name(settings::narrow(s.substr(7)));
    if (s.rfind(L"profile:", 0) == 0) return key + L"profile " + s.substr(8);
    return key + L"(not set)";
}

// ---------- profile name dialog ----------
INT_PTR CALLBACK name_dialog(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    static std::wstring *out;
    switch (msg) {
        case WM_INITDIALOG:
            out = reinterpret_cast<std::wstring *>(lp);
            SendDlgItemMessageW(dlg, IDC_NAME, EM_LIMITTEXT, 60, 0);
            SetForegroundWindow(dlg);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                wchar_t buf[64] = {0};
                GetDlgItemTextW(dlg, IDC_NAME, buf, 64);
                std::wstring name = buf;
                if (!settings::valid_profile_name(name)) {
                    MessageBoxW(dlg, L"Please type a name. It can't contain \\ / : * ? \" < > | or end with a dot or space.",
                                L"ZXKB", MB_OK | MB_ICONINFORMATION);
                    return TRUE;
                }
                Profile existing;
                if (settings::load_profile(name, &existing) &&
                    MessageBoxW(dlg, (L"Replace the profile \"" + name + L"\"?").c_str(), L"ZXKB", MB_YESNO | MB_ICONQUESTION) != IDYES)
                    return TRUE;
                *out = name;
                EndDialog(dlg, IDOK);
            } else if (LOWORD(wp) == IDCANCEL) {
                EndDialog(dlg, IDCANCEL);
            }
            return TRUE;
    }
    return FALSE;
}

// ---------- tray menu ----------
void add(HMENU m, UINT id, const std::wstring &text, bool checked = false, bool enabled = true, bool radio = false) {
    MENUITEMINFOW mi = {sizeof(mi)};
    mi.fMask      = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_FTYPE;
    mi.fType      = MFT_STRING | (radio ? MFT_RADIOCHECK : 0);
    mi.fState     = (checked ? MFS_CHECKED : 0) | (enabled ? 0 : MFS_DISABLED);
    mi.wID        = id;
    mi.dwTypeData = const_cast<wchar_t *>(text.c_str());
    InsertMenuItemW(m, GetMenuItemCount(m), TRUE, &mi);
}
void add_sub(HMENU m, HMENU sub, const std::wstring &text, bool enabled = true) {
    AppendMenuW(m, MF_POPUP | MF_STRING | (enabled ? 0 : MF_GRAYED), (UINT_PTR)sub, text.c_str());
}
void sep(HMENU m) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }

// Ask the keyboard for its current state (it may have changed from its own keys) and wait briefly for it.
void refresh_status() {
    if (!g_board) return;
    send({proto::INFO, proto::INFO_STATUS});
    send({proto::TUNE, proto::TUNE_GET});
    DWORD start = GetTickCount();
    bool  status = false, tune = false;
    MSG   msg;
    while (GetTickCount() - start < 150 && !(status && tune)) { // replies normally take a few ms
        if (PeekMessageW(&msg, g_hwnd, WM_ZX_STATUS, WM_ZX_TUNE, PM_REMOVE)) {
            status |= msg.message == WM_ZX_STATUS;
            tune |= msg.message == WM_ZX_TUNE;
            DispatchMessageW(&msg);
            continue;
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_POSTMESSAGE);
    }
}

// QMK's own version of an effect is hidden when the board also has ZXKB's (same menu name)
bool replaced(size_t i) {
    if (g_board->kinds[i] != 0) return false;
    std::wstring name = effects::friendly(g_board->names[i]);
    for (size_t j = 0; j < g_board->names.size(); j++)
        if (g_board->kinds[j] == 1 && effects::friendly(g_board->names[j]) == name) return true;
    return false;
}

void show_menu() {
    refresh_status();
    bool on = g_board != nullptr;
    bool fx      = on && !g_vu_on && g_st.enabled; // a keyboard effect is showing (not the VU meter)
    bool showing = fx || (on && g_vu_on);          // an effect or a VU style is showing
    std::string cur = !showing ? "" : g_vu_on ? vu_name(g_vu_style) : effect_name(g_st.mode);
    std::vector<std::wstring> profiles = settings::profile_names();
    if (profiles.size() > MAX_PROFILES) profiles.resize(MAX_PROFILES);

    HMENU menu = CreatePopupMenu();
    if (!on) { add(menu, 0, L"Keyboard not connected", false, false); sep(menu); }
    else { add(menu, 0, L"Now: " + current_effect_text(), false, false); sep(menu); }

    // effects, grouped
    HMENU fxm = CreatePopupMenu();
    if (on) {
        add(fxm, ID_OFF, L"Lighting off", !g_vu_on && !g_st.enabled, true, true);
        HMENU vu = CreatePopupMenu(); // the audio VU meter, one entry per style
        const int order[3] = {VU_MUSIC, VU_BANDS, VU_ABC};
        for (int s : order) add(vu, ID_STYLE_BANDS + s, vu_label(s), g_vu_on && g_vu_style == s, true, true);
        add_sub(fxm, vu, L"VU meter");
        HMENU groups[effects::GROUP_COUNT] = {};
        for (size_t i = 0; i < g_board->names.size(); i++) {
            effects::Group gr = effects::group(g_board->names[i], g_board->kinds[i]);
            if (gr == effects::HIDDEN || replaced(i)) continue;
            if (!groups[gr]) groups[gr] = CreatePopupMenu();
            add(groups[gr], ID_EFFECT + (UINT)i + 1, effects::friendly(g_board->names[i]), fx && g_st.mode == i + 1, true, true);
        }
        for (int gr = 0; gr < effects::HIDDEN; gr++)
            if (groups[gr]) add_sub(fxm, groups[gr], effects::group_names[gr]);
    }
    add_sub(menu, fxm, L"Effects", on);

    // favourites
    HMENU fav = CreatePopupMenu();
    bool any_fav = false;
    for (size_t i = 0; i < g_favs.size(); i++) {
        if (!is_vu_name(g_favs[i]) && !effect_number(g_favs[i])) continue; // not on this keyboard
        add(fav, ID_FAV + (UINT)i, menu_name(g_favs[i]), g_favs[i] == cur, true, true);
        any_fav = true;
    }
    if (!any_fav) add(fav, 0, L"(none yet)", false, false);
    sep(fav);
    bool is_fav = false;
    for (const std::string &f : g_favs) is_fav |= f == cur;
    if (is_fav) add(fav, ID_FAV_REMOVE, L"Remove " + menu_name(cur) + L" from favourites", false, showing);
    else add(fav, ID_FAV_ADD, showing ? L"Add " + menu_name(cur) + L" to favourites" : L"Add current effect to favourites", false, showing);
    add_sub(menu, fav, L"Favourites", on);

    // effect settings
    HMENU set = CreatePopupMenu();
    bool has_size = fx && g_tune.mode == g_st.mode && (g_tune.flags & proto::TUNE_HAS_SIZE);
    bool has_int  = fx && g_tune.mode == g_st.mode && (g_tune.flags & proto::TUNE_HAS_INTENSITY);
    add(set, ID_FASTER, L"Faster", false, on);
    add(set, ID_SLOWER, L"Slower", false, on);
    sep(set);
    add(set, ID_BIGGER, L"Bigger", false, has_size && g_tune.size < 255);
    add(set, ID_SMALLER, L"Smaller", false, has_size && g_tune.size > TUNE_MIN);
    add(set, ID_STRONGER, L"Stronger", false, has_int && g_tune.intensity < 255);
    add(set, ID_SOFTER, L"Softer", false, has_int && g_tune.intensity > TUNE_MIN);
    sep(set);
    for (size_t i = 0; i < ARRAYSIZE(colours); i++) {
        bool now = on && g_st.sat == colours[i].sat && (colours[i].sat == 0 || g_st.hue == colours[i].hue);
        add(set, ID_COLOUR + (UINT)i, colours[i].name, now, on, true);
    }
    add_sub(menu, set, L"Effect settings", on);
    add(menu, ID_BRIGHTER, L"Brighter", false, on && g_st.val < g_st.max_val);
    add(menu, ID_DIMMER, L"Dimmer", false, on && g_st.val > 0);
    sep(menu);

    // profiles
    HMENU pro = CreatePopupMenu(), del = CreatePopupMenu();
    for (size_t i = 0; i < profiles.size(); i++) {
        add(pro, ID_PLOAD + (UINT)i, profiles[i], false, on);
        add(del, ID_PDEL + (UINT)i, profiles[i]);
    }
    if (profiles.empty()) add(pro, 0, L"(none yet)", false, false);
    sep(pro);
    add(pro, ID_PROFILE_SAVE, L"Save current setup as...", false, on);
    add_sub(pro, del, L"Delete", !profiles.empty());
    add_sub(menu, pro, L"Profiles");

    // Fn + number hotkeys
    HMENU keys = CreatePopupMenu();
    for (int s = 0; s < 10; s++) {
        HMENU k = CreatePopupMenu();
        add(k, ID_SLOT_FX + s, showing ? L"Current effect (" + menu_name(cur) + L")" : L"Current effect", false, showing);
        for (size_t i = 0; i < profiles.size(); i++)
            add(k, ID_SLOT_PRO + s * MAX_PROFILES + (UINT)i, L"Profile " + profiles[i], g_slots[s] == L"profile:" + profiles[i]);
        sep(k);
        add(k, ID_SLOT_CLR + s, L"Clear", false, !g_slots[s].empty());
        add_sub(keys, k, slot_text(s));
    }
    add_sub(menu, keys, L"Fn + number hotkeys");
    sep(menu);
    add(menu, ID_AUTOSTART, L"Start with Windows", settings::autostart());
    add(menu, ID_QUIT, L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(menu); // destroys the submenus too
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    if (!cmd) return;

    if (cmd >= ID_STYLE_BANDS && cmd <= ID_STYLE_MUSIC) select_vu((int)(cmd - ID_STYLE_BANDS));
    else if (cmd == ID_OFF) set_effect(0);
    else if (cmd == ID_BRIGHTER) set_brightness(g_st.val + STEP);
    else if (cmd == ID_DIMMER) set_brightness(g_st.val - STEP);
    else if (cmd == ID_FASTER) set_speed(g_st.speed + STEP);
    else if (cmd == ID_SLOWER) set_speed(g_st.speed - STEP);
    else if (cmd == ID_BIGGER) set_tune(g_tune.size + STEP, g_tune.intensity);
    else if (cmd == ID_SMALLER) set_tune(g_tune.size - STEP, g_tune.intensity);
    else if (cmd == ID_STRONGER) set_tune(g_tune.size, g_tune.intensity + STEP);
    else if (cmd == ID_SOFTER) set_tune(g_tune.size, g_tune.intensity - STEP);
    else if (cmd == ID_FAV_ADD) { g_favs.push_back(cur); save_lists(); }
    else if (cmd == ID_FAV_REMOVE) {
        for (size_t i = 0; i < g_favs.size(); i++)
            if (g_favs[i] == cur) { g_favs.erase(g_favs.begin() + i); break; }
        save_lists();
    } else if (cmd == ID_PROFILE_SAVE) {
        std::wstring name;
        if (DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_NAME), g_hwnd, name_dialog, (LPARAM)&name) == IDOK)
            settings::save_profile(name, current_profile());
    } else if (cmd == ID_AUTOSTART) settings::set_autostart(!settings::autostart());
    else if (cmd == ID_QUIT) DestroyWindow(g_hwnd);
    else if (cmd >= ID_SLOT_PRO) {
        UINT s = (cmd - ID_SLOT_PRO) / MAX_PROFILES, i = (cmd - ID_SLOT_PRO) % MAX_PROFILES;
        if (s < 10 && i < profiles.size()) { g_slots[s] = L"profile:" + profiles[i]; save_lists(); }
    } else if (cmd >= ID_SLOT_CLR) { g_slots[cmd - ID_SLOT_CLR] = L""; save_lists(); }
    else if (cmd >= ID_SLOT_FX) { g_slots[cmd - ID_SLOT_FX] = L"effect:" + settings::widen(cur); save_lists(); }
    else if (cmd >= ID_PDEL) {
        const std::wstring &n = profiles[cmd - ID_PDEL];
        if (MessageBoxW(g_hwnd, (L"Delete the profile \"" + n + L"\"?").c_str(), L"ZXKB", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            settings::delete_profile(n);
            for (std::wstring &s : g_slots)
                if (s == L"profile:" + n) s.clear();
            save_lists();
        }
    } else if (cmd >= ID_PLOAD) apply_profile(profiles[cmd - ID_PLOAD]);
    else if (cmd >= ID_COLOUR) set_colour(colours[cmd - ID_COLOUR].hue, colours[cmd - ID_COLOUR].sat);
    else if (cmd >= ID_FAV) select_by_name(g_favs[cmd - ID_FAV]);
    else if (cmd >= ID_EFFECT) set_effect((int)(cmd - ID_EFFECT));
}

void add_tray_icon() {
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    g_nid.uCallbackMessage = WM_TRAY;
    if (!g_nid.hIcon) LoadIconMetric(g_inst, MAKEINTRESOURCEW(IDI_OFF), LIM_SMALL, &g_nid.hIcon);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    update_tray();
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TRAY:
            switch (LOWORD(lp)) {
                case WM_CONTEXTMENU: show_menu(); break;              // right click
                case NIN_SELECT:
                case NIN_KEYSELECT: if (g_board) set_vu(!g_vu_on); break; // left click toggles the VU meter, as in ZX VU
            }
            return 0;
        case WM_ZX_CONNECTED:
            delete g_board;
            g_board = reinterpret_cast<Board *>(lp);
            g_st    = g_board->status;
            send({proto::TUNE, proto::TUNE_GET});
            if (g_vu_on) set_vu(true); // tell the keyboard and start listening
            update_tray();
            return 0;
        case WM_ZX_DISCONNECTED:
            g_vu.stop();
            delete g_board;
            g_board = nullptr;
            KillTimer(hwnd, TIMER_SAVE);
            g_save_pending = false;
            update_tray();
            return 0;
        case WM_ZX_SLOT:
            apply_slot((int)wp);
            return 0;
        case WM_ZX_STATUS: { // a reply, or the keyboard telling us its state changed
            Status *s = reinterpret_cast<Status *>(lp);
            if (g_board) {
                g_st = *s;
                // an effect picked on the keyboard itself replaces the VU meter, as picking one in ZXKB does
                if (g_vu_on && !g_st.vu_requested && GetTickCount64() - g_vu_sent > VU_GRACE_MS) {
                    g_vu_on = false;
                    settings::set_int(L"vu", L"on", 0);
                }
                update_capture();
            }
            delete s;
            update_tray();
            return 0;
        }
        case WM_ZX_TUNE: {
            Tune *t = reinterpret_cast<Tune *>(lp);
            if (g_board) g_tune = *t;
            delete t;
            return 0;
        }
        case WM_TIMER:
            if (wp == TIMER_SAVE) save_now();
            return 0;
        case WM_DEVICECHANGE:
            if (wp == DBT_DEVICEARRIVAL && !g_board) g_link.rescan();
            return TRUE;
        case WM_DESTROY:
            if (g_board) {
                save_now();
                if (g_vu_on) send({proto::VU_MODE, 0}); // hand the keyboard back to its own effect, as ZX VU did
            }
            g_vu.stop();
            g_link.stop();
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            return 0;
    }
    if (msg == g_taskbar_created && msg) { add_tray_icon(); return 0; } // Explorer restarted
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    HANDLE single = CreateMutexW(nullptr, TRUE, L"Local\\ZXKB.single");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0; // already running
    g_inst = inst;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); // for the Startup-folder shortcut

    settings::import_zx_vu();
    g_vu_on  = settings::get_int(L"vu", L"on", 1) != 0;
    g_vu_style = settings::vu_style_from(settings::get(L"vu", L"style", L"bands"));
    g_vu.set_style(g_vu_style);
    std::string favs = settings::narrow(settings::get(L"favourites", L"effects"));
    for (size_t p = 0; !favs.empty() && p != std::string::npos;) {
        size_t c = favs.find(',', p);
        std::string f = favs.substr(p, c == std::string::npos ? std::string::npos : c - p);
        if (!f.empty()) g_favs.push_back(f);
        p = c == std::string::npos ? c : c + 1;
    }
    for (int i = 0; i < 10; i++) g_slots[i] = settings::get(L"slots", std::to_wstring(i + 1).c_str());

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = inst;
    wc.lpszClassName = L"ZXKB";
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(0, L"ZXKB", L"ZXKB", 0, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    DEV_BROADCAST_DEVICEINTERFACE_W filter = {sizeof(filter), DBT_DEVTYP_DEVICEINTERFACE};
    filter.dbcc_classguid = {0x4D1E55B2, 0xF16F, 0x11CF, {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}}; // HID
    HDEVNOTIFY notify = RegisterDeviceNotificationW(g_hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);

    add_tray_icon();
    g_link.start(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (notify) UnregisterDeviceNotification(notify);
    if (g_nid.hIcon) DestroyIcon(g_nid.hIcon);
    ReleaseMutex(single);
    return 0;
}
