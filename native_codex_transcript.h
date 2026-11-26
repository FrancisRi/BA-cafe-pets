#pragma once
#include "native_text.h"
#include <behaviortree_cpp/contrib/json.hpp>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

// Follow only visible commentary records for the turn named by the hook.
struct NativeCodexTranscript {
    using Json=nlohmann::json;
    static constexpr size_t blockSize=64*1024;
    std::string turn,session;
    std::filesystem::path path;
    std::streamoff offset=0;
    ULONGLONG lastRead=0,nextResolve=0;
    bool discard=false,inTurn=false,finished=false;
    std::string partial;

    static std::string_view field(const Json& value,const char* key){
        const auto item=value.find(key);
        return item!=value.end()&&item->is_string()?std::string_view(item->get_ref<const std::string&>()):std::string_view{};
    }
    static std::filesystem::path normalPath(std::wstring value){
        if(value.starts_with(L"\\\\?\\UNC\\"))value.replace(0,8,L"\\\\");
        else if(value.starts_with(L"\\\\?\\")&&value.size()>=7&&value[5]==L':'&&value[6]==L'\\')value.erase(0,4);
        return std::filesystem::path(std::move(value));
    }
    static std::filesystem::path sessionsDirectory(){
        namespace fs=std::filesystem;
        std::wstring home(32768,L'\0');
        DWORD n=GetEnvironmentVariableW(L"CODEX_HOME",home.data(),DWORD(home.size()));
        if(n>=home.size())return {};
        if(!n){
            n=GetEnvironmentVariableW(L"USERPROFILE",home.data(),DWORD(home.size()));
            if(!n||n>=home.size())return {};
            home.resize(n);home+=L"\\.codex";
        }else home.resize(n);
        std::error_code error;
        const auto root=fs::weakly_canonical(normalPath(std::move(home))/L"sessions",error);
        return error||!root.is_absolute()?fs::path{}:root;
    }
    static std::filesystem::path sessionPath(const std::string& raw){
        namespace fs=std::filesystem;
        const auto path=normalPath(wide(raw));
        if(!path.is_absolute()||path.extension()!=L".jsonl"||!path.filename().wstring().starts_with(L"rollout-"))return {};
        const auto root=sessionsDirectory();
        if(root.empty())return {};
        std::error_code error;
        const auto canonical=fs::weakly_canonical(path,error);
        if(error)return {};
        auto actual=canonical.begin();
        for(const auto& part:root){
            if(actual==canonical.end()||CompareStringOrdinal(actual->c_str(),-1,part.c_str(),-1,TRUE)!=CSTR_EQUAL)return {};
            ++actual;
        }
        return canonical;
    }
    static bool validSession(std::string_view id){
        if(id.size()!=36)return false;
        for(size_t i=0;i<id.size();++i){
            const char c=id[i];
            const bool separator=i==8||i==13||i==18||i==23;
            if(separator?c!='-':!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
        }
        return true;
    }
    static bool matchesSession(const std::filesystem::path& file,std::wstring_view suffix){
        if(file.extension()!=L".jsonl")return false;
        const auto stem=file.stem().wstring();
        std::wstring_view name=stem;
        if(!name.starts_with(L"rollout-"))return false;
        // Paginated history appends a page UUID after the session UUID.
        if(name.size()>37&&name[name.size()-37]==L'_'&&validSession(utf8(std::wstring(name.substr(name.size()-36)))))
            name.remove_suffix(37);
        return name.size()>suffix.size()&&CompareStringOrdinal(name.data()+name.size()-suffix.size(),
            int(suffix.size()),suffix.data(),int(suffix.size()),TRUE)==CSTR_EQUAL;
    }
    static std::filesystem::path findSession(const std::string& session,const std::filesystem::path& folder){
        namespace fs=std::filesystem;
        const auto suffix=L"-"+wide(session);
        fs::path latest;
        fs::file_time_type latestWrite{};
        const auto consider=[&](const fs::directory_entry& entry){
            if(!matchesSession(entry.path(),suffix))return;
            std::error_code error;
            if(!entry.is_regular_file(error))return;
            const auto modified=entry.last_write_time(error);
            if(!error&&(latest.empty()||modified>latestWrite||(modified==latestWrite&&entry.path()>latest))){
                latest=entry.path();
                latestWrite=modified;
            }
        };
        std::error_code error;
        if(folder.empty()){
            const auto root=sessionsDirectory();
            if(root.empty())return {};
            for(fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,error),end;
                !error&&it!=end;it.increment(error))consider(*it);
        }else{
            for(fs::directory_iterator it(folder,fs::directory_options::skip_permission_denied,error),end;
                !error&&it!=end;it.increment(error))consider(*it);
        }
        return latest.empty()?fs::path{}:sessionPath(utf8(latest.wstring()));
    }
    NativeCodexTranscript(std::string id,std::filesystem::path source,std::string sessionId,bool atStart):
        turn(std::move(id)),session(std::move(sessionId)),lastRead(GetTickCount64()){setPath(std::move(source),atStart);}
    void setPath(std::filesystem::path source,bool atStart=false){
        if(source.empty()||source==path)return;
        path=std::move(source);
        offset=0;partial.clear();discard=false;inTurn=false;
        // A path discovered after the start hook may already contain progress before the tail.
        if(!atStart)return;
        std::error_code error;
        const auto length=std::filesystem::file_size(path,error);
        if(!error&&length>blockSize){offset=std::streamoff(length-blockSize);discard=true;}
    }
    struct Result {std::vector<std::string> commentary;bool ended=false,caughtUp=true;};
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
        if(!text.empty())result.commentary.push_back(std::move(text));
    }
    bool resolvePage(bool final){
        const auto now=GetTickCount64();
        if(session.empty()||(!final&&now<nextResolve))return false;
        nextResolve=now+1500;
        auto latest=findSession(session,path.parent_path());
        if(latest.empty()||latest==path)return false;
        setPath(std::move(latest));
        return true;
    }
    Result read(){
        Result result;
        std::ifstream file(path,std::ios::binary);
        if(!file.seekg(offset))return result;
        char buffer[blockSize];
        for(int block=0;block<4&&!result.ended;++block){
            file.read(buffer,sizeof(buffer));
            const auto count=file.gcount();
            result.caughtUp=count<std::streamsize(sizeof(buffer));
            if(!count)break;
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
            if(result.caughtUp)break;
        }
        return result;
    }
    Result poll(bool final=false){
        if(finished)return {};
        const bool hadPath=!path.empty();
        if(!hadPath&&!resolvePage(final))return {};
        auto result=read();
        if(hadPath&&result.caughtUp&&!result.ended&&resolvePage(final))result.caughtUp=false;
        return result;
    }
};
