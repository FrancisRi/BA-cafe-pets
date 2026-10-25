// A native self-extracting launcher. Cabinet.dll decodes the appended CAB;
// only Windows system DLLs are needed, with no installer or script runtime.
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <fdi.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <algorithm>
#include <cstdint>

namespace fs=std::filesystem;
struct Footer {char magic[8];uint64_t offset,size,expanded;unsigned char sha[32];};
static_assert(sizeof(Footer)==64);
struct Context {
    fs::path self,cache,cabSource,destination;uint64_t cabOffset=0,cabSize=0;Footer footer{};
    std::atomic<uint64_t> written{0};std::atomic<int> phase{0};std::atomic<bool> cancel{false},done{false};
    std::string error;
};
static thread_local Context* context=nullptr;
struct File {HANDLE handle;int64_t base=0,limit=LLONG_MAX;};
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::wstring widen(const std::string& s){const int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);std::wstring v(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,v.data(),n);if(n)v.pop_back();return v;}
static std::wstring quote(const std::wstring& s){std::wstring out=L"\"";size_t slashes=0;for(wchar_t ch:s){if(ch==L'\\'){++slashes;continue;}out.append(slashes*(ch==L'\"'?2:1),L'\\');if(ch==L'\"')out.push_back(L'\\');out.push_back(ch);slashes=0;}out.append(slashes*2,L'\\');return out+L"\"";}
static std::string hex(const unsigned char* bytes,size_t n){std::ostringstream s;for(size_t i=0;i<n;++i)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(bytes[i]);return s.str();}
static fs::path environmentPath(const wchar_t* name){wchar_t s[32768]{};const auto n=GetEnvironmentVariableW(name,s,32768);return n&&n<32768?fs::path(s):fs::path{};}
static void* DIAMONDAPI allocate(ULONG n){return std::malloc(n);}
static void DIAMONDAPI deallocate(void* p){std::free(p);}
static INT_PTR DIAMONDAPI openCab(char*,int,int){
    const auto h=CreateFileW(context->cabSource.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return -1;
    LARGE_INTEGER start{};start.QuadPart=context->cabOffset;SetFilePointerEx(h,start,nullptr,FILE_BEGIN);
    return reinterpret_cast<INT_PTR>(new File{h,int64_t(context->cabOffset),int64_t(context->cabSize)});
}
static UINT DIAMONDAPI readFile(INT_PTR f,void* b,UINT n){
    if(context->cancel)return UINT(-1);auto* file=reinterpret_cast<File*>(f);LARGE_INTEGER zero{},pos{};
    if(!SetFilePointerEx(file->handle,zero,&pos,FILE_CURRENT))return UINT(-1);
    const auto remaining=file->limit-(pos.QuadPart-file->base);if(remaining<0)return UINT(-1);
    DWORD got=0;return ReadFile(file->handle,b,DWORD(std::min<int64_t>(n,remaining)),&got,nullptr)?got:UINT(-1);
}
static UINT DIAMONDAPI writeFile(INT_PTR f,void* b,UINT n){DWORD written=0;if(context->cancel||!WriteFile(reinterpret_cast<File*>(f)->handle,b,n,&written,nullptr))return UINT(-1);context->written+=written;return written;}
static int DIAMONDAPI closeFile(INT_PTR f){auto* file=reinterpret_cast<File*>(f);const bool ok=CloseHandle(file->handle);delete file;return ok?0:-1;}
static long DIAMONDAPI seekFile(INT_PTR f,long offset,int origin){auto* file=reinterpret_cast<File*>(f);LARGE_INTEGER delta{},result{};DWORD method=FILE_CURRENT;delta.QuadPart=offset;
    if(origin==SEEK_SET){delta.QuadPart+=file->base;method=FILE_BEGIN;}
    if(origin==SEEK_END){delta.QuadPart+=file->base+file->limit;method=FILE_BEGIN;}
    if(!SetFilePointerEx(file->handle,delta,&result,method))return -1;return long(result.QuadPart-file->base);
}
static INT_PTR DIAMONDAPI notify(FDINOTIFICATIONTYPE type,FDINOTIFICATION* event){
    try{
        if(context->cancel)return -1;
        if(type==fdintCOPY_FILE){
            auto relative=fs::path(widen(event->psz1));require(!relative.is_absolute()&&!relative.has_root_name(),"Invalid archive path");
            for(const auto& part:relative)require(part!=L"..","Archive path escapes the cache");
            const auto dest=context->destination/relative;fs::create_directories(dest.parent_path());
            const auto h=CreateFileW(dest.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            require(h!=INVALID_HANDLE_VALUE,"Cannot extract release file; check available space and folder permissions");
            return reinterpret_cast<INT_PTR>(new File{h});
        }
        if(type==fdintCLOSE_FILE_INFO)return closeFile(event->hf)==0;
        if(type==fdintNEXT_CABINET)throw std::runtime_error("Release archive unexpectedly has multiple cabinets");
    }catch(const std::exception& e){context->error=e.what();return -1;}
    return 0;
}
static void verify(Context& ctx){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Cannot initialize SHA256");
    const auto created=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0);
    if(created<0){BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("Cannot create SHA256 hash");}
    try{
        std::ifstream input(ctx.self,std::ios::binary);input.seekg(ctx.footer.offset);std::vector<unsigned char> bytes(1024*1024);uint64_t remaining=ctx.footer.size;
        while(remaining){require(!ctx.cancel,"Cancelled");const auto n=size_t(std::min<uint64_t>(bytes.size(),remaining));input.read(reinterpret_cast<char*>(bytes.data()),n);require(bool(input),"Release archive is truncated");require(BCryptHashData(hash,bytes.data(),ULONG(n),0)>=0,"SHA256 read failed");remaining-=n;}
        unsigned char digest[32]{};require(BCryptFinishHash(hash,digest,32,0)>=0,"SHA256 finish failed");require(std::memcmp(digest,ctx.footer.sha,32)==0,"Release archive checksum mismatch; download the complete EXE again");
    }catch(...){BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);throw;}
    BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
}
static void decodeCab(Context& ctx){context=&ctx;ERF error{};
    const auto decoder=FDICreate(allocate,deallocate,openCab,readFile,writeFile,closeFile,seekFile,cpuUNKNOWN,&error);require(decoder!=nullptr,"Cannot initialize Windows Cabinet decoder");
    char name[]="bundle.cab",folder[]="";const bool ok=FDICopy(decoder,name,folder,0,notify,nullptr,&ctx);FDIDestroy(decoder);
    require(!ctx.cancel,"Cancelled");
    require(ok,ctx.error.empty()?"Offline resource extraction failed":ctx.error.c_str());
}
static void extract(Context& ctx){context=&ctx;verify(ctx);ctx.phase=1;fs::create_directories(ctx.cache);
    ctx.cabSource=ctx.self;ctx.cabOffset=ctx.footer.offset;ctx.cabSize=ctx.footer.size;ctx.destination=ctx.cache;decodeCab(ctx);
    std::ofstream marker(ctx.cache/L".ready",std::ios::binary);marker<<hex(ctx.footer.sha,32);require(bool(marker),"Cannot write extraction marker");
}
static Context* progressContext=nullptr;static HWND label{},bar{};
static LRESULT CALLBACK progressProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_CLOSE){progressContext->cancel=true;SetWindowTextW(label,L"正在取消…");return 0;}
    if(m==WM_TIMER){const auto phase=progressContext->phase.load();SetWindowTextW(label,phase==0?L"首次启动：正在校验离线资源…":L"首次启动：正在释放桌宠文件…");const auto pct=progressContext->footer.expanded?std::min<uint64_t>(99,progressContext->written*100/progressContext->footer.expanded):0;SendMessageW(bar,WM_USER+2,pct,0);return 0;}
    return DefWindowProcW(h,m,w,l);
}
static void prepare(Context& ctx){
    std::ifstream marker(ctx.cache/L".ready");std::string ready;marker>>ready;
    if(ready==hex(ctx.footer.sha,32)&&fs::is_regular_file(ctx.cache/L"BA-Cafe-Pets.exe")&&fs::is_regular_file(ctx.cache/L"behavior/template.bt.xml")&&fs::is_regular_file(ctx.cache/L"defaults.ini")&&(fs::is_regular_file(ctx.cache/L"model/Shiroko.glb")||fs::is_regular_file(ctx.cache/L"models/Shiroko.glb")))return;
    progressContext=&ctx;WNDCLASSW wc{};wc.lpfnWndProc=progressProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BaCafeNativeReleaseProgress";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
    HWND window=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,L"BA Cafe Pets",WS_CAPTION|WS_SYSMENU,GetSystemMetrics(SM_CXSCREEN)/2-240,GetSystemMetrics(SM_CYSCREEN)/2-80,480,160,nullptr,nullptr,wc.hInstance,nullptr);
    label=CreateWindowExW(0,L"STATIC",L"正在准备离线资源…",WS_CHILD|WS_VISIBLE,22,22,420,28,window,nullptr,wc.hInstance,nullptr);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_PROGRESS_CLASS};InitCommonControlsEx(&controls);
    bar=CreateWindowExW(0,PROGRESS_CLASSW,L"",WS_CHILD|WS_VISIBLE,22,62,420,20,window,nullptr,wc.hInstance,nullptr);
    SendMessageW(bar,PBM_SETBARCOLOR,0,RGB(30,174,230));ShowWindow(window,SW_SHOW);SetTimer(window,1,100,nullptr);
    std::thread worker([&]{try{extract(ctx);}catch(const std::exception& e){ctx.error=e.what();}ctx.done=true;});
    while(!ctx.done){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}MsgWaitForMultipleObjectsEx(0,nullptr,50,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}
    worker.join();DestroyWindow(window);require(ctx.error.empty(),ctx.error.c_str());
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);HANDLE mutex=nullptr;int result=1;
    try{
        Context ctx;wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);ctx.self=path;const auto length=fs::file_size(ctx.self);
        require(length>=sizeof(Footer),"Incomplete release EXE");std::ifstream input(ctx.self,std::ios::binary);input.seekg(length-sizeof(Footer));input.read(reinterpret_cast<char*>(&ctx.footer),sizeof(Footer));
        require(bool(input)&&std::memcmp(ctx.footer.magic,"BAPET01",8)==0&&ctx.footer.size<=LONG_MAX&&ctx.footer.offset<length&&ctx.footer.offset+ctx.footer.size==length-sizeof(Footer),"Invalid release archive footer");
        auto home=environmentPath(L"BA_NATIVE_HOME");if(home.empty())home=environmentPath(L"LOCALAPPDATA")/L"ba_cafe_lite_native";require(home.is_absolute(),"Cannot locate user application folder");
        uint64_t key=14695981039346656037ull;for(wchar_t ch:home.wstring()){key^=ch;key*=1099511628211ull;}
        const auto mutexName=L"Local\\BaCafeNative-"+std::to_wstring(key);mutex=CreateMutexW(nullptr,TRUE,mutexName.c_str());require(mutex!=nullptr,"Cannot create launch lock");if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(mutex);return 0;}
        ctx.cache=home/L"app"/widen(hex(ctx.footer.sha,16));prepare(ctx);
        const auto data=home/L"data";fs::create_directories(data);SetEnvironmentVariableW(L"BA_NATIVE_DATA_DIR",data.c_str());
        SetEnvironmentVariableW(L"BA_NATIVE_PROGRAM_DIR",ctx.self.parent_path().c_str());
        int argc=0;auto** argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool prepareOnly=false;
        const auto exe=ctx.cache/L"BA-Cafe-Pets.exe";auto command=quote(exe.wstring());
        for(int i=1;i<argc;++i){if(std::wcscmp(argv[i],L"--prepare-only")==0)prepareOnly=true;else command+=L" "+quote(argv[i]);}LocalFree(argv);
        if(prepareOnly){ReleaseMutex(mutex);CloseHandle(mutex);return 0;}
        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
        // The core owns opt-in diagnostics; normal launches never open log files.
        HANDLE nul=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        require(nul!=INVALID_HANDLE_VALUE,"Cannot open process standard streams");
        STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;startup.hStdOutput=nul;startup.hStdError=nul;startup.hStdInput=nul;PROCESS_INFORMATION process{};
        const bool started=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,ctx.cache.c_str(),&startup,&process);CloseHandle(nul);require(started,"Cannot start desktop pet");
        WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);result=int(code);
    }catch(const std::exception& e){if(std::strcmp(e.what(),"Cancelled")!=0)MessageBoxW(nullptr,widen(e.what()).c_str(),L"BA Cafe Pets 启动失败",MB_ICONERROR);}
    if(mutex){ReleaseMutex(mutex);CloseHandle(mutex);}return result;
}
