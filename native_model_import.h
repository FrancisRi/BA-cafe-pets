#pragma once
#include <windows.h>
#include <commdlg.h>
#include <cderr.h>
#include <cwchar>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

struct NativeModelPicker {
    using Path=std::filesystem::path;
    // Explorer returns one absolute filename, or a directory followed by
    // NUL-separated leaf names. Spaces belong to names, not separators.
    static std::vector<Path> parse(const wchar_t* buffer,size_t capacity){
        if(!buffer||capacity==0)return {};
        size_t offset=0;
        const auto next=[&](){
            const auto* end=std::wmemchr(buffer+offset,L'\0',capacity-offset);
            if(!end)throw std::runtime_error("模型选择结果不完整，请重新选择");
            std::wstring value(buffer+offset,end);offset=size_t(end-buffer)+1;return value;
        };
        const auto first=next();if(first.empty())return {};
        if(offset>=capacity||buffer[offset]==L'\0')return {Path(first)};
        std::vector<Path> files;
        while(offset<capacity&&buffer[offset]!=L'\0'){
            const Path leaf(next());if(leaf.has_parent_path()||leaf.has_root_path())throw std::runtime_error("模型选择结果包含无效文件名");
            files.push_back(Path(first)/leaf);
        }
        if(offset>=capacity)throw std::runtime_error("模型选择结果不完整，请重新选择");
        return files;
    }
    static std::vector<Path> choose(HWND owner){
        std::vector<wchar_t> buffer(65536,L'\0');OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=owner;
        dialog.lpstrTitle=L"导入模型（可按住 Ctrl 或 Shift 多选）";dialog.lpstrFilter=L"GLB 模型\0*.glb\0\0";
        dialog.lpstrFile=buffer.data();dialog.nMaxFile=DWORD(buffer.size());
        dialog.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(GetOpenFileNameW(&dialog))return parse(buffer.data(),buffer.size());
        const DWORD error=CommDlgExtendedError();if(!error)return {};
        if(error==FNERR_BUFFERTOOSMALL)throw std::runtime_error("选择的文件过多，请分批导入");
        throw std::runtime_error("模型选择窗口未能完成操作，请重试");
    }
};

struct NativeImportBatch {
    struct Outcome {bool success=false,reused=false,displayed=false;std::string error;};
    struct Failure {std::filesystem::path source;std::string error;};
    struct Report {size_t added=0,reused=0,hidden=0;std::vector<Failure> failures;size_t successful() const{return added+reused;}};
    template<class Import> static Report run(const std::vector<std::filesystem::path>& files,Import import){
        Report report;
        for(const auto& source:files){
            try{
                const auto result=import(source);
                if(!result.success){report.failures.push_back({source,result.error});continue;}
                if(result.reused)++report.reused;else ++report.added;
                if(!result.displayed)++report.hidden;
            }catch(const std::exception& failure){report.failures.push_back({source,failure.what()});}
        }
        return report;
    }
};
