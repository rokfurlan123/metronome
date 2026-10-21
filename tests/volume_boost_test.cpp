#include "../src/main.cpp"
#include <iostream>
std::atomic<int> beatsObserved{0};
WNDPROC originalProc=nullptr;
LRESULT CALLBACK observedBoost(HWND h,UINT m,WPARAM w,LPARAM l) {
    if(m==WM_PULSE_BEAT) ++beatsObserved;
    return CallWindowProcW(originalProc,h,m,w,l);
}
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); std::cout<<"PASS "<<message<<std::endl; }
void setField(int id,const wchar_t* value) { SendMessageW(control(id),WM_SETTEXT,0,reinterpret_cast<LPARAM>(value)); }
void dragVolume(bool right) {
    if(GetForegroundWindow()!=window) {
        DWORD foregroundThread=GetWindowThreadProcessId(GetForegroundWindow(),nullptr);
        BOOL attached=foregroundThread && AttachThreadInput(GetCurrentThreadId(),foregroundThread,TRUE);
        SetForegroundWindow(window);
        if(attached) AttachThreadInput(GetCurrentThreadId(),foregroundThread,FALSE);
        Sleep(250);
    }
    HWND slider=control(VolumeSlider);
    if(GetForegroundWindow()!=window) {
        std::cout<<"SKIP physical drag: Windows did not grant foreground focus; checking trackbar notification instead"<<std::endl;
        SendMessageW(slider,TBM_SETPOS,TRUE,right?400:0);
        SendMessageW(window,WM_HSCROLL,MAKEWPARAM(TB_THUMBTRACK,right?400:0),reinterpret_cast<LPARAM>(slider));
        Sleep(650); return;
    }
    std::cout<<"Actual mouse drag: test owns foreground"<<std::endl;
    RECT thumb,channel; SendMessageW(slider,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb)); SendMessageW(slider,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&channel));
    POINT point{(thumb.left+thumb.right)/2,(thumb.top+thumb.bottom)/2}; ClientToScreen(slider,&point); SetCursorPos(point.x,point.y);
    INPUT input{}; input.type=INPUT_MOUSE; input.mi.dwFlags=MOUSEEVENTF_LEFTDOWN; SendInput(1,&input,sizeof(input)); Sleep(100);
    POINT end{right?channel.right:channel.left,(thumb.top+thumb.bottom)/2}; ClientToScreen(slider,&end);
    SetCursorPos(end.x,end.y); Sleep(250); input.mi.dwFlags=MOUSEEVENTF_LEFTUP; SendInput(1,&input,sizeof(input)); Sleep(200);
}
int main() {
    std::thread gui([] { wWinMain(GetModuleHandleW(nullptr),nullptr,nullptr,SW_SHOW); });
    int result=0;
    try {
        for(int i=0;i<100;++i) { if(window&&control(VolumeSlider)) break; Sleep(50); }
        Sleep(500); SendMessageW(control(FullEditor),BM_CLICK,0,0); SetForegroundWindow(window); Sleep(200);
        originalProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(observedBoost)));
        SendMessageW(control(ToolsTab),BM_CLICK,0,0); Sleep(200);
        require(IsWindowVisible(control(VolumeSlider)),"boost slider visible in Practice tools");
        require(SendMessageW(control(VolumeSlider),TBM_GETRANGEMAX,0,0)==400,"slider supports 400 percent");
        setField(Mute,L"100"); setField(Poly,L"0"); // Silent test playback without changing Windows volume.
        SendMessageW(control(Start),BM_CLICK,0,0); Sleep(700);
        require(IsWindowEnabled(control(Volume))&&IsWindowEnabled(control(VolumeSlider)),"both metronome volume controls enabled during playback");
        require(IsWindowEnabled(control(Sound)),"sound selection enabled during playback");
        require(SendMessageW(control(Sound),CB_GETCOUNT,0,0)==4,"full click sound is available");
        SendMessageW(control(Sound),CB_SETCURSEL,3,0);
        SendMessageW(window,WM_COMMAND,MAKEWPARAM(Sound,CBN_SELCHANGE),reinterpret_cast<LPARAM>(control(Sound)));
        require(SendMessageW(control(Sound),CB_GETCURSEL,0,0)==3,"full click selected during playback");
        dragVolume(true);
        require(value(Volume)==L"400","slider reaches 400 percent during playback");
        dragVolume(false);
        require(value(Volume)==L"0","slider reaches zero during playback");
        setField(Volume,L"150"); Sleep(600);
        require(SendMessageW(control(VolumeSlider),TBM_GETPOS,0,0)==150,"typed boost synchronizes slider");
        setField(Volume,L"401"); SendMessageW(window,WM_COMMAND,MAKEWPARAM(Volume,EN_KILLFOCUS),reinterpret_cast<LPARAM>(control(Volume))); Sleep(200);
        require(value(Volume)==L"150","invalid volume restores previous level");
        require(beatsObserved>=4&&value(Start).find(L"Stop")!=std::wstring::npos,"audio-clock beats continue through volume changes");
        SendMessageW(control(Start),BM_CLICK,0,0); Sleep(200);
        require(value(Volume)==L"150","boost retained after stop");
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<std::endl; result=1; }
    PostMessageW(window,WM_CLOSE,0,0); Sleep(300);
    HWND dialog=FindWindowW(L"#32770",L"Unsaved practice"); if(dialog) PostMessageW(dialog,WM_COMMAND,IDYES,0);
    gui.join(); return result;
}
