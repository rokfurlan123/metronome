#include "../src/main.cpp"
#include <iostream>
#include <mutex>
#include <deque>
std::mutex logMutex;
std::deque<double> ticks;
WNDPROC baseProc=nullptr;
LRESULT CALLBACK observed(HWND h,UINT m,WPARAM w,LPARAM l) {
    if(m==WM_PULSE_BEAT) {
        double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        std::lock_guard<std::mutex> lock(logMutex); ticks.push_back(now); if(ticks.size()>30) ticks.pop_front();
    }
    return CallWindowProcW(baseProc,h,m,w,l);
}
void tickCheck(double expected,const char* name) {
    std::lock_guard<std::mutex> lock(logMutex);
    if(ticks.size()<4) throw std::runtime_error("Too few audio-clock beat notifications");
    double average=(ticks.back()-ticks[ticks.size()-4])/3;
    std::cout<<name<<": measured "<<average<<" s, expected "<<expected<<" s"<<std::endl;
    if(std::abs(average-expected)>.08) throw std::runtime_error(name);
}
void key(WORD v,bool up=false) { INPUT i{}; i.type=INPUT_KEYBOARD; i.ki.wVk=v; i.ki.dwFlags=up?KEYEVENTF_KEYUP:0; SendInput(1,&i,sizeof(i)); }
void clickAt(HWND h) {
    if(GetForegroundWindow()!=window) throw std::runtime_error("Test window lost foreground; input stopped");
    RECT r; GetWindowRect(h,&r); SetCursorPos((r.left+r.right)/2,(r.top+r.bottom)/2);
    INPUT inputs[2]{}; for(auto& i:inputs)i.type=INPUT_MOUSE;
    inputs[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; inputs[1].mi.dwFlags=MOUSEEVENTF_LEFTUP; SendInput(2,inputs,sizeof(INPUT)); Sleep(150);
}
void typeTempo(const wchar_t* value,bool selectAll=true) {
    clickAt(control(Bpm));
    if(selectAll) { key(VK_CONTROL);key('A');key('A',true);key(VK_CONTROL,true); }
    for(auto p=value;*p;++p) { INPUT inputs[2]{};for(auto& i:inputs){i.type=INPUT_KEYBOARD;i.ki.wScan=*p;i.ki.dwFlags=KEYEVENTF_UNICODE;}inputs[1].ki.dwFlags|=KEYEVENTF_KEYUP;SendInput(2,inputs,sizeof(INPUT)); }
    Sleep(200);
    wchar_t actual[64]{}; SendMessageW(control(Bpm),WM_GETTEXT,64,reinterpret_cast<LPARAM>(actual));
    std::wcout<<L"Requested tempo "<<value<<L"; edit contains '"<<actual<<L"'"<<std::endl;
}
int tempoValue() { wchar_t value[32]{}; SendMessageW(control(Bpm),WM_GETTEXT,32,reinterpret_cast<LPARAM>(value)); return _wtoi(value); }
void holdTempo(int id,bool dragOutside=false) {
    if(GetForegroundWindow()!=window) throw std::runtime_error("Test window lost foreground");
    RECT r; GetWindowRect(control(id),&r); SetCursorPos((r.left+r.right)/2,(r.top+r.bottom)/2);
    INPUT input{}; input.type=INPUT_MOUSE; input.mi.dwFlags=MOUSEEVENTF_LEFTDOWN; SendInput(1,&input,sizeof(input));
    Sleep(100); int first=tempoValue(); Sleep(1100); int slow=tempoValue(); Sleep(1800); int fast=tempoValue();
    input.mi.dwFlags=MOUSEEVENTF_LEFTUP;
    if(dragOutside) { SetCursorPos(r.right+25,r.bottom+25); Sleep(100); }
    SendInput(1,&input,sizeof(input)); Sleep(600);
    int direction=id==Plus?1:-1;
    if((slow-first)*direction<5 || (fast-slow)*direction<35) throw std::runtime_error("Held tempo did not accelerate");
    if(tempoValue()!=fast) throw std::runtime_error("Tempo continued after release/drag");
    std::cout<<"Hold "<<id<<": "<<first<<" -> "<<slow<<" -> "<<fast<<"; release stable"<<std::endl;
}
int main() {
    std::atomic<bool> ready=false;
    std::thread gui([&]{ready=true;wWinMain(GetModuleHandleW(nullptr),nullptr,nullptr,SW_SHOW);});
    while(!ready||!window||!control(Start)) Sleep(50);
    Sleep(400);
    baseProc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(observed)));
    try {
        SetForegroundWindow(window); Sleep(200);
        SendMessageW(control(Volume),WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"0"));
        clickAt(control(Start)); Sleep(2300); tickCheck(.5,"120 BPM baseline");
        typeTempo(L"240",false); Sleep(2000); tickCheck(.25,"Typing 240 BPM while playing");
        typeTempo(L"60"); Sleep(4500); tickCheck(1.,"Typing 60 BPM while playing");
        clickAt(control(Plus)); Sleep(4400); tickCheck(60./61,"Plus button while playing");
        typeTempo(L"120"); Sleep(400);
        holdTempo(Plus); holdTempo(Minus,true);
        clickAt(control(ToolsTab));
        if(!IsWindowEnabled(control(Accents))) throw std::runtime_error("Accents disabled during playback");
        clickAt(control(Accents));
        if(SendMessageW(control(Accents),BM_GETCHECK,0,0)!=BST_UNCHECKED) throw std::runtime_error("Accent checkbox failed");
        clickAt(control(Accents));
        typeTempo(L"299"); Sleep(400); clickAt(control(Plus)); clickAt(control(Plus));
        if(tempoValue()!=300) throw std::runtime_error("Upper tempo limit failed");
        typeTempo(L"20"); Sleep(400); clickAt(control(Minus));
        if(tempoValue()!=20) throw std::runtime_error("Lower tempo limit failed");
        clickAt(control(Start));
        typeTempo(L"120"); Sleep(400); holdTempo(Plus);
        std::cout<<"PASS actual mouse/keyboard input changes audio-clock tempo without restart"<<std::endl;
        PostMessageW(window,WM_CLOSE,0,0); Sleep(200); HWND dialog=FindWindowW(L"#32770",L"Unsaved practice"); if(dialog) PostMessageW(dialog,WM_COMMAND,IDYES,0);
        gui.join();
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<std::endl; ExitProcess(1); }
}
