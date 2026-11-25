#pragma once
#include "native_fluent.h"
#include <optional>
#include <string_view>

struct NativeModelName {
    static constexpr int maximum=120;
    static bool space(wchar_t c){
        WORD type{};
        return GetStringTypeW(CT_CTYPE1,&c,1,&type)&&(type&C1_SPACE)!=0;
    }
    static std::optional<std::wstring> normalize(std::wstring_view name){
        while(!name.empty()&&space(name.front()))name.remove_prefix(1);
        while(!name.empty()&&space(name.back()))name.remove_suffix(1);
        if(name.empty()||name.size()>maximum||
            std::any_of(name.begin(),name.end(),[](wchar_t c){return c<32||(c>=127&&c<=159)||c==0x2028||c==0x2029;})||
            !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name.data(),int(name.size()),nullptr,0,nullptr,nullptr))
            return {};
        return std::wstring(name);
    }
    static std::optional<std::wstring> edit(HWND owner,const std::wstring& current){
        struct alignas(DWORD) Template {DLGTEMPLATE dialog;WORD menu=0,windowClass=0,title=0;};
        const Template layout{{WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER,0,0,0,0,240,86}};
        State state{current};
        const auto code=DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&layout.dialog,owner,procedure,LPARAM(&state));
        return code==IDOK?std::optional<std::wstring>(std::move(state.value)):std::nullopt;
    }
private:
    struct State {std::wstring value;NativeFluent theme;};
    static INT_PTR CALLBACK procedure(HWND dialog,UINT message,WPARAM w,LPARAM l){
        if(message==WM_INITDIALOG){
            auto& state=*reinterpret_cast<State*>(l);
            SetWindowLongPtrW(dialog,DWLP_USER,l);
            auto& theme=state.theme;
            theme.load(GetDpiForWindow(dialog));theme.frame(dialog);
            SetWindowTextW(dialog,L"重命名模型");
            RECT area{};GetClientRect(dialog,&area);
            const auto px=[&](int value){return theme.px(value);};
            const auto add=[&](const wchar_t* kind,const wchar_t* title,DWORD style,DWORD ex,int id,int x,int y,int width,int height){
                HWND child=CreateWindowExW(ex,kind,title,WS_CHILD|WS_VISIBLE|style,x,px(y),width,px(height),dialog,
                    reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
                SendMessageW(child,WM_SETFONT,WPARAM(theme.font),FALSE);theme.themed(child);return child;
            };
            const int pad=px(16),right=area.right-pad,button=px(84);
            add(L"STATIC",L"模型名称",0,0,0,pad,12,right-pad,20);
            const auto input=add(L"EDIT",state.value.c_str(),WS_TABSTOP|ES_AUTOHSCROLL,WS_EX_CLIENTEDGE,100,pad,36,right-pad,28);
            add(L"STATIC",L"",0,0,101,pad,68,right-pad,20);
            add(L"BUTTON",L"保存",WS_TABSTOP|BS_DEFPUSHBUTTON,0,IDOK,right-2*button-px(8),96,button,30);
            add(L"BUTTON",L"取消",WS_TABSTOP,0,IDCANCEL,right-button,96,button,30);
            SendMessageW(input,EM_SETLIMITTEXT,maximum,0);SendMessageW(dialog,DM_SETDEFID,IDOK,0);
            SetFocus(input);SendMessageW(input,EM_SETSEL,0,-1);return FALSE;
        }
        auto* state=reinterpret_cast<State*>(GetWindowLongPtrW(dialog,DWLP_USER));
        if(!state)return FALSE;
        if(message==WM_CTLCOLORDLG||message==WM_CTLCOLORSTATIC){
            const auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,state->theme.text);SetBkColor(dc,state->theme.canvas);
            return reinterpret_cast<INT_PTR>(state->theme.canvasBrush);
        }
        if(message==WM_CTLCOLOREDIT){
            const auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,state->theme.text);SetBkColor(dc,state->theme.surface);
            return reinterpret_cast<INT_PTR>(state->theme.surfaceBrush);
        }
        if(message==WM_COMMAND&&LOWORD(w)==IDOK){
            const auto input=GetDlgItem(dialog,100);
            const int length=GetWindowTextLengthW(input);
            wchar_t text[maximum+1]{};
            if(length<=maximum){
                GetWindowTextW(input,text,maximum+1);
                if(auto name=normalize(text)){state->value=std::move(*name);EndDialog(dialog,IDOK);return TRUE;}
            }
            SetDlgItemTextW(dialog,101,L"请输入 1–120 个字符的单行名称。");SetFocus(input);return TRUE;
        }
        if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(w)==IDCANCEL)){
            EndDialog(dialog,IDCANCEL);return TRUE;
        }
        return FALSE;
    }
};
