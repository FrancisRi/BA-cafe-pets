#pragma once

#ifdef BA_WINDOWS_NOTIFICATIONS
#include <appmodel.h>
extern "C" {
    using NativeToastReceiver=void(void*,const wchar_t*,size_t,const wchar_t*,size_t);
    void* ba_notifications_open();
    void ba_notifications_close(void*);
    int ba_notifications_request(void*);
    int ba_notifications_poll(void*,int,NativeToastReceiver*,void*);
}
#endif

struct NativeWindowsNotifications {
    int status=0;
#ifdef BA_WINDOWS_NOTIFICATIONS
    void* listener{};
    ULONGLONG nextPoll=0;
    bool attempted=false;
    ~NativeWindowsNotifications(){if(listener)ba_notifications_close(listener);}
    static bool packaged(){
        UINT32 length=0;
        return GetCurrentPackageFullName(&length,nullptr)==ERROR_INSUFFICIENT_BUFFER;
    }
    bool configure(){
        if(listener)return true;
        if(!attempted&&packaged()){
            attempted=true;listener=ba_notifications_open();status=listener?1:5;
        }
        return listener!=nullptr;
    }
    void request(){
        if(configure())status=ba_notifications_request(listener);
    }
    void update(NativeMessages& messages,int mode){
        if(!listener&&(mode==0||!configure()))return;
        const auto now=GetTickCount64();
        if(now<nextPoll)return;
        nextPoll=now+650;
        const auto receive=[](void* context,const wchar_t* title,size_t titleLength,const wchar_t* text,size_t textLength) noexcept {
            try{static_cast<NativeMessages*>(context)->push({title,titleLength},{text,textLength});}
            catch(const std::exception&){}
        };
        status=ba_notifications_poll(listener,mode,receive,&messages);
    }
#else
    static bool packaged(){return false;}
    void request(){}
    void update(NativeMessages&,int){}
#endif
};
