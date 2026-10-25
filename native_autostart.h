// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// User-controlled logon startup. Tests inject a private HKCU key; merely
// constructing or querying this service never creates a startup entry.
struct NativeAutostart {
    using Path=std::filesystem::path;
    inline static constexpr const wchar_t* RunKey=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    Path executable,dataDirectory;
    HKEY root=HKEY_CURRENT_USER;
    std::wstring key=RunKey,name=L"BA Cafe Pets";
    struct Key {HKEY value{};~Key(){if(value)RegCloseKey(value);}};
    explicit NativeAutostart(Path file,Path data={},std::wstring registryKey=RunKey):
        executable(std::filesystem::absolute(file).lexically_normal()),dataDirectory(std::move(data)),key(std::move(registryKey)){}
    static Path launchExecutable(){
        wchar_t launcher[32768]{};
        const auto size=GetEnvironmentVariableW(L"BA_NATIVE_LAUNCHER_PATH",launcher,32768);
        if(size){
            if(size>=32768||!std::filesystem::is_regular_file(launcher))throw std::runtime_error("无法定位启动程序，请从完整发行包重新启动后设置开机自启。");
            return std::filesystem::absolute(launcher);
        }
        wchar_t module[32768]{};const auto count=GetModuleFileNameW(nullptr,module,32768);
        if(!count||count>=32768)throw std::runtime_error("无法读取启动程序路径。");
        return Path(module);
    }
    static std::wstring quote(const std::wstring& text){
        std::wstring result=L"\"";size_t slashes=0;
        for(wchar_t c:text){if(c==L'\\'){++slashes;continue;}result.append(slashes*(c==L'\"'?2:1),L'\\');if(c==L'\"')result+=L'\\';result+=c;slashes=0;}
        result.append(slashes*2,L'\\');return result+L"\"";
    }
    std::wstring command() const {
        if(!std::filesystem::is_regular_file(executable))throw std::runtime_error("启动程序已移动或不存在，请重新打开软件后设置。");
        auto value=quote(executable.wstring());
        if(!dataDirectory.empty())value+=L" --data-dir "+quote(std::filesystem::absolute(dataDirectory).lexically_normal().wstring());
        if(value.size()>=260)throw std::runtime_error("启动路径过长，无法添加开机自启。请将程序或资料移至较短的路径。");
        return value;
    }
    bool enabled() const {
        Key handle;const auto opened=RegOpenKeyExW(root,key.c_str(),0,KEY_QUERY_VALUE,&handle.value);
        if(opened==ERROR_FILE_NOT_FOUND)return false;
        if(opened!=ERROR_SUCCESS)throw std::runtime_error("无法读取开机自启设置。");
        DWORD type{},bytes{};const auto status=RegQueryValueExW(handle.value,name.c_str(),nullptr,&type,nullptr,&bytes);
        if(status==ERROR_FILE_NOT_FOUND)return false;
        if(status!=ERROR_SUCCESS)throw std::runtime_error("无法读取开机自启设置。");
        return bytes>sizeof(wchar_t);
    }
    void set(bool enable) const {
        Key handle;
        if(enable){
            const auto value=command();
            if(RegCreateKeyExW(root,key.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&handle.value,nullptr)!=ERROR_SUCCESS||
                RegSetValueExW(handle.value,name.c_str(),0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)))!=ERROR_SUCCESS)
                throw std::runtime_error("无法开启开机自启，原设置保持不变。");
        }else{
            const auto opened=RegOpenKeyExW(root,key.c_str(),0,KEY_SET_VALUE,&handle.value);
            if(opened==ERROR_FILE_NOT_FOUND)return;
            if(opened!=ERROR_SUCCESS)throw std::runtime_error("无法关闭开机自启。");
            const auto removed=RegDeleteValueW(handle.value,name.c_str());
            if(removed!=ERROR_SUCCESS&&removed!=ERROR_FILE_NOT_FOUND)throw std::runtime_error("无法关闭开机自启，原设置保持不变。");
        }
    }
};
