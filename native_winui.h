#pragma once
#include <windows.h>
#include <filesystem>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// The settings process uses inherited pipes. No public listener is needed.
class NativeWinUi {
    struct Handle {
        HANDLE value{};
        ~Handle(){reset();}
        void reset(){if(value)CloseHandle(value);value=nullptr;}
    };
    Handle input,output,process;
    std::jthread reader,writer;
    std::mutex mutex;
    std::condition_variable outgoing;
    std::vector<std::string> inbox;
    struct Output {std::string line;bool snapshot;};
    std::deque<Output> outbox;
    bool closing=false;
    DWORD threadId=GetCurrentThreadId();
    DWORD processId=0;

    void read(){
        std::string partial;
        char bytes[4096];
        DWORD count{};
        while(ReadFile(output.value,bytes,sizeof(bytes),&count,nullptr)&&count){
            partial.append(bytes,count);
            size_t end;
            while((end=partial.find('\n'))!=std::string::npos){
                {
                    std::lock_guard lock(mutex);
                    inbox.push_back(partial.substr(0,end));
                }
                partial.erase(0,end+1);
                PostThreadMessageW(threadId,wake,0,0);
            }
        }
        PostThreadMessageW(threadId,wake,0,0);
    }
    void write(){
        for(;;){
            std::string line;
            {
                std::unique_lock lock(mutex);
                outgoing.wait(lock,[this]{return closing||!outbox.empty();});
                if(closing)return;
                line=std::move(outbox.front().line);
                outbox.pop_front();
            }
            DWORD written{};
            if(!WriteFile(input.value,line.data(),DWORD(line.size()),&written,nullptr)||written!=line.size())return;
        }
    }
public:
    static constexpr UINT wake=WM_APP+67;
    HWND full{},quick{};

    ~NativeWinUi(){
        if(alive()){
            send(R"({"type":"shutdown"})");
            if(WaitForSingleObject(process.value,500)==WAIT_TIMEOUT)TerminateProcess(process.value,1);
        }
        {std::lock_guard lock(mutex);closing=true;}
        outgoing.notify_one();
        if(writer.joinable())writer.join();
        input.reset();
        if(reader.joinable())reader.join();
    }
    bool launch(const std::filesystem::path& exe){
        if(!std::filesystem::is_regular_file(exe))return false;
        SECURITY_ATTRIBUTES inherit{sizeof(inherit),nullptr,TRUE};
        Handle childInput,childOutput;
        if(!CreatePipe(&childInput.value,&input.value,&inherit,0)||
            !CreatePipe(&output.value,&childOutput.value,&inherit,0)||
            !SetHandleInformation(input.value,HANDLE_FLAG_INHERIT,0)||
            !SetHandleInformation(output.value,HANDLE_FLAG_INHERIT,0))return false;
        Handle errors;
        errors.value=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&inherit,OPEN_EXISTING,0,nullptr);
        if(errors.value==INVALID_HANDLE_VALUE){errors.value=nullptr;return false;}
        HANDLE inherited[]={childInput.value,childOutput.value,errors.value};
        SIZE_T bytes=0;
        InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
        if(!bytes)return false;
        std::vector<std::byte> storage(bytes);
        const auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if(!InitializeProcThreadAttributeList(attributes,1,0,&bytes))return false;
        struct Cleanup {
            LPPROC_THREAD_ATTRIBUTE_LIST attributes;
            ~Cleanup(){DeleteProcThreadAttributeList(attributes);}
        } cleanup{attributes};
        if(!UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr))return false;
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb=sizeof(startup);
        startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput=childInput.value;
        startup.StartupInfo.hStdOutput=childOutput.value;
        startup.StartupInfo.hStdError=errors.value;
        startup.lpAttributeList=attributes;
        PROCESS_INFORMATION info{};
        auto command=L"\""+exe.wstring()+L"\"";
        if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT,
            nullptr,exe.parent_path().c_str(),&startup.StartupInfo,&info))return false;
        CloseHandle(info.hThread);process.value=info.hProcess;processId=info.dwProcessId;
        MSG ignored{};PeekMessageW(&ignored,nullptr,WM_USER,WM_USER,PM_NOREMOVE);
        reader=std::jthread([this]{read();});
        writer=std::jthread([this]{write();});
        return true;
    }
    bool alive() const {return process.value&&WaitForSingleObject(process.value,0)==WAIT_TIMEOUT;}
    void allowActivation() const {AllowSetForegroundWindow(processId);}
    void send(std::string line,bool snapshot=false){
        line+='\n';
        {
            std::lock_guard lock(mutex);
            if(snapshot&&!outbox.empty()&&outbox.back().snapshot)outbox.back().line=std::move(line);
            else outbox.push_back({std::move(line),snapshot});
        }
        outgoing.notify_one();
    }
    std::vector<std::string> take(){
        std::lock_guard lock(mutex);
        std::vector<std::string> result;
        result.swap(inbox);return result;
    }
    HWND handle(unsigned long long raw) const {
        const auto hwnd=reinterpret_cast<HWND>(static_cast<UINT_PTR>(raw));
        DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);
        return processId&&pid==processId?hwnd:nullptr;
    }
};
