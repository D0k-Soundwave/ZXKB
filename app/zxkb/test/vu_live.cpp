// Capture what Windows is playing and report how the three VU bars behave, for both styles, without sending
// anything to the keyboard. For checking whether the bars move independently.
//   vu_live <seconds> [file]   (file: also save the audio, 32-bit float interleaved, for vu_tune)
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/vumath.h"
#include <algorithm>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static const CLSID CLSID_Enumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const IID   IID_Enumerator   = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
static const IID   IID_Client       = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
static const IID   IID_Capture      = {0xC8ADBD64, 0xE71E, 0x48A0, {0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17}};

struct Stats { // per style: the three bars (top, middle, bottom) over time
    std::vector<double> bar[3];
    void add(const uint8_t lv[16]) {
        // each bar shows the loudest band in its group, as the keyboard does: top 11-15, middle 5-10, bottom 0-4
        const int first[3] = {11, 5, 0}, last[3] = {15, 10, 4};
        for (int k = 0; k < 3; k++) {
            int m = 0;
            for (int b = first[k]; b <= last[k]; b++) if (lv[b] > m) m = lv[b];
            bar[k].push_back(m);
        }
    }
    static double corr(const std::vector<double> &a, const std::vector<double> &b) {
        double n = a.size(), ma = 0, mb = 0, sab = 0, saa = 0, sbb = 0;
        for (size_t i = 0; i < a.size(); i++) { ma += a[i]; mb += b[i]; }
        ma /= n; mb /= n;
        for (size_t i = 0; i < a.size(); i++) { sab += (a[i] - ma) * (b[i] - mb); saa += (a[i] - ma) * (a[i] - ma); sbb += (b[i] - mb) * (b[i] - mb); }
        return saa > 0 && sbb > 0 ? sab / std::sqrt(saa * sbb) : 0;
    }
    static double mad(const std::vector<double> &a, const std::vector<double> &b) {
        double s = 0;
        for (size_t i = 0; i < a.size(); i++) s += std::fabs(a[i] - b[i]);
        return s / a.size();
    }
    void print(const char *name) const {
        const char *nm[3] = {"top", "middle", "bottom"};
        std::printf("%s (%zu blocks)\n", name, bar[0].size());
        for (int k = 0; k < 3; k++) {
            double m = 0, mx = 0;
            for (double v : bar[k]) { m += v; if (v > mx) mx = v; }
            std::vector<double> s = bar[k];
            std::sort(s.begin(), s.end());
            double p10 = s[s.size() / 10], p90 = s[s.size() * 9 / 10];
            std::printf("  %-6s bar: average %5.1f / 255, peak %3.0f, 10%%-90%% range %3.0f-%3.0f (swing %3.0f)\n", nm[k],
                        m / bar[k].size(), mx, p10, p90, p90 - p10);
        }
        std::printf("  top vs middle: correlation %.2f, average gap %.1f\n", corr(bar[0], bar[1]), mad(bar[0], bar[1]));
        std::printf("  top vs bottom: correlation %.2f, average gap %.1f\n", corr(bar[0], bar[2]), mad(bar[0], bar[2]));
        std::printf("  middle vs bottom: correlation %.2f, average gap %.1f\n", corr(bar[1], bar[2]), mad(bar[1], bar[2]));
    }
};

int main(int argc, char **argv) {
    int secs = argc > 1 ? std::atoi(argv[1]) : 20;
    FILE *save = argc > 2 ? std::fopen(argv[2], "wb") : nullptr;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator *en; IMMDevice *dev; IAudioClient *cl; IAudioCaptureClient *cap; WAVEFORMATEX *fmt;
    if (FAILED(CoCreateInstance(CLSID_Enumerator, nullptr, CLSCTX_ALL, IID_Enumerator, (void **)&en)) ||
        FAILED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) || FAILED(dev->Activate(IID_Client, CLSCTX_ALL, nullptr, (void **)&cl)) ||
        FAILED(cl->GetMixFormat(&fmt)) || FAILED(cl->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 1000000, 0, fmt, nullptr)) ||
        FAILED(cl->GetService(IID_Capture, (void **)&cap)) || FAILED(cl->Start())) {
        std::printf("could not open the audio output for capture\n");
        return 1;
    }
    int ch = fmt->nChannels;
    std::printf("capturing %d Hz, %d channels, %d-bit for %d s\n", (int)fmt->nSamplesPerSec, ch, fmt->wBitsPerSample, secs);
    static VuMath bands, abc, music;
    bands.setup(fmt->nSamplesPerSec);
    abc.setup(fmt->nSamplesPerSec);
    music.setup(fmt->nSamplesPerSec);
    Stats sb, sa, sm;
    std::vector<float> block(VuMath::BLOCK * ch);
    int fill = 0;
    double peak_in = 0;
    ULONGLONG end = GetTickCount64() + secs * 1000ULL;
    while (GetTickCount64() < end) {
        Sleep(10);
        UINT32 n;
        while (SUCCEEDED(cap->GetNextPacketSize(&n)) && n) {
            BYTE *d; UINT32 frames; DWORD flags;
            cap->GetBuffer(&d, &frames, &flags, nullptr, nullptr);
            const float *f = reinterpret_cast<const float *>(d);
            for (UINT32 i = 0; i < frames; i++) {
                for (int c = 0; c < ch; c++) {
                    float v = (flags & AUDCLNT_BUFFERFLAGS_SILENT) ? 0 : f[i * ch + c];
                    block[fill * ch + c] = v;
                    if (std::fabs(v) > peak_in) peak_in = std::fabs(v);
                }
                if (++fill == VuMath::BLOCK) {
                    if (save) std::fwrite(block.data(), sizeof(float), block.size(), save);
                    uint8_t lb[16], la[16], lm[16];
                    bands.process(block.data(), ch, VU_BANDS, lb);
                    abc.process(block.data(), ch, VU_ABC, la);
                    music.process(block.data(), ch, VU_MUSIC, lm);
                    sb.add(lb);
                    sa.add(la);
                    sm.add(lm);
                    fill = 0;
                }
            }
            cap->ReleaseBuffer(frames);
        }
    }
    if (save) std::fclose(save);
    std::printf("loudest sample %.2f (1.00 = full scale)\n", peak_in);
    if (sb.bar[0].empty()) { std::printf("no audio arrived - is anything playing?\n"); return 1; }
    sa.print("AY channels (ACB)");
    sb.print("Frequency bands");
    sm.print("Music (new)");
    return 0;
}
