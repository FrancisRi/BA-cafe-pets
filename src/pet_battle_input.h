#pragma once
#include <unordered_set>

// Read-only Win32 events on the existing message thread. Callbacks only set
// flags: no input text is retained and no event is intercepted.
struct BattleDesktopInput {
    inline static BattleDesktopInput* instance=nullptr;
    HHOOK keyboard{}, mouse{};
    HWINEVENTHOOK windows{};
    bool keyPressed=false, reload=false, mouseClick=false, windowClosed=false;
    ULONGLONG lastKeyboard=0;
    bool pollKeyboard=false; // Three.js fallback if Windows removes a delayed low-level hook.
    bool heldKeys[256]{};ULONGLONG nextHeldRepeat=0;
    std::unordered_set<HWND> applicationWindows;
    static bool application(HWND window) {
        if(!window || GetAncestor(window,GA_ROOT)!=window || !IsWindowVisible(window))return false;
        DWORD process=0;GetWindowThreadProcessId(window,&process);
        return process!=GetCurrentProcessId() && (GetWindowLongPtrW(window,GWL_STYLE)&WS_CAPTION)!=0
            && (GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)==0;
    }
    static LRESULT CALLBACK keyboardProc(int code,WPARAM w,LPARAM l) {
        if(code==HC_ACTION && instance && (w==WM_KEYDOWN || w==WM_SYSKEYDOWN)) {
            const auto& key=*reinterpret_cast<KBDLLHOOKSTRUCT*>(l);
            instance->keyPressed=true;instance->lastKeyboard=GetTickCount64();
            const bool shift=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;
            instance->reload|=key.vkCode==VK_RETURN || key.vkCode==VK_OEM_PERIOD || key.vkCode==VK_DECIMAL
                || key.vkCode==VK_OEM_1 || (key.vkCode==VK_OEM_2 && shift);
        }
        return CallNextHookEx(nullptr,code,w,l);
    }
    static LRESULT CALLBACK mouseProc(int code,WPARAM w,LPARAM l) {
        if(code==HC_ACTION && instance && (w==WM_LBUTTONUP || w==WM_RBUTTONUP))instance->mouseClick=true;
        return CallNextHookEx(nullptr,code,w,l);
    }
    static void CALLBACK windowProc(HWINEVENTHOOK,DWORD event,HWND window,LONG object,LONG child,DWORD,DWORD) {
        if(!instance || object!=OBJID_WINDOW || child!=CHILDID_SELF)return;
        if(event==EVENT_OBJECT_DESTROY) {
            if(instance->applicationWindows.erase(window))instance->windowClosed=true;
        } else if(event==EVENT_OBJECT_SHOW && application(window))instance->applicationWindows.insert(window);
    }
    void enable(bool enabled) {
        if(!enabled) {
            if(keyboard)UnhookWindowsHookEx(keyboard);
            if(mouse)UnhookWindowsHookEx(mouse);
            if(windows)UnhookWinEvent(windows);
            keyboard=mouse=nullptr;windows=nullptr;
            applicationWindows.clear();if(instance==this)instance=nullptr;clear();return;
        }
        if(instance==this)return;
        instance=this;
        keyboard=SetWindowsHookExW(WH_KEYBOARD_LL,keyboardProc,GetModuleHandleW(nullptr),0);
        mouse=SetWindowsHookExW(WH_MOUSE_LL,mouseProc,GetModuleHandleW(nullptr),0);
        windows=SetWinEventHook(EVENT_OBJECT_DESTROY,EVENT_OBJECT_SHOW,nullptr,windowProc,0,0,
            WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        EnumWindows([](HWND window,LPARAM data)->BOOL {
            if(application(window))reinterpret_cast<BattleDesktopInput*>(data)->applicationWindows.insert(window);
            return TRUE;
        },reinterpret_cast<LPARAM>(this));
        TraceLog(LOG_INFO,"BATTLE input hooks: keyboard=%d mouse=%d windows=%d",keyboard!=nullptr,mouse!=nullptr,windows!=nullptr);
    }
    void clear() {keyPressed=reload=mouseClick=windowClosed=false;}
    pet::BattleInput take(bool petClick,bool dragging,bool boundary) {
        if(pollKeyboard){
            const auto now=GetTickCount64();bool any=false;
            for(int key=8;key<256;++key){
                const bool held=(GetAsyncKeyState(key)&0x8000)!=0;
                const bool modifier=key==VK_SHIFT||key==VK_CONTROL||key==VK_MENU||(key>=VK_LSHIFT&&key<=VK_RMENU)||key==VK_LWIN||key==VK_RWIN;
                const bool press=held&&(!heldKeys[key]||(!modifier&&now>=nextHeldRepeat));
                if(press){keyPressed=true;lastKeyboard=now;
                    reload|=key==VK_RETURN||key==VK_OEM_PERIOD||key==VK_DECIMAL||key==VK_OEM_1||(key==VK_OEM_2&&(GetAsyncKeyState(VK_SHIFT)&0x8000));}
                heldKeys[key]=held;any|=held;
            }
            if(keyPressed)nextHeldRepeat=now+150;
            if(any)lastKeyboard=now;else nextHeldRepeat=0;
        }
        pet::BattleInput result{dragging,petClick,mouseClick,keyPressed,
            lastKeyboard!=0 && GetTickCount64()-lastKeyboard<=1000,reload,windowClosed,boundary};
        clear();return result;
    }
    void planJump(pet::BattleInput& input,const pet::Battle& battle,const pet::Playback& state,
                  const PetGround& ground,float yaw,LONG originX,LONG originY,
                  const std::function<float(float)>& visibleDistance={}) const {
        if(battle.clips.jump<0 || battle.phase!=pet::Battle::Phase::Moving || !state.walkEnabled)return;
        const float duration=static_cast<float>(state.clips[battle.clips.jump].duration);
        const float speed=static_cast<float>(state.walkSpeed*state.movementScale);
        // No outward jump into an unreachable desktop edge. Internal monitor
        // gaps are allowed during a jump, provided its landing is reachable.
        if(speed<=0 || !ground.canWalk(yaw,speed*duration))return;
        auto areas=ground.monitors;
        for(HWND window:applicationWindows) {
            if(visibleDistance)break; // Three.js supplies only exposed window edges.
            RECT r{};
            if(IsWindowVisible(window) && !IsIconic(window) && GetWindowRect(window,&r))
                areas.push_back({float(r.left-originX),float(r.top-originY),float(r.right-originX),float(r.bottom-originY)});
        }
        float distance=ground.boundaryDistance(yaw,speed*duration*0.5f,areas);
        if(visibleDistance)distance=std::min(distance,visibleDistance(speed*duration*0.5f));
        if(std::isfinite(distance)) {
            input.boundary=true;input.jumpOffset=std::max(0.0,double(duration*0.5f-distance/speed));
        }
    }
    ~BattleDesktopInput() {enable(false);}
};
