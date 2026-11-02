// ZXKB probe: talks to a ZXKB keyboard with the app's own HID code, for testing and measuring.
//   probe                 board info, status, effect names, LED positions
//   probe listen <s>      print Fn + number presses and status replies for s seconds
//   probe stream <s>      direct mode: stream full frames as fast as the board takes them, report frames/s
//   probe vu <s>          send a moving VU pattern for s seconds (checks the VU overlay and its 5 s timeout)
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/hidlink.h"
#include "../src/protocol.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static HidLink link;
static Board  *board;

static void pump(DWORD ms, bool print) {
    DWORD start = GetTickCount();
    MSG   m;
    while (GetTickCount() - start < ms) {
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_ZX_CONNECTED) board = reinterpret_cast<Board *>(m.lParam);
            else if (m.message == WM_ZX_SLOT && print) std::printf("slot %d pressed\n", (int)m.wParam);
            else if (m.message == WM_ZX_STATUS) {
                Status *s = reinterpret_cast<Status *>(m.lParam);
                if (print) std::printf("status: mode %d enabled %d val %d speed %d hue %d sat %d overlay %d\n", s->mode, s->enabled, s->val,
                                       s->speed, s->hue, s->sat, s->overlay);
                delete s;
            } else if (m.message == WM_ZX_DISCONNECTED) std::printf("disconnected\n");
            DispatchMessageW(&m);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    }
}

int main(int argc, char **argv) {
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"probe", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    link.start(hwnd);
    for (int i = 0; i < 400 && !board; i++) pump(10, false);
    QueryPerformanceCounter(&t1);
    if (!board) { std::printf("no ZXKB keyboard found\n"); return 1; }
    std::printf("connected in %.0f ms: %04X:%04X %ls, protocol %d, %d LEDs, %d effects\n", (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart,
                board->vid, board->pid, board->product.c_str(), board->protocol, board->led_count, (int)board->names.size());
    const Status &s = board->status;
    std::printf("status: mode %d (%s) enabled %d val %d/%d speed %d hue %d sat %d overlay %d vu_allowed %d\n", s.mode,
                s.mode ? board->names[s.mode - 1].c_str() : "-", s.enabled, s.val, s.max_val, s.speed, s.hue, s.sat, s.overlay, s.vu_allowed);
    const char *cmd = argc > 1 ? argv[1] : "";
    int secs = argc > 2 ? std::atoi(argv[2]) : 10;
    if (!*cmd) {
        for (size_t i = 0; i < board->names.size(); i++) std::printf("%3d %d %s\n", (int)i + 1, board->kinds[i], board->names[i].c_str());
        std::printf("LEDs (x,y,flags):");
        for (size_t i = 0; i + 2 < board->leds.size(); i += 3)
            std::printf("%s%d,%d,%d", i % 30 ? " " : "\n  ", board->leds[i], board->leds[i + 1], board->leds[i + 2]);
        std::printf("\n");
    } else if (!std::strcmp(cmd, "listen")) {
        std::printf("press Fn + 1..0 now (%d s)\n", secs);
        uint8_t q[2] = {proto::INFO, proto::INFO_STATUS};
        link.send(q, 2);
        pump(secs * 1000, true);
    } else if (!std::strcmp(cmd, "stream")) {
        uint8_t en[1] = {proto::DIRECT_ENABLE};
        link.send(en, 1);
        int n = board->led_count, frames = 0, packets = 0;
        QueryPerformanceCounter(&t0);
        double el = 0;
        while (el < secs) {
            for (int first = 0; first < n; first += 9) {
                int cnt = n - first < 9 ? n - first : 9;
                uint8_t p[proto::REPORT] = {proto::DIRECT_STREAM, (uint8_t)first, (uint8_t)cnt};
                for (int k = 0; k < cnt; k++) { // a white bar sweeping left to right over red
                    int  led = first + k;
                    bool lit = std::abs(board->leds[led * 3] - (frames * 4) % 240) < 16;
                    p[3 + k * 3] = 255; p[4 + k * 3] = lit ? 255 : 0; p[5 + k * 3] = lit ? 255 : 0;
                }
                if (!link.send(p, 3 + cnt * 3)) { std::printf("send failed\n"); return 1; }
                packets++;
            }
            frames++;
            QueryPerformanceCounter(&t1);
            el = (t1.QuadPart - t0.QuadPart) / (double)f.QuadPart;
        }
        std::printf("streamed %d frames (%d packets) in %.2f s: %.1f frames/s, %.0f packets/s\n", frames, packets, el, frames / el, packets / el);
        uint8_t dis[1] = {proto::DIRECT_DISABLE};
        link.send(dis, 1);
    } else if (!std::strcmp(cmd, "effect") && argc > 2) { // probe effect NAME: switch effect (not saved)
        for (size_t i = 0; i < board->names.size(); i++)
            if (board->names[i] == argv[2]) {
                uint8_t q[4] = {proto::VIA_SET, proto::VIA_RGB_MATRIX, proto::VIA_EFFECT, (uint8_t)(i + 1)};
                link.send(q, 4);
                std::printf("effect %d %s\n", (int)i + 1, argv[2]);
            }
        pump(100, false);
    } else if (!std::strcmp(cmd, "tune")) { // probe tune [size intensity]: show / set the current effect's
        uint8_t q[4] = {proto::TUNE, proto::TUNE_GET};
        if (argc > 3) { q[1] = proto::TUNE_SET; q[2] = (uint8_t)std::atoi(argv[2]); q[3] = (uint8_t)std::atoi(argv[3]); }
        link.send(q, 4);
        DWORD start = GetTickCount();
        MSG   m;
        while (GetTickCount() - start < 500) {
            if (PeekMessageW(&m, nullptr, WM_ZX_TUNE, WM_ZX_TUNE, PM_REMOVE)) {
                Tune *t = reinterpret_cast<Tune *>(m.lParam);
                std::printf("tune: mode %d flags %d size %d intensity %d\n", t->mode, t->flags, t->size, t->intensity);
                delete t;
                break;
            }
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_POSTMESSAGE);
        }
    } else if (!std::strcmp(cmd, "vu")) {
        uint8_t on[2] = {proto::VU_MODE, 1};
        link.send(on, 2);
        for (int i = 0; i < secs * 40; i++) {
            uint8_t p[17] = {proto::VU_LEVELS};
            for (int b = 0; b < 16; b++) p[1 + b] = (uint8_t)((i * 6 + b * 16) % 256);
            link.send(p, 17);
            pump(25, false);
        }
        std::printf("VU pattern sent for %d s; the keyboard should return to its effect about 5 s after this\n", secs);
    }
    link.stop();
    return 0;
}
