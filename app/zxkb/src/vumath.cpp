// VU meter maths, ported line for line from ZX VU (app/legacy/zx_vu.py).
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "vumath.h"
#include <cmath>

namespace {
// Tunables approved by D0k (same names and values as zx_vu.py)
constexpr double RANGE_DB    = 30;   // dynamic range shown across the keyboard; lower = longer bars
constexpr double HEADROOM_DB = 9;    // bars reach the far right only when this much louder than the recent average
constexpr double CURVE       = 1.7;  // >1 = more up-and-down movement
constexpr double FALL        = 0.72; // how fast bars drop each block (lower = faster)
constexpr double PI          = 3.14159265358979323846;

double clip01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
uint8_t to_level(double v) { return (uint8_t)(int)(v * 255); } // int() truncates, as in Python
} // namespace

void VuMath::setup(int rate) {
    rate_ = rate;
    const int N = BLOCK;
    for (int n = 0; n < N; n++) win_[n] = 0.5 - 0.5 * std::cos(2 * PI * n / (N - 1)); // np.hanning
    for (int k = 0; k < N / 2; k++) {
        cos_[k] = std::cos(-2 * PI * k / N);
        sin_[k] = std::sin(-2 * PI * k / N);
    }
    // edges = np.geomspace(40, 16000, BANDS + 1); band i = bins with edges[i] <= freq < edges[i + 1]
    double edges[BANDS + 1];
    const double l0 = std::log10(40.0), l1 = std::log10(16000.0);
    for (int i = 0; i <= BANDS; i++) edges[i] = std::pow(10.0, l0 + (l1 - l0) * i / BANDS);
    edges[0] = 40.0;
    edges[BANDS] = 16000.0;
    const double bin_hz = (double)rate / N; // np.fft.rfftfreq
    for (int i = 0; i < BANDS; i++) {
        band_lo_[i] = band_hi_[i] = 0;
        bool found = false;
        for (int k = 0; k <= N / 2; k++) {
            double f = k * bin_hz;
            if (f >= edges[i] && f < edges[i + 1]) {
                if (!found) band_lo_[i] = k;
                band_hi_[i] = k + 1;
                found = true;
            }
        }
    }
}

// Iterative radix-2 complex FFT, N = BLOCK
void VuMath::fft() {
    const int N = BLOCK;
    for (int i = 1, j = 0; i < N; i++) { // bit reversal
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re_[i]; re_[i] = re_[j]; re_[j] = t;
            t = im_[i]; im_[i] = im_[j]; im_[j] = t;
        }
    }
    for (int len = 2; len <= N; len <<= 1) {
        int step = N / len;
        for (int i = 0; i < N; i += len) {
            for (int k = 0; k < len / 2; k++) {
                double wr = cos_[k * step], wi = sin_[k * step];
                int a = i + k, b = i + k + len / 2;
                double xr = re_[b] * wr - im_[b] * wi;
                double xi = re_[b] * wi + im_[b] * wr;
                re_[b] = re_[a] - xr; im_[b] = im_[a] - xi;
                re_[a] += xr;         im_[a] += xi;
            }
        }
    }
}

