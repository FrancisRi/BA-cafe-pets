#include "native_model_import.h"
#include <iostream>
namespace fs=std::filesystem;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::vector<wchar_t> selection(std::initializer_list<std::wstring> values){std::vector<wchar_t> buffer;for(const auto& value:values){buffer.insert(buffer.end(),value.begin(),value.end());buffer.push_back(0);}buffer.push_back(0);return buffer;}
int main(){
    try{
        auto single=selection({L"F:\\模型\\Shiroko swimsuit.glb"});auto files=NativeModelPicker::parse(single.data(),single.size());check(files.size()==1&&files[0]==fs::path(L"F:\\模型\\Shiroko swimsuit.glb"),"Single-file selection remains compatible");
        auto multiple=selection({L"F:\\本地模型",L"中文角色.glb",L"Yuuka (Sportswear).glb",L"Shiroko.glb"});files=NativeModelPicker::parse(multiple.data(),multiple.size());check(files.size()==3&&files[0]==fs::path(L"F:\\本地模型\\中文角色.glb")&&files[1].filename()==L"Yuuka (Sportswear).glb","Multi-select preserves Unicode, spaces and input order");
        auto unc=selection({L"\\\\server\\models",L"one.glb",L"two.glb"});check(NativeModelPicker::parse(unc.data(),unc.size())[1]==fs::path(L"\\\\server\\models\\two.glb"),"Network directory selection");
        const wchar_t empty[]={0,0};check(NativeModelPicker::parse(empty,2).empty(),"Cancel/empty selection imports nothing");
        const wchar_t truncated[]={L'F',L':',L'\\',L'x'};bool failed=false;try{NativeModelPicker::parse(truncated,4);}catch(const std::exception&){failed=true;}check(failed,"Truncated picker results are rejected");
        std::vector<fs::path> visited;const std::vector<fs::path> batch={L"first.glb",L"bad.glb",L"duplicate.glb",L"stored.glb",L"over-cap.glb",L"last.glb"};
        const auto report=NativeImportBatch::run(batch,[&](const auto& path)->NativeImportBatch::Outcome{
            visited.push_back(path);if(path==L"bad.glb")throw std::runtime_error("invalid GLB");if(path==L"over-cap.glb")return {false,false,false,"history full"};return {true,path==L"duplicate.glb",path!=L"stored.glb",{}};
        });
        check(visited==batch,"Batch continues in order after a bad file or history-cap failure");check(report.added==3&&report.reused==1&&report.hidden==1&&report.successful()==4&&report.failures.size()==2,"Batch totals distinguish new, reused, stored-only and failed selections");check(report.failures[0].source==L"bad.glb"&&report.failures[1].error=="history full","Individual failures retain their file and reason");
        std::cout<<"Model import checks passed: single/multiple paths, Unicode/spaces, cancel, truncation, ordered processing, duplicates, limits and partial failures.\n";return 0;
    }catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}
}
