#pragma once
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

// The Windows SDK's RPC headers define `small` as a macro under MSVC.
#ifdef small
#undef small
#endif

// Shared Fluent styling for real Win32 controls. DWM owns the window frame,
// shadow and rounded corners; unsupported material attributes simply fall back.
struct NativeFluent {
    UINT dpi=96;
    bool dark=false,highContrast=false;
    COLORREF canvas{},surface{},hover{},pressed{},text{},muted{},line{},accent{},accentText{},selection{},danger{},disabled{};
    HBRUSH canvasBrush{},surfaceBrush{};
    HFONT font{},small{},heading{},title{},icons{};
    int px(int n) const {return MulDiv(n,dpi,96);}
    int dip(int n) const {return MulDiv(n,96,dpi);}
    ~NativeFluent(){clear();}
    void clear(){
        for(auto f:{font,small,heading,title,icons})if(f)DeleteObject(f);
        font=small=heading=title=icons=nullptr;
        if(canvasBrush)DeleteObject(canvasBrush);if(surfaceBrush)DeleteObject(surfaceBrush);
        canvasBrush=surfaceBrush=nullptr;
    }
    void load(UINT scale,int appearance=-1){
        clear();dpi=scale?scale:96;
        DWORD light=1,bytes=sizeof(light);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&bytes);
        HIGHCONTRASTW contrast{sizeof(contrast)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0);
        highContrast=(contrast.dwFlags&HCF_HIGHCONTRASTON)!=0;dark=appearance<0?!light:appearance!=0;
        canvas=dark?RGB(32,32,32):RGB(243,243,243);surface=dark?RGB(43,43,43):RGB(255,255,255);
        hover=dark?RGB(55,55,55):RGB(246,246,246);pressed=dark?RGB(48,48,48):RGB(239,239,239);
        text=dark?RGB(245,245,245):RGB(26,26,26);muted=dark?RGB(190,190,190):RGB(96,96,96);
        line=dark?RGB(62,62,62):RGB(229,229,229);accent=dark?RGB(96,205,255):RGB(0,95,184);
        accentText=dark?RGB(0,45,64):RGB(255,255,255);selection=dark?RGB(50,58,62):RGB(234,242,250);
        danger=dark?RGB(255,153,164):RGB(196,43,28);disabled=dark?RGB(133,133,133):RGB(155,155,155);
        if(highContrast){
            canvas=surface=GetSysColor(COLOR_WINDOW);text=muted=GetSysColor(COLOR_WINDOWTEXT);
            hover=selection=GetSysColor(COLOR_HIGHLIGHT);pressed=hover;accent=GetSysColor(COLOR_HIGHLIGHT);
            accentText=GetSysColor(COLOR_HIGHLIGHTTEXT);line=text;disabled=GetSysColor(COLOR_GRAYTEXT);danger=text;
        }
        canvasBrush=CreateSolidBrush(canvas);surfaceBrush=CreateSolidBrush(surface);
        const auto make=[&](int size,int weight,const wchar_t* face){return CreateFontW(-px(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,face);};
        NONCLIENTMETRICSW metrics{sizeof(metrics)};const wchar_t* face=SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0)?metrics.lfMessageFont.lfFaceName:L"Segoe UI";
        font=make(14,FW_NORMAL,face);small=make(12,FW_NORMAL,face);heading=make(14,FW_SEMIBOLD,face);title=make(28,FW_SEMIBOLD,face);icons=make(18,FW_NORMAL,L"Segoe Fluent Icons");
    }
    void frame(HWND hwnd,bool flyout=false) const {
        const BOOL immersive=dark;const DWORD corners=2,material=flyout?3:2;
        DwmSetWindowAttribute(hwnd,20,&immersive,sizeof(immersive));
        DwmSetWindowAttribute(hwnd,33,&corners,sizeof(corners));
        DwmSetWindowAttribute(hwnd,38,&material,sizeof(material));
        // Flyouts paint their own border. A second DWM/glass edge can become
        // white during activation or when newer frame attributes are ignored.
        const COLORREF border=flyout?0xFFFFFFFEu:highContrast?text:line;DwmSetWindowAttribute(hwnd,34,&border,sizeof(border));
        if(flyout){MARGINS margins{};DwmExtendFrameIntoClientArea(hwnd,&margins);}
    }
    void themed(HWND control) const {SetWindowTheme(control,dark?L"DarkMode_Explorer":L"Explorer",nullptr);}
    static void fill(HDC dc,RECT r,COLORREF color){auto b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);}
    static void round(HDC dc,RECT r,COLORREF color,COLORREF border,int radius){
        auto b=CreateSolidBrush(color);auto p=CreatePen(PS_SOLID,1,border);auto oldB=SelectObject(dc,b),oldP=SelectObject(dc,p);
        RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,oldB);SelectObject(dc,oldP);DeleteObject(b);DeleteObject(p);
    }
    void label(HDC dc,std::wstring value,RECT r,HFONT face,COLORREF color,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS) const {
        auto old=SelectObject(dc,face);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
        DrawTextW(dc,value.c_str(),int(value.size()),&r,flags|DT_NOPREFIX);SelectObject(dc,old);
    }
    void glyph(HDC dc,const wchar_t* value,RECT r,COLORREF color) const {label(dc,value,r,icons,color,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    void toggle(HDC dc,RECT r,bool checked,bool enabled) const {
        const auto color=enabled?(checked?accent:muted):disabled;
        round(dc,r,checked?color:surface,color,px(20));
        const int size=px(12),inset=px(4),x=checked?r.right-inset-size:r.left+inset;
        RECT knob{x,r.top+(r.bottom-r.top-size)/2,x+size,r.top+(r.bottom-r.top+size)/2};
        round(dc,knob,checked?accentText:color,checked?accentText:color,size);
    }
    void check(HDC dc,RECT r,bool checked,bool enabled) const {
        const auto color=enabled?(checked?accent:muted):disabled;
        round(dc,r,checked?color:surface,color,px(4));
        if(checked)glyph(dc,L"\xE73E",r,accentText);
    }
    static bool focusVisible(HWND hwnd,bool focused){return focused&&(SendMessageW(GetAncestor(hwnd,GA_ROOT),WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)==0;}
    void focus(HDC dc,RECT r,HWND hwnd,bool focused) const {
        if(!focusVisible(hwnd,focused))return;InflateRect(&r,-px(3),-px(3));
        auto pen=CreatePen(PS_SOLID,std::max(1,px(2)),accent);auto oldPen=SelectObject(dc,pen),oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
        RoundRect(dc,r.left,r.top,r.right,r.bottom,px(6),px(6));SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);
    }
    static bool vectorDrawing(){
        struct Runtime {ULONG_PTR token{};Runtime(){Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);}~Runtime(){if(token)Gdiplus::GdiplusShutdown(token);}};
        static Runtime runtime;return runtime.token!=0;
    }
    static Gdiplus::Color vectorColor(COLORREF color,BYTE alpha=255){return {alpha,GetRValue(color),GetGValue(color),GetBValue(color)};}
    static void capsule(Gdiplus::Graphics& g,float x,float y,float width,float height,COLORREF color){
        if(width<=0||height<=0)return;Gdiplus::SolidBrush brush(vectorColor(color));
        const float diameter=std::min(width,height);Gdiplus::GraphicsPath path;
        path.AddArc(x,y,diameter,diameter,90,180);path.AddArc(x+width-diameter,y,diameter,diameter,270,180);path.CloseFigure();g.FillPath(&brush,&path);
    }
    void slider(HDC dc,RECT r,RECT track,int center,bool enabled,bool hot,bool pressed) const {
        const float scale=dpi/96.0f,cy=(r.top+r.bottom)*0.5f,radius=10*scale;
        const float cx=std::clamp(float(center),radius+1,float(r.right)-radius-1);
        const float left=std::max(float(track.left),radius+1),right=std::min(float(track.right),float(r.right)-radius-1);
        if(vectorDrawing()){
            Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            capsule(g,left,cy-1.5f*scale,right-left,3*scale,enabled?muted:line);
            capsule(g,left,cy-1.5f*scale,cx-left,3*scale,enabled?accent:disabled);
            if(!highContrast){Gdiplus::SolidBrush shadow(vectorColor(RGB(0,0,0),dark?48:25));g.FillEllipse(&shadow,cx-radius,cy-radius+scale,2*radius,2*radius);}
            Gdiplus::SolidBrush shell(vectorColor(highContrast?surface:dark?RGB(69,69,69):RGB(255,255,255)));
            Gdiplus::Pen edge(vectorColor(highContrast?text:dark?RGB(152,152,152):RGB(132,132,132)),scale);
            g.FillEllipse(&shell,cx-radius,cy-radius,2*radius,2*radius);g.DrawEllipse(&edge,cx-radius,cy-radius,2*radius,2*radius);
            const float inner=(pressed?4:hot?6:5)*scale;Gdiplus::SolidBrush knob(vectorColor(enabled?accent:disabled));g.FillEllipse(&knob,cx-inner,cy-inner,2*inner,2*inner);
            g.Flush(Gdiplus::FlushIntentionSync);
        }else{
            RECT rail{int(left),int(cy-px(2)),int(right),int(cy+px(2))};round(dc,rail,muted,muted,px(4));rail.right=int(cx);round(dc,rail,accent,accent,px(4));
            RECT shell{int(cx-radius),int(cy-radius),int(cx+radius),int(cy+radius)};round(dc,shell,surface,muted,px(20));RECT knob{int(cx-px(5)),int(cy-px(5)),int(cx+px(5)),int(cy+px(5))};round(dc,knob,enabled?accent:disabled,enabled?accent:disabled,px(10));
        }
    }
    void inputFrame(HDC dc,RECT r,bool focused,bool invalid,bool enabled) const {
        const auto edge=invalid?danger:focused?accent:enabled?muted:line;
        round(dc,r,surface,enabled?line:disabled,px(8));
        RECT bottom{r.left+px(3),r.bottom-px(focused||invalid?2:1),r.right-px(3),r.bottom};fill(dc,bottom,edge);
    }
    // Subclass only hover/focus invalidation. Native controls retain keyboard,
    // accessibility, capture and command behavior.
    static LRESULT CALLBACK controlProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
        auto* theme=reinterpret_cast<NativeFluent*>(data);wchar_t cls[32]{};GetClassNameW(hwnd,cls,32);
        if(msg==WM_LBUTTONDOWN)SendMessageW(GetAncestor(hwnd,GA_ROOT),WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);
        if(msg==WM_KEYDOWN&&(w==VK_TAB||w==VK_LEFT||w==VK_RIGHT||w==VK_UP||w==VK_DOWN))SendMessageW(GetAncestor(hwnd,GA_ROOT),WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
        const bool combo=std::wcscmp(cls,L"ComboBox")==0,isSlider=std::wcscmp(cls,TRACKBAR_CLASSW)==0;
        if(theme&&combo&&msg==WM_NCPAINT)return 0;
        if(theme&&(combo||isSlider)&&msg==WM_ERASEBKGND){RECT r{};GetClientRect(hwnd,&r);fill(reinterpret_cast<HDC>(w),r,theme->surface);return 1;}
        if(theme&&(combo||isSlider)&&(msg==WM_PAINT||msg==WM_PRINTCLIENT||msg==WM_PRINT)){
            if(msg==WM_PRINT&&((l&PRF_CHECKVISIBLE)&&!IsWindowVisible(hwnd)))return 0;
            if(msg==WM_PRINT&&!(l&PRF_CLIENT))return 0;
            PAINTSTRUCT ps{};const bool printing=msg!=WM_PAINT;const auto dc=printing?reinterpret_cast<HDC>(w):BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);fill(dc,r,theme->surface);
            const bool enabled=IsWindowEnabled(hwnd),hot=GetPropW(hwnd,L"FluentHover")!=nullptr;
            if(combo){
                round(dc,r,hot?theme->hover:theme->surface,theme->line,theme->px(6));
                const int index=int(SendMessageW(hwnd,CB_GETCURSEL,0,0)),length=index<0?0:int(SendMessageW(hwnd,CB_GETLBTEXTLEN,index,0));
                std::wstring text(std::max(0,length)+1,L'\0');if(index>=0)SendMessageW(hwnd,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(text.data()));text.resize(std::max(0,length));
                RECT labelRect{theme->px(12),0,r.right-theme->px(30),r.bottom};theme->label(dc,text,labelRect,theme->font,enabled?theme->text:theme->disabled);
                RECT arrow{r.right-theme->px(28),0,r.right-theme->px(8),r.bottom};theme->glyph(dc,L"\xE70D",arrow,enabled?theme->muted:theme->disabled);
                if(GetFocus()==hwnd){RECT line{theme->px(3),r.bottom-theme->px(2),r.right-theme->px(3),r.bottom};fill(dc,line,theme->accent);}
            }else{
                RECT thumb{},track{};SendMessageW(hwnd,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));SendMessageW(hwnd,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&track));
                theme->slider(dc,r,track,(thumb.left+thumb.right)/2,enabled,hot,GetPropW(hwnd,L"FluentPressed")!=nullptr);
                theme->focus(dc,r,hwnd,GetFocus()==hwnd);
            }
            if(!printing)EndPaint(hwnd,&ps);return 0;
        }
        if(isSlider&&(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP||msg==WM_CAPTURECHANGED||msg==WM_CANCELMODE)){
            if(msg==WM_LBUTTONDOWN)SetPropW(hwnd,L"FluentPressed",reinterpret_cast<HANDLE>(1));else RemovePropW(hwnd,L"FluentPressed");InvalidateRect(hwnd,nullptr,FALSE);
        }
        if(msg==WM_MOUSEMOVE&&!GetPropW(hwnd,L"FluentHover")){
            SetPropW(hwnd,L"FluentHover",reinterpret_cast<HANDLE>(1));TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd,0};TrackMouseEvent(&track);InvalidateRect(hwnd,nullptr,FALSE);
        }
        if(msg==WM_MOUSELEAVE){RemovePropW(hwnd,L"FluentHover");InvalidateRect(hwnd,nullptr,FALSE);}
        if(msg==WM_SETFOCUS||msg==WM_KILLFOCUS||msg==WM_ENABLE)InvalidateRect(hwnd,nullptr,FALSE);
        if(msg==WM_NCDESTROY){RemovePropW(hwnd,L"FluentHover");RemovePropW(hwnd,L"FluentPressed");RemoveWindowSubclass(hwnd,controlProc,1);}
        return DefSubclassProc(hwnd,msg,w,l);
    }
};
