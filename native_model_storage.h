#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <behaviortree_cpp/contrib/json.hpp>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

// The launcher supplies the directory of the user's EXE, not its extraction
// cache. Configuration remains with user data; old libraries remain managed.
struct NativeModelStorage {
    using Path=std::filesystem::path;
    using Json=nlohmann::json;
    Path programDirectory,configuration,dataDirectory,current;
    bool discoverLegacy=true;
    std::vector<Path> previous,recoveredProfiles;
    static std::string utf8(const std::wstring& text){const int n=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);std::string result(n,' ');if(n)WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),result.data(),n,nullptr,nullptr);return result;}
    static std::wstring wide(const std::string& text){const int n=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);std::wstring result(n,L' ');if(n)MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),result.data(),n);return result;}
    static Path absolute(const Path& path){return std::filesystem::absolute(path).lexically_normal();}
    static bool same(const Path& a,const Path& b){const auto left=std::filesystem::weakly_canonical(a).wstring(),right=std::filesystem::weakly_canonical(b).wstring();return CompareStringOrdinal(left.c_str(),int(left.size()),right.c_str(),int(right.size()),TRUE)==CSTR_EQUAL;}
    static void append(std::vector<Path>& folders,const Path& path){if(path.empty())return;const auto value=absolute(path);if(std::none_of(folders.begin(),folders.end(),[&](const auto& item){return same(item,value);}))folders.push_back(value);}
    Path defaultDirectory() const {return programDirectory/L"model";}
    NativeModelStorage(Path program,Path data,bool discover=true):programDirectory(absolute(program)),configuration(absolute(data)/L"model-library.json"),dataDirectory(absolute(data)),current(defaultDirectory()),discoverLegacy(discover){
        if(!std::filesystem::exists(configuration))return;
        std::ifstream input(configuration);const auto value=Json::parse(input);
        const auto saved=value.value("directory",std::string{});if(!saved.empty())current=resolve(saved);
        for(const auto& item:value.value("previousDirectories",Json::array()))append(previous,resolve(item.get<std::string>()));
        for(const auto& item:value.value("recoveredProfiles",Json::array()))append(recoveredProfiles,resolve(item.get<std::string>()));
    }
    Path resolve(const std::string& text) const {const Path path(wide(text));return absolute(path.is_absolute()?path:programDirectory/path);}
    std::string encode(const Path& path) const {
        const auto relative=path.lexically_relative(programDirectory);
        if(!relative.empty()&&*relative.begin()!=L"..")return utf8(relative.wstring());
        return utf8(path.wstring());
    }
    std::vector<Path> legacyDataDirectories() const {
        if(!discoverLegacy)return {};
        std::vector<Path> result;append(result,programDirectory/L"data");append(result,programDirectory/L"build/bin/data");return result;
    }
    std::vector<Path> directories() const {
        std::vector<Path> result;append(result,current);append(result,programDirectory);append(result,dataDirectory/L"models");
        for(const auto& path:legacyDataDirectories())append(result,path/L"models");
        for(const auto& path:previous)append(result,path);return result;
    }
    static void writable(const Path& path){
        std::filesystem::create_directories(path);static std::atomic<unsigned long> serial{0};
        const auto probe=path/(L".ba-cafe-write-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial)+L".tmp");
        const auto handle=CreateFileW(probe.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
        if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("所选文件夹无法写入，请选择可保存模型的位置");CloseHandle(handle);
    }
    void persist(const Path& destination,const std::vector<Path>& history,const std::vector<Path>& recovered) const {
        Json paths=Json::array(),profiles=Json::array();for(const auto& path:history)paths.push_back(encode(path));for(const auto& path:recovered)profiles.push_back(encode(path));
        const Json value={{"directory",same(destination,defaultDirectory())?std::string{}:encode(destination)},{"previousDirectories",paths},{"recoveredProfiles",profiles}};
        std::filesystem::create_directories(dataDirectory);const auto temporary=configuration.wstring()+L".tmp";
        try{
            {std::ofstream output(Path(temporary),std::ios::binary);output<<value.dump(2);if(!output)throw std::runtime_error("无法保存模型目录设置");}
            if(!MoveFileExW(temporary.c_str(),configuration.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("无法保存模型目录设置，原目录保持不变");
        }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
    }
    void change(const Path& folder){
        const auto destination=absolute(folder);writable(destination);if(same(destination,current))return;
        auto history=previous;append(history,current);persist(destination,history,recoveredProfiles);
        current=destination;previous=std::move(history);
    }
    static std::optional<Path> choose(HWND owner,const Path& initial){
        const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
        struct Com {bool owned;~Com(){if(owned)CoUninitialize();}} com{SUCCEEDED(initialized)};
        if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)throw std::runtime_error("无法初始化文件夹选择窗口");
        IFileOpenDialog* dialog{};if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_IFileOpenDialog,reinterpret_cast<void**>(&dialog))))throw std::runtime_error("无法打开文件夹选择窗口");
        struct Release {IFileOpenDialog* value;~Release(){value->Release();}} release{dialog};
        FILEOPENDIALOGOPTIONS options{};dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
        dialog->SetTitle(L"选择模型保存文件夹");dialog->SetOkButtonLabel(L"使用此文件夹");
        IShellItem* start{};if(SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(),nullptr,IID_IShellItem,reinterpret_cast<void**>(&start)))){dialog->SetFolder(start);start->Release();}
        const auto shown=dialog->Show(owner);if(shown==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};
        if(FAILED(shown))throw std::runtime_error("模型文件夹选择失败，请重试");
        IShellItem* item{};if(FAILED(dialog->GetResult(&item)))throw std::runtime_error("无法读取所选文件夹");
        PWSTR name{};const auto named=item->GetDisplayName(SIGDN_FILESYSPATH,&name);item->Release();if(FAILED(named))throw std::runtime_error("请选择本地文件夹");
        const Path result(name);CoTaskMemFree(name);return result;
    }
    bool bundled(const Path& file,const Path& resource) const {
        if(same(file,resource))return true;
        if(file.filename()!=L"Shiroko.glb"||file.parent_path().filename()!=L"models")return false;
        if(same(file.parent_path().parent_path().parent_path(),dataDirectory.parent_path()/L"app"))return true;
        for(const auto& data:legacyDataDirectories())if(same(file,data.parent_path()/L"models/Shiroko.glb"))return true;
        return false;
    }
    template<class Roster> bool recover(Roster& roster,const Path& resource) {
        bool changed=false;
        auto recovered=recoveredProfiles;
        // Normalize old bundled-model cache references across launcher upgrades.
        for(auto& row:roster.entries)if(bundled(Path(wide(row.path)),resource)&&!same(Path(wide(row.path)),resource)){
            const auto old=row.id;row.id=Roster::key(resource);row.path=utf8(resource.wstring());if(roster.selected==old)roster.selected=row.id;changed=true;
        }
        bool hasUser=std::any_of(roster.entries.begin(),roster.entries.end(),[&](const auto& row){return !bundled(Path(wide(row.path)),resource)&&std::filesystem::is_regular_file(Path(wide(row.path)));});
        for(const auto& data:legacyDataDirectories()){
            if(same(data,dataDirectory)||!std::filesystem::is_regular_file(data/L"characters.json"))continue;
            if(std::any_of(recovered.begin(),recovered.end(),[&](const auto& item){return same(item,data);}))continue;
            Roster legacy;legacy.load(data/L"characters.json");
            append(recovered,data);
            const auto usable=[&](const auto& row){const Path file(wide(row.path));return !bundled(file,resource)&&std::filesystem::is_regular_file(file);};
            const bool adopt=!hasUser&&std::any_of(legacy.entries.begin(),legacy.entries.end(),[&](const auto& row){return usable(row)&&row.active;});
            if(adopt)for(auto& row:roster.entries)row.active=false;
            for(auto row:legacy.entries){
                if(!usable(row)||roster.find(row.id))continue;
                row.active=adopt&&row.active&&roster.activeCount()<Roster::Maximum;
                const auto id=row.id;roster.remember(std::move(row));if(adopt&&legacy.selected==id)roster.selected=id;changed=true;
            }
            hasUser|=std::any_of(legacy.entries.begin(),legacy.entries.end(),usable);
        }
        if(changed&&std::filesystem::is_regular_file(dataDirectory/L"characters.json")){
            const auto backup=dataDirectory/(L"characters.before-storage-"+std::to_wstring(GetTickCount64())+L".json");
            std::filesystem::copy_file(dataDirectory/L"characters.json",backup,std::filesystem::copy_options::none);
        }
        if(recovered.size()!=recoveredProfiles.size()){persist(current,previous,recovered);recoveredProfiles=std::move(recovered);}
        return changed;
    }
};
