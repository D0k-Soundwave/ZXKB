// Test: look up a keyboard's VIA definition online by USB ID and read its effect names, as the app does.
//   boards_test <vid hex> <pid hex>      e.g. boards_test 3434 0123
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/boards.h"
#include "../src/settings.h"
#include <cstdio>
#include <cstdlib>

int wmain(int argc, wchar_t **argv) {
    if (argc < 3) { std::printf("usage: boards_test <vid hex> <pid hex>\n"); return 2; }
    Board b;
    b.vid = (uint16_t)wcstoul(argv[1], nullptr, 16);
    b.pid = (uint16_t)wcstoul(argv[2], nullptr, 16);
    std::wstring err;
    DWORD t0 = GetTickCount();
    bool ok = boards::lookup_online(b.vid, b.pid, &err);
    std::printf("lookup %04X:%04X: %s in %lu ms %ls\n", b.vid, b.pid, ok ? "ok" : "failed", GetTickCount() - t0, err.c_str());
    if (!boards::apply_definition(&b)) { std::printf("no definition kept\n"); return 1; }
    std::printf("name: %ls, lighting channel %d, %d effects\n", b.def_name.c_str(), b.via_channel, (int)b.names.size());
    for (size_t i = 0; i < b.names.size(); i++) std::printf("  %2d %s\n", (int)i + 1, b.names[i].c_str());
    return 0;
}
