// Development-aware native layout capture; no playback or master-volume changes.
#ifndef PULSE_SOURCE_MAIN
#define PULSE_SOURCE_MAIN "../src/main.cpp"
#endif
#include PULSE_SOURCE_MAIN
#include <iostream>
void capture(const std::string& path,bool includeMenu=false) {
    Sleep(250);
    RECT r; GetClientRect(window,&r); int w=r.right,h=r.bottom;
    HDC dc=GetDC(window), mem=CreateCompatibleDC(dc);
    HBITMAP bitmap=CreateCompatibleBitmap(dc,w,h); auto old=SelectObject(mem,bitmap);
    SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(mem),PRF_CLIENT);
    for(const auto& c:controls) if(IsWindowVisible(c.hwnd)) {
        RECT child; GetWindowRect(c.hwnd,&child); MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&child),2);
        int saved=SaveDC(mem); SetViewportOrgEx(mem,child.left,child.top,nullptr);
        IntersectClipRect(mem,0,0,child.right-child.left,child.bottom-child.top);
        wchar_t cls[32]{}; GetClassNameW(c.hwnd,cls,32);
        if(wcscmp(cls,L"Button")==0 && (GetWindowLongPtrW(c.hwnd,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW) {
            DRAWITEMSTRUCT d{}; d.CtlType=ODT_BUTTON; d.CtlID=GetDlgCtrlID(c.hwnd); d.hwndItem=c.hwnd; d.hDC=mem;
            d.rcItem={0,0,child.right-child.left,child.bottom-child.top}; if(!IsWindowEnabled(c.hwnd)) d.itemState|=ODS_DISABLED;
            drawButton(&d);
        } else if(GetDlgCtrlID(c.hwnd)==Steps) {
            RECT r{0,0,child.right-child.left,child.bottom-child.top}; FillRect(mem,&r,panelBrush);
            int count=static_cast<int>(SendMessageW(c.hwnd,LB_GETCOUNT,0,0));
            int top=static_cast<int>(SendMessageW(c.hwnd,LB_GETTOPINDEX,0,0));
            for(int i=top;i<count;++i) {
                DRAWITEMSTRUCT d{}; d.CtlType=ODT_LISTBOX; d.CtlID=Steps; d.itemID=i; d.hwndItem=c.hwnd; d.hDC=mem;
                SendMessageW(c.hwnd,LB_GETITEMRECT,i,reinterpret_cast<LPARAM>(&d.rcItem));
                if(d.rcItem.top>=r.bottom) break;
                if(i==SendMessageW(c.hwnd,LB_GETCURSEL,0,0)) d.itemState=ODS_SELECTED;
                drawButton(&d);
            }
        } else SendMessageW(c.hwnd,WM_PRINT,reinterpret_cast<WPARAM>(mem),PRF_CLIENT|PRF_ERASEBKGND|PRF_CHILDREN);
        RestoreDC(mem,saved);
    }
    if(includeMenu) {
        HWND popup=FindWindowW(L"#32768",nullptr);
        if(popup) {
            RECT r; GetWindowRect(popup,&r); MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
            int saved=SaveDC(mem); SetViewportOrgEx(mem,r.left,r.top,nullptr);
            SendMessageW(popup,WM_PRINT,reinterpret_cast<WPARAM>(mem),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND|PRF_CHILDREN);
            RestoreDC(mem,saved);
            HMENU menu=reinterpret_cast<HMENU>(SendMessageW(popup,0x01e1,0,0));
            for(int i=0;menu&&i<GetMenuItemCount(menu);++i) {
                MENUITEMINFOW item{}; item.cbSize=sizeof(item); item.fMask=MIIM_DATA|MIIM_FTYPE|MIIM_STATE|MIIM_ID;
                GetMenuItemInfoW(menu,i,TRUE,&item);
                RECT row; if(!GetMenuItemRect(nullptr,menu,i,&row)) continue;
                MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&row),2);
                if(item.fType&MFT_SEPARATOR) { FillRect(mem,&row,panelBrush); continue; }
                if(item.fType&MFT_OWNERDRAW) {
                    DRAWITEMSTRUCT draw{}; draw.CtlType=ODT_MENU; draw.itemID=item.wID; draw.itemData=item.dwItemData; draw.hDC=mem; draw.rcItem=row;
                    if(item.fState&MFS_HILITE) draw.itemState=ODS_SELECTED;
                    drawButton(&draw);
                }
            }
        }
    }
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=w; info.bmiHeader.biHeight=-h; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    std::vector<unsigned char> pixels(w*h*4); SelectObject(mem,old); GetDIBits(dc,bitmap,0,h,pixels.data(),&info,DIB_RGB_COLORS);
    BITMAPFILEHEADER header{}; header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader); header.bfSize=header.bfOffBits+pixels.size();
    std::ofstream out(path,std::ios::binary); out.write(reinterpret_cast<char*>(&header),sizeof(header)); out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader)); out.write(reinterpret_cast<char*>(pixels.data()),pixels.size());
    DeleteObject(bitmap); DeleteDC(mem); ReleaseDC(window,dc);
}
#ifndef PULSE_BASELINE
void captureDesktopMenu(const std::string& path) {
    PostMessageW(control(Presets),BM_CLICK,0,0); Sleep(300);
    capture(path,true);
    PostMessageW(window,WM_CANCELMODE,0,0); Sleep(200);
}
#endif
int main(int argc,char** argv) {
    std::thread gui([] { wWinMain(GetModuleHandleW(nullptr),nullptr,nullptr,SW_SHOW); });
    for(int i=0;i<100&&(!window||!control(SystemVolumeSlider));++i) Sleep(100);
    if(!window) ExitProcess(1);
    for(int i=0;i<100;++i) {
        RECT r{}; GetClientRect(window,&r);
        if(IsWindowVisible(control(Meter))&&!IsWindowVisible(control(FullEditor))&&r.bottom==px(TinyHeight)) break;
        Sleep(50);
    }
    std::string dir=argc>1?argv[1]:"build";
    capture(dir+"/tiny.bmp");
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(CycleView,BN_CLICKED),0); capture(dir+"/compact.bmp");
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(CycleView,BN_CLICKED),0); capture(dir+"/rhythm.bmp");
    SendMessageW(control(Mode),CB_SETCURSEL,1,0); SendMessageW(window,WM_COMMAND,MAKEWPARAM(Mode,CBN_SELCHANGE),0); capture(dir+"/sequence.bmp");
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(ToolsTab,BN_CLICKED),0); capture(dir+"/tools.bmp");
    #ifndef PULSE_BASELINE
    if(argc>2) captureDesktopMenu(dir+"/presets.bmp");
    #endif
    PostMessageW(window,WM_CLOSE,0,0); gui.join();
    std::cout<<"Captured five native layouts (composited child controls).\n";
}
