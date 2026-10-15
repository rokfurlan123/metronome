#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

// Resolve the current default output for each operation, including after device changes.
class SystemVolume {
    template<class T> struct Com {
        T* p=nullptr;
        ~Com() { if(p) p->Release(); }
    };
    template<class F> static HRESULT endpoint(F action) {
        Com<IMMDeviceEnumerator> enumerator;
        HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&enumerator.p));
        if(FAILED(hr)) return hr;
        Com<IMMDevice> device;
        hr=enumerator.p->GetDefaultAudioEndpoint(eRender,eMultimedia,&device.p);
        if(FAILED(hr)) return hr;
        Com<IAudioEndpointVolume> volume;
        hr=device.p->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&volume.p));
        return FAILED(hr)?hr:action(volume.p);
    }
public:
    static HRESULT read(float& level,BOOL& muted) {
        return endpoint([&](IAudioEndpointVolume* v) {
            HRESULT hr=v->GetMasterVolumeLevelScalar(&level);
            return FAILED(hr)?hr:v->GetMute(&muted);
        });
    }
    static HRESULT set(float level) {
        return endpoint([&](IAudioEndpointVolume* v) { return v->SetMasterVolumeLevelScalar(level,nullptr); });
    }
};
