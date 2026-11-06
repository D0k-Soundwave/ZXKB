// Macros played by ZXKB. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "appmacros.h"
#include "app.h"
#include "fnkeys.h"
#include "inputhook.h"
#include "keystore.h"
#include "labels.h"
#include "layoutcheck.h"
#include "macros.h"
#include "protocol.h"
#include <atomic>
#include <cstdlib>
#include <vector>

namespace appmacros {
namespace {

constexpr uint16_t KC_F13 = 0x68;   // [P] QMK keycodes: KC_F13 0x68 ... KC_F24 0x73 (HID usages)
constexpr uint16_t QK_MACRO = 0x7700;
constexpr int      CODES = 12;      // F13-F24
// [A] the same gap as the firmware's repeat (ZXKB_MACRO_REPEAT_MS); D0k to judge
constexpr DWORD    REPEAT_GAP_MS = 100;

struct Slot { int macro = -1; Mode mode = ONCE; };
Slot g_slot[CODES]; // tray: what each code plays

// "f,mode" for a macro, or empty
bool entry(int macro, int *f, Mode *m) {
    std::wstring v = keystore::value(L"appmacros", (L"M" + std::to_wstring(macro)).c_str());
    if (v.empty()) return false;
    *f = _wtoi(v.c_str());
    size_t c = v.find(L',');
    *m = c == std::wstring::npos ? ONCE : (Mode)_wtoi(v.c_str() + c + 1);
    return *f >= 0 && *f < CODES;
}

bool code_taken(int f, int except_macro) {
    if (fnkeys::active() && f < 10) return true; // Fn + 1..0 on a standard keyboard use F13-F22
    for (int m = 0; m < 256; m++) {
        int g; Mode md;
        if (m != except_macro && entry(m, &g, &md) && g == f) return true;
    }
    return false;
}

// Every key on every layer that carries `from` gets `to` (and is kept by ZXKB as a key change)
bool move_keys(uint16_t from, uint16_t to, int *moved, std::wstring *error) {
    std::vector<uint16_t> keys;
    int layers = 0;
    if (!layoutcheck::read_keymap(&keys, &layers)) { *error = L"The keyboard's keymap could not be read."; return false; }
    int per = g_board->rows * g_board->cols;
    uint8_t r[proto::REPORT];
    for (int l = 0; l < layers; l++)
        for (int i = 0; i < per; i++) {
            if (keys[l * per + i] != from) continue;
            int row = i / g_board->cols, col = i % g_board->cols;
            uint8_t q[6] = {0x05, (uint8_t)l, (uint8_t)row, (uint8_t)col, (uint8_t)(to >> 8), (uint8_t)to};
            if (!keyboard_ask(q, 6, r, 4)) { *error = L"The keyboard did not take a key change."; return false; }
            keystore::record(l, row, col, to);
            (*moved)++;
        }
    return true;
}

// ---------- the player (its own thread, so long waits never hold up the tray) ----------
std::atomic<bool> g_stop{false}, g_busy{false};
std::atomic<int>  g_playing{-1}; // macro being played
HANDLE            g_thread = nullptr;
std::atomic<bool> g_loop{false}; // repeat after this run (let go of a "while held" key: this run is finished, then stop)
std::wstring      g_text;
bool              g_down[256];    // usages the macro holds down now: let go if it is stopped part-way

// QMK types plain text with US key positions (quantum/send_string ascii_to_keycode): the same here
bool us_key(char c, uint8_t *usage, bool *shift) {
    static const char *plain = "abcdefghijklmnopqrstuvwxyz1234567890";
    static const char *shifted_digits = "!@#$%^&*()";
    *shift = false;
    if (c >= 'A' && c <= 'Z') { *shift = true; c = (char)(c - 'A' + 'a'); }
    for (int i = 0; plain[i]; i++) if (plain[i] == c) { *usage = (uint8_t)(4 + i); return true; }
    for (int i = 0; shifted_digits[i]; i++) if (shifted_digits[i] == c) { *usage = (uint8_t)(30 + i); *shift = true; return true; }
    struct P { char c; uint8_t u; bool s; };
    static const P sym[] = {{' ', 44, 0}, {'-', 45, 0}, {'_', 45, 1}, {'=', 46, 0}, {'+', 46, 1}, {'[', 47, 0}, {'{', 47, 1},
                            {']', 48, 0}, {'}', 48, 1}, {'\\', 49, 0}, {'|', 49, 1}, {';', 51, 0}, {':', 51, 1}, {'\'', 52, 0},
                            {'"', 52, 1}, {'`', 53, 0}, {'~', 53, 1}, {',', 54, 0}, {'<', 54, 1}, {'.', 55, 0}, {'>', 55, 1},
                            {'/', 56, 0}, {'?', 56, 1}};
    for (const P &p : sym) if (p.c == c) { *usage = p.u; *shift = p.s; return true; }
    return false;
}

void send_usage(uint8_t usage, bool down) {
    uint16_t sc = labels::scan_of_usage(usage);
    if (!sc) return;
    INPUT in = {};
    in.type           = INPUT_KEYBOARD;
    in.ki.wScan       = (WORD)(sc & 0xFF);
    in.ki.dwFlags     = KEYEVENTF_SCANCODE | (sc & 0xE000 ? KEYEVENTF_EXTENDEDKEY : 0) | (down ? 0 : KEYEVENTF_KEYUP);
    in.ki.dwExtraInfo = inputhook::MINE;
    SendInput(1, &in, sizeof(in));
    g_down[usage] = down;
    Sleep(1); // [A] 1 ms between events, as a keyboard's own reports come; some programs miss faster input
}

bool wait_ms(DWORD ms) { // false: stopped
    for (DWORD t = 0; t < ms; t += 10) {
        if (g_stop) return false;
        Sleep(ms - t < 10 ? ms - t : 10);
    }
    return true;
}

// Plays QMK macro bytes, plus ZXKB's waits that vary (1, 5, min-max|)
bool play(const std::string &b) {
    for (size_t i = 0; i < b.size() && !g_stop; i++) {
        unsigned char c = (unsigned char)b[i];
        if (c == 1 && i + 1 < b.size()) {
            unsigned char op = (unsigned char)b[++i];
            if ((op == 1 || op == 2 || op == 3) && i + 1 < b.size()) {
                uint8_t u = (uint8_t)b[++i];
                if (op != 3) send_usage(u, true);
                if (op != 2) send_usage(u, false);
            } else if (op == 4 || op == 5) {
                int lo = 0, hi = 0;
                while (i + 1 < b.size() && isdigit((unsigned char)b[i + 1])) lo = lo * 10 + (b[++i] - '0');
                hi = lo;
                if (i + 1 < b.size() && b[i + 1] == '-') {
                    i++;
                    hi = 0;
                    while (i + 1 < b.size() && isdigit((unsigned char)b[i + 1])) hi = hi * 10 + (b[++i] - '0');
                }
                if (i + 1 < b.size() && b[i + 1] == '|') i++;
                if (hi < lo) { int t = lo; lo = hi; hi = t; }
                if (!wait_ms((DWORD)(lo + (hi > lo ? rand() % (hi - lo + 1) : 0)))) return false;
            }
            continue;
        }
        uint8_t u; bool shift;
        if (!us_key((char)c, &u, &shift)) continue;
        if (shift) send_usage(0xE1, true);
        send_usage(u, true);
        send_usage(u, false);
        if (shift) send_usage(0xE1, false);
    }
    return !g_stop;
}

DWORD WINAPI player(void *) {
    std::wstring e;
    std::string bytes = macros::to_bytes(g_text, &e, true);
    do {
        if (!play(bytes)) break;
    } while (g_loop && wait_ms(REPEAT_GAP_MS));
    for (int u = 0; u < 256; u++)
        if (g_down[u]) send_usage((uint8_t)u, false); // nothing left held
    g_playing = -1;
    g_busy    = false;
    return 0;
}

void start(int macro, bool loop) {
    if (g_busy) return; // one at a time, as the keyboard does
    std::wstring text = keystore::kept_macro(macro);
    if (text.empty()) return;
    if (g_thread) { CloseHandle(g_thread); g_thread = nullptr; }
    g_text    = text;
    g_loop    = loop;
    g_stop    = false;
    g_busy    = true;
    g_playing = macro;
    srand(GetTickCount());
    g_thread = CreateThread(nullptr, 64 * 1024, player, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (!g_thread) { g_busy = false; g_playing = -1; }
}

} // namespace

Mode mode(int macro) {
    int f; Mode m;
    return entry(macro, &f, &m) ? m : KEYBOARD;
}

uint16_t trigger_code(int macro) {
    int f; Mode m;
    return entry(macro, &f, &m) ? (uint16_t)(KC_F13 + f) : 0;
}

int free_codes() {
    int n = 0;
    for (int f = 0; f < CODES; f++) n += !code_taken(f, -1);
    return n;
}

bool set_mode(int macro, Mode m, std::wstring *error) {
    int f; Mode old;
    bool had = entry(macro, &f, &old);
    std::wstring key = L"M" + std::to_wstring(macro);
    int moved = 0;
    if (m == KEYBOARD) {
        if (!had) return true;
        if (!move_keys((uint16_t)(KC_F13 + f), (uint16_t)(QK_MACRO + macro), &moved, error)) return false;
        keystore::set_value(L"appmacros", key.c_str(), nullptr);
        settings_changed();
        return true;
    }
    if (!had) {
        for (f = 0; f < CODES && code_taken(f, macro); f++) {}
        if (f == CODES) {
            *error = L"All 12 spare key codes (F13-F24) are in use" +
                     std::wstring(fnkeys::active() ? L" (Fn + 1 to Fn + 0 use ten of them on this keyboard)" : L"") +
                     L". Let the keyboard play another macro first.";
            return false;
        }
        if (!move_keys((uint16_t)(QK_MACRO + macro), (uint16_t)(KC_F13 + f), &moved, error)) return false;
    }
    keystore::set_value(L"appmacros", key.c_str(), (std::to_wstring(f) + L"," + std::to_wstring((int)m)).c_str());
    settings_changed();
    return true;
}

void reload() {
    for (Slot &s : g_slot) s = Slot();
    for (int m = 0; m < 256; m++) {
        int f; Mode md;
        if (entry(m, &f, &md)) { g_slot[f].macro = m; g_slot[f].mode = md; }
    }
}

uint16_t hook_mask() {
    uint16_t mask = 0;
    for (int f = 0; f < CODES; f++)
        if (g_slot[f].macro >= 0) mask |= (uint16_t)(1 << f);
    return mask;
}

void key(int f, bool down) {
    if (f < 0 || f >= CODES || g_slot[f].macro < 0) return;
    const Slot &s = g_slot[f];
    if (s.mode == ONCE) { if (down) start(s.macro, false); }
    else if (s.mode == HELD) {
        if (down) start(s.macro, true);
        else if (g_playing == s.macro) g_loop = false; // this run finishes, then it stops
    } else if (down) { // on / off
        if (g_playing == s.macro) { g_loop = false; g_stop = true; }
        else start(s.macro, true);
    }
}

void stop_all() { g_stop = true; }

} // namespace appmacros
