// Link to a ZXKB keyboard over QMK raw HID: finds it, reads its effect list, reconnects after unplugging,
// and passes keyboard messages (Fn + number, status replies) to the app window.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "layout.h"

// Messages posted to the app window
#define WM_ZX_CONNECTED    (WM_APP + 1) // lParam = Board* (receiver deletes)
#define WM_ZX_DISCONNECTED (WM_APP + 2)
#define WM_ZX_SLOT         (WM_APP + 3) // wParam = slot 1-10
#define WM_ZX_STATUS       (WM_APP + 4) // lParam = Status* (receiver deletes)
#define WM_ZX_TUNE         (WM_APP + 5) // lParam = Tune* (receiver deletes)
#define WM_ZX_RAW          (WM_APP + 8) // test tools only (raw_reports(true)): lParam new uint8_t[32], any other report
#define WM_ZX_KEYMAP       (WM_APP + 7) // lParam: new std::vector<uint16_t>, layer 0 keycodes by row * cols + col
#define WM_ZX_MIRROR       (WM_APP + 6) // a full set of LED colours arrived: read them with HidLink::mirror()

struct Status {
    uint8_t mode = 0, enabled = 0, val = 0, speed = 0, hue = 0, sat = 0, max_val = 255;
    uint8_t overlay = 0, vu_allowed = 0, vu_requested = 0;
    uint8_t vu_possible = 1; // the VU meter could show now (lighting on, allowed, e.g. Windows mode)
    uint8_t fw_version = 0;  // the keyboard's firmware version (0 = not reported)
    uint8_t bootloader = 0;  // proto::BOOT_* (protocol 4 on; 0 from older firmware)
};

struct Tune { // size and intensity of the keyboard's current effect
    uint8_t mode = 0, flags = 0, size = 128, intensity = 255;
};

struct Board {
    enum Kind : uint8_t { ZXKB, VIA } kind = ZXKB; // ZXKB module, or plain VIA firmware (no ZXKB commands)
    uint8_t                  protocol = 0, led_count = 0; // ZXKB protocol (0 on VIA boards)
    uint16_t                 via_protocol = 0;  // VIA protocol version (0 = no VIA)
    uint8_t                  via_channel  = 3;  // VIA lighting channel: 3 RGB matrix, 2 RGB light, 0 none
    std::wstring             def_name;          // keyboard name from its VIA definition
    std::vector<KeyRect>     keys;              // physical layout (VIA definition), empty if not known
    int                      rows = 0, cols = 0;  // keyboard matrix (VIA definition)
    std::vector<std::wstring> custom_names;     // the keyboard's own key codes (QK_KB + i), from its VIA definition
    std::vector<uint16_t>    keymap;            // layer 0 as the keyboard has it now (read through VIA), row * cols + col
    std::vector<uint8_t>     led_map;           // ZXKB firmware: LED under each matrix position (255 none), row * map_cols + col
    int                      map_rows = 0, map_cols = 0;
    std::vector<uint8_t>     key;               // ZXKB firmware (protocol 7): proto::KEY_BYTES per LED
    std::vector<std::string> names; // names[m - 1] = effect number m
    std::vector<uint8_t>     kinds; // 0 QMK, 1 module, 2 board/keymap
    std::vector<uint8_t>     leds;  // x, y, flags per LED
    Status                   status;
    uint16_t                 vid = 0, pid = 0;
    std::wstring             product;
};

class HidLink {
public:
    void start(HWND notify);
    void stop();
    void rescan();                                 // a device arrived: look again
    bool send(const uint8_t *payload, int length); // thread-safe; pads to one report
    bool connected() const { return dev_ != INVALID_HANDLE_VALUE && !broken_; }
    void mirror(uint8_t *rgb, int leds); // copy of the LED colours the keyboard last reported (r, g, b per LED)
    void request_via_status();           // VIA boards: ask for brightness, speed, effect and colour (posts WM_ZX_STATUS)
    void request_keymap(int rows, int cols); // layer 0 of the keymap through VIA (posts WM_ZX_KEYMAP)
    void raw_reports(bool on) { raw_ = on; }  // test tools: pass reports ZXKB does not use on as WM_ZX_RAW

private:
    static DWORD WINAPI thread_main(LPVOID self);
    void run();
    bool open_board(Board *board);
    bool probe(HANDLE h, int in_len, int out_len, Board *board);
    bool probe_via(HANDLE h, int in_len, int out_len, Board *board);
    void adopt(HANDLE h, const struct _HIDP_CAPS &caps, const Board &board); // this keyboard is the one
    bool write_report(HANDLE h, const uint8_t *payload, int length, int out_len);
    bool read_report(HANDLE h, uint8_t *buf, int in_len, DWORD timeout_ms);
    bool transact(HANDLE h, uint8_t *buf, int in_len, int out_len); // request -> matching reply
    void read_loop();

    HWND             hwnd_   = nullptr;
    HANDLE           thread_ = nullptr;
    HANDLE           stop_ev_ = nullptr, rescan_ev_ = nullptr, read_ev_ = nullptr, write_ev_ = nullptr;
    HANDLE           dev_    = INVALID_HANDLE_VALUE;
    int              in_len_ = 0, out_len_ = 0;
    uint8_t          protocol_ = 0; // ZXKB protocol version of the connected board
    uint8_t          led_count_ = 0;
    uint8_t          kind_ = 0, via_channel_ = 3;
    Status           via_status_;                  // VIA boards: built up from the replies to request_via_status()
    std::vector<uint8_t> keymap_buf_;              // keymap bytes being read
    std::vector<bool>    keymap_got_;
    uint8_t          mirror_[256 * 3] = {};
    volatile LONG    mirror_posted_ = 0; // a WM_ZX_MIRROR is waiting to be handled (do not queue another)
    volatile bool    broken_ = false;
    volatile bool    raw_ = false;
    CRITICAL_SECTION write_lock_;
};
