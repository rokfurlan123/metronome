#include "../src/main.cpp"
#include <audiopolicy.h>
#include <iostream>
#include <iomanip>

template<class T> struct AuditCom {
    T* p=nullptr;
    ~AuditCom(){if(p)p->Release();}
    T* operator->(){return p;}
    void** address(){return reinterpret_cast<void**>(&p);}
};
void checkAudit(HRESULT hr,const char* what) { if(FAILED(hr)) { std::cerr<<what<<": 0x"<<std::hex<<hr<<std::dec<<std::endl; throw std::runtime_error(what); } }
class Capture {
    AuditCom<IAudioClient> client;
    AuditCom<IAudioCaptureClient> input;
    std::ofstream output;
    int channels=0, bits=0; bool floating=false;
    uint64_t lastEnd=0; bool haveLast=false;
public:
    int rate=0, discontinuities=0, positionGaps=0; uint64_t frames=0;
    explicit Capture(IMMDevice* device,const std::string& base) {
        checkAudit(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,client.address()),"activate loopback");
        WAVEFORMATEX* format=nullptr; checkAudit(client->GetMixFormat(&format),"mix format");
        rate=format->nSamplesPerSec; channels=format->nChannels; bits=format->wBitsPerSample;
        floating=format->wFormatTag==WAVE_FORMAT_IEEE_FLOAT;
        const GUID floatFormat{3,0,0x0010,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
        if(format->wFormatTag==WAVE_FORMAT_EXTENSIBLE) floating=IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat,floatFormat);
        if(!(floating&&bits==32)) { CoTaskMemFree(format); throw std::runtime_error("Audit requires float32 endpoint mix format"); }
        HRESULT hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,1000000,0,format,nullptr);
        CoTaskMemFree(format); checkAudit(hr,"initialize loopback");
        checkAudit(client->GetService(__uuidof(IAudioCaptureClient),input.address()),"capture service");
        output.open(base+".f32",std::ios::binary);
        if(!output) throw std::runtime_error("Cannot create capture file");
        std::ofstream meta(base+".format.txt"); meta<<rate<<' '<<channels<<'\n';
        checkAudit(client->Start(),"start capture");
    }
    ~Capture(){client->Stop();}
    void poll() {
        UINT32 pending=0; checkAudit(input->GetNextPacketSize(&pending),"next packet");
        while(pending) {
            BYTE* data=nullptr; UINT32 count=0; DWORD flags=0; UINT64 position=0,qpc=0;
            checkAudit(input->GetBuffer(&data,&count,&flags,&position,&qpc),"capture buffer");
            if(haveLast) { if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) ++discontinuities; if(position!=lastEnd) ++positionGaps; }
            lastEnd=position+count; haveLast=true;
            if(flags&AUDCLNT_BUFFERFLAGS_SILENT) { std::vector<float> zero(count*channels); output.write(reinterpret_cast<char*>(zero.data()),zero.size()*sizeof(float)); }
            else output.write(reinterpret_cast<char*>(data),count*channels*sizeof(float));
            frames+=count; input->ReleaseBuffer(count); checkAudit(input->GetNextPacketSize(&pending),"next packet");
        }
    }
    void collect(int milliseconds) {
        auto end=GetTickCount64()+milliseconds;
        while(GetTickCount64()<end) {poll();Sleep(3);} poll();
    }
};
void field(int id,const wchar_t* text) { SendMessageW(control(id),WM_SETTEXT,0,reinterpret_cast<LPARAM>(text)); }
void clickAudit(int id) { SendMessageW(control(id),BM_CLICK,0,0); }
void chooseAudit(int id,int index) {
    SendMessageW(control(id),CB_SETCURSEL,index,0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),reinterpret_cast<LPARAM>(control(id)));
}
int main(int argc,char** argv) {
    std::string base=argc>1?argv[1]:"build/windows-audio-audit";
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    std::thread gui([]{wWinMain(GetModuleHandleW(nullptr),nullptr,nullptr,SW_SHOW);});
    int result=0;
    try {
        for(int i=0;i<100;++i) { if(window&&control(Accents))break;Sleep(50); } Sleep(300); SendMessageW(control(FullEditor),BM_CLICK,0,0);
        AuditCom<IMMDeviceEnumerator> enumerator; AuditCom<IMMDevice> device;
        checkAudit(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),enumerator.address()),"enumerator");
        checkAudit(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device.p),"endpoint");
        // Lower only this test process's audio session; never change Windows master volume.
        AuditCom<IAudioSessionManager2> manager; AuditCom<ISimpleAudioVolume> session;
        checkAudit(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,manager.address()),"session manager");
        checkAudit(manager->GetSimpleAudioVolume(nullptr,0,&session.p),"test session volume");
        checkAudit(session->SetMasterVolume(.15f,nullptr),"lower test session volume");
        Capture capture(device.p,base); std::ofstream markers(base+".markers.csv");
        markers<<"case,start_frame,end_frame,accents,sound,volume,bpm,poly,subdivision\n";
        field(Bpm,L"160"); field(Volume,L"200"); field(Mute,L"0"); field(Poly,L"0");
        chooseAudit(Subdivision,0); clickAudit(ToolsTab); clickAudit(Start);
        capture.collect(600);
        auto record=[&](const std::string& name,int ms,int accents,int sound,int volume,int poly=0,int sub=1) {
            auto start=capture.frames; capture.collect(ms);
            markers<<name<<','<<start<<','<<capture.frames<<','<<accents<<','<<sound<<','<<volume<<",160,"<<poly<<','<<sub<<'\n';
            std::cout<<"Captured "<<name<<": "<<capture.frames-start<<" frames\n"<<std::flush;
        };
        record("accent_on_control",5000,1,0,200);
        clickAudit(Accents); // Actual UI command updates the live audio worker.
        capture.collect(700);
        record("live_off_clean_200",6000,0,0,200);
        for(int sound=0;sound<4;++sound) {
            chooseAudit(Sound,sound); field(Volume,L"400");
            SendMessageW(window,WM_COMMAND,MAKEWPARAM(Volume,EN_KILLFOCUS),reinterpret_cast<LPARAM>(control(Volume)));
            capture.collect(700);
            record("off_sound_"+std::to_string(sound)+"_400",5000,0,sound,400);
        }
        clickAudit(Start); field(Poly,L"3"); clickAudit(Start); capture.collect(700);
        record("off_full_poly3",5000,0,3,400,3);
        clickAudit(Start); field(Poly,L"0"); chooseAudit(Subdivision,3); clickAudit(Start); capture.collect(700);
        record("off_full_sub4",5000,0,3,400,0,4);
        clickAudit(Start); chooseAudit(Subdivision,0); clickAudit(Accents); clickAudit(Start); capture.collect(700);
        record("accents_restored_full_400",5000,1,3,400);
        clickAudit(Start); capture.collect(200);
        std::cout<<"Capture complete: rate="<<capture.rate<<", discontinuities="<<capture.discontinuities<<", position_gaps="<<capture.positionGaps<<"\n";
        if(capture.discontinuities||capture.positionGaps) result=3;
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; result=1; }
    PostMessageW(window,WM_CLOSE,0,0); gui.join(); CoUninitialize(); return result;
}
