#include "../src/main.cpp"
#include <iostream>
HWND app;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); std::cout<<"PASS "<<message<<std::endl; }
HWND item(int id) { return GetDlgItem(app,id); }
std::wstring fieldText(int id) { wchar_t buffer[256]{}; SendMessageW(item(id),WM_GETTEXT,256,reinterpret_cast<LPARAM>(buffer)); return buffer; }
void set(int id,const wchar_t* s) { SendMessageW(item(id),WM_SETTEXT,0,reinterpret_cast<LPARAM>(s)); }
void click(int id) { PostMessageW(item(id),BM_CLICK,0,0); Sleep(180); }
void select(int id,int index,bool list=false) { SendMessageW(item(id),list?LB_SETCURSEL:CB_SETCURSEL,index,0); SendMessageW(app,WM_COMMAND,MAKEWPARAM(id,list?LBN_SELCHANGE:CBN_SELCHANGE),reinterpret_cast<LPARAM>(item(id))); Sleep(180); }
void screenshot(const char* path,bool composite=false) {
    RECT r; GetWindowRect(app,&r); int w=r.right-r.left,h=r.bottom-r.top;
    HDC dc=GetDC(app),mem=CreateCompatibleDC(dc); HBITMAP bitmap=CreateCompatibleBitmap(dc,w,h); auto old=SelectObject(mem,bitmap);
    PrintWindow(app,mem,2);
    // Wine's PrintWindow omits children. Composite their WM_PRINT output for layout inspection.
    if(composite) for(const auto& c:controls) if(IsWindowVisible(c.hwnd)) {
        RECT child; GetWindowRect(c.hwnd,&child); MapWindowPoints(nullptr,app,reinterpret_cast<POINT*>(&child),2);
        int saved=SaveDC(mem); SetViewportOrgEx(mem,child.left,child.top,nullptr);
        IntersectClipRect(mem,0,0,child.right-child.left,child.bottom-child.top);
        if((GetWindowLongPtrW(c.hwnd,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW && GetDlgCtrlID(c.hwnd)!=Steps) {
            DRAWITEMSTRUCT d{}; d.CtlType=ODT_BUTTON; d.CtlID=GetDlgCtrlID(c.hwnd); d.hwndItem=c.hwnd; d.hDC=mem;
            d.rcItem={0,0,child.right-child.left,child.bottom-child.top};
            if(!IsWindowEnabled(c.hwnd)) d.itemState|=ODS_DISABLED;
            drawButton(&d);
        } else SendMessageW(c.hwnd,WM_PRINT,reinterpret_cast<WPARAM>(mem),PRF_CLIENT|PRF_ERASEBKGND|PRF_CHILDREN);
        RestoreDC(mem,saved);
    }
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=w; info.bmiHeader.biHeight=-h; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    std::vector<unsigned char> pixels(w*h*4); SelectObject(mem,old); GetDIBits(dc,bitmap,0,h,pixels.data(),&info,DIB_RGB_COLORS);
    BITMAPFILEHEADER header{}; header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader); header.bfSize=header.bfOffBits+pixels.size();
    std::ofstream out(path,std::ios::binary); out.write(reinterpret_cast<char*>(&header),sizeof(header)); out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader)); out.write(reinterpret_cast<char*>(pixels.data()),pixels.size());
    DeleteObject(bitmap); DeleteDC(mem); ReleaseDC(app,dc);
}
int main() {
    std::atomic<bool> exited{false};
    std::atomic<DWORD> guiId{0};
    std::thread gui([&] { guiId=GetCurrentThreadId(); wWinMain(GetModuleHandleW(nullptr),nullptr,nullptr,SW_SHOW); exited=true; });
    int result=0;
    try {
        for(int i=0;i<100&&!app;++i) { if(guiId) EnumThreadWindows(guiId,[](HWND h,LPARAM)->BOOL { wchar_t cls[64]{}; GetClassNameW(h,cls,64); if(wcscmp(cls,L"PulseMetronome")==0) { app=h; return FALSE; } return TRUE; },0); Sleep(100); }
        require(app!=nullptr,"native window launches"); Sleep(300);
        require((GetWindowLongPtrW(app,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0,"widget starts always on top");
        require((GetWindowLongPtrW(app,GWL_STYLE)&WS_CAPTION)!=WS_CAPTION,"widget has no title bar");
        RECT tinyClient; GetClientRect(app,&tinyClient);
        require(tinyClient.right==px(TinyWidth)&&tinyClient.bottom==px(TinyHeight),"starts as a tiny DPI-scaled bar");
        require(!IsWindowVisible(item(FullEditor))&&IsWindowVisible(item(Meter)),"tiny bar shows meter without full editor");
        for(int id:{Bpm,Meter,Start,PositionLock,MinimizeWidget,CycleView,CloseWidget}) {
            RECT r; GetWindowRect(item(id),&r); MapWindowPoints(nullptr,app,reinterpret_cast<POINT*>(&r),2);
            require(r.left>=0&&r.top>=0&&r.right<=tinyClient.right&&r.bottom<=tinyClient.bottom,"tiny control fits client area");
        }
        require(SendMessageW(item(Meter),CB_GETCOUNT,0,0)==128,"meter dropdown supports 1-32 beats and all four denominators");
        select(Meter,26); // 7/8
        require(fieldText(Beats)==L"7"&&fieldText(Groups)==L"7","tiny meter selection updates editor and resets obsolete grouping");
        select(Meter,127); // 32/16
        require(fieldText(Beats)==L"32"&&choice(Unit)==3,"tiny meter supports 32/16 boundary");
        select(Meter,13); // 4/4
        require(fieldText(Beats)==L"4","tiny meter can return to 4/4");
        POINT header{px(12),px(20)}; ClientToScreen(app,&header);
        require(SendMessageW(app,WM_NCHITTEST,0,MAKELPARAM(header.x,header.y))==HTCAPTION,"tiny grip is draggable");
        click(PositionLock);
        require(fieldText(PositionLock)==L"Unlock"&&IsWindowEnabled(item(PositionLock)),"locked bar keeps unlock button enabled");
        require((GetWindowLongPtrW(app,GWL_EXSTYLE)&WS_EX_TRANSPARENT)==0,"position lock never makes controls click-through");
        require(SendMessageW(app,WM_NCHITTEST,0,MAKELPARAM(header.x,header.y))!=HTCAPTION,"lock disables drag hit-testing");
        click(PositionLock);
        require(fieldText(PositionLock)==L"Lock"&&SendMessageW(app,WM_NCHITTEST,0,MAKELPARAM(header.x,header.y))==HTCAPTION,"same button unlocks dragging again");
        click(MinimizeWidget); require(IsIconic(app),"minimize button minimizes bar");
        ShowWindow(app,SW_RESTORE); Sleep(200);
        require(!IsIconic(app)&&IsWindowVisible(item(Start)),"bar can be restored from taskbar");
        screenshot("docs/tiny-bar.bmp",true);
        click(CycleView); // compact overlay
        RECT widgetClient; GetClientRect(app,&widgetClient);
        require(widgetClient.right==px(WidgetWidth)&&widgetClient.bottom==px(WidgetHeight),"Size cycles tiny to compact overlay");
        require(IsWindowVisible(item(FullEditor))&&!IsWindowVisible(item(Meter)),"compact overlay controls restored");
        for(int id:{Bpm,Minus,Plus,Tap,Start,FullEditor,Opacity,PositionLock,CloseWidget,NextStep,MinimizeWidget,CycleView}) {
            RECT r; GetWindowRect(item(id),&r); MapWindowPoints(nullptr,app,reinterpret_cast<POINT*>(&r),2);
            require(r.left>=0&&r.top>=0&&r.right<=widgetClient.right&&r.bottom<=widgetClient.bottom,"compact control fits client area");
        }
        BYTE alpha=0; DWORD alphaFlags=0;
        for(BYTE expected:{BYTE(217),BYTE(166),BYTE(255)}) {
            click(Opacity); require(GetLayeredWindowAttributes(app,nullptr,&alpha,&alphaFlags)&&alpha==expected,"opacity cycles 85, 65, 100 percent");
        }
        click(PositionLock); click(PositionLock);
        require(fieldText(PositionLock)==L"Lock","compact lock is also reversibly clickable");
        SetForegroundWindow(app); Sleep(250); screenshot("docs/widget.bmp",true);
        click(CycleView);
        require((GetWindowLongPtrW(app,GWL_EXSTYLE)&(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT))==0,"editor restores normal window styles");
        for(int i=0;i<40&&!IsWindowVisible(item(Beats));++i) Sleep(100);
        require(IsWindowVisible(item(Beats))&&!IsWindowVisible(item(FullEditor)),"editor controls restored");
        RECT editorRect; GetWindowRect(app,&editorRect);
        click(CycleView); GetClientRect(app,&tinyClient);
        require(tinyClient.bottom==px(TinyHeight),"Size cycles full editor back to tiny bar");
        click(CycleView); click(CycleView);
        click(Overlay); click(Plus); require(fieldText(Bpm)==L"121","widget plus changes tempo");
        click(Minus); click(FullEditor);
        RECT restored; GetWindowRect(app,&restored);
        require(EqualRect(&editorRect,&restored),"editor placement restored after widget round trip");
        require(fieldText(102)==L"120","default tempo 120"); require(!IsWindowEnabled(item(112)),"standard mode disables sequence list");
        select(103,1); require(IsWindowEnabled(item(112)),"sequence mode enables steps"); require(SendMessageW(item(112),LB_GETCOUNT,0,0)==3,"example has three steps");
        select(112,1,true); require(fieldText(104)==L"7"&&fieldText(106)==L"2+2+3","selecting 7/8 loads grouping");
        click(CycleView); // full -> tiny
        require(choice(Meter)==26,"tiny dropdown reflects selected sequence meter");
        select(Meter,27); // 7/16: keep 2+2+3 and repeat count
        require(fieldText(Groups)==L"2+2+3"&&fieldText(Repeats)==L"2","changing denominator preserves sequence grouping and repeats");
        select(Meter,26);
        click(CycleView); click(CycleView);
        set(106,L"3+2+2"); click(116); require(fieldText(106)==L"3+2+2","accent changes apply");
        click(115); require(SendMessageW(item(112),LB_GETCOUNT,0,0)==4,"add step");
        click(118); require(SendMessageW(item(112),LB_GETCURSEL,0,0)==1,"move step up");
        click(119); require(SendMessageW(item(112),LB_GETCURSEL,0,0)==2,"move step down");
        click(117); require(SendMessageW(item(112),LB_GETCOUNT,0,0)==3,"remove step");
        select(112,1,true); set(106,L"2+2+3"); click(116); set(108,L"3");
        screenshot("docs/preview.bmp");
        set(110,L"0"); click(100); Sleep(600);
        HWND dialog=FindWindowW(L"#32770",L"Audio unavailable");
        if(dialog) { std::cout<<"INFO Audio endpoint unavailable in Wine; error shown correctly"<<std::endl; PostMessageW(dialog,WM_COMMAND,IDOK,0); Sleep(200); require(IsWindowEnabled(item(102)),"audio failure restores controls"); }
        else {
            require(fieldText(Start).find(L"Stop")!=std::wstring::npos,"playback starts");
            require(IsWindowEnabled(item(Bpm))&&IsWindowEnabled(item(Plus))&&IsWindowEnabled(item(Minus))&&IsWindowEnabled(item(Tap)),"all tempo controls remain enabled during playback");
            require(!IsWindowEnabled(item(Beats)),"structural editing stays locked during playback");
            click(Overlay);
            require(fieldText(Start)==L"Stop"&&IsWindowEnabled(item(FullEditor))&&IsWindowEnabled(item(Opacity)),"widget conversion keeps playback and widget controls enabled");
            require(!IsWindowEnabled(item(Beats)),"switching view does not unlock structural edits");
            click(CycleView); // full editor
            click(CycleView); // tiny
            require(fieldText(Start)==L"Stop"&&!IsWindowEnabled(item(Meter)),"tiny bar retains playback and locks structural meter edits");
            click(PositionLock); click(PositionLock);
            require(fieldText(PositionLock)==L"Lock","lock can be clicked twice during playback");
            click(MinimizeWidget); require(IsIconic(app)&&fieldText(Start)==L"Stop","minimizing keeps playback running");
            ShowWindow(app,SW_RESTORE); Sleep(200);
            click(CycleView); click(Opacity); click(FullEditor);
            require(fieldText(Start).find(L"Stop")!=std::wstring::npos,"returning to editor preserves playback");
            set(Bpm,L"180"); Sleep(650);
            require(fieldText(Bpm)==L"180"&&fieldText(Start).find(L"Stop")!=std::wstring::npos,"typed live tempo keeps playback running");
            click(Plus); require(fieldText(Bpm)==L"181","plus changes live tempo");
            click(Minus); require(fieldText(Bpm)==L"180","minus changes live tempo");
            set(Bpm,L"999"); SendMessageW(app,WM_COMMAND,MAKEWPARAM(Bpm,EN_KILLFOCUS),reinterpret_cast<LPARAM>(item(Bpm)));
            require(fieldText(Bpm)==L"180","invalid live tempo restores last valid value");
            click(Tap); Sleep(250); click(Tap);
            require(fieldText(Start).find(L"Stop")!=std::wstring::npos&&std::stoi(fieldText(Bpm))>=20&&std::stoi(fieldText(Bpm))<=300,"tap tempo works while playback continues");
            click(Start); require(IsWindowEnabled(item(Beats)),"stop restores structural controls");
        }
        set(104,L"0"); click(116); Sleep(200); dialog=FindWindowW(L"#32770",L"Check your rhythm"); require(dialog!=nullptr,"invalid beat count is rejected"); PostMessageW(dialog,WM_COMMAND,IDOK,0); Sleep(200); set(104,L"7");
        require((GetWindowLongPtrW(app,GWL_STYLE)&(WS_VSCROLL|WS_HSCROLL))==0,"main window has no scrolling canvas");
        click(Forever); require(SendMessageW(item(Forever),BM_GETCHECK,0,0)==BST_CHECKED,"Forever checkbox selected");
        require(!IsWindowEnabled(item(Repeats)),"Forever disables finite repeat count");
        click(Apply); select(Steps,0,true); select(Steps,1,true);
        require(SendMessageW(item(Forever),BM_GETCHECK,0,0)==BST_CHECKED,"Forever retained when switching steps");
        for(int i=0;i<8;++i) {
            click(ToolsTab); require(IsWindowVisible(item(Subdivision))&&!IsWindowVisible(item(Steps)),"Practice tools replaces rhythm editor");
            click(RhythmTab); require(IsWindowVisible(item(Steps))&&!IsWindowVisible(item(Subdivision)),"Rhythm editor returns without overlapping controls");
            SendMessageW(app,WM_MOUSEWHEEL,MAKEWPARAM(0,static_cast<WORD>(-WHEEL_DELTA)),0);
        }
        RECT client; GetClientRect(app,&client);
        bool contained=true;
        for(int id:{Start,NextStep,Bpm,Mode,Beats,Groups,Steps,Apply,Forever}) {
            RECT r; GetWindowRect(item(id),&r); MapWindowPoints(nullptr,app,reinterpret_cast<POINT*>(&r),2);
            contained=contained&&r.left>=0&&r.top>=0&&r.right<=client.right&&r.bottom<=client.bottom;
        }
        require(contained,"all primary controls fit the compact client area");
        screenshot("docs/preview.bmp");
        click(ToolsTab); screenshot("docs/practice-tools.bmp"); click(RhythmTab);
        PostMessageW(app,WM_CLOSE,0,0);
        for(int i=0;i<30&&!exited;++i) Sleep(100);
        require(exited,"unsaved practice closes cleanly without a confirmation dialog");
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<std::endl; result=1; }
    if(result) ExitProcess(1);
    gui.join(); return result;
}
