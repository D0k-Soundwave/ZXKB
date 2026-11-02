// Audio VU meter: WASAPI loopback capture of the default output device, only while the VU meter is on.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "vu.h"
#include "hidlink.h"
#include "protocol.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <vector>
#include <cstring>

namespace {
const CLSID CLSID_Enumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
const IID   IID_Enumerator   = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
const IID   IID_Client       = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
const IID   IID_Capture      = {0xC8ADBD64, 0xE71E, 0x48A0, {0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17}};
const GUID  SUBTYPE_FLOAT    = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};
const GUID  SUBTYPE_PCM      = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

constexpr double   FPS               = 40;       // approved: levels sent to the keyboard at most 40 times a second
constexpr DWORD    POLL_MS           = 10;       // WASAPI's shared-mode period is 10 ms; polling at that rate keeps up
constexpr DWORD    RETRY_MS          = 2000;     // as the old app: wait 2 s after an audio error, then retry
constexpr DWORD    DEVICE_CHECK_MS   = 2000;     // how often to notice a new default output device
constexpr ULONGLONG SILENCE_AFTER_MS = 100;      // Windows sends nothing while nothing plays; after this, feed silence
constexpr REFERENCE_TIME BUFFER_HNS  = 1000000;  // 100 ms capture buffer (10 polls of headroom)

template <class T> void release(T *&p) { if (p) { p->Release(); p = nullptr; } }

double qpc_seconds() {
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / freq.QuadPart;
}
} // namespace

void VuMeter::start() {
    if (thread_) return;
    stop_ev_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    thread_  = CreateThread(nullptr, 256 * 1024, thread_main, this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
}

void VuMeter::stop() {
    if (!thread_) return;
    SetEvent(stop_ev_);
    WaitForSingleObject(thread_, 5000);
    CloseHandle(thread_);
    CloseHandle(stop_ev_);
    thread_ = stop_ev_ = nullptr;
}

DWORD WINAPI VuMeter::thread_main(LPVOID self) {
    static_cast<VuMeter *>(self)->run();
    return 0;
}

void VuMeter::block_ready(const float *frames, int ch) {
    uint8_t pkt[1 + VuMath::BANDS] = {proto::VU_LEVELS};
    math_.process(frames, ch, style_, pkt + 1);
    static double last = 0;
    double now = qpc_seconds();
    if (now - last >= 1.0 / FPS) {
        link_->send(pkt, sizeof(pkt));
        last = now;
    }
}

void VuMeter::run() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    while (WaitForSingleObject(stop_ev_, 0) != WAIT_OBJECT_0) {
        IMMDeviceEnumerator *en = nullptr;
        IMMDevice           *dev = nullptr;
        IAudioClient        *client = nullptr;
        IAudioCaptureClient *cap = nullptr;
        WAVEFORMATEX        *fmt = nullptr;
        LPWSTR               dev_id = nullptr;
        bool ok = SUCCEEDED(CoCreateInstance(CLSID_Enumerator, nullptr, CLSCTX_ALL, IID_Enumerator, (void **)&en)) &&
                  SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) && SUCCEEDED(dev->GetId(&dev_id)) &&
                  SUCCEEDED(dev->Activate(IID_Client, CLSCTX_ALL, nullptr, (void **)&client)) &&
                  SUCCEEDED(client->GetMixFormat(&fmt)) &&
                  SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, BUFFER_HNS, 0, fmt, nullptr)) &&
                  SUCCEEDED(client->GetService(IID_Capture, (void **)&cap)) && SUCCEEDED(client->Start());
        // sample format: 32-bit float (normal for the Windows mixer) or 16/32-bit integer
        bool is_float = false;
        if (ok) {
            if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) is_float = true;
            else if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                is_float = IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE *>(fmt)->SubFormat, SUBTYPE_FLOAT);
            bool is_pcm = fmt->wFormatTag == WAVE_FORMAT_PCM ||
                          (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE *>(fmt)->SubFormat, SUBTYPE_PCM));
            ok = (is_float && fmt->wBitsPerSample == 32) || (is_pcm && (fmt->wBitsPerSample == 16 || fmt->wBitsPerSample == 32));
        }
        if (ok) {
            const int ch = fmt->nChannels, rate = fmt->nSamplesPerSec, bits = fmt->wBitsPerSample;
            math_.setup(rate);
            std::vector<float> block(VuMath::BLOCK * ch);
            int       fill      = 0;
            ULONGLONG last_data = GetTickCount64(), last_fill = last_data, last_check = last_data;
            auto add = [&](const BYTE *data, UINT32 frames, bool silent) {
                for (UINT32 f = 0; f < frames; f++) {
                    for (int c = 0; c < ch; c++) {
                        float v = 0;
                        if (!silent) {
                            const BYTE *s = data + ((size_t)f * ch + c) * (bits / 8);
                            if (is_float) std::memcpy(&v, s, 4);
                            else if (bits == 16) { int16_t i; std::memcpy(&i, s, 2); v = i / 32768.0f; }
                            else { int32_t i; std::memcpy(&i, s, 4); v = (float)(i / 2147483648.0); }
                        }
                        block[fill * ch + c] = v;
                    }
                    if (++fill == VuMath::BLOCK) { block_ready(block.data(), ch); fill = 0; }
                }
            };
            while (WaitForSingleObject(stop_ev_, POLL_MS) == WAIT_TIMEOUT) {
                UINT32 packet = 0;
                bool   got = false, failed = false;
                while (!failed && SUCCEEDED(cap->GetNextPacketSize(&packet)) && packet) {
                    BYTE *data; UINT32 frames; DWORD flags;
                    if (FAILED(cap->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) { failed = true; break; }
                    add(data, frames, flags & AUDCLNT_BUFFERFLAGS_SILENT);
                    cap->ReleaseBuffer(frames);
                    got = true;
                }
                if (failed || FAILED(cap->GetNextPacketSize(&packet))) break; // device gone: reopen
                ULONGLONG now = GetTickCount64();
                if (got) {
                    last_data = last_fill = now;
                } else if (now - last_data > SILENCE_AFTER_MS) {
                    UINT32 frames = (UINT32)((now - last_fill) * rate / 1000);
                    if (frames) { add(nullptr, frames, true); last_fill = now; }
                }
                if (now - last_check > DEVICE_CHECK_MS) { // default output changed (e.g. headphones): follow it
                    last_check = now;
                    IMMDevice *cur = nullptr;
                    LPWSTR     cur_id = nullptr;
                    bool changed = SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &cur)) && SUCCEEDED(cur->GetId(&cur_id)) &&
                                   wcscmp(cur_id, dev_id) != 0;
                    if (cur_id) CoTaskMemFree(cur_id);
                    release(cur);
                    if (changed) break;
                }
            }
            client->Stop();
        }
        if (fmt) CoTaskMemFree(fmt);
        if (dev_id) CoTaskMemFree(dev_id);
        release(cap); release(client); release(dev); release(en);
        if (!ok) WaitForSingleObject(stop_ev_, RETRY_MS);
    }
    CoUninitialize();
}
