// Audio VU meter: captures what Windows is playing (WASAPI loopback) only while the VU meter is on,
// and sends the levels to the keyboard.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include "vumath.h"

class HidLink;

class VuMeter {
public:
    explicit VuMeter(HidLink *link) : link_(link) {}
    void start(); // starts the capture thread (no-op if running)
    void stop();  // stops it and releases the audio device
    void set_style(int style) { style_ = style; } // a VuStyle
    // Music pulse for standard VIA keyboards: instead of VU levels, the whole keyboard's brightness follows the
    // music, up to `peak`, through VIA lighting channel `channel` (0 = normal VU meter)
    void set_pulse(uint8_t channel, uint8_t peak) { pulse_peak_ = peak; pulse_channel_ = channel; }
    bool pulsing() const { return pulse_channel_ != 0; }
    bool running() const { return thread_ != nullptr; }

private:
    static DWORD WINAPI thread_main(LPVOID self);
    void run();
    void block_ready(const float *frames, int ch);

    HidLink      *link_;
    HANDLE        thread_ = nullptr, stop_ev_ = nullptr;
    volatile int  style_  = VU_BANDS;
    volatile uint8_t pulse_channel_ = 0, pulse_peak_ = 255;
    VuMath        math_;        // kept between runs, as the old app kept its running levels
};
