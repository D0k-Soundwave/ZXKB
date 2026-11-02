// Try music-style settings on a saved recording (from vu_live) and report how far each bar swings,
// next to the frequency-bands style as the reference for "enough movement".
//   vu_tune <file> <rate> <channels>
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/vumath.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

struct Result { double swing[3], avg[3]; };

static Result run(const std::vector<float> &a, int rate, int ch, int style, const VuMath::MusicTune *mt) {
    static VuMath vu;
    vu = VuMath();
    vu.setup(rate);
    if (mt) vu.music = *mt;
    std::vector<double> bars[3];
    const int first[3] = {11, 5, 0}, last[3] = {15, 10, 4};
    size_t step = (size_t)VuMath::BLOCK * ch;
    size_t skip = (size_t)rate * 3 * ch; // let the running levels settle for 3 s before measuring
    for (size_t off = 0; off + step <= a.size(); off += step) {
        uint8_t lv[16];
        vu.process(&a[off], ch, style, lv);
        if (off < skip) continue;
        for (int k = 0; k < 3; k++) {
            int m = 0;
            for (int b = first[k]; b <= last[k]; b++) m = std::max(m, (int)lv[b]);
            bars[k].push_back(m);
        }
    }
    Result r;
    for (int k = 0; k < 3; k++) {
        std::vector<double> s = bars[k];
        double sum = 0;
        for (double v : s) sum += v;
        std::sort(s.begin(), s.end());
        r.swing[k] = s[s.size() * 9 / 10] - s[s.size() / 10];
        r.avg[k] = sum / s.size();
    }
    return r;
}

int main(int argc, char **argv) {
    if (argc < 4) return 2;
    FILE *f = std::fopen(argv[1], "rb");
    if (!f) return 1;
    int rate = std::atoi(argv[2]), ch = std::atoi(argv[3]);
    std::vector<float> a;
    float buf[4096];
    size_t n;
    while ((n = std::fread(buf, sizeof(float), 4096, f)) > 0) a.insert(a.end(), buf, buf + n);
    std::fclose(f);
    std::printf("%.1f s of audio\n", a.size() / (double)ch / rate);
    Result ref = run(a, rate, ch, VU_BANDS, nullptr);
    std::printf("reference, frequency bands: swing top %3.0f middle %3.0f bottom %3.0f (mean %3.0f); average %3.0f %3.0f %3.0f\n",
                ref.swing[0], ref.swing[1], ref.swing[2], (ref.swing[0] + ref.swing[1] + ref.swing[2]) / 3, ref.avg[0], ref.avg[1], ref.avg[2]);
    const double ranges[] = {30, 18};
    const double refs[]   = {0, 1, 2, 4};      // 0 = as now (about 0.36 s at 96 kHz)
    const double falls[]  = {0, 0.06, 0.12};   // 0 = as now (about 22 ms at 96 kHz)
    for (double rg : ranges)
        for (double rs : refs)
            for (double fs : falls) {
                VuMath::MusicTune mt;
                mt.range_db = rg; mt.ref_seconds = rs; mt.fall_seconds = fs;
                Result r = run(a, rate, ch, VU_MUSIC, &mt);
                std::printf("range %2.0f dB, reference %s, fall %s: swing %3.0f %3.0f %3.0f (mean %3.0f); average %3.0f %3.0f %3.0f\n", rg,
                            rs ? (rs == 1 ? "1 s   " : rs == 2 ? "2 s   " : "4 s   ") : "as now", fs ? (fs < 0.1 ? "60 ms " : "120 ms") : "as now",
                            r.swing[0], r.swing[1], r.swing[2], (r.swing[0] + r.swing[1] + r.swing[2]) / 3, r.avg[0], r.avg[1], r.avg[2]);
            }
    return 0;
}
