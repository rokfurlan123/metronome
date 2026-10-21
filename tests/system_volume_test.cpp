#include "../src/main.cpp"
#include <iostream>
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); std::cout<<"PASS "<<message<<std::endl; }
int main() {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    float original=0; BOOL originalMute=FALSE;
    if(FAILED(SystemVolume::read(original,originalMute))) return 2;
    std::thread gui([] { wWinMain(GetModuleHandleW(nullptr),nullptr,nullptr,SW_SHOW); });
    int result=0;
    try {
        for(int i=0;i<100;++i) { if(window&&control(SystemVolumeSlider)) break; Sleep(50); }
        Sleep(500); SendMessageW(control(FullEditor),BM_CLICK,0,0); SetForegroundWindow(window); Sleep(200);
        require(IsWindowVisible(control(SystemVolumeSlider))&&IsWindowEnabled(control(SystemVolumeSlider)),"Windows volume slider visible and enabled");
        require(std::abs(SendMessageW(control(SystemVolumeSlider),TBM_GETPOS,0,0)-std::lround(original*100))<=1,"slider starts at existing system volume");
        // Use low levels, preserve mute, and restore the user's level before exiting.
        require(SUCCEEDED(SystemVolume::set(.10f)),"external endpoint volume changed"); Sleep(750);
        require(SendMessageW(control(SystemVolumeSlider),TBM_GETPOS,0,0)==10,"external volume changes synchronize to slider");
        SendMessageW(control(Volume),WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"0"));
        SendMessageW(control(Start),BM_CLICK,0,0); Sleep(500);
        require(value(Start).find(L"Stop")!=std::wstring::npos,"playback active");
        require(IsWindowEnabled(control(SystemVolumeSlider)),"slider remains enabled during playback");
        require(GetForegroundWindow()==window,"test owns foreground before mouse input");
        RECT thumb; SendMessageW(control(SystemVolumeSlider),TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
        POINT point{(thumb.left+thumb.right)/2,(thumb.top+thumb.bottom)/2}; ClientToScreen(control(SystemVolumeSlider),&point); SetCursorPos(point.x,point.y);
        INPUT input{}; input.type=INPUT_MOUSE; input.mi.dwFlags=MOUSEEVENTF_LEFTDOWN; SendInput(1,&input,sizeof(input)); Sleep(100);
        SetCursorPos(point.x+px(25),point.y); Sleep(250);
        float raised=0; BOOL muted=FALSE; SystemVolume::read(raised,muted);
        input.mi.dwFlags=MOUSEEVENTF_LEFTUP; SendInput(1,&input,sizeof(input));
        require(raised>.14f&&raised<.3f,"actual drag raises Windows endpoint volume while playing");
        require(muted==originalMute,"drag preserves Windows mute state");
        SendMessageW(control(ToolsTab),BM_CLICK,0,0); Sleep(200);
        require(IsWindowVisible(control(SystemVolumeSlider)),"slider visible in Practice tools");
        SendMessageW(control(Start),BM_CLICK,0,0); Sleep(200);
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<std::endl; result=1; }
    if(FAILED(SystemVolume::set(original))) { std::cerr<<"FAIL restoring original system volume\n"; result=1; }
    else std::cout<<"Restored original Windows volume\n";
    PostMessageW(window,WM_CLOSE,0,0); Sleep(300);
    HWND dialog=FindWindowW(L"#32770",L"Unsaved practice"); if(dialog) PostMessageW(dialog,WM_COMMAND,IDYES,0);
    gui.join(); CoUninitialize(); return result;
}
