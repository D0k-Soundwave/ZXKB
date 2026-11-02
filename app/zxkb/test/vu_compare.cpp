// Run ZXKB's VuMath on a test recording (48 kHz stereo float32) and print the levels per block.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/vumath.h"
#include <cstdio>
#include <cstring>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    bool abc = std::strcmp(argv[2], "abc") == 0;
    FILE *f = std::fopen(argv[1], "rb");
    if (!f) return 1;
    std::vector<float> blk(VuMath::BLOCK * 2);
    static VuMath vu;
    vu.setup(48000);
    uint8_t lv[VuMath::BANDS];
    while (std::fread(blk.data(), sizeof(float), blk.size(), f) == blk.size()) {
        vu.process(blk.data(), 2, abc, lv);
        for (int i = 0; i < VuMath::BANDS; i++) std::printf(i ? " %d" : "%d", lv[i]);
        std::printf("\n");
    }
    std::fclose(f);
    return 0;
}