void VuMath::process(const float *frames, int ch, int style, uint8_t out[BANDS]) {
    const bool abc = style == VU_ABC;
    const int N = BLOCK;
    // frames[:, :2] (mono input is used as both channels); mono = mean of the first two channels
    auto L = [&](int n) { return (double)frames[n * ch]; };
    auto R = [&](int n) { return (double)frames[n * ch + (ch > 1 ? 1 : 0)]; };
    for (int n = 0; n < N; n++) mono_[n] = (L(n) + R(n)) / 2;

    if (abc) {
        // ACB stereo: A left, C centre, B right. L = A + 0.7C, R = B + 0.7C -> C from what L and R share
        double sLR = 0, sLL = 0, sRR = 0;
        for (int n = 0; n < N; n++) {
            sLR += L(n) * R(n);
            sLL += L(n) * L(n);
            sRR += R(n) * R(n);
        }
        double lr = std::fmax(sLR / N, 0.0);
        double pa = std::fmax(sLL / N - lr, 0.0);
        double pc = std::fmax(sRR / N - lr, 0.0);
        double pb = lr * 2.0;
        double total = pa + pb + pc + 1e-12;
        if ((pa + pc) / total < 0.15) {
            // little stereo separation: split by pitch so all three bars still move
            for (int n = 0; n < N; n++) { re_[n] = mono_[n] * win_[n]; im_[n] = 0; }
            fft();
            const double bin_hz = (double)rate_ / N;
            double sb = 0, sc = 0, sa = 0;
            for (int k = 0; k <= N / 2; k++) {
                double f = k * bin_hz, p = re_[k] * re_[k] + im_[k] * im_[k];
                if (f >= 30 && f < 250) sb += p;           // centre slot -> C = bass
                else if (f >= 250 && f < 2000) sc += p;    // right slot  -> B = mids
                else if (f >= 2000 && f < 16000) sa += p;  // A = treble
            }
            sa *= 4;                                       // boosted, it is quieter
            const double nn = (double)N * N;
            pa = sa / nn; pb = sb / nn; pc = sc / nn;
        }
        double dbs[3] = {10 * std::log10(pa + 1e-12), 10 * std::log10(pb + 1e-12), 10 * std::log10(pc + 1e-12)};
        double mean = (dbs[0] + dbs[1] + dbs[2]) / 3;
        abc_peak_ = std::fmax(abc_peak_ + 0.03 * (mean - abc_peak_), -70.0); // ~1 s average level
        for (int i = 0; i < 3; i++) {
            double t = std::pow(clip01((dbs[i] - (abc_peak_ + HEADROOM_DB - RANGE_DB)) / RANGE_DB), CURVE);
            abc_[i] = t > abc_[i] ? t : abc_[i] * FALL;
        }
        // [left, centre, right] = [A, C, B]; rows: top two = A (bands 11-15), middle two = B (5-10), bottom two = C (0-4)
        uint8_t a = to_level(abc_[0]), c = to_level(abc_[1]), b = to_level(abc_[2]);
        for (int i = 0; i < 5; i++) out[i] = c;
        for (int i = 5; i < 11; i++) out[i] = b;
        for (int i = 11; i < 16; i++) out[i] = a;
        return;
    }

    if (style == VU_MUSIC) {
        // Ordinary music: three bars from the energy in treble (top), mids (middle) and bass (bottom), the same
        // ranges the AY style falls back on. Each bar follows its own ~1 s average level, so each uses its full
        // width and moves to its own part of the music (in the other styles one shared level sets all bars).
        for (int n = 0; n < N; n++) { re_[n] = mono_[n] * win_[n]; im_[n] = 0; }
        fft();
        const double bin_hz = (double)rate_ / N;
        double e[3] = {0, 0, 0}; // treble, mids, bass
        for (int k = 0; k <= N / 2; k++) {
            double f = k * bin_hz, p = re_[k] * re_[k] + im_[k] * im_[k];
            if (f >= 30 && f < 250) e[2] += p;
            else if (f >= 250 && f < 2000) e[1] += p;
            else if (f >= 2000 && f < 16000) e[0] += p;
        }
        uint8_t lv[3];
        for (int i = 0; i < 3; i++) {
            double db = 10 * std::log10(e[i] / ((double)N * N) + 1e-12);
            music_ref_[i] = std::fmax(music_ref_[i] + 0.03 * (db - music_ref_[i]), -70.0);
            double t = std::pow(clip01((db - (music_ref_[i] + HEADROOM_DB - RANGE_DB)) / RANGE_DB), CURVE);
            music_[i] = t > music_[i] ? t : music_[i] * FALL;
            lv[i] = to_level(music_[i]);
        }
        for (int i = 0; i < 5; i++) out[i] = lv[2];
        for (int i = 5; i < 11; i++) out[i] = lv[1];
        for (int i = 11; i < 16; i++) out[i] = lv[0];
        return;
    }

    for (int n = 0; n < N; n++) { re_[n] = mono_[n] * win_[n]; im_[n] = 0; }
    fft();
    double db[BANDS], dbmax = -1e300;
    for (int i = 0; i < BANDS; i++) {
        double raw = 0;
        for (int k = band_lo_[i]; k < band_hi_[i]; k++) {
            double m = std::sqrt(re_[k] * re_[k] + im_[k] * im_[k]) / (N / 4);
            if (m > raw) raw = m;
        }
        db[i] = 20 * std::log10(raw + 1e-9);
        if (db[i] > dbmax) dbmax = db[i];
    }
    // auto-gain: follow the loudest band so bars use the full width at any volume
    peak_ = std::fmax(peak_ + 0.03 * (dbmax - peak_), -70.0); // ~1 s average of the loudest band
    for (int i = 0; i < BANDS; i++) {
        double t = std::pow(clip01((db[i] - (peak_ + HEADROOM_DB - RANGE_DB)) / RANGE_DB), CURVE);
        levels_[i] = t > levels_[i] ? t : levels_[i] * FALL;
        out[i] = to_level(levels_[i]);
    }
}
