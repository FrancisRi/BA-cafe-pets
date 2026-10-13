#include "native_health.h"
#include "native_loop.h"
#include "native_session.h"
#include <stdexcept>
#include <iostream>

static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){
    try {
        NativeSession session;
        check(session.event(WM_WTSSESSION_CHANGE,WTS_SESSION_LOCK,0)&&session.paused(),"Lock pauses rendering");
        session.event(WM_POWERBROADCAST,PBT_APMSUSPEND,0);
        check(!session.event(WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0)&&session.paused(),"Unlock cannot override sleep");
        check(session.event(WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC,0)&&!session.paused(),"Resume after unlock resets clocks");
        struct {GUID id;DWORD length;DWORD state;} power{GUID_CONSOLE_DISPLAY_STATE,sizeof(DWORD),0};
        check(session.event(WM_POWERBROADCAST,PBT_POWERSETTINGCHANGE,reinterpret_cast<LPARAM>(&power))&&session.paused(),"Display off pauses rendering");
        session.event(WM_WTSSESSION_CHANGE,WTS_SESSION_LOCK,0);power.state=1;
        check(!session.event(WM_POWERBROADCAST,PBT_POWERSETTINGCHANGE,reinterpret_cast<LPARAM>(&power))&&session.paused(),"Display on cannot override lock");
        check(session.event(WM_WTSSESSION_CHANGE,WTS_SESSION_UNLOCK,0)&&!session.paused(),"Unlock on active display resumes");
        MSG message{};PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);
        const DWORD mainThread=GetCurrentThreadId();constexpr UINT burst=WM_APP+5;
        PostThreadMessageW(mainThread,burst,0,0);unsigned messages=0;
        auto dispatch=[&](MSG& m){if(m.message==burst){++messages;PostThreadMessageW(mainThread,burst,0,0);}};
        HANDLE timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);check(timer!=nullptr,"Create motion timer");
        LARGE_INTEGER due{};due.QuadPart=-5000000;SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE);
        const auto wake=MsgWaitForMultipleObjectsEx(1,&timer,0,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        check(wake==WAIT_OBJECT_0+1,"Input can wake the loop before its timer");Sleep(550);
        check(drainNativeMessages(dispatch)&&messages>0&&messages<=128,"Continuous messages have bounded work");
        check(WaitForSingleObject(timer,0)==WAIT_OBJECT_0,"Check the timer again after input; the original wake was input only");CloseHandle(timer);
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){}
        PostQuitMessage(0);check(!drainNativeMessages(dispatch),"WM_QUIT exits even during input handling");
        if(argc>1){const auto logs=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(logs);
            NativeHealth health(logs);{NativeHealth::Pause modal;NativeHealth::mark("test modal");Sleep(9500);}
            check(!std::filesystem::exists(logs/"hang.txt"),"Modal waits must not be reported as hangs");
            NativeHealth::mark("test stalled main thread");Sleep(10500);
            check(std::filesystem::file_size(logs/"hang.txt")>0&&std::filesystem::file_size(logs/"hang.dmp")>0,"A real stalled stage leaves a diagnostic dump");
        }
        std::cout<<"Message fairness, quit, sleep/lock/display state and hang diagnostics passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
