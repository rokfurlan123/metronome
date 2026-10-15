#pragma once
#include "rhythm.h"
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <avrt.h>
#include <atomic>
#include <thread>
#include <deque>

constexpr UINT WM_PULSE_BEAT = WM_APP + 1;
constexpr UINT WM_PULSE_ERROR = WM_APP + 2;
class Audio {
    std::thread worker;
    HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::atomic<bool> active{false};
    std::atomic<bool> advance{false};
    std::atomic<bool> requestedAccents{true};
    std::atomic<int> requestedBpm{120};
    std::atomic<int> requestedVolume{70};
    std::atomic<int> requestedSound{0};
    template<class T> struct Com {
        T* p = nullptr;
        ~Com() { if (p) p->Release(); }
        T* operator->() { return p; }
        void** address() { return reinterpret_cast<void**>(&p); }
    };
    static void check(HRESULT hr) { if (FAILED(hr)) throw hr; }
    void render(HWND window, pulse::Settings settings) {
        HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        DWORD taskIndex = 0;
        HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
        HANDLE ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HRESULT failure = S_OK;
        if (FAILED(initialized)) failure = initialized;
        else {
            Com<IMMDeviceEnumerator> enumerator; Com<IMMDevice> device;
            Com<IAudioClient> client; Com<IAudioRenderClient> output; Com<IAudioClock> clock;
            try {
                if (!ready || !stopEvent) throw HRESULT(E_OUTOFMEMORY);
                check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), enumerator.address()));
                check(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device.p));
                check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, client.address()));
                WAVEFORMATEX format{WAVE_FORMAT_IEEE_FLOAT, 2, 48000, 48000 * 8, 8, 32, 0};
                check(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 0, 0, &format, nullptr));
                check(client->SetEventHandle(ready));
                check(client->GetService(__uuidof(IAudioRenderClient), output.address()));
                check(client->GetService(__uuidof(IAudioClock), clock.address()));
                UINT64 frequency; check(clock->GetFrequency(&frequency));
                UINT32 capacity; check(client->GetBufferSize(&capacity));
                pulse::Timeline timeline(settings); pulse::Synth synth;
                auto events = timeline.nextBar(); size_t index = 0; int64_t frame = 0;
                std::deque<pulse::Event> visual;
                auto fill = [&](UINT32 frames) {
                    if (!frames) return;
                    BYTE* bytes; check(output->GetBuffer(frames, &bytes));
                    auto samples = reinterpret_cast<float*>(bytes);
                    for (UINT32 i = 0; i < frames; ++i, ++frame) {
                        if (index == events.size() && frame >= std::llround(timeline.nextBarFrame())) { if (advance.exchange(false)) timeline.advanceStep(); events = timeline.nextBar(); index = 0; }
                        while (index < events.size() && events[index].frame <= frame) {
                            if (!events[index].poly && events[index].accent > 0) {
                                int bpm = requestedBpm.load(std::memory_order_relaxed);
                                if (bpm != timeline.config().bpm) timeline.changeTempo(bpm, events, index);
                            }
                            settings.sound = requestedSound.load(std::memory_order_relaxed);
                            settings.volume = requestedVolume.load(std::memory_order_relaxed);
                            settings.accents = requestedAccents.load(std::memory_order_relaxed);
                            int untilNext=pulse::framesUntilNextClick(events,index,static_cast<int64_t>(std::llround(timeline.nextBarFrame())));
                            const auto& e = events[index++]; synth.trigger(e, settings,untilNext);
                            if (!e.poly && e.accent > 0) visual.push_back(e);
                        }
                        float value = synth.sample(); samples[i * 2] = value; samples[i * 2 + 1] = value;
                    }
                    check(output->ReleaseBuffer(frames, 0));
                };
                fill(capacity); check(client->Start());
                HANDLE handles[]{stopEvent, ready};
                while (true) {
                    DWORD wait = WaitForMultipleObjects(2, handles, FALSE, 2000);
                    if (wait == WAIT_OBJECT_0) break;
                    if (wait != WAIT_OBJECT_0 + 1) throw HRESULT(AUDCLNT_E_DEVICE_INVALIDATED);
                    UINT64 position; check(clock->GetPosition(&position, nullptr));
                    auto played = static_cast<int64_t>(static_cast<long double>(position) * 48000 / frequency);
                    while (!visual.empty() && visual.front().frame <= played) {
                        const auto e = visual.front(); visual.pop_front();
                        PostMessageW(window, WM_PULSE_BEAT, MAKEWPARAM(e.beat, e.step), MAKELPARAM(e.repeat, e.silent));
                    }
                    UINT32 padding; check(client->GetCurrentPadding(&padding)); fill(capacity - padding);
                }
                client->Stop();
            } catch (HRESULT hr) { failure = hr; if (client.p) client->Stop(); }
              catch (...) { failure = E_FAIL; if (client.p) client->Stop(); }
        }
        if (ready) CloseHandle(ready);
        if (task) AvRevertMmThreadCharacteristics(task);
        if (SUCCEEDED(initialized)) CoUninitialize();
        active = false;
        if (FAILED(failure)) PostMessageW(window, WM_PULSE_ERROR, static_cast<WPARAM>(failure), 0);
    }
public:
    ~Audio() { stop(); if (stopEvent) CloseHandle(stopEvent); }
    bool running() const { return active; }
    void nextStep() { if (active) advance = true; }
    void setSound(int sound) { requestedSound.store(std::clamp(sound,0,3),std::memory_order_relaxed); }
    void setVolume(int volume) { requestedVolume.store(std::clamp(volume,0,400),std::memory_order_relaxed); }
    void setAccents(bool enabled) { requestedAccents.store(enabled,std::memory_order_relaxed); }
    void setTempo(int bpm) { if (bpm >= 20 && bpm <= 300) requestedBpm.store(bpm,std::memory_order_relaxed); }
    void start(HWND window, const pulse::Settings& settings) {
        stop(); ResetEvent(stopEvent); advance = false; requestedBpm = settings.bpm; requestedAccents = settings.accents; requestedVolume = settings.volume; requestedSound = settings.sound; active = true;
        try { worker = std::thread([=, this] { render(window, settings); }); }
        catch (...) { active = false; throw; }
    }
    void stop() { if (stopEvent) SetEvent(stopEvent); if (worker.joinable()) worker.join(); active = false; }
};
