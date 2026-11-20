// Native smoke test on a private, never-activated Windows desktop. No system
// input, master-volume writes, or interaction with an existing Pulse process.
#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
struct Search { DWORD pid; HWND window; };
BOOL CALLBACK locate(HWND w, LPARAM param) {
    auto& search=*reinterpret_cast<Search*>(param);
    wchar_t cls[64]{}; DWORD pid=0; GetWindowThreadProcessId(w,&pid);GetClassNameW(w,cls,64);
    if(pid==search.pid && std::wstring(cls)==L"PulseMetronome") {search.window=w;return FALSE;}
    return TRUE;
}
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) return 2;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    std::wstring name=L"PulseSmoke-"+std::to_wstring(GetCurrentProcessId());
    HDESK desktop=CreateDesktopW(name.c_str(),nullptr,nullptr,0,GENERIC_ALL,nullptr);
    if(!desktop){std::cerr<<"Private desktop unavailable: "<<GetLastError()<<'\n';return 2;}
    STARTUPINFOW si{};si.cb=sizeof(si);si.lpDesktop=name.data();
    PROCESS_INFORMATION proc{}; Search search{};int result=0;
    try {
        check(CreateProcessW(argv[1],nullptr,nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&proc),"launch failed");
        search.pid=proc.dwProcessId;
        for(int i=0;i<100 && !search.window;i++) {Sleep(100);EnumDesktopWindows(desktop,locate,reinterpret_cast<LPARAM>(&search));}
        check(search.window!=nullptr,"no Pulse test window");
        WaitForInputIdle(proc.hProcess,5000);Sleep(250);
        HWND w=search.window;UINT dpi=GetDpiForWindow(w);RECT client{};GetClientRect(w,&client);
        std::cout<<"Native tiny: "<<client.right<<" x "<<client.bottom<<" at "<<dpi<<" DPI\n";
        check(client.right==MulDiv(410,dpi,96)&&client.bottom==MulDiv(54,dpi,96),"tiny size incorrect");
        LONG last=0;
        for(int id:{100,102,110,139,135,138,137,136}) {
            HWND c=GetDlgItem(w,id);check(c&&IsWindowVisible(c),"missing tiny control");RECT r{};GetWindowRect(c,&r);
            MapWindowPoints(nullptr,w,reinterpret_cast<POINT*>(&r),2);
            check(r.left>=last&&r.top>=0&&r.right<=client.right&&r.bottom<=client.bottom,"control overlaps or clips");last=r.right;
        }
        check(!GetDlgItem(w,106)&&!GetDlgItem(w,129),"legacy accent controls exist");
        for(int id:{102,110}) {
            HWND c=GetDlgItem(w,id);wchar_t field[32]{};
            SendMessageW(c,WM_GETTEXT,32,reinterpret_cast<LPARAM>(field));
            check(std::wstring(field)==(id==102?L"120":L"200"),"incorrect startup value");
            check(SendMessageW(c,CB_GETCOUNT,0,0)>5,"native presets missing");
            SendMessageW(c,CB_SHOWDROPDOWN,TRUE,0);check(SendMessageW(c,CB_GETDROPPEDSTATE,0,0)!=0,"dropdown did not open");
            SendMessageW(c,CB_SHOWDROPDOWN,FALSE,0);
        }
        std::cout<<"PASS: native transport, current values, both menus, no accents, no overlap. No playback started.\n";
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<" ("<<GetLastError()<<")\n";result=1;}
    if(search.window) PostMessageW(search.window,WM_CLOSE,0,0);
    if(proc.hProcess) {
        if(WaitForSingleObject(proc.hProcess,5000)!=WAIT_OBJECT_0) TerminateProcess(proc.hProcess,1); // own test instance only
        CloseHandle(proc.hProcess);CloseHandle(proc.hThread);
    }
    CloseDesktop(desktop);
    return result;
}
