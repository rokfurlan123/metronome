#ifndef PULSE_SOURCE_MAIN
#define PULSE_SOURCE_MAIN "../src/main.cpp"
#endif
#include PULSE_SOURCE_MAIN
#include <audiopolicy.h>
#include <iostream>
#include <iomanip>

// Process-loopback ABI from Microsoft's audioclientactivationparams.h (SDK 20348+).
// The locally installed MinGW headers predate this three-DWORD structure.
struct ProcessLoopbackParams { DWORD activationType=1,targetProcessId=GetCurrentProcessId(),includeProcessTree=0; };
static_assert(sizeof(ProcessLoopbackParams)==12);
template<class T> struct EvenCom { T* p=nullptr; ~EvenCom(){if(p)p->Release();} T* operator->(){return p;} void** address(){return reinterpret_cast<void**>(&p);} };
void checked(HRESULT hr,const char* what) {if(FAILED(hr)){std::cerr<<what<<" HRESULT=0x"<<std::hex<<hr<<std::dec<<std::endl;throw std::runtime_error(what);}}
class ActivateCapture final:public IActivateAudioInterfaceCompletionHandler,public IAgileObject {
    std::atomic<ULONG> refs{1};
public:
    HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);HRESULT result=E_PENDING;IAudioClient* client=nullptr;
    ~ActivateCapture(){if(client)client->Release();CloseHandle(ready);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;
        *out=nullptr;
        if(IsEqualIID(iid,IID_IUnknown)||IsEqualIID(iid,__uuidof(IActivateAudioInterfaceCompletionHandler))) *out=static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
        else if(IsEqualIID(iid,__uuidof(IAgileObject))) *out=static_cast<IAgileObject*>(this);
        else return E_NOINTERFACE;
        AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override {ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override {
        EvenCom<IUnknown> object;HRESULT activation=E_FAIL;
        result=operation->GetActivateResult(&activation,&object.p);
        if(SUCCEEDED(result))result=activation;
        if(SUCCEEDED(result))result=object->QueryInterface(__uuidof(IAudioClient),reinterpret_cast<void**>(&client));
        SetEvent(ready);return S_OK;
    }
};
class ProcessCapture {
    EvenCom<IAudioClient> client;EvenCom<IAudioCaptureClient> input;std::ofstream out;
    UINT64 previous=0;bool havePrevious=false,positionAvailable=false;
public:
    uint64_t frames=0;int discontinuities=0,gaps=0,timestampErrors=0;
    bool hasDevicePosition()const{return positionAvailable;}
    explicit ProcessCapture(const std::string& base) {
        EvenCom<ActivateCapture> callback;callback.p=new ActivateCapture;
        ProcessLoopbackParams params;PROPVARIANT activation{};activation.vt=VT_BLOB;activation.blob.cbSize=sizeof(params);activation.blob.pBlobData=reinterpret_cast<BYTE*>(&params);
        EvenCom<IActivateAudioInterfaceAsyncOperation> operation;
        checked(ActivateAudioInterfaceAsync(L"VAD\\Process_Loopback",__uuidof(IAudioClient),&activation,callback.p,&operation.p),"activate app-only loopback");
        if(WaitForSingleObject(callback->ready,15000)!=WAIT_OBJECT_0)throw std::runtime_error("App-only loopback activation timed out");
        checked(callback->result,"app-only activation result");client.p=callback->client;client.p->AddRef();
        WAVEFORMATEX format{WAVE_FORMAT_IEEE_FLOAT,2,48000,48000*8,8,32,0};
        checked(client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,1000000,0,&format,nullptr),"initialize app-only loopback");
        checked(client->GetService(__uuidof(IAudioCaptureClient),input.address()),"capture service");
        out.open(base+".f32",std::ios::binary);if(!out)throw std::runtime_error("Cannot write app-only capture");
        std::ofstream meta(base+".format.txt");meta<<"48000 2\n";checked(client->Start(),"start capture");
    }
    ~ProcessCapture(){client->Stop();}
    void poll() {
        UINT32 pending=0;checked(input->GetNextPacketSize(&pending),"packet size");
        while(pending) {
            BYTE* data=nullptr;UINT32 count=0;DWORD flags=0;UINT64 pos=0,qpc=0;
            checked(input->GetBuffer(&data,&count,&flags,&pos,&qpc),"capture buffer");
            if(havePrevious){
                if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)++discontinuities;
                // Process loopback may return zero for both optional clock fields.
                // Do not misreport unsupported device positions as missing audio.
                if((positionAvailable||pos>0)&&pos!=previous)++gaps;
            }
            if(pos>0)positionAvailable=true;
            if(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)++timestampErrors;
            previous=pos+count;havePrevious=true;
            if(flags&AUDCLNT_BUFFERFLAGS_SILENT){std::vector<float> zeros(count*2);out.write(reinterpret_cast<char*>(zeros.data()),count*8);}
            else out.write(reinterpret_cast<char*>(data),count*8);
            frames+=count;checked(input->ReleaseBuffer(count),"release capture");checked(input->GetNextPacketSize(&pending),"next packet");
        }
    }
    void collect(int ms){auto end=GetTickCount64()+ms;while(GetTickCount64()<end){poll();Sleep(2);}poll();}
};
void fieldEven(int id,int number){auto text=std::to_wstring(number);SendMessageW(control(id),WM_SETTEXT,0,reinterpret_cast<LPARAM>(text.c_str()));}
void clickEven(int id){SendMessageW(control(id),BM_CLICK,0,0);}
void choiceEven(int id,int choice){SendMessageW(control(id),CB_SETCURSEL,choice,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),reinterpret_cast<LPARAM>(control(id)));}
void accentsEven(bool wanted){bool current=SendMessageW(control(Accents),BM_GETCHECK,0,0)==BST_CHECKED;if(current!=wanted)clickEven(Accents);}
int main(int argc,char** argv) {
    const std::string folder=argc>1?argv[1]:"build/native-evenness";
    std::filesystem::create_directories(folder);checked(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM initialization");
    std::atomic<DWORD> guiId{0};HWND app=nullptr;
    std::thread gui([&]{guiId=GetCurrentThreadId();wWinMain(GetModuleHandleW(nullptr),nullptr,nullptr,SW_SHOW);});int result=0;
    try {
        for(int i=0;i<100&&!app;++i){if(guiId)EnumThreadWindows(guiId,[](HWND h,LPARAM param)->BOOL{wchar_t cls[64]{};GetClassNameW(h,cls,64);if(wcscmp(cls,L"PulseMetronome")==0){*reinterpret_cast<HWND*>(param)=h;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&app));Sleep(50);}
        if(!app)throw std::runtime_error("Test GUI did not launch");
        Sleep(300);
        clickEven(CycleView);clickEven(CycleView);clickEven(ToolsTab);
        EvenCom<IMMDeviceEnumerator> enumerator;EvenCom<IMMDevice> device;
        checked(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),enumerator.address()),"enumerator");checked(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device.p),"output device");
        EvenCom<IPropertyStore> properties;checked(device->OpenPropertyStore(STGM_READ,&properties.p),"device properties");PROPVARIANT property{};
        const PROPERTYKEY effectsKey{{0x1da5d803,0xd492,0x4edd,{0x8c,0x23,0xe0,0xc0,0xff,0xee,0x7f,0x0e}},5};
        if(SUCCEEDED(properties->GetValue(effectsKey,&property))) {std::cout<<"INFO endpoint Disable_SysFx vt="<<property.vt;if(property.vt==VT_UI4)std::cout<<" value="<<property.ulVal;std::cout<<std::endl;PropVariantClear(&property);}
        EvenCom<IAudioSessionManager2> manager;EvenCom<ISimpleAudioVolume> session;
        checked(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,manager.address()),"test session manager");checked(manager->GetSimpleAudioVolume(nullptr,0,&session.p),"test session");checked(session->SetMasterVolume(.15f,nullptr),"lower only test-session volume");
        std::ofstream metrics(folder+"/capture.csv");metrics<<"case,bpm,beats,unit,subdivision,poly,sound,volume,accents,frames,discontinuities,position_gaps,timestamp_errors,device_position_available\n";
        auto run=[&](const std::string& name,int bpm,int beats,int unit,int sub,int poly,int sound,int volume,bool accents,int ms) {
            fieldEven(Bpm,bpm);fieldEven(Volume,volume);SendMessageW(window,WM_COMMAND,MAKEWPARAM(Volume,EN_KILLFOCUS),reinterpret_cast<LPARAM>(control(Volume)));
            fieldEven(Beats,beats);fieldEven(Groups,beats);choiceEven(Unit,unit==2?0:unit==4?1:unit==8?2:3);choiceEven(Subdivision,sub-1);fieldEven(Poly,poly);fieldEven(Mute,0);choiceEven(Sound,sound);accentsEven(accents);
            ProcessCapture capture(folder+"/"+name);capture.collect(200);clickEven(Start);capture.collect(ms);clickEven(Start);capture.collect(300);
            metrics<<name<<','<<bpm<<','<<beats<<','<<unit<<','<<sub<<','<<poly<<','<<sound<<','<<volume<<','<<accents<<','<<capture.frames<<','<<capture.discontinuities<<','<<capture.gaps<<','<<capture.timestampErrors<<','<<capture.hasDevicePosition()<<'\n';metrics.flush();
            std::cout<<"CAPTURE "<<name<<" frames="<<capture.frames<<" discontinuities="<<capture.discontinuities<<" gaps="<<capture.gaps<<" timestamp_errors="<<capture.timestampErrors<<std::endl;
            if(capture.discontinuities||capture.gaps||capture.timestampErrors)result=3;
        };
        if(argc>2&&std::string(argv[2])=="--live-only") {
            fieldEven(Bpm,137);fieldEven(Beats,7);fieldEven(Groups,7);choiceEven(Unit,3);choiceEven(Subdivision,3);fieldEven(Poly,5);fieldEven(Mute,0);choiceEven(Sound,3);fieldEven(Volume,200);accentsEven(false);
            ProcessCapture capture(folder+"/live");capture.collect(200);clickEven(Start);
            std::ofstream segments(folder+"/segments.csv");segments<<"case,start_frame,end_frame,bpm,beats,unit,subdivision,poly,sound,volume,accents\n";
            auto phase=[&](const char* name,int bpm,int sound,int gain,bool accents) {
                fieldEven(Bpm,bpm);SendMessageW(window,WM_COMMAND,MAKEWPARAM(Bpm,EN_KILLFOCUS),reinterpret_cast<LPARAM>(control(Bpm)));
                choiceEven(Sound,sound);fieldEven(Volume,gain);SendMessageW(window,WM_COMMAND,MAKEWPARAM(Volume,EN_KILLFOCUS),reinterpret_cast<LPARAM>(control(Volume)));accentsEven(accents);
                capture.collect(2000);auto start=capture.frames;capture.collect(3000);
                segments<<name<<','<<start<<','<<capture.frames<<','<<bpm<<",7,16,4,5,"<<sound<<','<<gain<<','<<accents<<'\n';segments.flush();
                std::cout<<"LIVE "<<name<<std::endl;
            };
            phase("live137",137,3,200,false);phase("live199",199,3,200,false);phase("live300",300,3,200,false);phase("live137_return",137,3,200,false);
            phase("live_gain400",137,3,400,false);phase("live_sound2",137,2,400,false);phase("positive_control_live",137,2,400,true);phase("live_accents_off",137,2,400,false);phase("live_restore200",137,2,200,false);
            clickEven(Start);capture.collect(300);
            std::ofstream liveStatus(folder+"/live-status.csv");liveStatus<<"discontinuities,position_gaps,timestamp_errors,device_position_available\n"<<capture.discontinuities<<','<<capture.gaps<<','<<capture.timestampErrors<<','<<capture.hasDevicePosition()<<'\n';
            std::cout<<"LIVE capture discontinuities="<<capture.discontinuities<<" gaps="<<capture.gaps<<" timestamp_errors="<<capture.timestampErrors<<std::endl;
            if(capture.discontinuities||capture.gaps||capture.timestampErrors)result=3;
        } else {
        run("plain120_200",120,4,4,1,0,0,200,false,12000);
        run("plain160_200",160,4,4,1,0,0,200,false,12000);
        for(int sound=0;sound<4;++sound)for(int gain:{200,400})run("sound"+std::to_string(sound)+"_"+std::to_string(gain),160,4,4,1,0,sound,gain,false,6000);
        for(int poly:{0,3,5,16})run("dense_poly"+std::to_string(poly),199,7,16,4,poly,3,200,false,6000);
        run("positive_control",120,4,4,1,0,0,200,true,6000);
        // Explicit UI live-toggle path, followed by a stable flat segment.
        accentsEven(false);run("after_toggle",160,4,4,1,0,0,200,false,6000);
        run("long160",160,4,4,1,0,0,200,false,60000);
        }
        std::cout<<"INFO capture includes only this audit process; no microphone/system-wide capture or Windows master-volume changes"<<std::endl;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;result=1;}
    if(app)PostMessageW(app,WM_CLOSE,0,0);else if(guiId)PostThreadMessageW(guiId,WM_QUIT,0,0);
    gui.join();CoUninitialize();return result;
}
