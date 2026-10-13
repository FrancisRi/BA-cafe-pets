#pragma once
#include <windows.h>
#include <wtsapi32.h>
#include <cstring>

// Rendering a layered OpenGL surface is unnecessary on an unavailable desktop.
struct NativeSession {
    bool asleep=false,locked=false,displayOff=false;
    HPOWERNOTIFY power{};HWND window{};
    bool paused()const{return asleep||locked||displayOff;}
    void attach(HWND hwnd){window=hwnd;WTSRegisterSessionNotification(hwnd,NOTIFY_FOR_THIS_SESSION);
        power=RegisterPowerSettingNotification(hwnd,&GUID_CONSOLE_DISPLAY_STATE,DEVICE_NOTIFY_WINDOW_HANDLE);}
    bool event(UINT message,WPARAM w,LPARAM l){
        const bool before=paused();
        if(message==WM_WTSSESSION_CHANGE){if(w==WTS_SESSION_LOCK)locked=true;else if(w==WTS_SESSION_UNLOCK)locked=false;}
        if(message==WM_POWERBROADCAST){
            if(w==PBT_APMSUSPEND)asleep=true;
            else if(w==PBT_APMRESUMEAUTOMATIC||w==PBT_APMRESUMESUSPEND)asleep=false;
            else if(w==PBT_POWERSETTINGCHANGE&&l){const auto* setting=reinterpret_cast<const POWERBROADCAST_SETTING*>(l);
                if(IsEqualGUID(setting->PowerSetting,GUID_CONSOLE_DISPLAY_STATE)&&setting->DataLength==sizeof(DWORD)){
                    DWORD state{};std::memcpy(&state,setting->Data,sizeof(state));displayOff=state==0;
                }
            }
        }
        return before!=paused();
    }
    void detach(){if(power)UnregisterPowerSettingNotification(power);if(window)WTSUnRegisterSessionNotification(window);power=nullptr;window=nullptr;}
    ~NativeSession(){detach();}
};
