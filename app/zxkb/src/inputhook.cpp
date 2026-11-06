// Keyboard functions done by ZXKB itself, for any keyboard. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "inputhook.h"
#include <atomic>

namespace inputhook {
namespace {

std::atomic<bool>    g_game{false};
std::atomic<uint8_t> g_block{7}, g_socd{0};
std::atomic<DWORD>   g_last{0};
std::atomic<HWND>    g_notify_wnd{nullptr};
std::atomic<UINT>    g_notify_msg{0};
HANDLE               g_thread = nullptr;
// ZXKB's own injected keys carry this, so the hook passes them; test switch ZXKB_TEST_HOOK=1 treats other injected
// keys (a test's) as typed
constexpr ULONG_PTR MINE = 0x5A584B42; // "ZXKB"
bool                g_test = GetEnvironmentVariableW(L"ZXKB_TEST_HOOK", nullptr, 0) > 0;
DWORD                g_thread_id = 0;

// SOCD pairs and what is held. active: which side of the pair Windows has down (-1 none). Hook thread only.
const DWORD pairs[4][2] = {{'A', 'D'}, {'W', 'S'}, {VK_LEFT, VK_RIGHT}, {VK_UP, VK_DOWN}};
bool held[4][2];
int  active[4] = {-1, -1, -1, -1};

void send_key(DWORD vk, bool down) { // injected: the hook lets its own keys through
    INPUT in = {};
    in.type       = INPUT_KEYBOARD;
    in.ki.wVk     = (WORD)vk;
    in.ki.wScan   = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    in.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | (vk >= VK_LEFT && vk <= VK_DOWN ? KEYEVENTF_EXTENDEDKEY : 0);
    in.ki.dwExtraInfo = MINE;
    SendInput(1, &in, sizeof(in));
}

// true: the key is dealt with here (not passed on)
bool socd(DWORD vk, bool down) {
    uint8_t on = g_socd.load();
    for (int p = 0; p < 4; p++) {
        if (!(on & (p < 2 ? 1 : 2))) continue;
        for (int s = 0; s < 2; s++) {
            if (vk != pairs[p][s]) continue;
            int o = 1 - s;
            if (down) {
                bool repeat = held[p][s];
                held[p][s]  = true;
                if (repeat) return active[p] != s; // Windows' key repeat: only for the side that counts
                if (active[p] == o) send_key(pairs[p][o], false); // the last pressed wins
                active[p] = s;
                return false;
            }
            held[p][s] = false;
            if (active[p] != s) return true; // already let go for the other side
            active[p] = -1;
            if (held[p][o]) { // the other is still held: it counts again
                send_key(vk, false);
                send_key(pairs[p][o], true);
                active[p] = o;
                return true;
            }
            return false;
        }
    }
    return false;
}

LRESULT CALLBACK proc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        auto *k = reinterpret_cast<KBDLLHOOKSTRUCT *>(lp);
        bool down = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
        bool typed = !(k->flags & LLKHF_INJECTED) || (g_test && k->dwExtraInfo != MINE);
        if (typed) {
            if (down) {
                g_last = GetTickCount();
                if (HWND w = g_notify_wnd.exchange(nullptr)) PostMessageW(w, g_notify_msg, 0, 0);
            }
            if (g_game) {
                uint8_t b = g_block & 7 ? g_block & 7 : 7;
                bool alt = (k->flags & LLKHF_ALTDOWN) != 0;
                if ((b & 1) && k->vkCode == VK_TAB && alt) return 1;
                if ((b & 2) && k->vkCode == VK_F4 && alt) return 1;
                if ((b & 4) && (k->vkCode == VK_LWIN || k->vkCode == VK_RWIN)) return 1;
            }
            if (g_socd && socd(k->vkCode, down)) return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

DWORD WINAPI thread(void *) {
    HHOOK h = SetWindowsHookExW(WH_KEYBOARD_LL, proc, GetModuleHandleW(nullptr), 0);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {} // WM_QUIT ends it
    if (h) UnhookWindowsHookEx(h);
    return 0;
}

} // namespace

void configure(const Config &c) {
    g_game  = c.game;
    g_block = c.block;
    g_socd  = c.socd;
    if (!c.socd) { // let go of anything SOCD was holding back
        for (int p = 0; p < 4; p++) { held[p][0] = held[p][1] = false; active[p] = -1; }
    }
    bool need = c.game || c.socd || c.watch;
    if (need && !g_thread) {
        g_thread = CreateThread(nullptr, 64 * 1024, thread, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, &g_thread_id);
    } else if (!need && g_thread) {
        PostThreadMessageW(g_thread_id, WM_QUIT, 0, 0);
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}

DWORD last_key() { return g_last; }

void notify_next_key(HWND w, UINT msg) {
    g_notify_msg = msg;
    g_notify_wnd = w;
}

} // namespace inputhook
