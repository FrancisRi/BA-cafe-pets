#pragma once
#include "native_text.h"
#include <behaviortree_cpp/contrib/json.hpp>
#include <deque>
#include <optional>
#include <string_view>

// Local text ingress. This window receives text only and never takes focus.
struct NativeMessages {
    using Json=nlohmann::json;
    struct Message {std::wstring title,body;};
    static constexpr wchar_t endpointClass[]=L"BaCafeNativeMessages.v1";
    static constexpr ULONG_PTR textMessage=0x42414D31;
    static constexpr size_t maxBytes=32768;
    HWND endpoint{};
    std::deque<Message> pending;

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
    bool push(std::wstring title,std::wstring body){
        title=clean(std::move(title),180);
        body=clean(std::move(body),1000);
        if(title.empty()&&body.empty())return false;
        if(pending.size()==16)pending.pop_front();
        pending.push_back({std::move(title),std::move(body)});
        return true;
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
            return self->push(wide(value.value("title",std::string{})),wide(value.value("body",std::string{})));
        }catch(const std::exception&){return FALSE;}
    }
    void open(){
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.lpszClassName=endpointClass;
        RegisterClassW(&wc);
        endpoint=CreateWindowExW(0,endpointClass,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,this);
    }
    static bool send(const std::wstring& title,const std::wstring& body){
        auto payload=Json{{"title",utf8(clean(title,180))},{"body",utf8(clean(body,1000))}}.dump();
        COPYDATASTRUCT data{textMessage,DWORD(payload.size()),payload.data()};
        for(HWND target=nullptr;(target=FindWindowExW(HWND_MESSAGE,target,endpointClass,nullptr));){
            DWORD_PTR result=0;
            if(SendMessageTimeoutW(target,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&data),
                SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&result)&&result)return true;
        }
        return false;
    }
    static bool codex(std::string_view event,const char* typeKey,std::string_view expected,const char* bodyKey){
        const auto json=Json::parse(event,nullptr,false);
        const auto type=json.find(typeKey);
        if(type==json.end()||!type->is_string())return false;
        if(type->get_ref<const std::string&>()!=expected)return true;
        std::wstring body;
        const auto content=json.find(bodyKey);
        if(content!=json.end()&&content->is_string())body=wide(content->get_ref<const std::string&>());
        return send(L"Codex · 已完成",body);
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
            return argc==3&&codex(utf8(argv[2]),"type","agent-turn-complete","last-assistant-message")?0:1;
        if(flag!=L"--codex-hook")return {};
        if(argc!=2)return 1;
        codex(hookInput(),"hook_event_name","Stop","last_assistant_message");
        // This handler only displays a message. Stop expects a JSON response.
        static constexpr char response[]="{}\n";
        DWORD written=0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),response,sizeof(response)-1,&written,nullptr);
        return 0;
    }
};
