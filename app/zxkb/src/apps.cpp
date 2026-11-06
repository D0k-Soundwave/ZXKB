// Profiles by program (see apps.h).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps.h"
#include "app.h"
#include "settings.h"
#include <cwctype>

Profile current_profile();
void    apply_profile_data(const Profile &p);

namespace apps {
namespace {

HWINEVENTHOOK g_hook = nullptr;
bool          g_active = false;  // a program's profile is in use
Profile       g_before;          // the lighting before it
std::wstring  g_applied;         // the profile in use

void CALLBACK on_event(HWINEVENTHOOK, DWORD, HWND w, LONG, LONG, DWORD, DWORD) {
    std::wstring exe = exe_of(w);
    if (!exe.empty()) front_changed(exe);
}

} // namespace

std::wstring exe_of(HWND w) {
    DWORD pid = 0;
    if (!w || !GetWindowThreadProcessId(w, &pid) || !pid) return L"";
    if (pid == GetCurrentProcessId()) return L"zxkb.exe";
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); // allowed for most programs, games too
    if (!p) return L"";
    wchar_t path[MAX_PATH];
    DWORD   n = MAX_PATH;
    std::wstring exe;
    if (QueryFullProcessImageNameW(p, 0, path, &n)) {
        exe = path;
        size_t slash = exe.find_last_of(L"\\/");
        if (slash != std::wstring::npos) exe = exe.substr(slash + 1);
        for (wchar_t &c : exe) c = (wchar_t)towlower(c);
    }
    CloseHandle(p);
    return exe;
}

std::wstring profile_for(const std::wstring &exe) { return exe.empty() ? L"" : settings::get(L"apps", exe.c_str()); }

void front_changed(const std::wstring &exe) {
    if (exe.empty() || exe == L"zxkb.exe") return; // ZXKB's own windows (tray menu, window) change nothing
    if (!g_board) return;
    std::wstring p = profile_for(exe);
    Profile      want;
    if (!p.empty() && settings::load_profile(p, &want)) {
        if (!g_active) { g_before = current_profile(); g_active = true; }
        if (p != g_applied) {
            apply_profile_data(want);
            g_applied = p;
        }
    } else if (g_active) {
        g_active = false;
        g_applied.clear();
        apply_profile_data(g_before); // saved to the keyboard as usual (it is what the keyboard had)
    }
}

void start() {
    bool any = !settings::keys(L"apps").empty();
    if (any && !g_hook)
        g_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, on_event, 0, 0,
                                 WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    else if (!any) stop();
    check_now();
}

void stop() {
    if (g_hook) UnhookWinEvent(g_hook);
    g_hook = nullptr;
    if (g_active) front_changed(L"\\none"); // the lighting from before back
}

bool active() { return g_active; }

void forget() {
    g_active = false;
    g_applied.clear();
}

void check_now() {
    if (!g_hook) return;
    std::wstring exe = exe_of(GetForegroundWindow());
    if (!exe.empty()) front_changed(exe);
}

} // namespace apps
