// VU meter maths, ported from ZX VU (app/legacy/zx_vu.py) with the same tunables D0k approved.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

class VuMath {
public:
    static constexpr int BLOCK = 1024; // frames per analysis block
    static constexpr int BANDS = 16;

    // Call when the sample rate is known (or changes). Keeps the running levels.
    void setup(int rate);

    // One block of BLOCK frames, interleaved, ch channels. abc = AY channels (ACB) style, else frequency bands.
    // Writes the 16 levels the keyboard expects (0-255).
    void process(const float *frames, int ch, bool abc, uint8_t out[BANDS]);

private:
    void fft(); // in place on re_/im_

    int    rate_ = 0;
    int    band_lo_[BANDS] = {}, band_hi_[BANDS] = {}; // FFT bin range per band (hi exclusive)
    double win_[BLOCK] = {};
    double cos_[BLOCK / 2] = {}, sin_[BLOCK / 2] = {};
    double re_[BLOCK] = {}, im_[BLOCK] = {};
    double mono_[BLOCK] = {};

    double peak_     = -40.0; // running reference level, bands style
    double abc_peak_ = -40.0; // running reference level, ACB style
    double levels_[BANDS] = {};
    double abc_[3] = {};
};
