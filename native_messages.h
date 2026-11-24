#pragma once
#include "native_text.h"
#include "native_codex_transcript.h"
#include <behaviortree_cpp/contrib/json.hpp>
#include <algorithm>
#include <deque>
#include <optional>
#include <string_view>

// Local text ingress. This window receives text only and never takes focus.
struct NativeMessages {
    using Json=nlohmann::json;
    enum class Phase {Other,Start,Progress,End};
    struct Message {std::wstring title,body;std::string turn;Phase phase;};
    static constexpr wchar_t endpointClass[]=L"BaCafeNativeMessages.v1";
    static constexpr ULONG_PTR textMessage=0x42414D31;
    static constexpr size_t maxBytes=32768;
    HWND endpoint{};
    std::deque<Message> pending;
    std::deque<NativeCodexTranscript> transcripts;
    ULONGLONG nextPoll=0;

    ~NativeMessages(){if(endpoint)DestroyWindow(endpoint);}
    static std::wstring clean(std::wstring text,size_t limit){
        for(auto& c:text)if(c<32&&c!=L'\n'&&c!=L'\t')c=L' ';
        const auto begin=text.find_first_not_of(L" \t\n");
        if(begin==text.npos)return {};
        text=text.substr(begin,text.find_last_not_of(L" \t\n")-begin+1);
        if(text.size()>limit){
            text.resize(limit-1);
            if(text.back()>=0xd800&&text.back()<=0xdbff)text.pop_back();
            text+=L'…';
        }
        return text;
    }
    bool push(std::wstring title,std::wstring body,std::string turn={},Phase phase=Phase::Other){
        title=clean(std::move(title),180);
        body=clean(std::move(body),1000);
        if(title.empty()&&body.empty())return false;
        if(phase==Phase::Progress&&!turn.empty()){
            for(auto& message:pending)if(message.turn==turn&&message.phase==Phase::Progress){
                message.body=std::move(body);return true;
            }
        }
        if(pending.size()==16)pending.pop_front();
        pending.push_back({std::move(title),std::move(body),std::move(turn),phase});
        return true;
    }
    void progress(NativeCodexTranscript& source){
        const auto result=source.poll();
        if(!result.commentary.empty())push(L"Codex · 进行中",wide(result.commentary),source.turn,Phase::Progress);
        if(result.ended)source.lastRead=0;
    }
    void pollCodex(){
        const auto now=GetTickCount64();
        if(transcripts.empty()||now<nextPoll)return;
        nextPoll=now+400;
        for(auto it=transcripts.begin();it!=transcripts.end();){
            progress(*it);
            if(!it->lastRead||now>it->lastRead+3600000)it=transcripts.erase(it);
            else ++it;
        }
    }
    bool accept(const Json& value){
        const auto title=NativeCodexTranscript::field(value,"title"),body=NativeCodexTranscript::field(value,"body");
        const auto turn=NativeCodexTranscript::field(value,"turn");
        const auto event=NativeCodexTranscript::field(value,"phase");
        if(event=="start"){
            if(!turn.empty()){
                const auto found=std::find_if(transcripts.begin(),transcripts.end(),[&](const auto& t){return t.turn==turn;});
                if(found==transcripts.end()){
                    const auto path=NativeCodexTranscript::sessionPath(std::string(NativeCodexTranscript::field(value,"transcript")));
                    if(!path.empty()){
                        if(transcripts.size()==8)transcripts.pop_front();
                        transcripts.emplace_back(std::string(turn),path);
                    }
                }
            }
            return push(L"Codex",L"任务开始了哦...",std::string(turn),Phase::Start);
        }
        if(event=="end"||event=="interrupt"){
            if(!turn.empty()){
                for(auto it=transcripts.begin();it!=transcripts.end();++it)if(it->turn==turn){
                    progress(*it);transcripts.erase(it);break;
                }
            }
            if(event=="interrupt")return push(L"Codex · 已中断",L"任务已中断。",std::string(turn),Phase::End);
            return push(L"Codex · 已完成",wide(std::string(body)),std::string(turn),Phase::End);
        }
        return push(wide(std::string(title)),wide(std::string(body)));
    }
    static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
        if(msg==WM_NCCREATE){
            const auto* args=reinterpret_cast<const CREATESTRUCTW*>(l);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(args->lpCreateParams));
        }
        auto* self=reinterpret_cast<NativeMessages*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(msg!=WM_COPYDATA||!self)return DefWindowProcW(hwnd,msg,w,l);
        const auto* data=reinterpret_cast<const COPYDATASTRUCT*>(l);
        if(!data||data->dwData!=textMessage||!data->lpData||!data->cbData||data->cbData>maxBytes)return FALSE;
        try{
            const auto* bytes=static_cast<const char*>(data->lpData);
            const auto value=Json::parse(bytes,bytes+data->cbData,nullptr,false);
            if(!value.is_object()||(value.contains("title")&&!value["title"].is_string())||
                (value.contains("body")&&!value["body"].is_string()))return FALSE;
            return self->accept(value);
        }catch(const std::exception&){return FALSE;}
    }
    void open(){
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.lpszClassName=endpointClass;
        RegisterClassW(&wc);
        endpoint=CreateWindowExW(0,endpointClass,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,this);
    }
    static bool send(Json value){
        auto payload=value.dump();
        if(payload.size()>maxBytes)return false;
        COPYDATASTRUCT data{textMessage,DWORD(payload.size()),payload.data()};
        for(HWND target=nullptr;(target=FindWindowExW(HWND_MESSAGE,target,endpointClass,nullptr));){
            DWORD_PTR result=0;
            if(SendMessageTimeoutW(target,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&data),
                SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&result)&&result)return true;
        }
        return false;
    }
    static bool codex(std::string_view event,bool hook){
        const auto json=Json::parse(event,nullptr,false);
        const auto type=NativeCodexTranscript::field(json,hook?"hook_event_name":"type");
        const auto turn=NativeCodexTranscript::field(json,hook?"turn_id":"turn-id");
        if(type.empty())return false;
        if(hook&&type=="UserPromptSubmit"){
            return send({{"phase","start"},{"turn",turn},{"transcript",NativeCodexTranscript::field(json,"transcript_path")}});
        }
        if(hook&&type=="Interrupt")return send({{"phase","interrupt"},{"turn",turn}});
        if(type!=(hook?"Stop":"agent-turn-complete"))return true;
        const auto body=NativeCodexTranscript::field(json,hook?"last_assistant_message":"last-assistant-message");
        return send({{"phase","end"},{"turn",turn},{"title",utf8(L"Codex · 已完成")},
            {"body",utf8(clean(wide(std::string(body)),1000))}});
    }
    static std::string hookInput(){
        const HANDLE input=GetStdHandle(STD_INPUT_HANDLE);
        const DWORD type=GetFileType(input);
        if(type!=FILE_TYPE_PIPE&&type!=FILE_TYPE_DISK)return {};
        constexpr size_t limit=1024*1024;
        std::string event;
        char chunk[4096];
        DWORD count=0;
        while(ReadFile(input,chunk,sizeof(chunk),&count,nullptr)&&count){
            if(event.size()+count>limit)return {};
            event.append(chunk,count);
        }
        return event;
    }
    static std::optional<int> command(int argc,wchar_t* const* argv){
        if(argc<2)return {};
        const std::wstring_view flag=argv[1];
        if(flag==L"--codex-notify")
            return argc==3&&codex(utf8(argv[2]),false)?0:1;
        if(flag!=L"--codex-hook")return {};
        if(argc!=2)return 1;
        codex(hookInput(),true);
        // A display-only hook has no effect on Codex's decisions.
        static constexpr char response[]="{}\n";
        DWORD written=0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),response,sizeof(response)-1,&written,nullptr);
        return 0;
    }
};
