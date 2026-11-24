#pragma once
#include "native_text.h"
#include <behaviortree_cpp/contrib/json.hpp>
#include <filesystem>
#include <fstream>
#include <string_view>

// Follow only visible commentary records for the turn named by the hook.
struct NativeCodexTranscript {
    using Json=nlohmann::json;
    static constexpr size_t blockSize=64*1024;
    std::string turn;
    std::filesystem::path path;
    std::streamoff offset=0;
    ULONGLONG lastRead=0;
    bool discard=false,inTurn=false;
    std::string partial;

    static std::string_view field(const Json& value,const char* key){
        const auto item=value.find(key);
        return item!=value.end()&&item->is_string()?std::string_view(item->get_ref<const std::string&>()):std::string_view{};
    }
    static std::filesystem::path sessionPath(const std::string& raw){
        namespace fs=std::filesystem;
        const fs::path path=wide(raw);
        if(!path.is_absolute()||path.extension()!=L".jsonl"||!path.filename().wstring().starts_with(L"rollout-"))return {};
        std::wstring home(32768,L'\0');
        DWORD n=GetEnvironmentVariableW(L"CODEX_HOME",home.data(),DWORD(home.size()));
        if(n>=home.size())return {};
        if(!n){
            n=GetEnvironmentVariableW(L"USERPROFILE",home.data(),DWORD(home.size()));
            if(!n||n>=home.size())return {};
            home.resize(n);home+=L"\\.codex";
        }else home.resize(n);
        std::error_code error;
        const auto root=fs::weakly_canonical(fs::path(home)/L"sessions",error);
        if(error||!root.is_absolute())return {};
        const auto canonical=fs::weakly_canonical(path,error);
        if(error)return {};
        auto actual=canonical.begin();
        for(const auto& part:root){
            if(actual==canonical.end()||CompareStringOrdinal(actual->c_str(),-1,part.c_str(),-1,TRUE)!=CSTR_EQUAL)return {};
            ++actual;
        }
        return canonical;
    }
    NativeCodexTranscript(std::string id,std::filesystem::path source):turn(std::move(id)),path(std::move(source)),lastRead(GetTickCount64()){
        std::error_code error;
        const auto length=std::filesystem::file_size(path,error);
        if(!error&&length>blockSize){offset=std::streamoff(length-blockSize);discard=true;}
    }
    struct Result {std::string commentary;bool ended=false;};
    void record(std::string_view line,Result& result){
        const auto value=Json::parse(line,nullptr,false);
        const auto payload=value.find("payload");
        if(payload==value.end()||!payload->is_object())return;
        const auto type=field(*payload,"type");
        if(field(value,"type")=="event_msg"){
            if(type=="task_started"||type=="turn_started")inTurn=field(*payload,"turn_id")==turn;
            if((type=="task_complete"||type=="turn_aborted")&&field(*payload,"turn_id")==turn)result.ended=true;
            return;
        }
        if(field(value,"type")!="response_item"||type!="message"||field(*payload,"role")!="assistant"||
            field(*payload,"phase")!="commentary")return;
        const auto meta=payload->find("internal_chat_message_metadata_passthrough");
        const auto owner=meta==payload->end()?std::string_view{}:field(*meta,"turn_id");
        if(owner.empty()?!inTurn:owner!=turn)return;
        const auto content=payload->find("content");
        if(content==payload->end()||!content->is_array())return;
        std::string text;
        for(const auto& item:*content){
            const auto part=field(item,"text");
            if(field(item,"type")!="output_text"||part.empty())continue;
            if(!text.empty())text+='\n';
            text+=part;
        }
        if(!text.empty())result.commentary=std::move(text);
    }
    Result poll(){
        Result result;
        std::ifstream file(path,std::ios::binary);
        if(!file.seekg(offset))return result;
        char buffer[blockSize];
        file.read(buffer,sizeof(buffer));
        const auto count=file.gcount();
        if(!count)return result;
        offset+=count;
        lastRead=GetTickCount64();
        std::string_view chunk(buffer,size_t(count));
        while(!chunk.empty()&&!result.ended){
            const auto newline=chunk.find('\n');
            const auto part=chunk.substr(0,newline);
            if(!discard&&partial.size()+part.size()<=blockSize)partial+=part;
            else discard=true;
            if(newline==chunk.npos)break;
            if(!discard)record(partial,result);
            partial.clear();discard=false;
            chunk.remove_prefix(newline+1);
        }
        return result;
    }
};
