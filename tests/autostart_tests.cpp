// SPDX-License-Identifier: GPL-3.0-only
#include "native_autostart.h"
#include "native_outline_input.h"
#include <shellapi.h>
#include <fstream>
#include <iostream>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::wstring read(const NativeAutostart& startup,const wchar_t* name=L"BA Cafe Pets"){
    NativeAutostart::Key handle;check(RegOpenKeyExW(startup.root,startup.key.c_str(),0,KEY_QUERY_VALUE,&handle.value)==ERROR_SUCCESS,"Read isolated registry key");
    wchar_t value[1024]{};DWORD bytes=sizeof(value),type{};check(RegQueryValueExW(handle.value,name,nullptr,&type,reinterpret_cast<BYTE*>(value),&bytes)==ERROR_SUCCESS&&type==REG_SZ,"Read a string startup entry");return value;
}
int main(int argc,char** argv){try{
    check(argc==2,"Pass a fresh isolated directory");const auto root=std::filesystem::absolute(argv[1]);check(!std::filesystem::exists(root),"Use a fresh test directory");
    std::filesystem::create_directories(root/L"空 格");const auto executable=root/L"空 格/重命名桌宠.exe";
    {std::ofstream file(executable);file<<"test fixture, never executed";}
    const auto key=L"Software\\BA-Cafe-Pets-Tests\\Startup-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    struct Cleanup {std::wstring key;~Cleanup(){RegDeleteTreeW(HKEY_CURRENT_USER,key.c_str());RegDeleteKeyW(HKEY_CURRENT_USER,L"Software\\BA-Cafe-Pets-Tests");}} cleanup{key};
    NativeAutostart startup(executable,root/L"独立 资料/",key);
    check(!startup.enabled(),"Default startup is off and read-only");
    NativeAutostart::Key absent;check(RegOpenKeyExW(HKEY_CURRENT_USER,key.c_str(),0,KEY_QUERY_VALUE,&absent.value)==ERROR_FILE_NOT_FOUND,"Reading startup does not create a registry entry");
    startup.set(true);check(startup.enabled(),"Enable startup only on explicit request");const auto saved=read(startup);
    int count{};auto** args=CommandLineToArgvW(saved.c_str(),&count);check(args&&count==3,"Startup command has only the executable and explicit profile");
    check(std::filesystem::path(args[0])==executable&&std::wstring(args[1])==L"--data-dir"&&std::filesystem::path(args[2])==std::filesystem::absolute(startup.dataDirectory).lexically_normal(),"Unicode, spaces and trailing backslashes round-trip as individual arguments");LocalFree(args);
    NativeAutostart restored(executable,startup.dataDirectory,key);check(restored.enabled(),"Startup state is read from Windows across instances");
    NativeAutostart::Key handle;check(RegOpenKeyExW(HKEY_CURRENT_USER,key.c_str(),0,KEY_SET_VALUE,&handle.value)==ERROR_SUCCESS,"Open isolated registry key");
    const wchar_t other[]=L"another application";RegSetValueExW(handle.value,L"OtherApp",0,REG_SZ,reinterpret_cast<const BYTE*>(other),sizeof(other));
    startup.dataDirectory=root/std::wstring(270,L'x');bool rejected=false;try{startup.set(true);}catch(const std::exception&){rejected=true;}
    check(rejected&&read(startup)==saved,"An overlong command does not corrupt an existing entry");
    startup.executable=root/L"missing.exe";startup.set(false);check(!startup.enabled()&&read(startup,L"OtherApp")==other,"Disable works after a file move and preserves unrelated entries");startup.set(false);
    const auto previousSize=GetEnvironmentVariableW(L"BA_NATIVE_LAUNCHER_PATH",nullptr,0);std::wstring previous(previousSize,L'\0');if(previousSize){GetEnvironmentVariableW(L"BA_NATIVE_LAUNCHER_PATH",previous.data(),previousSize);previous.resize(previousSize-1);}
    struct Environment {bool existed;std::wstring value;~Environment(){SetEnvironmentVariableW(L"BA_NATIVE_LAUNCHER_PATH",existed?value.c_str():nullptr);}} environment{previousSize!=0,previous};
    SetEnvironmentVariableW(L"BA_NATIVE_LAUNCHER_PATH",executable.c_str());check(NativeAutostart::launchExecutable()==executable,"Use the outer renamed single-file launcher, not its cache executable");
    SetEnvironmentVariableW(L"BA_NATIVE_LAUNCHER_PATH",(root/L"missing.exe").c_str());bool missing=false;try{NativeAutostart::launchExecutable();}catch(const std::exception&){missing=true;}check(missing,"A missing launcher is reported rather than registering a temporary cache path");
    for(auto [text,expected]:{std::pair{L"10",10},{L"300%",300},{L" 175 % ",175},{L"00100",100}})check(parseOutlineWidth(text)==expected,"Accept bounded integer percentages");
    for(const auto* text:{L"",L"9",L"301",L"-10",L"NaN",L"175.5",L"1e2",L"100%%",L"999999999999999999999"})check(!parseOutlineWidth(text),"Reject invalid, fractional and overflowing outline values");
    std::cout<<"Autostart passed: isolated registry, opt-in, quoting, Unicode, profiles, launcher path, rollback and disable; outline numeric validation passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
