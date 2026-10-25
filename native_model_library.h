#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Imported GLBs live in the configured library, outside release extraction
// caches. Preparing an import is read-only; commit creates a verified copy.
struct NativeModelLibrary {
    using Path=std::filesystem::path;
    Path directory;
    struct Import {Path source,file;std::wstring name;std::string digest;bool reused=false;};
    struct Hash {
        BCRYPT_ALG_HANDLE provider{};BCRYPT_HASH_HANDLE hash{};std::vector<unsigned char> object;
        Hash(){
            if(BCryptOpenAlgorithmProvider(&provider,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("无法初始化模型校验");
            DWORD size{},bytes{};if(BCryptGetProperty(provider,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&bytes,0)<0){BCryptCloseAlgorithmProvider(provider,0);provider=nullptr;throw std::runtime_error("无法初始化模型校验");}
            object.resize(size);if(BCryptCreateHash(provider,&hash,object.data(),size,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(provider,0);provider=nullptr;throw std::runtime_error("无法初始化模型校验");}
        }
        ~Hash(){if(hash)BCryptDestroyHash(hash);if(provider)BCryptCloseAlgorithmProvider(provider,0);}
        Hash(const Hash&)=delete;Hash& operator=(const Hash&)=delete;
    };
    explicit NativeModelLibrary(Path path):directory(std::filesystem::absolute(std::move(path)).lexically_normal()){}
    bool owns(const Path& file) const {
        if(!glbExtension(file))return false;
        const auto parent=std::filesystem::weakly_canonical(std::filesystem::absolute(file).parent_path()).wstring();
        const auto root=std::filesystem::weakly_canonical(directory).wstring();
        if(CompareStringOrdinal(parent.c_str(),int(parent.size()),root.c_str(),int(root.size()),TRUE)!=CSTR_EQUAL)return false;
        const auto attributes=GetFileAttributesW(file.c_str());
        return attributes==INVALID_FILE_ATTRIBUTES||!(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY));
    }
    static std::wstring displayName(const Path& file){
        auto name=file.stem().wstring();const auto split=name.rfind(L"--");
        if(split!=std::wstring::npos){const auto suffix=name.substr(split+2);if((suffix.size()==16||suffix.size()==64)&&std::all_of(suffix.begin(),suffix.end(),[](wchar_t ch){return (ch>=L'0'&&ch<=L'9')||(ch>=L'a'&&ch<=L'f');}))name.resize(split);}
        return name.empty()?L"model":name;
    }
    std::vector<Path> files() const {
        std::vector<Path> result;if(!std::filesystem::exists(directory))return result;
        for(const auto& item:std::filesystem::directory_iterator(directory))if(owns(item.path())&&item.is_regular_file())result.push_back(item.path());
        std::sort(result.begin(),result.end());return result;
    }
    bool recycle(const Path& file,HWND owner=nullptr) const {
        if(!owns(file))throw std::runtime_error("只能删除本地模型库中的模型副本，原始文件和程序自带模型保留");
        if(!std::filesystem::exists(file))return false;
        auto source=std::filesystem::absolute(file).lexically_normal().wstring();source.push_back(L'\0');
        SHFILEOPSTRUCTW operation{};operation.hwnd=owner;operation.wFunc=FO_DELETE;operation.pFrom=source.c_str();
        operation.fFlags=FOF_ALLOWUNDO|FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_SILENT;
        if(SHFileOperationW(&operation)!=0||operation.fAnyOperationsAborted||std::filesystem::exists(file))throw std::runtime_error("模型未能删除，请检查文件是否被其他程序占用或目录是否可写");
        return true;
    }
    static std::string digest(const Path& path){
        std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("无法读取模型文件");Hash state;std::array<unsigned char,65536> buffer{};
        while(input){input.read(reinterpret_cast<char*>(buffer.data()),buffer.size());const auto count=input.gcount();if(count&&BCryptHashData(state.hash,buffer.data(),ULONG(count),0)<0)throw std::runtime_error("模型校验失败");}
        if(!input.eof())throw std::runtime_error("模型文件读取失败");std::array<unsigned char,32> bytes{};if(BCryptFinishHash(state.hash,bytes.data(),ULONG(bytes.size()),0)<0)throw std::runtime_error("模型校验失败");
        constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);for(auto b:bytes){result.push_back(hex[b>>4]);result.push_back(hex[b&15]);}return result;
    }
    static bool glbExtension(const Path& file){const auto ext=file.extension().wstring();return CompareStringOrdinal(ext.c_str(),int(ext.size()),L".glb",4,TRUE)==CSTR_EQUAL;}
    static void validate(const Path& path,bool checkExtension=true){
        if(!std::filesystem::is_regular_file(path)||(checkExtension&&!glbExtension(path)))throw std::runtime_error("请选择有效的 GLB 模型文件");
        std::ifstream input(path,std::ios::binary);std::array<uint32_t,3> header{};input.read(reinterpret_cast<char*>(header.data()),sizeof(header));
        if(!input||header[0]!=0x46546c67||header[1]!=2||header[2]!=std::filesystem::file_size(path))throw std::runtime_error("文件不是完整的 GLB 2.0 模型");
    }
    static std::wstring safeStem(const Path& path){
        auto name=path.stem().wstring();for(auto& ch:name)if(ch<32||std::wstring(L"<>:\"/\\|?*").find(ch)!=std::wstring::npos)ch=L'_';
        if(name.size()>72)name.resize(72);if(!name.empty()&&name.back()>=0xD800&&name.back()<=0xDBFF)name.pop_back();
        while(!name.empty()&&(name.back()==L'.'||name.back()==L' '))name.pop_back();return name.empty()?L"model":name;
    }
    Import prepare(const Path& file) const {
        Import result;result.source=std::filesystem::canonical(file);validate(result.source);result.name=file.stem().wstring();result.digest=digest(result.source);
        const std::wstring shortHash(result.digest.begin(),result.digest.begin()+16),fullHash(result.digest.begin(),result.digest.end());
        const auto shortSuffix=L"--"+shortHash+L".glb",fullSuffix=L"--"+fullHash+L".glb";
        if(std::filesystem::exists(directory)){
            // An explicit import of a file already in the library only registers
            // it. Never make nested copies or overwrite a manually managed file.
            if(std::filesystem::equivalent(result.source.parent_path(),directory)){
                result.file=result.source;result.reused=true;auto stem=result.file.stem().wstring();
                for(const auto& hash:{shortHash,fullHash})if(stem.ends_with(L"--"+hash))result.name=stem.substr(0,stem.size()-hash.size()-2);
                return result;
            }
            for(const auto& entry:std::filesystem::directory_iterator(directory)){
                if(!owns(entry.path())||!entry.is_regular_file()||entry.file_size()!=std::filesystem::file_size(result.source))continue;
                if(digest(entry.path())==result.digest){result.file=entry.path();result.reused=true;return result;}
            }
        }
        const auto stem=safeStem(file);result.file=directory/(stem+shortSuffix);
        if(std::filesystem::exists(result.file))result.file=directory/(stem+fullSuffix);
        if(std::filesystem::exists(result.file))throw std::runtime_error("本地同名文件已被修改，请在模型文件夹中检查后重试");
        return result;
    }
    Path commit(const Import& plan) const {
        if(plan.file.parent_path().lexically_normal()!=directory&&!std::filesystem::equivalent(plan.file.parent_path(),directory))throw std::runtime_error("模型目标路径不在本地模型库内");
        if(plan.reused){if(digest(plan.file)!=plan.digest)throw std::runtime_error("本地模型文件已变化，请重新导入");return plan.file;}
        std::filesystem::create_directories(directory);static std::atomic<unsigned long> serial{0};
        const auto temporary=directory/(L".import-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(++serial)+L".tmp");
        bool ownsTemporary=false;
        try{
            const auto handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("无法在模型文件夹中创建文件");ownsTemporary=true;
            {
                struct Output {HANDLE handle;~Output(){CloseHandle(handle);}} output{handle};
                std::ifstream input(plan.source,std::ios::binary);if(!input)throw std::runtime_error("无法读取源模型");std::array<char,65536> buffer{};
                while(input){input.read(buffer.data(),buffer.size());const auto count=input.gcount();if(count){DWORD written{};if(!WriteFile(handle,buffer.data(),DWORD(count),&written,nullptr)||written!=DWORD(count))throw std::runtime_error("模型复制失败，请检查剩余空间");}}
                if(!input.eof()||!FlushFileBuffers(handle))throw std::runtime_error("模型复制未完成，请重试");
            }
            if(digest(temporary)!=plan.digest)throw std::runtime_error("导入期间源文件发生变化，请重试");
            validate(temporary,false);
            if(!MoveFileExW(temporary.c_str(),plan.file.c_str(),MOVEFILE_WRITE_THROUGH)){
                // Another instance may have finished the exact same import.
                if(!std::filesystem::is_regular_file(plan.file)||digest(plan.file)!=plan.digest)throw std::runtime_error("无法保存模型副本，请检查模型文件夹权限与剩余空间");
                std::error_code ignored;std::filesystem::remove(temporary,ignored);
            }
            return plan.file;
        }catch(...){if(ownsTemporary){std::error_code ignored;std::filesystem::remove(temporary,ignored);}throw;}
    }
    Path store(const Path& source) const {return commit(prepare(source));}
    // Library migration preserves filenames (including intentional aliases),
    // while the normal import path continues to deduplicate by content.
    Path copyToLibrary(const Path& source) const {
        if(owns(source)&&std::filesystem::is_regular_file(source))return std::filesystem::absolute(source).lexically_normal();
        auto plan=prepare(source);const auto named=directory/source.filename();
        if(!std::filesystem::exists(named)){plan.file=named;plan.reused=false;}
        else if(owns(named)&&std::filesystem::is_regular_file(named)&&digest(named)==plan.digest){plan.file=named;plan.reused=true;}
        return commit(plan);
    }
    Path ensureDirectory() const {std::filesystem::create_directories(directory);return directory;}
};
