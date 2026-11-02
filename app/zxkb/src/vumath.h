// VU meter maths, ported from ZX VU (app/legacy/zx_vu.py) with the same tunables D0k approved.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

// VU styles. Stored in settings as "bands", "abc", "music".
enum VuStyle { VU_BANDS = 0, VU_ABC = 1, VU_MUSIC = 2 };

class VuMath {
public:
    static constexpr int BLOCK = 1024; // frames per analysis block
    static constexpr int BANDS = 16;

    // Call when the sample rate is known (or changes). Keeps the running levels.
    void setup(int rate);

    // One block of BLOCK frames, interleaved, ch channels, in one of the VuStyle styles.
    // Writes the 16 levels the keyboard expects (0-255): bands 0-4 bottom bar, 5-10 middle, 11-15 top.
    void process(const float *frames, int ch, int style, uint8_t out[BANDS]);

    // Music style settings (the other styles keep ZX VU's approved values). Chosen 2026-11-02 with test/vu_tune on
    // 20 s of D0k's music at 96 kHz: mean 10-90% swing 118 of 255, against 84 with ZX VU's values and 86 for the
    // frequency-bands style. Not yet judged by eye.
    struct MusicTune {
        double range_db = 18, headroom_db = 9, curve = 1.7;
        double ref_seconds  = 4;    // how fast each bar's reference follows the music; 0 = per block as ZX VU (0.03)
        double fall_seconds = 0.12; // time for a bar to fall to half; 0 = per block as ZX VU (x0.72)
    } music;

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
    double music_ref_[3] = {-40.0, -40.0, -40.0}; // music style: each bar's own running reference level
    double music_[3] = {};
};
