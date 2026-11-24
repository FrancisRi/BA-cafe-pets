#pragma once
#include <deque>
#include <optional>

// Local text ingress. This window receives text only and never takes focus.
struct NativeMessages {
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
    static bool codex(const std::wstring& event){
        const auto json=Json::parse(utf8(event),nullptr,false);
        if(!json.is_object()||!json.contains("type")||!json["type"].is_string())return false;
        if(json["type"]!="agent-turn-complete")return true;
        std::wstring body;
        if(json.contains("last-assistant-message")&&json["last-assistant-message"].is_string())
            body=wide(json["last-assistant-message"].get<std::string>());
        return send(L"Codex · 已完成",body);
    }
};
