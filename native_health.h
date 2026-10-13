#pragma once
#include <windows.h>
#include <dbghelp.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>
#include <cstdio>

// One lightweight observer. It never touches the renderer or typed input.
class NativeHealth {
    inline static NativeHealth* instance=nullptr;
    std::atomic<const char*> stage{"startup"};std::atomic<ULONGLONG> progress{GetTickCount64()};
    std::atomic<unsigned> paused{0};std::mutex mutex;std::condition_variable wake;bool stopping=false;
    std::thread observer;std::filesystem::path logs;
    void record(const char* where,ULONGLONG delay){
        const auto text=logs/L"hang.txt";
        FILE* file=_wfopen(text.c_str(),L"a");if(file){SYSTEMTIME time{};GetLocalTime(&time);
            std::fprintf(file,"%04u-%02u-%02u %02u:%02u:%02u pid=%lu stage=%s stalled=%llu ms GDI=%lu USER=%lu\n",
                time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,GetCurrentProcessId(),where,delay,
                GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS),GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS));std::fclose(file);}
        // Load the Windows diagnostic DLL only on a stall. Keep a bounded, small
        // dump (thread/module information, not all process memory), overwrite old.
        const auto library=LoadLibraryExW(L"dbghelp.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library)return;
        const auto dump=reinterpret_cast<decltype(&MiniDumpWriteDump)>(GetProcAddress(library,"MiniDumpWriteDump"));
        const auto handle=CreateFileW((logs/L"hang.dmp").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle!=INVALID_HANDLE_VALUE){if(dump)dump(GetCurrentProcess(),GetCurrentProcessId(),handle,MiniDumpWithThreadInfo,nullptr,nullptr,nullptr);CloseHandle(handle);}
        FreeLibrary(library);
    }
public:
    explicit NativeHealth(std::filesystem::path path):logs(std::move(path)){
        instance=this;observer=std::thread([this]{ULONGLONG reported=0;std::unique_lock lock(mutex);
            while(!wake.wait_for(lock,std::chrono::seconds(1),[this]{return stopping;})){
                const auto last=progress.load(),now=GetTickCount64();
                if(!paused.load()&&now-last>=8000&&reported!=last){reported=last;const auto where=stage.load();lock.unlock();record(where,now-last);lock.lock();}
            }
        });
    }
    static void mark(const char* name){if(instance){instance->stage=name;instance->progress=GetTickCount64();}}
    struct Pause {
        NativeHealth* health=instance;
        Pause(){if(health)++health->paused;}
        ~Pause(){if(health){health->progress=GetTickCount64();--health->paused;}}
    };
    ~NativeHealth(){{std::lock_guard lock(mutex);stopping=true;}wake.notify_all();observer.join();instance=nullptr;}
};
inline int nativeMessageBox(HWND hwnd,const wchar_t* text,const wchar_t* title,UINT flags){NativeHealth::Pause pause;return MessageBoxW(hwnd,text,title,flags);}
