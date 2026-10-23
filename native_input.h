#pragma once
#include "pet_battle_input.h"

// Raw Input is posted asynchronously. The OS never waits for the renderer to
// call a WH_MOUSE_LL / WH_KEYBOARD_LL callback. Legacy pet drag messages remain
// enabled; this receiver only observes button/key flags, never typed text.
struct NativeDesktopInput : BattleDesktopInput {
    HWND receiver{};bool enabled=false;
    static LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
        auto* self=reinterpret_cast<NativeDesktopInput*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){self=static_cast<NativeDesktopInput*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(message==WM_INPUT&&self){
            RAWINPUT raw{};UINT size=sizeof(raw);
            if(GetRawInputData(reinterpret_cast<HRAWINPUT>(l),RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER))!=UINT(-1)){
                if(raw.header.dwType==RIM_TYPEKEYBOARD){
                    const auto& key=raw.data.keyboard;UINT code=key.VKey;
                    if(code==VK_SHIFT)code=MapVirtualKeyW(key.MakeCode,MAPVK_VSC_TO_VK_EX);
                    else if(code==VK_CONTROL)code=key.Flags&RI_KEY_E0?VK_RCONTROL:VK_LCONTROL;
                    else if(code==VK_MENU)code=key.Flags&RI_KEY_E0?VK_RMENU:VK_LMENU;
                    if(code<256&&code!=255){
                        const bool down=(key.Flags&RI_KEY_BREAK)==0;self->heldKeys[code]=down;
                        if(down){self->keyPressed=true;self->lastKeyboard=GetTickCount64();
                            const bool shift=self->heldKeys[VK_LSHIFT]||self->heldKeys[VK_RSHIFT]||(GetAsyncKeyState(VK_SHIFT)&0x8000);
                            self->reload|=code==VK_RETURN||code==VK_OEM_PERIOD||code==VK_DECIMAL||code==VK_OEM_1||(code==VK_OEM_2&&shift);
                        }
                    }
                }else if(raw.header.dwType==RIM_TYPEMOUSE)
                    self->mouseClick|=(raw.data.mouse.usButtonFlags&(RI_MOUSE_LEFT_BUTTON_UP|RI_MOUSE_RIGHT_BUTTON_UP))!=0;
            }
            // DefWindowProc performs foreground WM_INPUT cleanup.
            return DefWindowProcW(hwnd,message,w,l);
        }
        return DefWindowProcW(hwnd,message,w,l);
    }
    void enable(bool value){
        if(!value){
            if(enabled){RAWINPUTDEVICE devices[]{{1,2,RIDEV_REMOVE,nullptr},{1,6,RIDEV_REMOVE,nullptr}};RegisterRawInputDevices(devices,2,sizeof(RAWINPUTDEVICE));}
            enabled=false;for(auto& key:heldKeys)key=false;lastKeyboard=nextHeldRepeat=0;if(windows)UnhookWinEvent(windows);windows=nullptr;
            if(instance==this)instance=nullptr;applicationWindows.clear();clear();
            if(receiver){DestroyWindow(receiver);receiver=nullptr;}return;
        }
        if(enabled)return;
        WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BaCafeNativeRawInput";RegisterClassW(&wc);
        receiver=CreateWindowExW(0,wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,this);
        if(!receiver)throw std::runtime_error("Cannot create asynchronous input receiver");
        RAWINPUTDEVICE devices[]{{1,2,RIDEV_INPUTSINK,receiver},{1,6,RIDEV_INPUTSINK,receiver}};
        if(!RegisterRawInputDevices(devices,2,sizeof(RAWINPUTDEVICE))){DestroyWindow(receiver);receiver=nullptr;throw std::runtime_error("Cannot register Raw Input");}
        enabled=true;instance=this;
        // Window visibility events are asynchronous; no system input hooks.
        windows=SetWinEventHook(EVENT_OBJECT_DESTROY,EVENT_OBJECT_SHOW,nullptr,windowProc,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        EnumWindows([](HWND hwnd,LPARAM data)->BOOL{if(application(hwnd))reinterpret_cast<NativeDesktopInput*>(data)->applicationWindows.insert(hwnd);return TRUE;},reinterpret_cast<LPARAM>(this));
        TraceLog(LOG_INFO,"NATIVE input: asynchronous Raw Input; low-level keyboard/mouse hooks=0");
    }
    ~NativeDesktopInput(){enable(false);}
};
