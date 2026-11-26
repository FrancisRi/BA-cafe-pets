#include <windows.h>
#include <windowsx.h>
#include <behaviortree_cpp/contrib/json.hpp>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <set>
using Json=nlohmann::json;
#include "native_text.h"
#include "native_preferences.h"
#include "native_health.h"
#include "native_settings.h"
#include "native_message_bubble.h"
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void CALLBACK submitModelName(HWND parent,UINT,UINT_PTR timer,DWORD){
    HWND dialog{};
    EnumThreadWindows(GetCurrentThreadId(),[](HWND candidate,LPARAM result)->BOOL{
        wchar_t title[48]{};GetWindowTextW(candidate,title,48);
        if(std::wstring_view(title)!=L"重命名模型")return TRUE;
        *reinterpret_cast<HWND*>(result)=candidate;return FALSE;
    },LPARAM(&dialog));
    if(!dialog)return;
    KillTimer(parent,timer);
    SetDlgItemTextW(dialog,100,L"　 ");
    SendMessageW(dialog,WM_COMMAND,IDOK,0);
    if(IsWindow(dialog)){
        SetDlgItemTextW(dialog,100,L"  水着优香　");
        SendMessageW(dialog,WM_COMMAND,IDOK,0);
    }
}
static void checkBubbleRendering(){
    NativeMessageBubble bubble;
    bubble.cleanBounds={4,0,8,2};
    bubble.cleanBackdrop.assign(4*2*4,std::byte{60});
    std::array<std::byte,6*2*4> captured;
    captured.fill(std::byte{230});
    bubble.restoreBackdrop(captured.data(),{3,0,9,2},{4,0,9,1},247);
    check(captured[0]==std::byte{230}&&captured[4]==std::byte{60}&&captured[20]==std::byte{247}&&
          captured[28]==std::byte{230},"Backdrop refresh replaces the bubble's pixels before filtering and keeps clean screen pixels");

    using Phase=NativeMessages::Phase;
    bubble.current=NativeMessages::Message{L"Codex",L"任务开始了哦...",{},Phase::Start};
    const RECT model{250,400,440,600},work{0,0,900,700};
    check(bubble.paint(model,work,96,0,0),"Draw clear bubble text");
    const auto count=size_t(bubble.textLayer->width)*bubble.textLayer->height*4;
    const std::vector<std::byte> clear(bubble.textLayer->bits,bubble.textLayer->bits+count);
    check(bubble.paint(model,work,96,85,14),"Draw blurred bubble text");
    size_t opaque=0;
    for(size_t i=0;i<count;i+=4){
        const auto* text=bubble.textLayer->bits+i;
        const auto* output=bubble.surface->bits+i;
        check(clear[i+3]==text[3],"Opacity and blur do not alter the glyph coverage mask");
        if(text[3]!=std::byte{255})continue;
        ++opaque;
        check(text[0]==text[1]&&text[1]==text[2],"Opaque glyphs use neutral ink");
        check(std::equal(text,text+4,output),"Background filters leave opaque glyph pixels intact");
    }
    check(opaque>20&&NativeMessageBubble::inkFor(20)>NativeMessageBubble::inkFor(230),
        "Rendered text retains solid pixels and uses contrasting ink on light and dark backgrounds");
    bubble.current->body=L"复杂汉字清晰度与换行测试。Mixed English and 中文 12345。";
    size_t lowerCount=0;
    for(UINT dpi:{96u,144u,192u}){
        check(bubble.paint(model,work,dpi,0,0),"DirectWrite draws mixed Chinese and Latin text at monitor DPI");
        check(bubble.textLayer->width==MulDiv(352,dpi,96),"Text surface uses the monitor's full physical width");
        size_t solid=0,edges=0;
        const auto* pixels=bubble.textLayer->bits;
        for(int row=MulDiv(45,dpi,96);row<bubble.textLayer->height;++row)
            for(int col=0;col<bubble.textLayer->width;++col){
                const auto coverage=pixels[(size_t(row)*bubble.textLayer->width+col)*4+3];
                solid+=coverage==std::byte{255};
                edges+=coverage!=std::byte{}&&coverage!=std::byte{255};
            }
        check(solid>lowerCount&&edges>20,"Higher DPI adds samples to solid glyph strokes and preserves anti-aliased edges");
        lowerCount=solid;
    }
}
static std::string contents(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);
    check(bool(input),"Open desktop integration fixture");
    return std::string(std::istreambuf_iterator<char>(input),{});
}
static void checkCodexConnection(NativeSettings& menu){
    namespace fs=std::filesystem;
    const auto root=fs::current_path()/L"artifacts"/(L"ba-codex-connect-"+std::to_wstring(GetCurrentProcessId()));
    const auto launcher=root/L"BA Cafe 中文"/L"BA-Cafe-Pets-1.2.0-x64.exe";
    const auto home=root/L"custom-codex-home",legacy=root/L"inline";
    fs::create_directories(launcher.parent_path());fs::create_directories(home);fs::create_directories(legacy);
    std::ofstream(launcher).put('\n');
    const auto file=home/L"hooks.json";
    const auto command=std::string("\"")+utf8(launcher.wstring())+"\" --codex-hook";
    const Json thirdParty={{"type","command"},{"command","other-hook.exe"},{"timeout",10}};
    const Json stop={{"matcher","*"},{"hooks",Json::array({thirdParty,
        {{"type","command"},{"command","C:/Old/BA-Cafe-Pets.exe --codex-hook"}},
        {{"type","command"},{"command","C:/Old/BA-Cafe-Pets-1.1.0-x64.exe --codex-hook"}}})}};
    std::ofstream(file)<<Json{{"description","keep me"},{"hooks",{{"Stop",Json::array({stop})},
        {"PreToolUse",Json::array({{{"hooks",Json::array({thirdParty})}}})}}}}.dump();
    check(SetEnvironmentVariableW(L"CODEX_HOME",home.c_str())&&
        SetEnvironmentVariableW(L"BA_NATIVE_LAUNCHER_PATH",launcher.c_str()),"Configure isolated single-file Codex integration fixture");
    SendMessageW(GetDlgItem(menu.panel,2217),BM_CLICK,0,0);
    const auto edited=contents(file);
    const auto document=Json::parse(edited);
    const auto& hooks=document.at("hooks");
    check(document.at("description")=="keep me"&&hooks.at("PreToolUse")[0]["hooks"][0]==thirdParty,
        "Connecting preserves unrelated hook events and metadata");
    const auto& handlers=hooks.at("Stop")[0]["hooks"];
    check(handlers.size()==2&&handlers[0]==thirdParty&&handlers[1]["command"]==command,
        "Connecting replaces duplicate desktop pet commands and resolves the outer executable");
    check(hooks.at("UserPromptSubmit")[0]["hooks"][0]["command"]==command&&
        hooks.at("Interrupt")[0]["hooks"][0]["command"]==command,"Connect installs all three events");
    wchar_t status[128]{};
    GetWindowTextW(GetDlgItem(menu.panel,2218),status,int(std::size(status)));
    check(std::wstring_view(status).find(L"审核并信任")!=std::wstring_view::npos,"Connect explains Codex trust review");
    SendMessageW(GetDlgItem(menu.panel,2217),BM_CLICK,0,0);
    check(contents(file)==edited,"Connecting twice leaves an identical configuration");
    SetEnvironmentVariableW(L"CODEX_HOME",nullptr);
    SetEnvironmentVariableW(L"BA_NATIVE_LAUNCHER_PATH",nullptr);

    const auto config=legacy/L"config.toml";
    const std::string original="notify = ['other.exe'] # 保留原通知\r\n"
        "[[hooks.Stop]]\r\n"
        "[[hooks.Stop.hooks]]\r\n"
        "type = 'command'\r\n"
        "command = 'C:/Old/BA-Cafe-Pets.exe --codex-hook' # 保留行内注释\r\n"
        "[hooks.state]\r\n";
    std::ofstream(config,std::ios::binary)<<original;
    NativeAgentHooks::connect(legacy,launcher);
    const auto migrated=contents(config);
    check(migrated.starts_with("notify = ['other.exe'] # 保留原通知\r\n")&&
        migrated.find("# 保留行内注释\r\n")!=std::string::npos&&
        migrated.find("C:/Old/BA-Cafe-Pets.exe")==std::string::npos&&
        migrated.find("[[hooks.UserPromptSubmit.hooks]]")!=std::string::npos&&
        migrated.find("[[hooks.Interrupt.hooks]]")!=std::string::npos&&
        !fs::exists(legacy/L"hooks.json"),"Connect updates legacy inline hooks without rewriting surrounding TOML");
    NativeAgentHooks::connect(legacy,launcher);
    check(contents(config)==migrated,"Connecting again leaves migrated inline hooks unchanged");

    const auto fresh=root/L"new-install";
    NativeAgentHooks::connect(fresh,launcher);
    const auto freshHooks=Json::parse(contents(fresh/L"hooks.json")).at("hooks");
    check(freshHooks.size()==3&&freshHooks.at("UserPromptSubmit")[0]["hooks"][0]["command"]==command&&
        !fs::exists(fresh/L"config.toml"),"First-time connection creates a standalone three-event hook file");

    std::ofstream(file,std::ios::binary|std::ios::trunc)<<"{invalid";
    bool rejected=false;
    try{NativeAgentHooks::connect(home,launcher);}catch(const std::exception&){rejected=true;}
    check(rejected&&contents(file)=="{invalid","Malformed user config remains intact");
}
static void checkOtherAgentConnections(){
    namespace fs=std::filesystem;
    using NativeAgentHooks::Preset;
    const auto root=fs::current_path()/L"artifacts"/(L"ba-agent-connect-"+std::to_wstring(GetCurrentProcessId()));
    const auto first=root/L"桌宠's folder"/L"BA-Cafe-Pets.exe",second=root/L"moved"/L"BA-Cafe-Pets.exe";
    fs::create_directories(first.parent_path());fs::create_directories(second.parent_path());
    std::ofstream(first).put('\n');std::ofstream(second).put('\n');
    const auto claude=root/L"claude",cursor=root/L"cursor";
    fs::create_directories(claude);fs::create_directories(cursor);
    const Json external={{"type","command"},{"command","other-hook.exe"}};
    const auto claudeFile=claude/L"settings.json",cursorFile=cursor/L"hooks.json";
    std::ofstream(claudeFile)<<Json{{"model","existing"},{"hooks",{{"Stop",Json::array({{{"hooks",Json::array({external})}}})}}}}.dump();
    const Json cursorHook={{"command","./other.ps1"}};
    std::ofstream(cursorFile)<<Json{{"version",1},{"hooks",{{"stop",Json::array({cursorHook})}}}}.dump();
    NativeAgentHooks::connect(claude,first,Preset::ClaudeCode);
    NativeAgentHooks::connect(cursor,first,Preset::Cursor);
    const auto claudeWritten=contents(claudeFile),cursorWritten=contents(cursorFile);
    const auto claudeConfig=Json::parse(claudeWritten),cursorConfig=Json::parse(cursorWritten);
    const auto& claudeHooks=claudeConfig.at("hooks");
    check(claudeConfig.at("model")=="existing"&&claudeHooks.at("Stop")[0]["hooks"][0]==external&&
        claudeHooks.at("UserPromptSubmit")[0]["hooks"][0]["args"]==Json::array({"--agent-hook","claude-code"})&&
        claudeHooks.at("StopFailure").size()==1,"Claude Code preset adds executable hooks and preserves user settings");
    const auto& cursorHooks=cursorConfig.at("hooks");
    check(cursorHooks.at("stop")[0]==cursorHook&&cursorHooks.at("stop").size()==2&&
        cursorHooks.at("afterAgentResponse")[0]["command"].get<std::string>().find("桌宠''s folder")!=std::string::npos,
        "Cursor preset preserves existing hooks and escapes PowerShell paths");
    check(NativeAgentHooks::notificationExample(first).find(L"桌宠''s folder")!=std::wstring::npos&&
        NativeAgentHooks::notificationExample(first).ends_with(L"--notify --stdin"),"The copied example invokes the portable launcher");
    NativeAgentHooks::connect(claude,first,Preset::ClaudeCode);
    NativeAgentHooks::connect(cursor,first,Preset::Cursor);
    check(contents(claudeFile)==claudeWritten&&contents(cursorFile)==cursorWritten,"Agent presets are idempotent");
    NativeAgentHooks::connect(claude,second,Preset::ClaudeCode);
    NativeAgentHooks::connect(cursor,second,Preset::Cursor);
    check(Json::parse(contents(claudeFile))["hooks"]["Stop"].size()==2&&
        Json::parse(contents(cursorFile))["hooks"]["stop"].size()==2&&
        contents(claudeFile).find("桌宠") == std::string::npos&&contents(cursorFile).find("桌宠")==std::string::npos,
        "Reconnecting after moving the executable updates only the existing pet hooks");
    std::ofstream(cursorFile,std::ios::trunc)<<"{}broken";
    bool rejected=false;
    try{NativeAgentHooks::connect(cursor,second,Preset::Cursor);}catch(const std::exception&){rejected=true;}
    check(rejected&&contents(cursorFile)=="{}broken","Malformed Agent config remains intact");
}
struct TestDesktop {
    HDESK previous=GetThreadDesktop(GetCurrentThreadId()),isolated{};
    TestDesktop(){const auto name=L"BaCafeUiTests-"+std::to_wstring(GetCurrentProcessId());isolated=CreateDesktopW(name.c_str(),nullptr,nullptr,0,GENERIC_ALL,nullptr);check(isolated&&SetThreadDesktop(isolated),"Create isolated UI test desktop");}
    ~TestDesktop(){SetThreadDesktop(previous);if(isolated)CloseDesktop(isolated);}
};
static void pump(NativeSettings& menu){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(!menu.route(message)){TranslateMessage(&message);DispatchMessageW(&message);}}}
static std::vector<unsigned char> drawButton(NativeSettings& menu,HWND button,UINT state){
    RECT rect{};GetClientRect(button,&rect);auto dc=GetDC(button),memory=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=rect.right;info.bmiHeader.biHeight=-rect.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* pixels{};
    auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(memory,bitmap);
    DRAWITEMSTRUCT item{ODT_BUTTON,UINT(GetDlgCtrlID(button)),0,ODA_DRAWENTIRE,state,button,memory,rect,0};menu.draw(item);
    const size_t size=size_t(rect.right)*rect.bottom*4;auto* start=static_cast<unsigned char*>(pixels);std::vector<unsigned char> image(start,start+size);
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(button,dc);return image;
}
struct ControlPixels {
    HWND window;HDC dc{};HBITMAP bitmap{};HGDIOBJ previous{};unsigned char* pixels{};int width,height;
    explicit ControlPixels(HWND hwnd):window(hwnd){
        RECT rect{};GetClientRect(hwnd,&rect);width=rect.right;height=rect.bottom;auto source=GetDC(hwnd);dc=CreateCompatibleDC(source);
        BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB};void* data{};
        bitmap=CreateDIBSection(source,&info,DIB_RGB_COLORS,&data,nullptr,0);ReleaseDC(hwnd,source);check(bitmap&&dc,"Allocate UI pixel probe");pixels=static_cast<unsigned char*>(data);previous=SelectObject(dc,bitmap);
    }
    ~ControlPixels(){SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);}
    std::vector<unsigned char> read(bool print=false){
        if(print){NativeFluent::fill(dc,{0,0,width,height},RGB(255,0,255));SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_ERASEBKGND);}
        else{auto source=GetDC(window);check(BitBlt(dc,0,0,width,height,source,0,0,SRCCOPY),"Read actual window pixels");ReleaseDC(window,source);}
        GdiFlush();std::vector<unsigned char> image(pixels,pixels+size_t(width)*height*4);for(size_t i=3;i<image.size();i+=4)image[i]=0;return image;
    }
};
static void checkRepainting(NativeSettings& menu){
    for(int appearance:{0,1})for(UINT dpi:{96u,144u,192u}){
        menu.reloadTheme(menu.window,dpi,appearance);SetWindowPos(menu.window,nullptr,0,0,menu.u(920),menu.u(680),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);menu.scrollTo(0);pump(menu);
        RedrawWindow(menu.panel,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);pump(menu);
        const auto slider=GetDlgItem(menu.panel,103);ControlPixels track(slider);
        const auto live=track.read(),printed=track.read(true);
        check(live==printed,"Slider must use the same Fluent drawing for normal and composited repainting");
        RECT thumb{};SendMessageW(slider,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
        std::set<unsigned> shades;const int cx=(thumb.left+thumb.right)/2,cy=track.height/2,radius=menu.u(11);
        for(int py=std::max(0,cy-radius);py<std::min(track.height,cy+radius);++py)for(int px=std::max(0,cx-radius);px<std::min(track.width,cx+radius);++px){const auto index=(size_t(py)*track.width+px)*4;shades.insert(unsigned(live[index])|(unsigned(live[index+1])<<8)|(unsigned(live[index+2])<<16));}
        check(shades.size()>12,"Slider thumb has smooth vector edge coverage at every theme and DPI");
        ControlPixels panel(menu.panel);
        for(int offset:{40,192,288,96,400,240,0}){
            menu.scrollTo(menu.u(offset));pump(menu);const auto incremental=panel.read();
            RedrawWindow(menu.panel,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);pump(menu);
            check(incremental==panel.read(),"Scrolled settings must match a fresh repaint without stale labels or controls");
        }
    }
    menu.reloadTheme(menu.window,96);menu.scrollTo(0);pump(menu);
    const auto slider=GetDlgItem(menu.panel,103);SetFocus(slider);SendMessageW(slider,WM_KEYDOWN,VK_HOME,0);check(SendMessageW(slider,TBM_GETPOS,0,0)==10,"Styled slider retains Home navigation");SendMessageW(slider,WM_KEYDOWN,VK_END,0);check(SendMessageW(slider,TBM_GETPOS,0,0)==300,"Styled slider retains End navigation");
    RECT channel{},bounds{};SendMessageW(slider,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&channel));GetClientRect(slider,&bounds);const int x=channel.left+(channel.right-channel.left)/4,y=bounds.bottom/2;
    SendMessageW(slider,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(slider,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(slider,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    check(SendMessageW(slider,TBM_GETPOS,0,0)<300,"Styled slider retains pointer interaction");SetFocus(menu.window);
}
static void checkOutlineInput(NativeSettings& menu,Json& state,std::vector<Json>& sent){
    const auto edit=menu.outlineValue,slider=GetDlgItem(menu.panel,103);check(edit,"Outline width exposes a native numeric edit");
    const auto text=[&]{wchar_t value[64]{};GetWindowTextW(edit,value,64);return std::wstring(value);};
    SetFocus(menu.window);SendMessageW(edit,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(menu.u(20),menu.u(8)));SendMessageW(edit,WM_LBUTTONUP,0,MAKELPARAM(menu.u(20),menu.u(8)));
    DWORD selectionStart{},selectionEnd{};SendMessageW(edit,EM_GETSEL,reinterpret_cast<WPARAM>(&selectionStart),reinterpret_cast<LPARAM>(&selectionEnd));
    check(GetFocus()==edit&&selectionStart==0&&selectionEnd==3,"Clicking the displayed number focuses the native edit and selects its value");
    ControlPixels frame(menu.outlineHost);const auto focusedFrame=frame.read(true);SetFocus(menu.window);check(focusedFrame!=frame.read(true),"Native number box exposes a visible Fluent focus underline");
    SetFocus(edit);SetWindowTextW(edit,L"1");menu.sync();check(text()==L"1","Background snapshots preserve a partially typed value");
    const auto invalidCount=sent.size();SendMessageW(edit,WM_KEYDOWN,VK_RETURN,0);
    check(sent.size()==invalidCount&&GetFocus()==edit&&GetPropW(edit,L"FluentInputError"),"Invalid Enter retains focus and shows inline validation without saving");
    MSG escape{};escape.hwnd=edit;escape.message=WM_KEYDOWN;escape.wParam=VK_ESCAPE;check(menu.route(escape)&&IsWindowVisible(menu.window),"Esc cancels numeric editing rather than closing settings");
    check(text()==L"175%"&&state["value"]["outlineWidth"]==175,"Esc restores the saved value");
    SetFocus(edit);SetWindowTextW(edit,L"137%");SendMessageW(edit,WM_KEYDOWN,VK_RETURN,0);
    check(state["value"]["outlineWidth"]==137&&SendMessageW(slider,TBM_GETPOS,0,0)==137&&text()==L"137%","Enter synchronizes the saved value, text and slider");
    SetFocus(edit);SetWindowTextW(edit,L"164");SetFocus(GetDlgItem(menu.panel,101));
    check(state["value"]["outlineWidth"]==164,"Leaving a valid numeric input commits the value");
    SetFocus(edit);SetWindowTextW(edit,L"999");SetFocus(menu.window);
    check(state["value"]["outlineWidth"]==164&&text()==L"164%","Leaving an invalid input restores the last valid width");
    SetFocus(edit);SetWindowTextW(edit,L"180");MSG tab{};tab.hwnd=edit;tab.message=WM_KEYDOWN;tab.wParam=VK_TAB;menu.route(tab);
    check(GetFocus()!=edit&&state["value"]["outlineWidth"]==180,"Tab commits the number and follows native focus navigation");
    SendMessageW(slider,TBM_SETPOS,TRUE,201);SendMessageW(menu.panel,WM_HSCROLL,TB_THUMBPOSITION,reinterpret_cast<LPARAM>(slider));
    check(text()==L"201%"&&state["value"]["outlineWidth"]==201,"Dragging updates the editable number");
    state["value"]["outlineMode"]=0;menu.sync();check(!IsWindowEnabled(edit)&&!IsWindowEnabled(slider),"Number and slider are disabled together when outlines are off");
    state["value"]["outlineMode"]=2;state["value"]["outlineWidth"]=175;menu.sync();
    const auto previous=state["value"];menu.command(2210,BN_CLICKED);
    check(sent.back()["type"]=="set_autostart"&&state["startup"]["enabled"]==true&&state["value"]==previous,"Startup uses a separate explicit application-level setting");
    wchar_t name[64]{};GetWindowTextW(GetDlgItem(menu.panel,2210),name,64);check(std::wstring(name).find(L"已开启")!=std::wstring::npos,"Startup toggle exposes its actual state to native accessibility");
    state["busy"]=true;menu.sync();check(IsWindowEnabled(GetDlgItem(menu.panel,2210)),"Model loading does not disable application startup settings");state["busy"]=false;menu.sync();menu.command(2210,BN_CLICKED);check(!state["startup"]["enabled"].get<bool>(),"Startup can be explicitly disabled again");
}
static void checkFlyoutFrame(HWND window,const NativeFluent& theme){
    ControlPixels image(window);NativeFluent::fill(image.dc,{0,0,image.width,image.height},RGB(255,0,255));
    SendMessageW(window,WM_ERASEBKGND,reinterpret_cast<WPARAM>(image.dc),0);GdiFlush();
    check(GetPixel(image.dc,theme.px(12),theme.px(2))==theme.surface,"Flyout erasure must paint its surface before the first frame, without a white flash");
    check(!(GetWindowLongPtrW(window,GWL_STYLE)&(WS_THICKFRAME|WS_BORDER|WS_DLGFRAME)),"Fixed flyouts must not expose a system sizing border");
    COLORREF border{};if(SUCCEEDED(DwmGetWindowAttribute(window,34,&border,sizeof(border))))check(border==0xFFFFFFFEu,"Flyouts use their own themed border instead of a second DWM border");
    RECT client{},outer{};GetClientRect(window,&client);MapWindowPoints(window,nullptr,reinterpret_cast<POINT*>(&client),2);GetWindowRect(window,&outer);
    check(EqualRect(&client,&outer),"The entire flyout is custom client content at the current DPI");
}
static void checkFlyoutThemes(NativeSettings& menu){
    for(int appearance:{0,1})for(UINT dpi:{96u,144u,192u}){
        menu.reloadTheme(menu.quickWindow,dpi,appearance);SetWindowPos(menu.quickWindow,nullptr,0,0,menu.quickUi.px(344),menu.quickUi.px(menu.layoutQuick()),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);pump(menu);
        for(bool active:{false,true}){SendMessageW(menu.quickWindow,WM_NCACTIVATE,active,0);SendMessageW(menu.quickWindow,WM_NCPAINT,1,0);checkFlyoutFrame(menu.quickWindow,menu.quickUi);}
    }
    menu.reloadTheme(menu.quickWindow,GetDpiForSystem());SetWindowPos(menu.quickWindow,nullptr,0,0,menu.quickUi.px(344),menu.quickUi.px(menu.layoutQuick()),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);pump(menu);
}
static void capture(HWND hwnd,const std::filesystem::path& path){
    RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_ERASE);
    RECT r{};GetWindowRect(hwnd,&r);const int width=r.right-r.left,height=r.bottom-r.top;
    auto dc=GetDC(hwnd),memory=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* pixels{};
    auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(memory,bitmap);check(PrintWindow(hwnd,memory,2)!=0,"Capture native UI");
    BITMAPFILEHEADER file{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+width*height*4;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));out.write(static_cast<const char*>(pixels),width*height*4);
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(hwnd,dc);
}
int main(int argc,char** argv){
    try{
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        TestDesktop desktop; // Test fixtures never appear on the user's desktop.
        checkBubbleRendering();
        NativePreferences preferences;Json state={{"name","Shiroko"},{"selectedId","a"},{"activeCount",1},{"battleAvailable",true},{"canFollow",true},{"busy",false},{"error",""},{"percent",0},{"value",preferences.json()}};
        state["modelsDirectory"]="data/models";
        state["startup"]={{"enabled",false},{"available",true}};
        state["value"].update({{"mode","cafe"},{"look",true},{"idleSeconds",30},{"activeSeconds",5}});
        state["characters"]=Json::array();for(int i=0;i<15;++i)state["characters"].push_back({{"id",i==0?"a":std::to_string(i)},{"name",i==0?"Shiroko":i==1?"Yuuka (Sportswear)":"Imported test model "+std::to_string(i+1)},{"path","fixtures/model-"+std::to_string(i)+".glb"},{"active",i==0},{"available",i!=14},{"ready",i==0},{"mode","cafe"}});
        std::vector<Json> sent;std::vector<bool> visibility,pauses;
        NativeSettings menu;menu.snapshot=[&]{return state;};menu.visibility=[&](bool value){visibility.push_back(value);};
        menu.pauseChanged=[&](bool value){pauses.push_back(value);};
        menu.receive=[&](const Json& message){sent.push_back(message);const auto type=message.at("type");if(type=="set_autostart")state["startup"]["enabled"]=message["enabled"];if(type=="save")state["value"]=message.at("value");if(type=="bubble_settings")for(const auto* key:{"bubbleOpacity","bubbleBlur"})state["value"][key]=message.at(key);if(type=="roster_toggle")for(auto& row:state["characters"])if(row["id"]==message["id"]){row["active"]=message["active"];state["activeCount"]=state["activeCount"].get<int>()+(message["active"].get<bool>()?1:-1);}if(type=="roster_select")state["selectedId"]=message.at("id");};
        menu.sync();check(!menu.visible(),"Initial sync creates no windows");
        menu.showQuick(nullptr,std::filesystem::current_path());pump(menu);
        checkFlyoutFrame(menu.quickWindow,menu.quickUi);
        checkFlyoutThemes(menu);
        check(IsWindowVisible(menu.quickWindow),"Right-click entry opens the compact interface");check(!menu.window,"Opening quick menu does not create full settings");
        check(GetWindowLongPtrW(menu.quickWindow,GWL_EXSTYLE)&WS_EX_TOPMOST,"Pet quick menu remains above topmost pets");
        check(menu.pausesPets()&&pauses==std::vector<bool>{true},"Quick menu pauses pets for stable interaction");
        check(SendMessageW(menu.quickList,LB_GETCOUNT,0,0)==15,"Quick roster includes all fifteen imported models");
        state["characters"].push_back({{"name","Unimported placeholder"},{"active",false}});menu.sync();check(SendMessageW(menu.quickList,LB_GETCOUNT,0,0)==15,"Quick menu excludes placeholders without import metadata");state["characters"].erase(15);
        const auto untouched=state["value"];menu.command(3002,BN_CLICKED);check(state["value"]["follow"]==true,"Quick follow saves to the existing host");
        check(state["value"]["renderQuality"]==untouched["renderQuality"]&&state["value"]["followNear"]==untouched["followNear"],"Quick commands preserve unrelated settings");
        check(IsWindowVisible(menu.quickWindow),"Quick commands keep the menu open for multiple edits");
        RECT modelRow{};check(SendMessageW(menu.quickList,LB_GETITEMRECT,1,reinterpret_cast<LPARAM>(&modelRow))!=LB_ERR,"Locate the hidden model row");
        const auto modelPoint=MAKELPARAM(modelRow.left+menu.quickUi.px(60),(modelRow.top+modelRow.bottom)/2);
        const auto click=[&](UINT down){SendMessageW(menu.quickList,down,MK_LBUTTON,modelPoint);SendMessageW(menu.quickList,WM_LBUTTONUP,0,modelPoint);};
        const auto beforeClick=sent.size();
        click(WM_LBUTTONDOWN);check(sent.size()==beforeClick,"A single click only selects a quick roster row");
        click(WM_LBUTTONDBLCLK);check(sent.size()==beforeClick+1&&state["characters"][1]["active"]==true,"Double-click shows the clicked model once");
        click(WM_LBUTTONDOWN);click(WM_LBUTTONDBLCLK);check(sent.size()==beforeClick+2&&state["characters"][1]["active"]==false,"Double-click hides the clicked model once");
        SendMessageW(menu.quickList,LB_SETCURSEL,1,0);SendMessageW(menu.quickList,WM_KEYDOWN,VK_SPACE,0);check(state["characters"][1]["active"]==true,"Space toggles the focused imported model");
        const auto count=sent.size();menu.toggleRoster(14);check(sent.size()==count,"Missing model cannot be activated");
        state["activeCount"]=5;menu.sync();menu.toggleRoster(2);check(sent.size()==count,"Five-model cap is respected");menu.toggleRoster(1);check(state["characters"][1]["active"]==false,"Models can still be hidden at the cap");state["activeCount"]=1;
        state["battleAvailable"]=false;menu.sync();const auto before=sent.size();menu.command(3001,BN_CLICKED);check(sent.size()==before,"Unsupported battle mode is disabled");
        state["battleAvailable"]=true;menu.sync();menu.command(3001,BN_CLICKED);check(state["value"]["mode"]=="battle","Quick mode switch uses existing save protocol");
        state["formationAvailable"]=true;menu.sync();menu.command(3006,BN_CLICKED);
        check(state["value"]["mode"]=="formation"&&menu.quickChecked(3006)&&!menu.quickChecked(3000),"Quick formation mode is independently selected");
        const auto formationSaved=state["value"];for(int id:{3002,3003,3004,3005}){check(!IsWindowEnabled(GetDlgItem(menu.quickWindow,id)),"Formation disables movement and tracking toggles");menu.command(id,BN_CLICKED);}
        check(state["value"]==formationSaved,"Disabled formation toggles preserve existing behavior settings");
        check(IsWindowEnabled(GetDlgItem(menu.quickWindow,3007))&&menu.quickChecked(3007),"Formation exposes the enabled global rotation switch in the quick menu");
        menu.command(3007,BN_CLICKED);check(!state["value"]["rightDragRotation"].get<bool>()&&!menu.quickChecked(3007),"Quick rotation switch saves while in formation");menu.command(3007,BN_CLICKED);
        menu.command(3000,BN_CLICKED);check(state["value"]["mode"]=="cafe"&&state["value"]["follow"]==formationSaved["follow"],"Leaving formation restores the saved follow preference");
        for(const char* mode:{"cafe","battle","formation"}){
            state["value"]["mode"]=mode;menu.sync();const auto options=state["value"];
            menu.command(3008,BN_CLICKED);check(menu.quickChecked(3008)&&state["value"]["mode"]==mode,"Quick inspection is an independent toggle in every primary mode");
            const auto enabled=state["value"];for(int id:{3002,3003,3004,3005,3007}){check(!IsWindowEnabled(GetDlgItem(menu.quickWindow,id))&&!menu.quickChecked(id),"Inspection displays blocked behavior and normal rotation as disabled/off");menu.command(id,BN_CLICKED);}
            check(state["value"]==enabled,"Blocked quick controls cannot overwrite saved preferences");
            menu.command(3008,BN_CLICKED);check(state["value"]==options,"Disabling inspection restores quick toggle values without changing the primary mode");
        }
        state["value"]["mode"]="cafe";menu.sync();
        state["battleAvailable"]=false;menu.sync();check(menu.modeChoices()==std::vector<std::string>({"cafe","formation"})&&menu.modeIndex("formation")==1,"Formation combo mapping works without a battle option");
        state["formationAvailable"]=false;menu.sync();const auto noFormation=sent.size();menu.command(3006,BN_CLICKED);check(sent.size()==noFormation,"Missing Formation_Idle disables formation mode");state["formationAvailable"]=state["battleAvailable"]=true;menu.sync();
        state["busy"]=true;menu.sync();check(!IsWindowEnabled(GetDlgItem(menu.quickWindow,3002)),"Loading disables behavior edits");check(IsWindowEnabled(GetDlgItem(menu.quickWindow,3011)),"Full settings remain accessible while loading");state["busy"]=false;menu.sync();
        menu.command(3011,BN_CLICKED);pump(menu);check(IsWindowVisible(menu.window)&&!IsWindowVisible(menu.quickWindow),"Full interface replaces the flyout");
        check(!(GetWindowLongPtrW(menu.window,GWL_EXSTYLE)&WS_EX_TOPMOST),"Full settings must be a normal non-topmost window");
        check(visibility.size()==1&&visibility[0],"Quick-to-full transition keeps interface visibility continuous");
        check(!menu.pausesPets()&&pauses==std::vector<bool>({true,false}),"Switching from quick menu to full settings resumes pets");
        const auto chooseMode=[&](int index){auto combo=GetDlgItem(menu.panel,100);SendMessageW(combo,CB_SETCURSEL,index,0);menu.command(100,CBN_SELCHANGE);pump(menu);};
        chooseMode(2);check(state["value"]["mode"]=="formation","Full interface selects formation through its actual combo/save path");
        const auto savedFollow=state["value"]["follow"];for(const auto& c:menu.controls)if(c.field->kind==0)check(bool(IsWindowEnabled(c.input))==(std::string(c.field->key)=="rightDragRotation"||std::string(c.field->key)=="inspectionMode"),"Formation keeps manual rotation and inspection available and disables autonomous behavior toggles");
        const bool rotation=state["value"]["rightDragRotation"];menu.command(121,BN_CLICKED);check(state["value"]["rightDragRotation"]==!rotation,"Full formation interface saves the global rotation switch");menu.command(121,BN_CLICKED);
        menu.save();check(state["value"]["follow"]==savedFollow,"Saving formation appearance does not overwrite disabled behavior values");
        chooseMode(0);check(state["value"]["mode"]=="cafe"&&state["value"]["follow"]==savedFollow,"Full interface restores cafe preferences");
        state["battleAvailable"]=false;menu.sync();chooseMode(1);check(state["value"]["mode"]=="formation","Full interface maps the second combo choice to formation when battle is unavailable");chooseMode(0);state["battleAvailable"]=true;menu.sync();
        menu.hide();menu.show(nullptr,std::filesystem::current_path());pump(menu);
        check(!(GetWindowLongPtrW(menu.window,GWL_EXSTYLE)&WS_EX_TOPMOST),"Reopening full settings must not promote it to topmost");
        const auto pauseCount=pauses.size();ShowWindow(menu.window,SW_MINIMIZE);pump(menu);ShowWindow(menu.window,SW_RESTORE);pump(menu);
        check(!menu.pausesPets()&&pauses.size()==pauseCount,"Showing, minimizing and restoring full settings never pauses pets");
        SendMessageW(menu.window,WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);
        for(auto button:{GetDlgItem(menu.window,1000),GetDlgItem(menu.window,2200),GetDlgItem(menu.panel,121)}){
            check(drawButton(menu,button,0)==drawButton(menu,button,ODS_FOCUS|ODS_NOFOCUSRECT),"Mouse-clicked buttons must not paint a focus rectangle");
        }
        SendMessageW(menu.window,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);const auto navigation=GetDlgItem(menu.window,1000);check(drawButton(menu,navigation,0)!=drawButton(menu,navigation,ODS_FOCUS),"Keyboard navigation retains a visible Fluent focus cue");
        menu.quickCommand(3013);check(sent.back().at("type")=="open_models_folder","Quick menu opens the persistent model folder");
        size_t fieldCount=0;for(int p=0;p<3;++p){menu.page=p;menu.build();check(!IsWindowVisible(GetDlgItem(menu.window,2200)),"Import is hidden outside the Models page");fieldCount+=menu.controls.size();for(const auto& c:menu.controls)check(IsWindow(c.input),"Every existing setting has a real input control");}
        check(fieldCount==23,"Bubble controls are outside the main settings pages");
        menu.page=0;menu.build();const auto inspectionOptions=state["value"];menu.command(122,BN_CLICKED);
        check(state["value"]["inspectionMode"]==true&&IsWindowEnabled(GetDlgItem(menu.panel,122)),"Full interface enables inspection through its real save path");
        for(const auto& c:menu.controls)if(NativeSettings::inspectionBlocks(c.field->key))check(!IsWindowEnabled(c.input)&&!c.checked,"Full interface blocks overridden behavior without erasing its saved value");
        menu.save();menu.command(122,BN_CLICKED);check(state["value"]==inspectionOptions,"Full interface restores original values after inspection and an intervening save");
        check(!GetDlgItem(menu.panel,114)&&!GetDlgItem(menu.panel,115),"Global settings exclude cafe gaze tracking");
        SendMessageW(GetDlgItem(menu.window,1001),BM_CLICK,0,0);
        wchar_t movementTitle[32]{};
        GetWindowTextW(GetDlgItem(menu.window,1001),movementTitle,int(std::size(movementTitle)));
        const auto gaze=GetDlgItem(menu.panel,115);
        check(menu.page==1&&std::wstring_view(movementTitle)==L"移动"&&GetDlgItem(menu.panel,114)&&gaze,
            "Movement navigation contains the cafe gaze controls");
        SendMessageW(gaze,CB_SETCURSEL,1,0);menu.command(115,CBN_SELCHANGE);
        check(state["value"]["cafeLookMode"]=="threshold","Movement saves the cafe gaze mode through its selector");
        check(!GetDlgItem(menu.panel,2215)&&!GetDlgItem(menu.panel,2216),"Movement settings exclude bubble controls");
        const auto messageTab=GetDlgItem(menu.window,1000+NativeSettings::messagesPage);
        SendMessageW(messageTab,BM_CLICK,0,0);
        const auto opacitySlider=GetDlgItem(menu.panel,2215);
        const auto blurSlider=GetDlgItem(menu.panel,2216);
        check(menu.page==NativeSettings::messagesPage&&IsWindowVisible(opacitySlider)&&IsWindowVisible(blurSlider),
            "The left navigation opens the Codex message page in the main panel");
        check(!IsWindowVisible(GetDlgItem(menu.window,2200)),"Import is hidden on the Codex page");
        checkCodexConnection(menu);
        checkOtherAgentConnections();
        SendMessageW(opacitySlider,TBM_SETPOS,TRUE,63);
        SendMessageW(blurSlider,TBM_SETPOS,TRUE,14);
        SendMessageW(menu.panel,WM_HSCROLL,TB_THUMBPOSITION,reinterpret_cast<LPARAM>(blurSlider));
        check(state["value"]["bubbleOpacity"]==63&&state["value"]["bubbleBlur"]==14,
            "The message page saves both bubble preferences");
        check(sent.back().at("type")=="bubble_settings","The message page saves independently of global settings");
        SendMessageW(GetDlgItem(menu.panel,2214),BM_CLICK,0,0);
        check(sent.back().at("type")=="preview_bubble","The preview button requests a real bubble");
        menu.page=1;menu.build();SetWindowTextW(GetDlgItem(menu.panel,108),L"not a number");const auto invalidCount=sent.size();menu.save();check(sent.size()==invalidCount,"Invalid numeric input is not submitted");
        SetWindowTextW(GetDlgItem(menu.panel,108),L"45");menu.save();check(state["value"]["idleSeconds"]==45,"Movement still saves edited numeric values");
        menu.page=0;menu.build();checkRepainting(menu);
        const auto quality=GetDlgItem(menu.panel,101);wchar_t qualityLabel[80]{};SendMessageW(quality,CB_GETLBTEXT,2,reinterpret_cast<LPARAM>(qualityLabel));check(std::wstring(qualityLabel).find(L"4× MSAA")!=std::wstring::npos,"Quality choice explicitly identifies MSAA");
        const auto samplingLabel=GetDlgItem(menu.panel,2111);check(samplingLabel,"Global settings expose actual sampling status");
        const auto samplingText=[&]{wchar_t text[256]{};GetWindowTextW(samplingLabel,text,256);return std::wstring(text);};
        state["renderSampling"]={{"visible",true},{"ready",false},{"scale",2},{"msaa",4}};menu.sync();check(samplingText().find(L"正在应用")!=std::wstring::npos,"Unrendered settings are not reported as enabled");
        state["renderSampling"]["ready"]=true;menu.sync();check(samplingText().find(L"已生效：2× 超采样 + 4× MSAA")!=std::wstring::npos,"Display actual allocated sample counts");
        state["renderSampling"]["msaa"]=0;menu.sync();check(samplingText().find(L"自动降低采样")!=std::wstring::npos,"Unsupported or oversized surfaces visibly report fallback");
        state["renderSampling"]["visible"]=false;menu.sync();check(samplingText().find(L"已隐藏")!=std::wstring::npos,"Hidden models do not claim to be rendering");
        state["renderSampling"]={{"visible",true},{"ready",true},{"scale",2},{"msaa",4}};menu.sync();
        for(UINT dpi:{96u,144u,192u}){
            menu.reloadTheme(menu.window,dpi);SetWindowPos(menu.window,nullptr,0,0,menu.u(800),menu.u(680),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);pump(menu);
            auto combo=GetDlgItem(menu.panel,101);RECT bounds{};GetClientRect(combo,&bounds);auto dc=GetDC(combo);auto previous=SelectObject(dc,menu.ui.font);SIZE extent{};GetTextExtentPoint32W(dc,qualityLabel,int(std::wcslen(qualityLabel)),&extent);SelectObject(dc,previous);ReleaseDC(combo,dc);check(extent.cx+menu.u(32)<bounds.right,"MSAA option fits without clipping at every DPI");
        }
        menu.reloadTheme(menu.window,96);
        SendMessageW(GetDlgItem(menu.panel,101),CB_SETCURSEL,0,0);menu.command(101,CBN_SELCHANGE);check(state["value"]["renderQuality"]==0,"Styled combo preserves native selection commands");
        SendMessageW(GetDlgItem(menu.panel,103),TBM_SETPOS,TRUE,175);SendMessageW(menu.panel,WM_HSCROLL,TB_THUMBPOSITION,reinterpret_cast<LPARAM>(GetDlgItem(menu.panel,103)));check(state["value"]["outlineWidth"]==175,"Styled slider retains native value/drag behavior");
        checkOutlineInput(menu,state,sent);
        menu.page=3;menu.build();check(IsWindowVisible(GetDlgItem(menu.window,2200)),"Import is visible on the Models page");
        check(SendMessageW(menu.list,LB_GETCOUNT,0,0)==15,"Full roster preserves the entire import history");
        const auto lastRowVisible=[&]{RECT row{},client{},viewport{};const auto count=SendMessageW(menu.list,LB_GETCOUNT,0,0);SendMessageW(menu.list,LB_GETITEMRECT,count-1,reinterpret_cast<LPARAM>(&row));GetClientRect(menu.list,&client);if(row.top<0||row.bottom>client.bottom)return false;MapWindowPoints(menu.list,menu.panel,reinterpret_cast<POINT*>(&row),2);GetClientRect(menu.panel,&viewport);return row.top>=0&&row.bottom<=viewport.bottom;};
        for(UINT dpi:{96u,120u,144u,168u,192u}){
            menu.reloadTheme(menu.window,dpi);SetWindowPos(menu.window,nullptr,0,0,menu.u(920),menu.u(680),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);pump(menu);menu.scrollTo(0);
            for(int wheel=0;wheel<20;++wheel)SendMessageW(menu.list,WM_MOUSEWHEEL,MAKEWPARAM(0,SHORT(-WHEEL_DELTA)),0);
            check(lastRowVisible(),"Scrolling the model list to the bottom reveals the entire last row inside the page viewport");
            menu.scrollTo(0);SendMessageW(menu.list,WM_KEYDOWN,VK_END,0);check(lastRowVisible(),"Keyboard End reveals the entire last model at every DPI");
            menu.scrollTo(0);SendMessageW(menu.list,LB_SETTOPINDEX,0,0);SendMessageW(menu.list,WM_VSCROLL,SB_BOTTOM,0);check(lastRowVisible(),"The list scrollbar can also reveal the complete final model");
        }
        menu.reloadTheme(menu.window,96);menu.scrollTo(0);SendMessageW(menu.list,LB_SETCURSEL,0,0);SendMessageW(menu.list,LB_SETTOPINDEX,0,0);
        state["characters"][1]["managed"]=true;
        RECT clicked{};SendMessageW(menu.list,LB_GETITEMRECT,1,reinterpret_cast<LPARAM>(&clicked));POINT location{clicked.left+menu.u(80),clicked.top+menu.u(20)};ClientToScreen(menu.list,&location);
        SendMessageW(menu.list,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(menu.list),MAKELPARAM(location.x,location.y));pump(menu);
        check(menu.contextId=="1"&&menu.highlighted=="1"&&SendMessageW(menu.list,LB_GETCURSEL,0,0)==1,"Right-click selects the pointed model rather than the current actor");
        check(IsWindowVisible(menu.modelMenu)&&IsWindowEnabled(GetDlgItem(menu.modelMenu,4003)),"Selected local models expose deletion in the Fluent context menu");
        check(IsWindowEnabled(GetDlgItem(menu.modelMenu,4004))&&IsWindowEnabled(GetDlgItem(menu.panel,2109)),
            "Model management provides rename from the row actions and context menu");
        checkFlyoutFrame(menu.modelMenu,menu.modelUi);
        check(!menu.pausesPets(),"Model management context menu never pauses pets");
        const auto renameTimer=SetTimer(menu.window,4500,15,submitModelName);
        SendMessageW(GetDlgItem(menu.modelMenu,4004),BM_CLICK,0,0);
        KillTimer(menu.window,renameTimer);
        check(sent.back()==Json{{"type","model_rename"},{"id","1"},{"name","水着优香"}},
            "The real rename dialog rejects an empty name, trims spaces, and submits the right-clicked model ID");
        SendMessageW(menu.list,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(menu.list),MAKELPARAM(-1,-1));pump(menu);
        menu.modelCommand(4000);check(sent.back()["type"]=="roster_toggle"&&sent.back()["id"]=="1","Context actions address the right-clicked model");
        SendMessageW(menu.list,LB_SETCURSEL,1,0);SendMessageW(menu.list,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(menu.list),MAKELPARAM(-1,-1));pump(menu);
        check(menu.contextId=="1"&&IsWindowVisible(menu.modelMenu),"Keyboard context-menu invocation uses the selected model");
        MSG escape{};escape.hwnd=GetDlgItem(menu.modelMenu,4000);escape.message=WM_KEYDOWN;escape.wParam=VK_ESCAPE;check(menu.route(escape)&&!IsWindowVisible(menu.modelMenu)&&IsWindowVisible(menu.window),"Esc dismisses only the model context menu");
        menu.command(2106,BN_CLICKED);check(sent.back()["type"]=="refresh_models","Model management refreshes the actual local model library");
        menu.command(2107,BN_CLICKED);check(sent.back()["type"]=="change_models_folder","Model management opens the folder selector");
        menu.command(2108,BN_CLICKED);check(sent.back()["type"]=="reset_models_folder","Model management restores the default folder");
        state["characters"][1]["active"]=false;state["activeCount"]=1;menu.sync();
        menu.command(2105,BN_CLICKED);check(sent.back().at("type")=="open_models_folder","Model management opens the same model folder");
        state["characters"].push_back({{"name","Unimported placeholder"},{"active",false}});menu.sync();check(SendMessageW(menu.list,LB_GETCOUNT,0,0)==15,"Full roster excludes unimported placeholders");state["characters"].erase(15);
        menu.highlighted="1";state["selectedId"]="1";state["characters"][1]["active"]=true;state["activeCount"]=2;menu.sync();
        state["selectedId"]="a";state["characters"][1]["active"]=false;state["activeCount"]=1;menu.sync();
        check(menu.highlighted=="1"&&SendMessageW(menu.list,LB_GETCURSEL,0,0)==1,"Hiding the current model preserves its list selection for showing it again");
        for(UINT dpi:{96u,144u,192u}){
            menu.reloadTheme(menu.window,dpi);SetWindowPos(menu.window,nullptr,0,0,menu.u(920),menu.u(680),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);pump(menu);
            RECT panel{};GetClientRect(menu.panel,&panel);
            for(auto h:menu.content){RECT r{};GetWindowRect(h,&r);MapWindowPoints(nullptr,menu.panel,reinterpret_cast<POINT*>(&r),2);if(r.left<0||r.right>panel.right+2)std::cerr<<"Layout: dpi="<<dpi<<" id="<<GetDlgCtrlID(h)<<" left="<<r.left<<" right="<<r.right<<" panel="<<panel.right<<'\n';check(r.left>=0&&r.right<=panel.right+2,"Full UI fits at 100/150/200 percent DPI");}
            SendMessageW(messageTab,BM_CLICK,0,0);
            RECT content{},navigation{};
            GetWindowRect(menu.panel,&content);GetWindowRect(messageTab,&navigation);
            check(navigation.right<=content.left,"The Codex tab is to the left of the main panel");
            for(int id:{2215,2216,2214}){
                const auto h=GetDlgItem(menu.panel,id);
                RECT bounds{};GetWindowRect(h,&bounds);
                check(IsWindowVisible(h)&&bounds.left>=content.left&&bounds.right<=content.right&&bounds.bottom<=content.bottom,
                    "The Codex sliders and preview fit inside the message page");
            }
            menu.command(1003,BN_CLICKED);pump(menu);
        }
        menu.showQuick(nullptr,std::filesystem::current_path());pump(menu);
        check(menu.pausesPets()&&pauses.back(),"Quick menu can pause pets while full settings remain open");
        menu.hideQuick();pump(menu);
        check(menu.visible()&&!menu.pausesPets()&&!pauses.back(),"Closing quick menu resumes pets even with full settings still visible");
        menu.hide();check(!menu.visible()&&!visibility.back(),"Closing settings reports hidden interface");
        if(argc>1){
            const auto output=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(output);
            // Preview only actual supplied import history, never synthetic data.
            state["characters"]=Json::array();state["activeCount"]=0;state["name"]="桌面角色";
            if(argc>2){const auto library=std::filesystem::absolute(argv[2]).parent_path()/L"models";const auto path=library.u8string();state["modelsDirectory"]=std::string(path.begin(),path.end());std::ifstream history(argv[2]);const auto imported=Json::parse(history);state["selectedId"]=imported.value("selected",std::string{});for(auto row:imported.at("characters")){row["available"]=std::filesystem::is_regular_file(wide(row.value("path",std::string{})));row["ready"]=row.value("active",false);state["characters"].push_back(row);if(row.value("active",false))state["activeCount"]=state["activeCount"].get<int>()+1;if(row["id"]==state["selectedId"]){state["name"]=row["name"];state["value"]["mode"]=row.value("mode","cafe");}}}
            menu.sync();
            for(auto& row:state["characters"])row["managed"]=std::filesystem::path(wide(row.value("path",std::string{}))).parent_path().lexically_normal()==std::filesystem::path(wide(state.value("modelsDirectory",std::string{}))).lexically_normal();
            menu.reloadTheme(menu.window,GetDpiForSystem());menu.page=0;menu.show(nullptr,std::filesystem::current_path());pump(menu);capture(menu.window,output/L"settings-global.bmp");
            menu.scrollTo(menu.u(menu.contentHeight));pump(menu);capture(menu.window,output/L"settings-app.bmp");menu.scrollTo(0);
            SetFocus(menu.outlineValue);pump(menu);capture(menu.window,output/L"settings-number-edit.bmp");menu.cancelOutline();SetFocus(menu.window);
            for(int p=1;p<4;++p){menu.page=p;menu.build();pump(menu);capture(menu.window,output/(std::wstring(L"settings-")+std::to_wstring(p)+L".bmp"));}
            if(SendMessageW(menu.list,LB_GETCOUNT,0,0)>0){size_t chosen=0;for(size_t i=0;i<state["characters"].size();++i)if(state["characters"][i].value("managed",false)){chosen=i;break;}SendMessageW(menu.list,LB_SETCURSEL,chosen,0);menu.showModelMenu(menu.list,MAKELPARAM(-1,-1));pump(menu);capture(menu.modelMenu,output/L"model-context.bmp");menu.hideModelMenu();}
            SendMessageW(messageTab,BM_CLICK,0,0);pump(menu);capture(menu.window,output/L"settings-codex.bmp");
            menu.hide();menu.showQuick(nullptr,std::filesystem::current_path());pump(menu);capture(menu.quickWindow,output/L"quick-menu.bmp");menu.hide();
            menu.page=0;menu.show(nullptr,std::filesystem::current_path());menu.reloadTheme(menu.window,GetDpiForSystem(),0);pump(menu);capture(menu.window,output/L"settings-light.bmp");
            menu.scrollTo(menu.u(menu.contentHeight));pump(menu);capture(menu.window,output/L"settings-app-light.bmp");menu.hide();
            menu.showQuick(nullptr,std::filesystem::current_path());menu.reloadTheme(menu.quickWindow,GetDpiForSystem(),0);pump(menu);capture(menu.quickWindow,output/L"quick-light.bmp");menu.hide();
        }
        std::cout<<"Native UI regression checks passed: incremental repaint pixels, Fluent sliders, quick roster, caps, mode/follow, loading, full settings, validation, visibility and DPI.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
