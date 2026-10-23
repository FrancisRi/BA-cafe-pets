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
using Json=nlohmann::json;
std::wstring wide(const std::string& text){const int count=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);std::wstring out(count,L' ');MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),out.data(),count);return out;}
#include "native_preferences.h"
#include "native_health.h"
#include "native_settings.h"
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
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
        NativePreferences preferences;Json state={{"name","Shiroko"},{"selectedId","a"},{"activeCount",1},{"battleAvailable",true},{"canFollow",true},{"busy",false},{"error",""},{"percent",0},{"value",preferences.json()}};
        state["modelsDirectory"]="data/models";
        state["value"].update({{"mode","cafe"},{"look",true},{"idleSeconds",30},{"activeSeconds",5}});
        state["characters"]=Json::array();for(int i=0;i<15;++i)state["characters"].push_back({{"id",i==0?"a":std::to_string(i)},{"name",i==0?"Shiroko":i==1?"Yuuka (Sportswear)":"Imported test model "+std::to_string(i+1)},{"path","fixtures/model-"+std::to_string(i)+".glb"},{"active",i==0},{"available",i!=14},{"ready",i==0},{"mode","cafe"}});
        std::vector<Json> sent;std::vector<bool> visibility,pauses;
        NativeSettings menu;menu.snapshot=[&]{return state;};menu.visibility=[&](bool value){visibility.push_back(value);};
        menu.pauseChanged=[&](bool value){pauses.push_back(value);};
        menu.receive=[&](const Json& message){sent.push_back(message);const auto type=message.at("type");if(type=="save")state["value"]=message.at("value");if(type=="roster_toggle")for(auto& row:state["characters"])if(row["id"]==message["id"]){row["active"]=message["active"];state["activeCount"]=state["activeCount"].get<int>()+(message["active"].get<bool>()?1:-1);}if(type=="roster_select")state["selectedId"]=message.at("id");};
        menu.sync();check(!menu.visible(),"Initial sync creates no windows");
        menu.showQuick(nullptr,std::filesystem::current_path());pump(menu);
        check(IsWindowVisible(menu.quickWindow),"Right-click entry opens the compact interface");check(!menu.window,"Opening quick menu does not create full settings");
        check(GetWindowLongPtrW(menu.quickWindow,GWL_EXSTYLE)&WS_EX_TOPMOST,"Pet quick menu remains above topmost pets");
        check(menu.pausesPets()&&pauses==std::vector<bool>{true},"Quick menu pauses pets for stable interaction");
        check(SendMessageW(menu.quickList,LB_GETCOUNT,0,0)==15,"Quick roster includes all fifteen imported models");
        state["characters"].push_back({{"name","Unimported placeholder"},{"active",false}});menu.sync();check(SendMessageW(menu.quickList,LB_GETCOUNT,0,0)==15,"Quick menu excludes placeholders without import metadata");state["characters"].erase(15);
        const auto untouched=state["value"];menu.command(3002,BN_CLICKED);check(state["value"]["follow"]==true,"Quick follow saves to the existing host");
        check(state["value"]["renderQuality"]==untouched["renderQuality"]&&state["value"]["followNear"]==untouched["followNear"],"Quick commands preserve unrelated settings");
        check(IsWindowVisible(menu.quickWindow),"Quick commands keep the menu open for multiple edits");
        SendMessageW(menu.quickList,LB_SETCURSEL,1,0);SendMessageW(menu.quickList,WM_KEYDOWN,VK_SPACE,0);check(state["characters"][1]["active"]==true,"Space toggles the focused imported model");
        const auto count=sent.size();menu.toggleRoster(14);check(sent.size()==count,"Missing model cannot be activated");
        state["activeCount"]=5;menu.sync();menu.toggleRoster(2);check(sent.size()==count,"Five-model cap is respected");menu.toggleRoster(1);check(state["characters"][1]["active"]==false,"Models can still be hidden at the cap");state["activeCount"]=1;
        state["battleAvailable"]=false;menu.sync();const auto before=sent.size();menu.command(3001,BN_CLICKED);check(sent.size()==before,"Unsupported battle mode is disabled");
        state["battleAvailable"]=true;menu.sync();menu.command(3001,BN_CLICKED);check(state["value"]["mode"]=="battle","Quick mode switch uses existing save protocol");
        state["busy"]=true;menu.sync();check(!IsWindowEnabled(GetDlgItem(menu.quickWindow,3002)),"Loading disables behavior edits");check(IsWindowEnabled(GetDlgItem(menu.quickWindow,3011)),"Full settings remain accessible while loading");state["busy"]=false;menu.sync();
        menu.command(3011,BN_CLICKED);pump(menu);check(IsWindowVisible(menu.window)&&!IsWindowVisible(menu.quickWindow),"Full interface replaces the flyout");
        check(!(GetWindowLongPtrW(menu.window,GWL_EXSTYLE)&WS_EX_TOPMOST),"Full settings must be a normal non-topmost window");
        check(visibility.size()==1&&visibility[0],"Quick-to-full transition keeps interface visibility continuous");
        check(!menu.pausesPets()&&pauses==std::vector<bool>({true,false}),"Switching from quick menu to full settings resumes pets");
        menu.hide();menu.show(nullptr,std::filesystem::current_path());pump(menu);
        check(!(GetWindowLongPtrW(menu.window,GWL_EXSTYLE)&WS_EX_TOPMOST),"Reopening full settings must not promote it to topmost");
        const auto pauseCount=pauses.size();ShowWindow(menu.window,SW_MINIMIZE);pump(menu);ShowWindow(menu.window,SW_RESTORE);pump(menu);
        check(!menu.pausesPets()&&pauses.size()==pauseCount,"Showing, minimizing and restoring full settings never pauses pets");
        SendMessageW(menu.window,WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);
        for(auto button:{GetDlgItem(menu.window,1000),GetDlgItem(menu.window,2200),GetDlgItem(menu.panel,104)}){
            check(drawButton(menu,button,0)==drawButton(menu,button,ODS_FOCUS|ODS_NOFOCUSRECT),"Mouse-clicked buttons must not paint a focus rectangle");
        }
        SendMessageW(menu.window,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);const auto navigation=GetDlgItem(menu.window,1000);check(drawButton(menu,navigation,0)!=drawButton(menu,navigation,ODS_FOCUS),"Keyboard navigation retains a visible Fluent focus cue");
        menu.quickCommand(3013);check(sent.back().at("type")=="open_models_folder","Quick menu opens the persistent model folder");
        size_t fieldCount=0;for(int p=0;p<3;++p){menu.page=p;menu.build();fieldCount+=menu.controls.size();for(const auto& c:menu.controls)check(IsWindow(c.input),"Every existing setting has a real input control");}
        check(fieldCount==21,"All twenty-one settings survive the redesign");
        menu.page=0;menu.build();checkRepainting(menu);SetWindowTextW(GetDlgItem(menu.panel,108),L"not a number");const auto invalidCount=sent.size();menu.save();check(sent.size()==invalidCount,"Invalid numeric input is not submitted");
        const auto quality=GetDlgItem(menu.panel,101);wchar_t qualityLabel[80]{};SendMessageW(quality,CB_GETLBTEXT,2,reinterpret_cast<LPARAM>(qualityLabel));check(std::wstring(qualityLabel).find(L"4× MSAA")!=std::wstring::npos,"Quality choice explicitly identifies MSAA");
        const auto samplingLabel=GetDlgItem(menu.panel,2111);check(samplingLabel,"Global settings expose actual sampling status");
        const auto samplingText=[&]{wchar_t text[256]{};GetWindowTextW(samplingLabel,text,256);return std::wstring(text);};
        state["renderSampling"]={{"visible",true},{"ready",false},{"scale",2},{"msaa",4}};menu.sync();check(samplingText().find(L"正在应用")!=std::wstring::npos,"Unrendered settings are not reported as enabled");
        state["renderSampling"]["ready"]=true;menu.sync();check(samplingText().find(L"已生效：2× 超采样 + 4× MSAA")!=std::wstring::npos,"Display actual allocated sample counts");
        state["renderSampling"]["msaa"]=0;menu.sync();check(samplingText().find(L"自动降低采样")!=std::wstring::npos,"Unsupported or oversized surfaces visibly report fallback");
        state["renderSampling"]["visible"]=false;menu.sync();check(samplingText().find(L"已隐藏")!=std::wstring::npos,"Hidden models do not claim to be rendering");
        state["renderSampling"]={{"visible",true},{"ready",true},{"scale",2},{"msaa",4}};menu.sync();
        for(UINT dpi:{96u,144u,192u}){
            menu.reloadTheme(menu.window,dpi);SetWindowPos(menu.window,nullptr,0,0,menu.u(920),menu.u(680),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);pump(menu);
            auto combo=GetDlgItem(menu.panel,101);RECT bounds{};GetClientRect(combo,&bounds);auto dc=GetDC(combo);auto previous=SelectObject(dc,menu.ui.font);SIZE extent{};GetTextExtentPoint32W(dc,qualityLabel,int(std::wcslen(qualityLabel)),&extent);SelectObject(dc,previous);ReleaseDC(combo,dc);check(extent.cx+menu.u(32)<bounds.right,"MSAA option fits without clipping at every DPI");
        }
        menu.reloadTheme(menu.window,96);
        SetWindowTextW(GetDlgItem(menu.panel,108),L"45");menu.save();check(state["value"]["idleSeconds"]==45,"Full settings still save edited numeric values");
        SendMessageW(GetDlgItem(menu.panel,101),CB_SETCURSEL,0,0);menu.command(101,CBN_SELCHANGE);check(state["value"]["renderQuality"]==0,"Styled combo preserves native selection commands");
        SendMessageW(GetDlgItem(menu.panel,103),TBM_SETPOS,TRUE,175);SendMessageW(menu.panel,WM_HSCROLL,TB_THUMBPOSITION,reinterpret_cast<LPARAM>(GetDlgItem(menu.panel,103)));check(state["value"]["outlineWidth"]==175,"Styled slider retains native value/drag behavior");
        menu.page=3;menu.build();check(SendMessageW(menu.list,LB_GETCOUNT,0,0)==15,"Full roster preserves the entire import history");
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
        check(!menu.pausesPets(),"Model management context menu never pauses pets");
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
        for(UINT dpi:{96u,144u,192u}){menu.reloadTheme(menu.window,dpi);SetWindowPos(menu.window,nullptr,0,0,menu.u(920),menu.u(680),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);pump(menu);RECT panel{};GetClientRect(menu.panel,&panel);for(auto h:menu.content){RECT r{};GetWindowRect(h,&r);MapWindowPoints(nullptr,menu.panel,reinterpret_cast<POINT*>(&r),2);if(r.left<0||r.right>panel.right+2)std::cerr<<"Layout: dpi="<<dpi<<" id="<<GetDlgCtrlID(h)<<" left="<<r.left<<" right="<<r.right<<" panel="<<panel.right<<'\n';check(r.left>=0&&r.right<=panel.right+2,"Full UI fits at 100/150/200 percent DPI");}}
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
            for(int p=1;p<4;++p){menu.page=p;menu.build();pump(menu);capture(menu.window,output/(std::wstring(L"settings-")+std::to_wstring(p)+L".bmp"));}
            if(SendMessageW(menu.list,LB_GETCOUNT,0,0)>0){size_t chosen=0;for(size_t i=0;i<state["characters"].size();++i)if(state["characters"][i].value("managed",false)){chosen=i;break;}SendMessageW(menu.list,LB_SETCURSEL,chosen,0);menu.showModelMenu(menu.list,MAKELPARAM(-1,-1));pump(menu);capture(menu.modelMenu,output/L"model-context.bmp");menu.hideModelMenu();}
            menu.hide();menu.showQuick(nullptr,std::filesystem::current_path());pump(menu);capture(menu.quickWindow,output/L"quick-menu.bmp");menu.hide();
            menu.page=0;menu.show(nullptr,std::filesystem::current_path());menu.reloadTheme(menu.window,GetDpiForSystem(),0);pump(menu);capture(menu.window,output/L"settings-light.bmp");menu.hide();
            menu.showQuick(nullptr,std::filesystem::current_path());menu.reloadTheme(menu.quickWindow,GetDpiForSystem(),0);pump(menu);capture(menu.quickWindow,output/L"quick-light.bmp");menu.hide();
        }
        std::cout<<"Native UI regression checks passed: incremental repaint pixels, Fluent sliders, quick roster, caps, mode/follow, loading, full settings, validation, visibility and DPI.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
