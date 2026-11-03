// Link to a ZXKB keyboard over QMK raw HID.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hidlink.h"
#include "protocol.h"
#include "boards.h"
#include <setupapi.h>
#include <cstring>
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}

namespace {
// [M] 2026-11-02, Q3: probe read status, 59 names and 88 LED positions (70 round trips) in 308-342 ms.
// [C] about 5 ms per reply (342 / 70); 300 ms is ~60x that, so a board that stays silent is not ZXKB.
constexpr DWORD REPLY_TIMEOUT_MS = 300;
constexpr DWORD WRITE_TIMEOUT_MS = 1000; // [A] a 32-byte write completes within one 1 ms USB poll; 1 s only catches a dead link
// [A] not measured: how long a keyboard takes to answer after Windows announces it. 5 x 400 ms = 2 s of retries.
constexpr int   OPEN_ATTEMPTS    = 5;
constexpr DWORD OPEN_RETRY_MS    = 400;
}

void HidLink::start(HWND notify) {
    hwnd_      = notify;
    stop_ev_   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    rescan_ev_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    read_ev_   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    write_ev_  = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    InitializeCriticalSection(&write_lock_);
    // [A] 64 KB stack reserved (not committed); the thread's largest locals are a few hundred bytes
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
        if (in[1] == 0xFF && cmd != 0xFF) return false; // VIA: "unknown command", e.g. a ZXKB query to a plain VIA board
        // The tray copy and the window copy of ZXKB both read the keyboard, and Windows gives every reply to both,
        // so a reply must echo what was asked: the same sub-command, and the same effect, LED or map position.
        // (M 2026-11-03: without this the window took the tray's map replies, and G lit the "-" key.)
        bool same = in[1] == cmd;
        if (same && (cmd == proto::INFO || cmd == proto::KEYS)) same = in[2] == sub;
        if (same && cmd == proto::INFO && (sub == proto::INFO_NAME || sub == proto::INFO_LEDS)) same = in[3] == buf[2];
        if (same && cmd == proto::INFO && sub == proto::INFO_MATRIX) same = in[5] == buf[2];
        if (same && cmd == proto::KEYS && sub == proto::KEYS_GET) same = in[3] == buf[2];
        if (same && cmd == proto::VIA_GET) same = in[2] == buf[1] && in[3] == buf[2];
        if (same) {
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
    s.fw_version = p[16]; s.bootloader = p[17];
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
    board->led_map.clear();
    if (board->protocol >= 5) // which LED sits under each key
        for (int first = 0;;) {
            uint8_t q[proto::REPORT] = {proto::INFO, proto::INFO_MATRIX, (uint8_t)first};
            if (!transact(h, q, in_len, out_len)) return false;
            board->map_rows = q[2];
            board->map_cols = q[3];
            int total = q[2] * q[3], n = q[5];
            board->led_map.insert(board->led_map.end(), q + 6, q + 6 + n);
            first += n;
            if (n == 0 || first >= total || first >= 256) break;
        }
    board->key.assign(board->led_count * proto::KEY_BYTES, 0);
    if (board->protocol >= 8) // per-key colours and effects as the keyboard keeps them
        for (int first = 0; first < board->led_count;) {
            uint8_t q[proto::REPORT] = {proto::KEYS, proto::KEYS_GET, (uint8_t)first};
            if (!transact(h, q, in_len, out_len) || q[3] == 0) break;
            for (int k = 0; k < q[3] && first + k < board->led_count; k++)
                std::memcpy(&board->key[(first + k) * proto::KEY_BYTES], &q[4 + k * proto::KEY_BYTES], proto::KEY_BYTES);
            first += q[3];
        }
    { // most ZXKB keyboards have VIA too: needed to read their keymap for the key labels
        uint8_t q[proto::REPORT] = {proto::VIA_PROTOCOL};
        if (transact(h, q, in_len, out_len)) board->via_protocol = (uint16_t)(q[1] << 8 | q[2]);
    }
    boards::apply_definition(board); // the layout, if the keyboard's definition is kept
    return true;
}

// Plain VIA firmware: VIA protocol version, then the lighting state through VIA's lighting values
bool HidLink::probe_via(HANDLE h, int in_len, int out_len, Board *board) {
    uint8_t buf[proto::REPORT] = {proto::VIA_PROTOCOL};
    if (!transact(h, buf, in_len, out_len)) return false;
    uint16_t v = (uint16_t)(buf[1] << 8 | buf[2]);
    if (v == 0 || v == 0xFFFF) return false;
    board->kind         = Board::VIA;
    board->via_protocol = v;
    board->protocol     = 0;
    board->led_count    = 0;
    boards::apply_definition(board); // effect names and lighting channel, if the definition is kept
    Status &s = board->status;
    s         = Status();
    s.max_val = 255; // VIA scales brightness to 0-255 itself
    s.vu_possible = 0;
    if (board->via_channel) {
        const uint8_t ids[4] = {proto::VIA_BRIGHTNESS, proto::VIA_SPEED, proto::VIA_EFFECT, proto::VIA_COLOUR};
        for (uint8_t id : ids) {
            uint8_t q[proto::REPORT] = {proto::VIA_GET, board->via_channel, id};
            if (!transact(h, q, in_len, out_len)) continue;
            if (id == proto::VIA_BRIGHTNESS) s.val = q[3];
            else if (id == proto::VIA_SPEED) s.speed = q[3];
            else if (id == proto::VIA_EFFECT) { s.mode = q[3]; s.enabled = q[3] != 0; }
            else { s.hue = q[3]; s.sat = q[4]; }
        }
    }
    return true;
}

// [P] VIA reads the keymap in pieces of up to 28 bytes (32-byte report minus a 4-byte header): QMK quantum/via.c
constexpr int KEYMAP_CHUNK = 28;

void HidLink::request_keymap(int rows, int cols) {
    int bytes = rows * cols * 2;
    if (bytes <= 0 || bytes > 4096) return;
    EnterCriticalSection(&write_lock_);
    keymap_buf_.assign(bytes, 0);
    keymap_got_.assign((bytes + KEYMAP_CHUNK - 1) / KEYMAP_CHUNK, false);
    LeaveCriticalSection(&write_lock_);
    for (int off = 0; off < bytes; off += KEYMAP_CHUNK) {
        uint8_t n = (uint8_t)(bytes - off < KEYMAP_CHUNK ? bytes - off : KEYMAP_CHUNK);
        uint8_t q[4] = {proto::VIA_KEYMAP_BUFFER, (uint8_t)(off >> 8), (uint8_t)off, n};
        send(q, 4);
    }
}

void HidLink::request_via_status() {
    if (kind_ != Board::VIA || !via_channel_) return;
    const uint8_t ids[4] = {proto::VIA_BRIGHTNESS, proto::VIA_SPEED, proto::VIA_EFFECT, proto::VIA_COLOUR}; // colour last: it posts
    for (uint8_t id : ids) {
        uint8_t q[3] = {proto::VIA_GET, via_channel_, id};
        send(q, 3);
    }
}

bool HidLink::open_board(Board *board) {
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);
    HDEVINFO set = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    // A ZXKB keyboard is preferred; the first plain VIA keyboard is kept open in case there is none
    HANDLE   via_h = INVALID_HANDLE_VALUE;
    Board    via_board;
    HIDP_CAPS via_caps = {};
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
        if (!raw) { CloseHandle(h); continue; }
        HIDD_ATTRIBUTES at = {sizeof(at)};
        wchar_t name[128] = {0};
        HidD_GetAttributes(h, &at);
        HidD_GetProductString(h, name, sizeof(name) - sizeof(wchar_t));
        board->vid = at.VendorID; board->pid = at.ProductID; board->product = name;
        // test switch: ZXKB_TEST_VIA=1 treats a ZXKB keyboard as plain VIA (it answers VIA too)
        static const bool test_via = GetEnvironmentVariableW(L"ZXKB_TEST_VIA", nullptr, 0) > 0;
        if (!test_via && probe(h, caps.InputReportByteLength, caps.OutputReportByteLength, board)) {
            board->kind = Board::ZXKB;
            adopt(h, caps, *board);
            found = true;
        } else if (via_h == INVALID_HANDLE_VALUE) {
            via_board.vid = at.VendorID; via_board.pid = at.ProductID; via_board.product = name;
            if (probe_via(h, caps.InputReportByteLength, caps.OutputReportByteLength, &via_board)) { via_h = h; via_caps = caps; }
            else CloseHandle(h);
        } else {
            CloseHandle(h);
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    if (found) {
        if (via_h != INVALID_HANDLE_VALUE) CloseHandle(via_h);
    } else if (via_h != INVALID_HANDLE_VALUE) {
        *board = via_board;
        adopt(via_h, via_caps, *board);
        found = true;
    }
    return found;
}

void HidLink::adopt(HANDLE h, const HIDP_CAPS &caps, const Board &board) {
    EnterCriticalSection(&write_lock_);
    dev_         = h;
    protocol_    = board.protocol;
    led_count_   = board.led_count;
    kind_        = board.kind;
    via_channel_ = board.via_channel;
    via_status_  = board.status;
    in_len_      = caps.InputReportByteLength;
    out_len_     = caps.OutputReportByteLength;
    broken_      = false;
    LeaveCriticalSection(&write_lock_);
}

void HidLink::mirror(uint8_t *rgb, int leds) {
    InterlockedExchange(&mirror_posted_, 0);
    if (leds > 256) leds = 256;
    std::memcpy(rgb, mirror_, leds * 3); // a frame may be half-updated: the next one follows within ~33 ms
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
        } else if (p[0] == proto::MIRROR) {
            int first = p[1], count = p[2];
            if (count <= 9 && first + count <= 256) std::memcpy(mirror_ + first * 3, p + 3, count * 3);
            // from v17 the keyboard sends only the LEDs that changed, so any packet can be the last of a frame
            if (InterlockedExchange(&mirror_posted_, 1) == 0) PostMessageW(hwnd_, WM_ZX_MIRROR, 0, 0);
        } else if (p[0] == proto::VIA_GET && kind_ == Board::VIA && p[1] == via_channel_) {
            Status &s = via_status_;
            if (p[2] == proto::VIA_BRIGHTNESS) s.val = p[3];
            else if (p[2] == proto::VIA_SPEED) s.speed = p[3];
            else if (p[2] == proto::VIA_EFFECT) { s.mode = p[3] ? p[3] : s.mode; s.enabled = p[3] != 0; }
            else if (p[2] == proto::VIA_COLOUR) {
                s.hue = p[3]; s.sat = p[4];
                PostMessageW(hwnd_, WM_ZX_STATUS, 0, (LPARAM) new Status(s)); // the last of request_via_status()
            }
        } else if (p[0] == proto::VIA_KEYMAP_BUFFER) {
            int off = p[1] << 8 | p[2], n = p[3];
            EnterCriticalSection(&write_lock_);
            bool done = false;
            if (n <= KEYMAP_CHUNK && off % KEYMAP_CHUNK == 0 && off + n <= (int)keymap_buf_.size()) {
                std::memcpy(&keymap_buf_[off], p + 4, n);
                keymap_got_[off / KEYMAP_CHUNK] = true;
                done = true;
                for (bool g : keymap_got_) done &= g;
            }
            if (done) {
                auto *km = new std::vector<uint16_t>(keymap_buf_.size() / 2);
                for (size_t i = 0; i < km->size(); i++) (*km)[i] = (uint16_t)(keymap_buf_[i * 2] << 8 | keymap_buf_[i * 2 + 1]);
                keymap_got_.assign(keymap_got_.size(), false);
                PostMessageW(hwnd_, WM_ZX_KEYMAP, 0, (LPARAM)km);
            }
            LeaveCriticalSection(&write_lock_);
        } else if (p[0] == proto::TUNE) {
            Tune *t = new Tune;
            t->mode = p[2]; t->flags = p[3]; t->size = p[4]; t->intensity = p[5];
            PostMessageW(hwnd_, WM_ZX_TUNE, 0, (LPARAM)t);
        } else if (raw_) {
            uint8_t *copy = new uint8_t[proto::REPORT];
            std::memcpy(copy, p, proto::REPORT);
            PostMessageW(hwnd_, WM_ZX_RAW, 0, (LPARAM)copy);
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
