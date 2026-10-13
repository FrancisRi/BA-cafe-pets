#pragma once
#include <windows.h>

// Bound message work so Raw Input/window event bursts cannot starve motion.
template<class Dispatch>
bool drainNativeMessages(Dispatch dispatch) {
    const auto started=GetTickCount64();MSG message{};
    for(unsigned count=0;count<128&&GetTickCount64()-started<4;++count) {
        if(!PeekMessageW(&message,nullptr,0,0,PM_REMOVE))break;
        if(message.message==WM_QUIT)return false;
        dispatch(message);
    }
    return true;
}
