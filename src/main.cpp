#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <array>
#include "audio.h"
#include "system_volume.h"

namespace {
const COLORREF BG = RGB(244,246,248), PANEL = RGB(255,255,255), INK = RGB(30,43,50), MUTED = RGB(91,108,116), LINE = RGB(226,232,235), TEAL = RGB(12,110,93);
enum Id { Start=100, Tap, Bpm, Mode, Beats, Unit, Groups, Subdivision, Poly, Mute, Volume, Sound, Steps, Repeats, StepTempo, Add, Apply, Remove, Up, Down, Save, Load, Example, Minus, Plus, Forever, NextStep, RhythmTab, ToolsTab, Accents, SystemVolumeSlider, VolumeSlider, Overlay, FullEditor, Opacity, PositionLock, CloseWidget, MinimizeWidget, CycleView, Meter };
struct Control { HWND hwnd; int x,y,w,h; };
HWND window;
std::vector<Control> controls;
HFONT font, smallFont, titleFont, numberFont, beatFont;
HBRUSH bgBrush, panelBrush;
float scale = 1;
bool toolsPage = false, overlayMode = false, positionLocked = false;
int viewMode = 2; // 0: tiny bar, 1: compact overlay, 2: full editor
int opacityIndex = 0;
constexpr BYTE overlayAlphas[]{255,217,166};
constexpr int WidgetWidth = 400, WidgetHeight = 248, TinyWidth = 410, TinyHeight = 44;
int overlayWidth() { return viewMode==0?TinyWidth:WidgetWidth; }
int overlayHeight() { return viewMode==0?TinyHeight:WidgetHeight; }
WINDOWPLACEMENT fullPlacement{};
RECT widgetRect{};
void arrange();
void visibility();
void setView(int next);
void setOverlay(bool enabled) { setView(enabled?1:2); }
void syncMeter();
pulse::Settings settings;
Audio audio;
int selected = 0, currentBeat = -1, currentStep = 0, currentRepeat = 0;
bool currentSilent = false, playing = false, dirty = false, populating = false;
std::wstring notice = L"Ready. Press Start to play.";
std::vector<double> taps;
constexpr UINT_PTR TempoEditTimer = 1, VolumeSyncTimer = 2, VolumeEditTimer = 3;
std::wstring systemVolumeLabel=L"Windows volume";
int px(int n) { return static_cast<int>(n * scale + .5f); }
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L' '); MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n); return w;
}
HWND control(int id) { return GetDlgItem(window, id); }
std::wstring value(int id) { int n=GetWindowTextLengthW(control(id)); std::wstring s(n+1,L'\0'); GetWindowTextW(control(id),s.data(),n+1); s.resize(n); return s; }
void text(int id, const std::wstring& s) { SetWindowTextW(control(id),s.c_str()); }
int integer(int id, int lo, int hi, const char* name) {
    auto s=value(id); size_t used=0; int n;
    try { n=std::stoi(s,&used); } catch (...) { throw std::runtime_error(std::string(name)+" needs a whole number."); }
    if (used != s.size() || n<lo || n>hi) throw std::runtime_error(std::string(name)+" must be between "+std::to_string(lo)+" and "+std::to_string(hi)+".");
    return n;
}
int choice(int id) { return static_cast<int>(SendMessageW(control(id),CB_GETCURSEL,0,0)); }
void choose(int id,int index) { SendMessageW(control(id),CB_SETCURSEL,index,0); }
void status(const std::wstring& s) { notice=s; InvalidateRect(window,nullptr,FALSE); }
void syncSystemVolume() {
    float level=0; BOOL muted=FALSE;
    bool available=SUCCEEDED(SystemVolume::read(level,muted));
    EnableWindow(control(SystemVolumeSlider),available);
    std::wstring label=available?L"Windows volume  "+std::to_wstring(static_cast<int>(std::lround(level*100)))+L"%"+(muted?L"  (muted)":L""):L"Windows volume unavailable";
    if(available && GetCapture()!=control(SystemVolumeSlider)) SendMessageW(control(SystemVolumeSlider),TBM_SETPOS,TRUE,static_cast<LPARAM>(std::lround(level*100)));
    if(label!=systemVolumeLabel) { systemVolumeLabel=label; RECT r{px(350),px(4),px(638),px(30)}; InvalidateRect(window,&r,FALSE); }
}
void acceptTempo(bool restoreInvalid=false) {
    KillTimer(window,TempoEditTimer);
    try {
        int bpm=integer(Bpm,20,300,"Tempo");
        settings.bpm=bpm;
        if(playing) {
            audio.setTempo(bpm);
            status(settings.sequence&&settings.bars[currentStep].bpm?L"Main tempo updated. This step keeps its own BPM.":L"Tempo updated for the next beat.");
        }
    } catch(const std::exception&) {
        if(restoreInvalid) {
            bool previous=populating; populating=true;
            text(Bpm,std::to_wstring(settings.bpm)); populating=previous;
        }
        status(L"Enter a tempo from 20 to 300 BPM. Playback keeps the last valid tempo.");
    }
}
void acceptVolume(bool restoreInvalid=false) {
    KillTimer(window,VolumeEditTimer);
    try {
        settings.volume=integer(Volume,0,400,"Metronome volume");
        audio.setVolume(settings.volume);
        SendMessageW(control(VolumeSlider),TBM_SETPOS,TRUE,settings.volume);
    } catch(const std::exception&) {
        if(restoreInvalid) { bool previous=populating; populating=true; text(Volume,std::to_wstring(settings.volume)); populating=previous; }
        status(L"Enter metronome volume from 0 to 400%.");
    }
}
void error(const std::exception& e) { MessageBoxW(window,widen(e.what()).c_str(),L"Check your rhythm",MB_OK|MB_ICONINFORMATION); }
void title() { SetWindowTextW(window,dirty?L"Pulse 0.3.1 • unsaved changes":L"Pulse 0.3.1 — Metronome & mixed meters"); }
void changed() { dirty=true; title(); }
HWND make(const wchar_t* cls, const wchar_t* label, int id,int x,int y,int w,int h,DWORD style) {
    HWND c=CreateWindowExW(0,cls,label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_CLIPSIBLINGS|style,px(x),px(y),px(w),px(h),window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    controls.push_back({c,x,y,w,h});
    SetWindowTheme(c,L"Explorer",nullptr);
    return c;
}
struct TempoHold { bool active=false; ULONGLONG started=0; };
LRESULT CALLBACK tempoButton(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR subclass,DWORD_PTR data) {
    auto* hold=reinterpret_cast<TempoHold*>(data);
    auto cancel=[&] {
        hold->active=false; KillTimer(hwnd,1); SendMessageW(hwnd,BM_SETSTATE,FALSE,0);
        if(GetCapture()==hwnd) ReleaseCapture();
    };
    auto step=[&] { SendMessageW(GetParent(hwnd),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(hwnd),BN_CLICKED),reinterpret_cast<LPARAM>(hwnd)); };
    switch(msg) {
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
            if(!IsWindowEnabled(hwnd)) return 0;
            SetFocus(hwnd); SetCapture(hwnd); hold->active=true; hold->started=GetTickCount64();
            SendMessageW(hwnd,BM_SETSTATE,TRUE,0); SetTimer(hwnd,1,400,nullptr); step(); return 0;
        case WM_TIMER:
            if(wp==1) {
                if(!hold->active) return 0;
                POINT point; RECT rect; GetCursorPos(&point); GetWindowRect(hwnd,&rect);
                if(GetCapture()!=hwnd || !(GetAsyncKeyState(VK_LBUTTON)&0x8000) || !PtInRect(&rect,point) || GetForegroundWindow()!=GetAncestor(hwnd,GA_ROOT)) { cancel(); return 0; }
                step(); SetTimer(hwnd,1,GetTickCount64()-hold->started>=1400?30:100,nullptr); return 0;
            } break;
        case WM_MOUSEMOVE:
            if(hold->active) { POINT point; RECT rect; GetCursorPos(&point); GetWindowRect(hwnd,&rect); if(!PtInRect(&rect,point)) cancel(); }
            break;
        case WM_LBUTTONUP: if(hold->active) { cancel(); return 0; } break;
        case WM_CAPTURECHANGED: case WM_CANCELMODE: case WM_KILLFOCUS:
            if(hold->active) cancel();
            break;
        case WM_ENABLE: if(!wp && hold->active) cancel(); break;
        case WM_NCDESTROY:
            cancel(); RemoveWindowSubclass(hwnd,tempoButton,subclass); delete hold; break;
    }
    return DefSubclassProc(hwnd,msg,wp,lp);
}
void button(const wchar_t* label,int id,int x,int y,int w,int h=36) {
    HWND hwnd=make(L"BUTTON",label,id,x,y,w,h,BS_OWNERDRAW);
    if(id==Minus||id==Plus) {
        auto* hold=new TempoHold;
        if(!SetWindowSubclass(hwnd,tempoButton,1,reinterpret_cast<DWORD_PTR>(hold))) delete hold;
    }
}
void edit(const wchar_t* label,int id,int x,int y,int w) { auto c=make(L"EDIT",label,id,x,y,w,32,WS_BORDER|ES_AUTOHSCROLL); SendMessageW(c,EM_SETLIMITTEXT,100,0); }
void combo(int id,int x,int y,int w,const std::vector<std::wstring>& options,int index) {
    auto c=make(L"COMBOBOX",L"",id,x,y,w,220,CBS_DROPDOWNLIST|WS_VSCROLL);
    for (auto& s:options) { SendMessageW(c,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str())); }
    choose(id,index);
}
void setFonts() {
    for (HFONT f:{font,smallFont,titleFont,numberFont,beatFont}) if (f) DeleteObject(f);
    auto create=[](int size,int weight) { return CreateFontW(-px(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI"); };
    font=create(15,FW_NORMAL); smallFont=create(12,FW_NORMAL); titleFont=create(28,FW_SEMIBOLD); numberFont=create(46,FW_SEMIBOLD); beatFont=create(20,FW_SEMIBOLD);
    for (auto& c:controls) SendMessageW(c.hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(GetDlgCtrlID(c.hwnd)==Bpm?numberFont:font),TRUE);
}
pulse::Bar readBar() {
    pulse::Bar b; b.beats=integer(Beats,1,32,"Beats"); b.unit=std::array<int,4>{2,4,8,16}[std::max(0,choice(Unit))];
    b.repeats=SendMessageW(control(Forever),BM_GETCHECK,0,0)==BST_CHECKED?0:integer(Repeats,1,9999,"Repeats"); b.bpm=integer(StepTempo,0,300,"Step tempo");
    auto w=value(Groups); std::string g(w.begin(),w.end()); b.groups=pulse::parseGroups(g,b.beats); pulse::validate(b); return b;
}
void readGlobals() {
    settings.bpm=integer(Bpm,20,300,"Tempo"); settings.sequence=choice(Mode)==1;
    settings.subdivision=choice(Subdivision)+1; settings.poly=integer(Poly,0,16,"Second pulse");
    settings.mute=integer(Mute,0,100,"Silent beats"); settings.volume=integer(Volume,0,400,"Metronome volume"); settings.sound=choice(Sound); settings.accents=SendMessageW(control(Accents),BM_GETCHECK,0,0)==BST_CHECKED;
}
void refreshList() {
    SendMessageW(control(Steps),LB_RESETCONTENT,0,0);
    for (size_t i=0;i<settings.bars.size();++i) {
        const auto& b=settings.bars[i];
        std::wstring s=std::to_wstring(i+1)+L"    "+std::to_wstring(b.beats)+L"/"+std::to_wstring(b.unit)+L"     "+widen(pulse::groupText(b))+(b.repeats?L"     ×"+std::to_wstring(b.repeats):L"     Forever")+L"     "+(b.bpm?std::to_wstring(b.bpm)+L" BPM":L"Main tempo");
        SendMessageW(control(Steps),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str()));
    }
    selected=std::clamp(selected,0,static_cast<int>(settings.bars.size())-1);
    SendMessageW(control(Steps),LB_SETCURSEL,selected,0);
}
void editor() {
    populating=true;
    const auto& b=settings.sequence?settings.bars[selected]:settings.simple;
    text(Beats,std::to_wstring(b.beats)); choose(Unit,b.unit==2?0:b.unit==4?1:b.unit==8?2:3);
    text(Groups,widen(pulse::groupText(b))); text(Repeats,std::to_wstring(b.repeats?b.repeats:1));
    SendMessageW(control(Forever),BM_SETCHECK,b.repeats==0?BST_CHECKED:BST_UNCHECKED,0); text(StepTempo,std::to_wstring(b.bpm));
    for(int id:{Steps,StepTempo,Add,Remove,Up,Down,Forever}) EnableWindow(control(id),settings.sequence&&!playing);
    EnableWindow(control(Repeats),settings.sequence&&!playing&&b.repeats!=0);
    EnableWindow(control(NextStep),playing&&settings.sequence);
    syncMeter(); visibility();
    populating=false; InvalidateRect(window,nullptr,FALSE);
}
void syncMeter() {
    const auto& b=settings.sequence?settings.bars[playing?currentStep:selected]:settings.simple;
    int unit=b.unit==2?0:b.unit==4?1:b.unit==8?2:3;
    choose(Meter,(b.beats-1)*4+unit);
    EnableWindow(control(Meter),!playing);
}
void populate() {
    populating=true;
    text(Bpm,std::to_wstring(settings.bpm)); choose(Mode,settings.sequence?1:0); choose(Subdivision,settings.subdivision-1);
    text(Poly,std::to_wstring(settings.poly)); text(Mute,std::to_wstring(settings.mute)); text(Volume,std::to_wstring(settings.volume)); SendMessageW(control(VolumeSlider),TBM_SETPOS,TRUE,settings.volume); choose(Sound,settings.sound);
    SendMessageW(control(Accents),BM_SETCHECK,settings.accents?BST_CHECKED:BST_UNCHECKED,0);
    refreshList(); editor(); populating=false;
}
void commit() {
    readGlobals(); auto b=readBar();
    if(settings.sequence) settings.bars[selected]=b;
    else { b.bpm=0; b.repeats=1; settings.simple=b; }
    pulse::validate(settings); refreshList();
}
void stop() {
    audio.stop(); playing=false; currentBeat=-1;
    // Discard notifications queued before Stop so a previous run cannot flash stale beats.
    MSG msg; while(PeekMessageW(&msg,window,WM_PULSE_BEAT,WM_PULSE_ERROR,PM_REMOVE)) {}
    for(auto& c:controls) EnableWindow(c.hwnd,TRUE);
    editor(); text(Start,overlayMode?L"Start":L"Start   [Space]"); status(L"Stopped. Adjust your rhythm, then start again.");
}
void toggle() {
    if(playing) { stop(); return; }
    commit(); currentBeat=-1; currentStep=0; currentRepeat=0; playing=true;
    for(auto& c:controls) if(GetDlgCtrlID(c.hwnd)!=MinimizeWidget&&GetDlgCtrlID(c.hwnd)!=CycleView&&GetDlgCtrlID(c.hwnd)!=Overlay&&GetDlgCtrlID(c.hwnd)!=FullEditor&&GetDlgCtrlID(c.hwnd)!=Opacity&&GetDlgCtrlID(c.hwnd)!=PositionLock&&GetDlgCtrlID(c.hwnd)!=CloseWidget&&GetDlgCtrlID(c.hwnd)!=Start&&GetDlgCtrlID(c.hwnd)!=NextStep&&GetDlgCtrlID(c.hwnd)!=RhythmTab&&GetDlgCtrlID(c.hwnd)!=ToolsTab&&GetDlgCtrlID(c.hwnd)!=Bpm&&GetDlgCtrlID(c.hwnd)!=Minus&&GetDlgCtrlID(c.hwnd)!=Plus&&GetDlgCtrlID(c.hwnd)!=Tap&&GetDlgCtrlID(c.hwnd)!=Accents&&GetDlgCtrlID(c.hwnd)!=SystemVolumeSlider&&GetDlgCtrlID(c.hwnd)!=Volume&&GetDlgCtrlID(c.hwnd)!=VolumeSlider&&GetDlgCtrlID(c.hwnd)!=Sound) EnableWindow(c.hwnd,FALSE);
    EnableWindow(control(NextStep),settings.sequence);
    text(Start,overlayMode?L"Stop":L"Stop   [Space]"); status(L"Playing. Change BPM, use + / −, or tap a new tempo at any time.");
    try { audio.start(window,settings); } catch(...) { stop(); throw; }
    syncMeter(); SetFocus(control(Start));
}
void tap() {
    double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if(!taps.empty()&&now-taps.back()>3) taps.clear();
    if(!taps.empty()&&now-taps.back()<.15) return;
    taps.push_back(now); if(taps.size()>7) taps.erase(taps.begin());
    if(taps.size()>1) { int bpm=std::clamp(static_cast<int>(std::lround(60*(taps.size()-1)/(taps.back()-taps.front()))),20,300); settings.bpm=bpm; text(Bpm,std::to_wstring(bpm)); acceptTempo(); changed(); }
    status(taps.size()==1?L"Keep tapping quarter notes…":(playing?L"Tapped tempo takes effect on the next beat.":L"Tempo captured. Tap again to refine it, or press Start."));
}
bool discard() { return !dirty||MessageBoxW(window,L"Discard unsaved changes to this practice?",L"Unsaved practice",MB_YESNO|MB_ICONQUESTION)==IDYES; }
void preset(bool saving) {
    if(saving) commit(); else if(!discard()) return;
    wchar_t path[32768]=L"My practice.pulse";
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=window;
    dialog.lpstrFilter=L"Pulse practice (*.pulse)\0*.pulse\0\0"; dialog.lpstrFile=path; dialog.nMaxFile=32768; dialog.lpstrDefExt=L"pulse";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(saving?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(!(saving?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))) return;
    if(saving) {
        // Replace only after the complete preset is safely written beside the destination.
        std::filesystem::path target(path), temp=target; temp+=L".tmp-"+std::to_wstring(GetCurrentProcessId());
        try {
            std::ofstream out(temp,std::ios::binary|std::ios::trunc); pulse::save(out,settings); out.close(); if(!out) throw std::runtime_error("Could not finish writing the preset.");
            if(!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Could not replace the preset. Check folder permissions.");
        } catch(...) { std::error_code ignored; std::filesystem::remove(temp,ignored); throw; }
    } else { std::ifstream in{std::filesystem::path(path)}; auto loaded=pulse::load(in); settings=loaded; selected=0; populate(); }
    dirty=false; title(); status(saving?L"Practice saved.":L"Practice loaded. Press Start when you’re ready.");
}
void drawText(HDC dc,const std::wstring& s,int x,int y,int w,int h,HFONT f,COLORREF color,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE) {
    SelectObject(dc,f); SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT); RECT r{px(x),px(y),px(x+w),px(y+h)}; DrawTextW(dc,s.c_str(),-1,&r,flags);
}
void box(HDC dc,int x,int y,int w,int h,COLORREF color,int radius=16) {
    HBRUSH b=CreateSolidBrush(color); auto oldB=SelectObject(dc,b); auto oldP=SelectObject(dc,GetStockObject(NULL_PEN));
    RoundRect(dc,px(x),px(y),px(x+w),px(y+h),px(radius),px(radius)); SelectObject(dc,oldB); SelectObject(dc,oldP); DeleteObject(b);
}
void paintWidget(HDC dc) {
    RECT r; GetClientRect(window,&r); FillRect(dc,&r,bgBrush);
    if(viewMode==0) {
        drawText(dc,positionLocked?L"•":L"⋮",4,4,22,34,beatFont,positionLocked?TEAL:MUTED,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        return;
    }
    drawText(dc,L"Pulse",16,8,96,32,beatFont,TEAL);
    drawText(dc,L"BPM",16,44,100,18,smallFont,MUTED);
    const auto& b=settings.sequence?settings.bars[playing?currentStep:selected]:settings.simple;
    drawText(dc,std::to_wstring(b.beats)+L"/"+std::to_wstring(b.unit)+(settings.sequence?L" · Step "+std::to_wstring((playing?currentStep:selected)+1):L""),244,48,140,24,font,INK);
    int cols=std::min(b.beats,16), gap=3, cw=(368-(cols-1)*gap)/cols;
    int start=0; std::vector<int> accents;
    for(int n:b.groups) { accents.push_back(start); start+=n; }
    for(int i=0;i<b.beats;++i) {
        bool active=playing&&currentBeat==i;
        bool accent=settings.accents&&std::find(accents.begin(),accents.end(),i)!=accents.end();
        int x=16+(i%16)*(cw+gap), y=128+(i/16)*26;
        box(dc,x,y,cw,22,active?(currentSilent?RGB(182,121,24):TEAL):(accent?RGB(211,234,227):LINE),6);
        drawText(dc,std::to_wstring(i+1),x,y,cw,22,smallFont,active?PANEL:INK,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    std::wstring hint=playing?L"Playing":L"Stopped";
    if(playing&&b.bpm) hint+=L" · Step tempo "+std::to_wstring(b.bpm)+L" BPM";
    if(playing&&currentSilent) hint+=L" · Silent beat";
    drawText(dc,hint,16,184,248,22,smallFont,TEAL);
    drawText(dc,positionLocked?L"Position locked · click Unlock to move":L"Drag header · Lock fixes position, not clicks",16,218,368,22,smallFont,MUTED);
}
void paint(HDC dc) {
    if(overlayMode) { paintWidget(dc); return; }
    RECT r; GetClientRect(window,&r); FillRect(dc,&r,bgBrush);
    drawText(dc,L"Pulse",24,13,180,40,titleFont,INK);
    drawText(dc,L"Find your time.",124,24,220,24,font,MUTED);
    drawText(dc,systemVolumeLabel,360,7,278,22,smallFont,MUTED);
    box(dc,24,72,852,144,PANEL);
    drawText(dc,L"QUARTER-NOTE BPM",44,80,225,20,smallFont,MUTED);
    const auto& b=settings.sequence?settings.bars[playing?currentStep:selected]:settings.simple;
    drawText(dc,std::to_wstring(b.beats)+L"/"+std::to_wstring(b.unit),320,86,76,32,beatFont,INK);
    std::wstring detail=settings.sequence?L"Step "+std::to_wstring((playing?currentStep:selected)+1)+L" of "+std::to_wstring(settings.bars.size()):L"Repeats forever";
    if(playing&&settings.sequence) detail+=b.repeats?L"  ·  Bar "+std::to_wstring(currentRepeat+1)+L"/"+std::to_wstring(b.repeats):L"  ·  Forever";
    drawText(dc,detail,397,88,264,28,smallFont,MUTED);
    int cols=std::min(b.beats,16),gap=5,cw=std::min(48,(330-(cols-1)*gap)/cols);
    int groupStart=0; std::vector<int> accents; for(int n:b.groups) { accents.push_back(groupStart); groupStart+=n; }
    for(int i=0;i<b.beats;++i) {
        bool accent=settings.accents&&std::find(accents.begin(),accents.end(),i)!=accents.end(); bool active=playing&&i==currentBeat;
        int x=320+(i%16)*(cw+gap),y=126+(i/16)*33;
        box(dc,x,y,cw,28,active?(currentSilent?RGB(182,121,24):TEAL):(accent?RGB(211,234,227):LINE),8);
        drawText(dc,std::to_wstring(i+1),x,y,cw,28,cw<25?smallFont:font,active?PANEL:INK,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    std::wstring pulseHint=settings.accents?L"Accents: "+widen(pulse::groupText(b)):L"Accents off — equal clicks";
    if(playing&&b.bpm) pulseHint+=L"  ·  "+std::to_wstring(b.bpm)+L" BPM";
    drawText(dc,pulseHint,320,184,338,20,smallFont,MUTED);
    box(dc,24,282,852,242,PANEL);
    if(!toolsPage) {
        if(settings.sequence) drawText(dc,L"Your sequence  ·  loops continuously",44,296,370,27,font,INK);
        else {
            drawText(dc,L"One steady meter",44,303,350,32,beatFont,INK);
            drawText(dc,L"Set a tempo, choose a meter, and press Start.",44,348,364,22,font,MUTED);
            drawText(dc,L"The metronome keeps going until you stop it.",44,379,364,22,font,MUTED);
            drawText(dc,L"For changing meters, switch to Sequence.",44,411,364,22,font,MUTED);
        }
        drawText(dc,L"Beats",444,301,90,22,smallFont,MUTED);
        drawText(dc,L"Note value",538,301,100,22,smallFont,MUTED);
        drawText(dc,L"Accent groups",650,301,200,22,smallFont,MUTED);
        drawText(dc,L"For 7/8, try 2+2+3. Leave blank for a first-beat accent.",444,367,408,34,smallFont,MUTED,DT_LEFT|DT_WORDBREAK);
        if(settings.sequence) {
            drawText(dc,L"Bars per step",444,399,130,22,smallFont,MUTED);
            drawText(dc,L"Step BPM (0 = main)",697,399,160,22,smallFont,MUTED);
        } else drawText(dc,L"This meter repeats indefinitely.",444,423,400,25,font,TEAL);
    } else {
        drawText(dc,L"Subdivisions per beat",44,303,220,22,smallFont,MUTED);
        drawText(dc,L"Second pulse per bar",320,303,220,22,smallFont,MUTED);
        drawText(dc,L"Silent beats (%)",596,303,240,22,smallFont,MUTED);
        drawText(dc,L"Click sound",44,394,220,22,smallFont,MUTED);
        drawText(dc,L"Metronome volume (0–400%)",320,394,220,22,smallFont,MUTED);
        drawText(dc,L"Second pulse: 0 is off. Try 3 over 4/4 for a 3:4 polyrhythm.",44,475,786,22,smallFont,MUTED);
        drawText(dc,L"Turn off Accent beats for the same tone and volume on every click.",44,496,786,20,smallFont,MUTED);
    }
    drawText(dc,notice,24,535,852,34,font,TEAL,DT_LEFT|DT_WORDBREAK);
    drawText(dc,L"Space  start / stop       T  tap tempo       Ctrl+S  save       Ctrl+O  open",24,577,852,20,smallFont,MUTED);
}
void drawButton(DRAWITEMSTRUCT* d) {
    if(d->CtlID==Steps) {
        if(d->itemID==static_cast<UINT>(-1)) return;
        COLORREF color=(d->itemState&ODS_SELECTED)?RGB(220,240,233):PANEL; HBRUSH b=CreateSolidBrush(color); FillRect(d->hDC,&d->rcItem,b); DeleteObject(b);
        std::array<wchar_t,256> label{}; SendMessageW(d->hwndItem,LB_GETTEXT,d->itemID,reinterpret_cast<LPARAM>(label.data()));
        RECT r=d->rcItem; r.left+=px(12); SelectObject(d->hDC,font); SetBkMode(d->hDC,TRANSPARENT); SetTextColor(d->hDC,(d->itemState&ODS_DISABLED)?MUTED:INK); DrawTextW(d->hDC,label.data(),-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        if(d->itemState&ODS_FOCUS) DrawFocusRect(d->hDC,&d->rcItem);
        return;
    }
    bool primary=d->CtlID==Start || (d->CtlID==RhythmTab&&!toolsPage) || (d->CtlID==ToolsTab&&toolsPage);
    COLORREF color=primary?TEAL:LINE;
    if(d->itemState&ODS_SELECTED) color=primary?RGB(9,86,72):RGB(202,213,218);
    if(d->itemState&ODS_DISABLED) color=PANEL;
    HBRUSH brush=CreateSolidBrush(color); FillRect(d->hDC,&d->rcItem,brush); DeleteObject(brush);
    auto s=value(d->CtlID); SelectObject(d->hDC,font); SetTextColor(d->hDC,(d->itemState&ODS_DISABLED)?MUTED:primary?PANEL:INK); SetBkMode(d->hDC,TRANSPARENT);
    DrawTextW(d->hDC,s.c_str(),-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(d->itemState&ODS_FOCUS) { RECT r=d->rcItem; InflateRect(&r,-4,-4); DrawFocusRect(d->hDC,&r); }
}
void command(int id,int code) {
    if(populating) return;
    if(code==BN_CLICKED) {
        if(id==CycleView) { setView((viewMode+1)%3); return; }
        if(id==MinimizeWidget) { ShowWindow(window,SW_MINIMIZE); return; }
        if(id==Overlay || id==FullEditor) { setOverlay(id==Overlay); return; }
        if(id==CloseWidget) { SendMessageW(window,WM_CLOSE,0,0); return; }
        if(id==Opacity) {
            opacityIndex=(opacityIndex+1)%3;
            SetLayeredWindowAttributes(window,0,overlayAlphas[opacityIndex],LWA_ALPHA);
            text(Opacity,opacityIndex==0?L"100%":opacityIndex==1?L"85%":L"65%"); return;
        }
        if(id==PositionLock && overlayMode) {
            positionLocked=!positionLocked;
            text(PositionLock,positionLocked?L"Unlock":L"Lock");
            InvalidateRect(window,nullptr,FALSE); return;
        }
    }
    if(id==RhythmTab||id==ToolsTab) { if(code==BN_CLICKED) { toolsPage=id==ToolsTab; visibility(); } return; }
    if(playing&&id!=Start&&id!=NextStep&&id!=Bpm&&id!=Minus&&id!=Plus&&id!=Tap&&id!=Accents&&id!=Volume&&id!=Sound) return;
    if(code==EN_CHANGE) {
        changed();
        if(id==Volume) { KillTimer(window,VolumeEditTimer); SetTimer(window,VolumeEditTimer,250,nullptr); }
        if(id==Bpm) {
            // Wait briefly for a complete number (e.g. 240, not its intermediate 24).
            KillTimer(window,TempoEditTimer); SetTimer(window,TempoEditTimer,250,nullptr);
        }
        return;
    }
    if(id==Volume&&code==EN_KILLFOCUS) { acceptVolume(true); return; }
    if(id==Bpm&&code==EN_SETFOCUS) {
        // Defer selection until after the mouse has placed the caret.
        PostMessageW(control(Bpm),EM_SETSEL,0,-1); return;
    }
    if(id==Bpm&&code==EN_KILLFOCUS) { acceptTempo(true); return; }
    if(id==Steps&&code==LBN_SELCHANGE) {
        int next=static_cast<int>(SendMessageW(control(Steps),LB_GETCURSEL,0,0));
        if(next>=0) {
            try { settings.bars[selected]=readBar(); selected=next; refreshList(); editor(); }
            catch(...) { SendMessageW(control(Steps),LB_SETCURSEL,selected,0); throw; }
        }
        return;
    }
    if(id==Mode&&code==CBN_SELCHANGE) {
        bool next=choice(Mode)==1;
        try { auto b=readBar(); if(settings.sequence) settings.bars[selected]=b; else { b.bpm=0; b.repeats=1; settings.simple=b; } }
        catch(...) { choose(Mode,settings.sequence?1:0); throw; }
        settings.sequence=next; refreshList(); editor(); changed(); return;
    }
    if(id==Meter && code==CBN_SELCHANGE) {
        int index=choice(Meter);
        if(index<0 || index>=128) return;
        commit();
        auto& b=settings.sequence?settings.bars[selected]:settings.simple;
        int beats=index/4+1, unit=std::array<int,4>{2,4,8,16}[index%4];
        if(b.beats!=beats) b.groups={beats};
        b.beats=beats; b.unit=unit;
        refreshList(); editor(); changed(); return;
    }
    if(id==Sound&&code==CBN_SELCHANGE) { settings.sound=choice(Sound); audio.setSound(settings.sound); changed(); return; }
    if(code==CBN_SELCHANGE) { changed(); return; }
    if(code!=BN_CLICKED) return;
    switch(id) {
        case Start: toggle(); break;
        case NextStep: audio.nextStep(); status(L"Next step queued for the next bar."); break;
        case Forever: EnableWindow(control(Repeats),SendMessageW(control(Forever),BM_GETCHECK,0,0)!=BST_CHECKED); changed(); break;
        case Accents:
            settings.accents=SendMessageW(control(Accents),BM_GETCHECK,0,0)==BST_CHECKED;
            audio.setAccents(settings.accents); changed();
            status(settings.accents?L"Accents on.":L"Accents off: every click uses the same tone and volume."); break;
        case Tap: tap(); break;
        case Minus: case Plus: settings.bpm=std::clamp(integer(Bpm,20,300,"Tempo")+(id==Plus?1:-1),20,300); text(Bpm,std::to_wstring(settings.bpm)); acceptTempo(); changed(); break;
        case Apply: commit(); changed(); status(L"Rhythm applied. Press Start to hear it."); break;
        case Add: { readGlobals(); auto b=readBar(); if(settings.bars.size()>=128) throw std::runtime_error("A sequence can contain up to 128 steps."); settings.bars.insert(settings.bars.begin()+selected+1,b); ++selected; refreshList(); editor(); changed(); break; }
        case Remove: if(settings.bars.size()>1) { settings.bars.erase(settings.bars.begin()+selected); refreshList(); editor(); changed(); } else status(L"Keep at least one step in your sequence."); break;
        case Up: case Down: { int next=selected+(id==Up?-1:1); if(next>=0&&next<static_cast<int>(settings.bars.size())) { commit(); std::swap(settings.bars[selected],settings.bars[next]); selected=next; refreshList(); editor(); changed(); } break; }
        case Save: preset(true); break;
        case Load: preset(false); break;
        case Example: if(discard()) { settings=pulse::Settings{}; settings.sequence=true; selected=0; populate(); changed(); status(L"Try it: two bars each of 4/4, 7/8 (2+2+3), and 3/4. The sequence loops."); } break;
    }
}
void visibility() {
    for(const auto& c:controls) {
        int id=GetDlgCtrlID(c.hwnd);
        bool widget=id==FullEditor||id==Opacity||id==PositionLock||id==CloseWidget||id==MinimizeWidget;
        bool shared=id==Start||id==Bpm||id==Minus||id==Plus||id==Tap||id==NextStep;
        bool tiny=id==Bpm||id==Start||id==Meter||id==PositionLock||id==MinimizeWidget||id==CycleView||id==CloseWidget;
        bool visible=viewMode==0?tiny:overlayMode?(widget||shared||id==CycleView):(!widget&&id!=Meter);
        ShowWindow(c.hwnd,visible?SW_SHOW:SW_HIDE);
    }
    if(overlayMode) {
        EnableWindow(control(PositionLock),TRUE);
        RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN); return;
    }
    const std::vector<int> rhythm{Beats,Unit,Groups,Apply};
    const std::vector<int> sequence{Steps,Repeats,StepTempo,Forever,Add,Remove,Up,Down};
    const std::vector<int> practice{Subdivision,Poly,Mute,Volume,Sound,Accents,VolumeSlider};
    for(int id:rhythm) ShowWindow(control(id),toolsPage?SW_HIDE:SW_SHOW);
    for(int id:sequence) ShowWindow(control(id),!toolsPage&&settings.sequence?SW_SHOW:SW_HIDE);
    for(int id:practice) ShowWindow(control(id),toolsPage?SW_SHOW:SW_HIDE);
    ShowWindow(control(Example),!toolsPage&&!settings.sequence?SW_SHOW:SW_HIDE);
    RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
void arrange() {
    SendMessageW(control(Bpm),WM_SETFONT,reinterpret_cast<WPARAM>(viewMode==0?beatFont:numberFont),TRUE);
    if(overlayMode) {
        auto move=[](int id,int x,int y,int w,int h) { SetWindowPos(control(id),nullptr,px(x),px(y),px(w),px(h),SWP_NOZORDER|SWP_NOACTIVATE); };
        if(viewMode==0) {
            move(Bpm,30,6,58,32); move(Meter,94,8,70,240); move(Start,170,8,54,28);
            move(PositionLock,230,8,48,28); move(MinimizeWidget,284,8,28,28);
            move(CycleView,318,8,50,28); move(CloseWidget,374,8,28,28);
        } else {
            move(FullEditor,112,10,54,28); move(CycleView,170,10,44,28); move(Opacity,218,10,52,28);
            move(PositionLock,274,10,52,28); move(MinimizeWidget,330,10,24,28); move(CloseWidget,358,10,26,28);
            move(Bpm,16,64,140,58); move(Minus,164,72,32,36); move(Plus,202,72,32,36);
            move(Tap,244,82,60,32); move(Start,312,78,72,40); move(NextStep,274,180,110,30);
        }
        RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_FRAME); return;
    }
    // No scrolling canvas or pixel copying. Reposition controls only on resize/DPI changes.
    HDWP batch=BeginDeferWindowPos(static_cast<int>(controls.size()));
    constexpr UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOREDRAW;
    for(const auto& c:controls) {
        if(!batch) break;
        batch=DeferWindowPos(batch,c.hwnd,nullptr,px(c.x),px(c.y),px(c.w),px(c.h),flags);
    }
    if(!batch||!EndDeferWindowPos(batch))
        for(const auto& c:controls) SetWindowPos(c.hwnd,nullptr,px(c.x),px(c.y),px(c.w),px(c.h),flags);
    RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_FRAME);
}
void setView(int next) {
    if(next==viewMode) return;
    bool enabled=next!=2;
    // Commit a partially typed BPM before moving focus or hiding the editor.
    acceptTempo(true);
    if(enabled && !overlayMode) {
        fullPlacement.length=sizeof(fullPlacement); GetWindowPlacement(window,&fullPlacement);
        if(fullPlacement.showCmd==SW_HIDE) fullPlacement.showCmd=SW_SHOWNORMAL;
        if(IsZoomed(window)||IsIconic(window)) ShowWindow(window,SW_RESTORE);
    }
    if(overlayMode && !IsIconic(window)) GetWindowRect(window,&widgetRect);
    overlayMode=enabled; viewMode=next;
    DWORD style=enabled?WS_POPUP|WS_BORDER|WS_CLIPCHILDREN:WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN;
    DWORD ex=WS_EX_CONTROLPARENT|(enabled?WS_EX_LAYERED:WS_EX_COMPOSITED);
    SetWindowLongPtrW(window,GWL_STYLE,style|WS_VISIBLE);
    SetWindowLongPtrW(window,GWL_EXSTYLE,ex);
    if(enabled) {
        SetLayeredWindowAttributes(window,0,overlayAlphas[opacityIndex],LWA_ALPHA);
        RECT r{0,0,px(overlayWidth()),px(overlayHeight())};
        AdjustWindowRectExForDpi(&r,style,FALSE,ex,GetDpiForWindow(window));
        RECT work{}; MONITORINFO mi{}; mi.cbSize=sizeof(mi);
        if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&mi)) work=mi.rcWork;
        else SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
        int w=r.right-r.left,h=r.bottom-r.top;
        LONG x=widgetRect.right?widgetRect.left:work.right-w-px(24);
        LONG y=widgetRect.bottom?widgetRect.top:work.top+px(24);
        x=std::max(work.left,std::min(x,work.right-w)); y=std::max(work.top,std::min(y,work.bottom-h));
        SetWindowPos(window,HWND_TOPMOST,x,y,w,h,SWP_FRAMECHANGED|SWP_NOACTIVATE);
    } else {
        SetWindowPos(window,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED|SWP_NOACTIVATE);
        SetWindowPlacement(window,&fullPlacement);
    }
    text(Tap,enabled?L"Tap":L"Tap tempo");
    text(Start,playing?(enabled?L"Stop":L"Stop   [Space]"):(enabled?L"Start":L"Start   [Space]"));
    arrange(); syncMeter(); visibility(); SetFocus(control(Start));
}
LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    switch(message) {
        case WM_CREATE: {
            window=hwnd; scale=GetDpiForWindow(hwnd)/96.f; bgBrush=CreateSolidBrush(BG); panelBrush=CreateSolidBrush(PANEL); setFonts();
            BOOL dark=FALSE; DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));
            button(L"Widget",Overlay,322,236,100,34);
            button(L"Size",CycleView,430,236,80,34);
            button(L"−",MinimizeWidget,0,0,28,28);
            std::vector<std::wstring> meters;
            for(int beats=1;beats<=32;++beats) for(int unit:{2,4,8,16}) meters.push_back(std::to_wstring(beats)+L"/"+std::to_wstring(unit));
            combo(Meter,0,0,70,meters,13);
            button(L"Editor",FullEditor,0,0,64,28);
            button(L"100%",Opacity,0,0,58,28);
            button(L"Lock",PositionLock,0,0,52,28);
            button(L"×",CloseWidget,0,0,32,28);
            button(L"Open…",Load,652,18,102,34); button(L"Save…",Save,766,18,110,34);
            edit(L"120",Bpm,44,104,140);
            controls.back().h=60; SetWindowPos(control(Bpm),nullptr,px(44),px(104),px(140),px(60),SWP_NOZORDER);
            SendMessageW(control(Bpm),WM_SETFONT,reinterpret_cast<WPARAM>(numberFont),TRUE);
            button(L"−",Minus,202,104,40,36); button(L"+",Plus,250,104,40,36);
            button(L"Tap tempo",Tap,202,154,88,34);
            button(L"Start   [Space]",Start,680,99,172,46); button(L"Next step",NextStep,680,157,172,34);
            button(L"Rhythm",RhythmTab,24,236,124,34); button(L"Practice tools",ToolsTab,158,236,156,34);
            combo(Mode,530,237,346,{L"Metronome — one repeating meter",L"Sequence — combine meters"},0);
            make(L"LISTBOX",L"",Steps,44,332,374,128,LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|WS_VSCROLL|WS_BORDER);
            SendMessageW(control(Steps),LB_SETITEMHEIGHT,0,px(38));
            button(L"Add step",Add,44,476,94,32); button(L"Remove",Remove,146,476,88,32);
            button(L"↑",Up,246,476,78,32); button(L"↓",Down,334,476,84,32);
            button(L"Try a mixed-meter sequence",Example,44,466,374,36);
            edit(L"4",Beats,444,330,78); combo(Unit,538,330,94,{L"/ 2",L"/ 4",L"/ 8",L"/ 16"},1); edit(L"4",Groups,650,330,202);
            edit(L"1",Repeats,444,426,90);
            make(L"BUTTON",L"Forever",Forever,548,426,128,32,BS_AUTOCHECKBOX);
            edit(L"0",StepTempo,697,426,155);
            button(L"Apply rhythm",Apply,444,476,408,32);
            combo(Subdivision,44,335,236,{L"1 — Beat",L"2 — Halves",L"3 — Triplets",L"4 — Quarters"},0);
            edit(L"0",Poly,320,335,236); edit(L"0",Mute,596,335,256);
            combo(Sound,44,426,236,{L"Clean tone",L"Woodblock",L"Bright click",L"Full click — longer"},0); edit(L"70",Volume,320,426,64);
            make(TRACKBAR_CLASSW,L"Metronome volume, 0 to 400 percent",VolumeSlider,392,426,164,32,TBS_HORZ|TBS_NOTICKS);
            SendMessageW(control(VolumeSlider),TBM_SETRANGE,TRUE,MAKELPARAM(0,400));
            SendMessageW(control(VolumeSlider),TBM_SETPAGESIZE,0,10);
            make(L"BUTTON",L"Accent beats",Accents,596,426,256,32,BS_AUTOCHECKBOX);
            make(TRACKBAR_CLASSW,L"Windows volume",SystemVolumeSlider,360,30,270,26,TBS_HORZ|TBS_NOTICKS);
            SendMessageW(control(SystemVolumeSlider),TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
            SendMessageW(control(SystemVolumeSlider),TBM_SETPAGESIZE,0,5);
            syncSystemVolume(); SetTimer(hwnd,VolumeSyncTimer,500,nullptr);
            populate(); dirty=false; title(); return 0;
        }
        case WM_NCHITTEST:
            if(overlayMode) {
                POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ScreenToClient(hwnd,&point);
                if(!positionLocked && point.x>=0&&point.x<px(viewMode==0?26:108)&&point.y>=0&&point.y<px(44)) return HTCAPTION;
            } break;
        case WM_SYSCOMMAND:
            if(overlayMode && positionLocked && (wp&0xfff0)==SC_MOVE) return 0;
            break;
        case WM_HSCROLL:
            if(reinterpret_cast<HWND>(lp)==control(VolumeSlider)) {
                settings.volume=static_cast<int>(SendMessageW(control(VolumeSlider),TBM_GETPOS,0,0));
                bool previous=populating; populating=true; text(Volume,std::to_wstring(settings.volume)); populating=previous;
                acceptVolume(); changed(); return 0;
            }
            if(reinterpret_cast<HWND>(lp)==control(SystemVolumeSlider)) {
                int level=static_cast<int>(SendMessageW(control(SystemVolumeSlider),TBM_GETPOS,0,0));
                if(FAILED(SystemVolume::set(level/100.f))) status(L"Could not change Windows volume. Check your output device.");
                syncSystemVolume(); return 0;
            } break;
        case WM_TIMER: if(wp==VolumeEditTimer) { acceptVolume(); return 0; } if(wp==VolumeSyncTimer) { syncSystemVolume(); return 0; } if(wp==TempoEditTimer) { acceptTempo(); return 0; } break;
        case WM_COMMAND: try { command(LOWORD(wp),HIWORD(wp)); } catch(const std::exception& e) { error(e); } return 0;
        case WM_PULSE_BEAT: if(playing) { currentBeat=LOWORD(wp); currentStep=HIWORD(wp); currentRepeat=LOWORD(lp); currentSilent=HIWORD(lp)!=0; syncMeter(); InvalidateRect(hwnd,nullptr,FALSE); } return 0;
        case WM_PULSE_ERROR: if(playing) { stop(); wchar_t messageText[256]; swprintf(messageText,256,L"Could not play audio (0x%08lX). Check your default Windows output device, then press Start to reconnect.",static_cast<unsigned long>(wp)); status(messageText); MessageBoxW(hwnd,messageText,L"Audio unavailable",MB_OK|MB_ICONWARNING); } return 0;
        case WM_DRAWITEM: drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp)); return TRUE;
        case WM_MEASUREITEM: reinterpret_cast<MEASUREITEMSTRUCT*>(lp)->itemHeight=px(38); return TRUE;
        case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORSTATIC: { HDC dc=reinterpret_cast<HDC>(wp); SetBkColor(dc,PANEL); SetTextColor(dc,INK); return reinterpret_cast<LRESULT>(panelBrush); }
        case WM_PRINTCLIENT: paint(reinterpret_cast<HDC>(wp)); SetViewportOrgEx(reinterpret_cast<HDC>(wp),0,0,nullptr); return 0;
        case WM_ERASEBKGND: { RECT r; GetClientRect(hwnd,&r); FillRect(reinterpret_cast<HDC>(wp),&r,bgBrush); return 1; }
        case WM_SIZE: if(window) arrange(); return 0;



        case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); RECT r; GetClientRect(hwnd,&r); HDC buffer=CreateCompatibleDC(dc); HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom); auto old=SelectObject(buffer,bitmap); paint(buffer); SetViewportOrgEx(buffer,0,0,nullptr); BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY); SelectObject(buffer,old); DeleteObject(bitmap); DeleteDC(buffer); EndPaint(hwnd,&ps); return 0; }
        case WM_GETMINMAXINFO: { auto mm=reinterpret_cast<MINMAXINFO*>(lp); RECT r{0,0,px(overlayMode?overlayWidth():900),px(overlayMode?overlayHeight():610)}; AdjustWindowRectExForDpi(&r,overlayMode?WS_POPUP|WS_BORDER:WS_OVERLAPPEDWINDOW,FALSE,overlayMode?WS_EX_LAYERED:0,static_cast<UINT>(96*scale)); mm->ptMinTrackSize={r.right-r.left,r.bottom-r.top}; return 0; }
        case WM_DPICHANGED: { scale=HIWORD(wp)/96.f; setFonts(); SendMessageW(control(Steps),LB_SETITEMHEIGHT,0,px(38)); auto r=reinterpret_cast<RECT*>(lp); SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); arrange(); InvalidateRect(hwnd,nullptr,TRUE); return 0; }
        case WM_CLOSE: stop(); DestroyWindow(hwnd); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    struct ComApartment { HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); ~ComApartment(){if(SUCCEEDED(hr)) CoUninitialize();} } apartment;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES}; InitCommonControlsEx(&common);
    WNDCLASSEXW cls{}; cls.cbSize=sizeof(cls); cls.lpfnWndProc=proc; cls.hInstance=instance; cls.hCursor=LoadCursorW(nullptr,IDC_ARROW); cls.hIcon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_DEFAULTCOLOR)); cls.hIconSm=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_DEFAULTCOLOR)); cls.lpszClassName=L"PulseMetronome"; RegisterClassExW(&cls);
    UINT dpi=GetDpiForSystem(); scale=dpi/96.f;
    RECT r{0,0,px(900),px(610)}; AdjustWindowRectExForDpi(&r,WS_OVERLAPPEDWINDOW,FALSE,0,dpi);
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    auto hwnd=CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_COMPOSITED,cls.lpszClassName,L"Pulse",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,std::min(r.right-r.left,work.right-work.left),std::min(r.bottom-r.top,work.bottom-work.top),nullptr,nullptr,instance,nullptr);
    if(!hwnd) return 1;
    setView(0); ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG msg;
    while(GetMessageW(&msg,nullptr,0,0)>0) {
        bool handled=false;
        if(msg.message==WM_KEYDOWN && !(msg.lParam&(1LL<<30))) {
            wchar_t name[32]{}; GetClassNameW(GetFocus(),name,32); bool editing=wcscmp(name,L"Edit")==0;
            try {
                if(GetKeyState(VK_CONTROL)&0x8000) { if(editing&&msg.wParam=='A') { SendMessageW(GetFocus(),EM_SETSEL,0,-1); handled=true; } else if(!playing&&msg.wParam=='S') { preset(true); handled=true; } else if(!playing&&msg.wParam=='O') { preset(false); handled=true; } }
                else if(editing&&GetDlgCtrlID(GetFocus())==Volume&&msg.wParam==VK_RETURN) { acceptVolume(true); handled=true; }
                else if(editing&&GetDlgCtrlID(GetFocus())==Bpm&&msg.wParam==VK_RETURN) { acceptTempo(true); handled=true; }
                else if(!editing&&msg.wParam==VK_SPACE) { toggle(); handled=true; }
                else if(!editing&&msg.wParam=='T') { tap(); handled=true; }
            } catch(const std::exception& e) { error(e); handled=true; }
        }
        if(!handled&&!IsDialogMessageW(hwnd,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    }
    audio.stop(); for(HFONT f:{font,smallFont,titleFont,numberFont,beatFont}) DeleteObject(f); DeleteObject(bgBrush); DeleteObject(panelBrush); return 0;
}
