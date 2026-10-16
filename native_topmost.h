#pragma once
#include <windows.h>
#include <vector>

// Keep the complete front-to-back chain together, including alpha input
// windows. Inspect OS state at 4 Hz; do not fight z-order on every video frame.
class NativeTopmostOrder {
    std::vector<HWND> applied;
    HWND foreground{};
    double nextCheck=0;
public:
    bool apply(const std::vector<HWND>& desired,double now){
        bool repair=desired!=applied;
        if(now>=nextCheck){
            nextCheck=now+0.25;
            const auto active=GetForegroundWindow();
            repair|=active!=foreground;foreground=active;
            for(size_t i=0;i<desired.size();++i){
                const auto hwnd=desired[i];
                repair|=!IsWindowVisible(hwnd)||IsIconic(hwnd)||!(GetWindowLongPtrW(hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST);
                if(i)repair|=GetWindow(hwnd,GW_HWNDPREV)!=desired[i-1];
            }
        }
        if(!repair)return false;
        bool success=true;HWND anchor=HWND_TOPMOST;
        for(HWND hwnd:desired){
            if(!IsWindow(hwnd)){success=false;continue;}
            if(IsIconic(hwnd)||!IsWindowVisible(hwnd))ShowWindow(hwnd,SW_SHOWNOACTIVATE);
            success&=SetWindowPos(hwnd,anchor,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER)!=FALSE;
            anchor=hwnd;
        }
        if(success)applied=desired;else applied.clear();
        return true;
    }
};
