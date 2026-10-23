#pragma once
#include <windows.h>
#include <shellapi.h>
#include <functional>
#include <stdexcept>
#include <string>

class NativeTray {
    NOTIFYICONDATAW data{};HICON icon{};bool added=false;int displayed=-1;
    UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    std::function<void()> open,menu,exit;
    bool add(){
        if((added&&Shell_NotifyIconW(NIM_MODIFY,&data))||Shell_NotifyIconW(NIM_ADD,&data)){
            added=true;data.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&data);KillTimer(window,1);return true;
        }
        added=false;SetTimer(window,1,5000,nullptr);return false;
    }
    static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
        auto* self=reinterpret_cast<NativeTray*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(msg==WM_NCCREATE){self=static_cast<NativeTray*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(!self)return DefWindowProcW(hwnd,msg,w,l);
        if(msg==self->taskbarCreated||msg==WM_TIMER){self->add();return 0;}
        try{
            if(msg==Callback){const auto event=LOWORD(l);if(event==NIN_SELECT||event==NIN_KEYSELECT||event==WM_LBUTTONUP)self->open();else if(event==WM_CONTEXTMENU||event==WM_RBUTTONUP)self->menu();return 0;}
            if(msg==WM_CLOSE){self->exit();return 0;}
        }catch(const std::exception& error){MessageBoxA(hwnd,error.what(),"BA Cafe Pets",MB_OK|MB_ICONERROR);}
        return DefWindowProcW(hwnd,msg,w,l);
    }
public:
    static constexpr UINT Callback=WM_APP+61;
    HWND window{};
    NativeTray(std::function<void()> onOpen,std::function<void()> onMenu,std::function<void()> onExit):open(std::move(onOpen)),menu(std::move(onMenu)),exit(std::move(onExit)){
        const auto module=GetModuleHandleW(nullptr);WNDCLASSW wc{};wc.hInstance=module;wc.lpszClassName=L"BaCafeNativeTray";wc.lpfnWndProc=proc;wc.hIcon=LoadIconW(module,MAKEINTRESOURCEW(101));RegisterClassW(&wc);
        // Hidden top-level windows receive Explorer's TaskbarCreated broadcast.
        window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"BA Cafe Pets",0,0,0,0,0,nullptr,nullptr,module,this);
        if(!window)throw std::runtime_error("Cannot create notification area window");
        icon=static_cast<HICON>(LoadImageW(module,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0));if(!icon)icon=CopyIcon(LoadIconW(nullptr,IDI_APPLICATION));
        data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;data.uCallbackMessage=Callback;data.hIcon=icon;
        lstrcpynW(data.szTip,L"BA Cafe Pets · 单击打开，右键切换模型",128);add();
    }
    ~NativeTray(){if(added)Shell_NotifyIconW(NIM_DELETE,&data);if(window){KillTimer(window,1);DestroyWindow(window);}if(icon)DestroyIcon(icon);}
    NativeTray(const NativeTray&)=delete;NativeTray& operator=(const NativeTray&)=delete;
    bool available() const {return added;}
    void update(int count){if(displayed==count)return;displayed=count;const auto text=L"BA Cafe Pets · "+std::to_wstring(count)+L" 个模型已显示\n单击打开，右键切换模型";lstrcpynW(data.szTip,text.c_str(),128);if(added&&!Shell_NotifyIconW(NIM_MODIFY,&data)){added=false;SetTimer(window,1,5000,nullptr);}}
};
