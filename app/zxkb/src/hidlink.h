// Link to a ZXKB keyboard over QMK raw HID: finds it, reads its effect list, reconnects after unplugging,
// and passes keyboard messages (Fn + number, status replies) to the app window.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

// Messages posted to the app window
#define WM_ZX_CONNECTED    (WM_APP + 1) // lParam = Board* (receiver deletes)
#define WM_ZX_DISCONNECTED (WM_APP + 2)
#define WM_ZX_SLOT         (WM_APP + 3) // wParam = slot 1-10
#define WM_ZX_STATUS       (WM_APP + 4) // lParam = Status* (receiver deletes)

struct Status {
    uint8_t mode = 0, enabled = 0, val = 0, speed = 0, hue = 0, sat = 0, max_val = 255;
    uint8_t overlay = 0, vu_allowed = 0, vu_requested = 0;
};

struct Board {
    uint8_t                  protocol = 0, led_count = 0;
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

private:
    static DWORD WINAPI thread_main(LPVOID self);
    void run();
    bool open_board(Board *board);
    bool probe(HANDLE h, int in_len, int out_len, Board *board);
    bool write_report(HANDLE h, const uint8_t *payload, int length, int out_len);
    bool read_report(HANDLE h, uint8_t *buf, int in_len, DWORD timeout_ms);
    bool transact(HANDLE h, uint8_t *buf, int in_len, int out_len); // request -> matching reply
    void read_loop();

    HWND             hwnd_   = nullptr;
    HANDLE           thread_ = nullptr;
    HANDLE           stop_ev_ = nullptr, rescan_ev_ = nullptr, read_ev_ = nullptr, write_ev_ = nullptr;
    HANDLE           dev_    = INVALID_HANDLE_VALUE;
    int              in_len_ = 0, out_len_ = 0;
    volatile bool    broken_ = false;
    CRITICAL_SECTION write_lock_;
};
