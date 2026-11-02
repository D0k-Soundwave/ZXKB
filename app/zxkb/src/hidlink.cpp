// Link to a ZXKB keyboard over QMK raw HID.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hidlink.h"
#include "protocol.h"
#include <setupapi.h>
#include <cstring>
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}

namespace {
constexpr DWORD REPLY_TIMEOUT_MS = 300;  // a reply normally takes a few ms; a board that says nothing is not ZXKB
constexpr DWORD WRITE_TIMEOUT_MS = 1000;
constexpr int   OPEN_ATTEMPTS    = 5;    // a keyboard needs a moment after it appears before it answers
constexpr DWORD OPEN_RETRY_MS    = 400;
}

void HidLink::start(HWND notify) {
    hwnd_      = notify;
    stop_ev_   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    rescan_ev_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    read_ev_   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    write_ev_  = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    InitializeCriticalSection(&write_lock_);
    thread_ = CreateThread(nullptr, 64 * 1024, thread_main, this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
}

void HidLink::stop() {
    if (!thread_) return;
    SetEvent(stop_ev_);
    WaitForSingleObject(thread_, 3000);
    CloseHandle(thread_);
    thread_ = nullptr;
}

void HidLink::rescan() {
    if (rescan_ev_) SetEvent(rescan_ev_);
}

DWORD WINAPI HidLink::thread_main(LPVOID self) {
    static_cast<HidLink *>(self)->run();
    return 0;
}

bool HidLink::write_report(HANDLE h, const uint8_t *payload, int length, int out_len) {
    uint8_t buf[65] = {0}; // report id 0 + payload
    if (out_len > (int)sizeof(buf)) out_len = sizeof(buf);
    if (length > out_len - 1) length = out_len - 1;
    std::memcpy(buf + 1, payload, length);
    OVERLAPPED ov = {};
    ov.hEvent = write_ev_;
    ResetEvent(write_ev_);
    DWORD n = 0;
    if (!WriteFile(h, buf, out_len, nullptr, &ov) && GetLastError() != ERROR_IO_PENDING) return false;
    if (WaitForSingleObject(write_ev_, WRITE_TIMEOUT_MS) != WAIT_OBJECT_0) {
        CancelIoEx(h, &ov);
        GetOverlappedResult(h, &ov, &n, TRUE);
        return false;
    }
    return GetOverlappedResult(h, &ov, &n, FALSE) && n == (DWORD)out_len;
}

bool HidLink::read_report(HANDLE h, uint8_t *buf, int in_len, DWORD timeout_ms) {
    OVERLAPPED ov = {};
    ov.hEvent = read_ev_;
    ResetEvent(read_ev_);
    DWORD n = 0;
    if (!ReadFile(h, buf, in_len, nullptr, &ov) && GetLastError() != ERROR_IO_PENDING) return false;
    HANDLE waits[2] = {read_ev_, stop_ev_};
    if (WaitForMultipleObjects(2, waits, FALSE, timeout_ms) != WAIT_OBJECT_0) {
        CancelIoEx(h, &ov);
        GetOverlappedResult(h, &ov, &n, TRUE);
        return false;
    }
    return GetOverlappedResult(h, &ov, &n, FALSE) && n > 0;
}

// Send a request and wait for the reply to it (other reports arriving meanwhile are skipped).
bool HidLink::transact(HANDLE h, uint8_t *buf, int in_len, int out_len) {
    uint8_t cmd = buf[0], sub = buf[1];
    if (!write_report(h, buf, proto::REPORT, out_len)) return false;
    uint8_t in[65];
    if (in_len > (int)sizeof(in)) in_len = sizeof(in);
    DWORD start = GetTickCount();
    while (GetTickCount() - start < REPLY_TIMEOUT_MS) {
        if (!read_report(h, in, in_len, REPLY_TIMEOUT_MS)) return false;
        if (in[1] == cmd && (cmd != proto::INFO || in[2] == sub)) {
            std::memcpy(buf, in + 1, proto::REPORT);
            return true;
        }
    }
    return false;
}

static Status parse_status(const uint8_t *p) {
    Status s;
    s.mode = p[5]; s.enabled = p[6]; s.val = p[7]; s.speed = p[8]; s.hue = p[9]; s.sat = p[10];
    s.max_val = p[11]; s.overlay = p[12]; s.vu_allowed = p[13]; s.vu_requested = p[14]; s.vu_possible = p[15];
    return s;
}

bool HidLink::probe(HANDLE h, int in_len, int out_len, Board *board) {
    uint8_t buf[proto::REPORT] = {proto::INFO, proto::INFO_STATUS};
    if (!transact(h, buf, in_len, out_len) || buf[2] < 1) return false; // not a ZXKB board
    board->protocol  = buf[2];
    board->led_count = buf[3];
    board->status    = parse_status(buf);
    if (board->protocol < 2) board->status.vu_possible = 1; // older firmware does not report it
    int effects      = buf[4];
    board->names.clear();
    board->kinds.clear();
    for (int m = 1; m <= effects; m++) {
        uint8_t q[proto::REPORT] = {proto::INFO, proto::INFO_NAME, (uint8_t)m};
        if (!transact(h, q, in_len, out_len)) return false;
        q[proto::REPORT - 1] = 0;
        board->kinds.push_back(q[3]);
        board->names.emplace_back(q[3] == 0xFF ? "" : (const char *)&q[4]);
    }
    board->leds.clear();
    for (int first = 0; first < board->led_count;) {
        uint8_t q[proto::REPORT] = {proto::INFO, proto::INFO_LEDS, (uint8_t)first};
        if (!transact(h, q, in_len, out_len) || q[3] == 0) return false;
        board->leds.insert(board->leds.end(), q + 4, q + 4 + q[3] * 3);
        first += q[3];
    }
    return true;
}

bool HidLink::open_board(Board *board) {
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);
    HDEVINFO set = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    SP_DEVICE_INTERFACE_DATA ifd = {sizeof(ifd)};
    for (DWORD i = 0; !found && SetupDiEnumDeviceInterfaces(set, nullptr, &hid_guid, i, &ifd); i++) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifd, nullptr, 0, &need, nullptr);
        if (!need) continue;
        std::vector<uint8_t> mem(need);
        auto *detail   = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(mem.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &ifd, detail, need, nullptr, nullptr)) continue;
        HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue; // keyboards' own key collections are not ours to open
        PHIDP_PREPARSED_DATA pp = nullptr;
        HIDP_CAPS caps = {};
        bool raw = false;
        if (HidD_GetPreparsedData(h, &pp)) {
            raw = HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS && caps.UsagePage == proto::USAGE_PAGE && caps.Usage == proto::USAGE;
            HidD_FreePreparsedData(pp);
        }
        if (raw && probe(h, caps.InputReportByteLength, caps.OutputReportByteLength, board)) {
            HIDD_ATTRIBUTES at = {sizeof(at)};
            if (HidD_GetAttributes(h, &at)) { board->vid = at.VendorID; board->pid = at.ProductID; }
            wchar_t name[128] = {0};
            if (HidD_GetProductString(h, name, sizeof(name) - sizeof(wchar_t))) board->product = name;
            EnterCriticalSection(&write_lock_);
            dev_     = h;
            protocol_ = board->protocol;
            in_len_  = caps.InputReportByteLength;
            out_len_ = caps.OutputReportByteLength;
            broken_  = false;
            LeaveCriticalSection(&write_lock_);
            found = true;
        } else {
            CloseHandle(h);
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

bool HidLink::send(const uint8_t *payload, int length) {
    EnterCriticalSection(&write_lock_);
    bool ok = dev_ != INVALID_HANDLE_VALUE && !broken_ && write_report(dev_, payload, length, out_len_);
    if (!ok && dev_ != INVALID_HANDLE_VALUE && !broken_) {
        broken_ = true;          // unplugged or reflashed: the reader notices and reconnects
        CancelIoEx(dev_, nullptr);
    }
    LeaveCriticalSection(&write_lock_);
    return ok;
}

void HidLink::read_loop() {
    uint8_t buf[65];
    int len = in_len_ > (int)sizeof(buf) ? (int)sizeof(buf) : in_len_;
    while (!broken_) {
        if (!read_report(dev_, buf, len, INFINITE)) return; // unplugged, cancelled or stopping
        const uint8_t *p = buf + 1;
        if (p[0] == proto::SLOT && p[1] >= 1 && p[1] <= 10) {
            PostMessageW(hwnd_, WM_ZX_SLOT, p[1], 0);
        } else if (p[0] == proto::INFO && p[1] == proto::INFO_STATUS) {
            Status *s = new Status(parse_status(p));
            if (protocol_ < 2) s->vu_possible = 1; // older firmware does not report it
            PostMessageW(hwnd_, WM_ZX_STATUS, 0, (LPARAM)s);
        } else if (p[0] == proto::TUNE) {
            Tune *t = new Tune;
            t->mode = p[2]; t->flags = p[3]; t->size = p[4]; t->intensity = p[5];
            PostMessageW(hwnd_, WM_ZX_TUNE, 0, (LPARAM)t);
        }
    }
}

void HidLink::run() {
    for (;;) {
        Board *board = new Board;
        bool   ok    = false;
        for (int attempt = 0; attempt < OPEN_ATTEMPTS && !ok; attempt++) {
            ok = open_board(board);
            if (!ok && WaitForSingleObject(stop_ev_, OPEN_RETRY_MS) == WAIT_OBJECT_0) { delete board; return; }
        }
        if (!ok) {
            delete board;
            HANDLE waits[2] = {stop_ev_, rescan_ev_};
            if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) == WAIT_OBJECT_0) return;
            continue;
        }
        PostMessageW(hwnd_, WM_ZX_CONNECTED, 0, (LPARAM)board);
        read_loop();
        EnterCriticalSection(&write_lock_);
        CloseHandle(dev_);
        dev_ = INVALID_HANDLE_VALUE;
        LeaveCriticalSection(&write_lock_);
        PostMessageW(hwnd_, WM_ZX_DISCONNECTED, 0, 0);
        if (WaitForSingleObject(stop_ev_, 0) == WAIT_OBJECT_0) return;
    }
}
