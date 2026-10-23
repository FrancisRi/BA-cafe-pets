#pragma once
#include "native_fluent.h"
#include <array>
#include <cwchar>

// Both interfaces use the existing host messages; the host remains responsible
// for validation, loading, behavior and persistence.
struct NativeSettings {
    HWND window{},panel{},list{},listEmpty{},progress{},quickWindow{},quickList{},modelMenu{};
    NativeFluent ui,quickUi,modelUi;
    std::function<Json()> snapshot;
    std::function<void(const Json&)> receive;
    std::function<void(bool)> visibility;
    std::function<void(bool)> pauseChanged;
    Json last=Json::object();
    std::filesystem::path defaultsFile;
    std::unique_ptr<NativeHealth::Pause> moveWait;
    struct Field {const char* key;const wchar_t* title;const wchar_t* detail;int kind;const wchar_t* icon;};
    struct Control {const Field* field;HWND input{},label{},hint{};int y=0;bool checked=false;};
    struct Placement {HWND hwnd;int x,y,w,h;bool stretch=false;};
    std::vector<Control> controls;
    std::vector<HWND> content;
    std::vector<Placement> placements;
    int page=0,scroll=0,contentHeight=0,quickRows=0;
    bool syncing=false,closing=false,notifiedVisible=false,notifiedPaused=false;
    std::string highlighted,contextId;
    Json readSnapshot(){
        auto result=snapshot();auto imported=Json::array();
        for(const auto& row:result.value("characters",Json::array())){
            // A display name alone is a placeholder, not an imported model.
            // Retain missing-file history so it can still be removed/reimported.
            if(!row.is_object())continue;
            bool valid=true;for(const char* key:{"id","name","path"})valid&=row.contains(key)&&row[key].is_string()&&!row[key].get<std::string>().empty();
            if(valid)imported.push_back(row);
        }
        result["characters"]=std::move(imported);return result;
    }
    inline static constexpr const wchar_t* pages[]={L"全局设置",L"咖啡厅",L"战斗",L"模型管理"};
    inline static constexpr const wchar_t* pageIcons[]={L"\xE713",L"\xE80F",L"\xE7FC",L"\xE716"};
    inline static constexpr Field fields[]={
        {"mode",L"动作模式",L"仅切换当前角色的动作模式",2,L"\xE768"},
        {"renderQuality",L"渲染精度",L"平衡画面清晰度与显卡负担",2,L"\xE7F4"},
        {"outlineMode",L"角色线条",L"描边与内部线条的显示方式",2,L"\xE790"},
        {"outlineWidth",L"线条粗细",L"描边宽度 · 10–300%",3,L"\xE70F"},
        {"wandering",L"允许散步",L"无输入时随机移动，独立于位置跟随",0,L"\xE7C1"},
        {"follow",L"位置跟随鼠标",L"超出距离区间后靠近或远离鼠标",0,L"\xE7C2"},
        {"followNear",L"最近跟随距离",L"身位 · 设为 0 时不主动远离鼠标",1,L"\xE8A0"},
        {"followFar",L"最远跟随距离",L"身位 · 移动到区间中点后停止",1,L"\xE8A0"},
        {"idleSeconds",L"无输入等待",L"日常行为开始前的等待时间 · 秒",1,L"\xE823"},
        {"activeSeconds",L"持续输入时间",L"触发追踪或战斗行为所需时间 · 秒",1,L"\xE765"},
        {"moveInterval",L"走动间隔",L"两次走动之间的等待时间 · 秒",1,L"\xE787"},
        {"moveBodies",L"走动距离",L"每次自动走动的距离 · 身位",1,L"\xE81D"},
        {"walkTimeJitter",L"等待随机幅度",L"设定等待时间的随机变化 · ±%",1,L"\xE8B1"},
        {"walkDistanceJitter",L"距离随机幅度",L"设定走动距离的随机变化 · ±%",1,L"\xE8B1"},
        {"cafeTracking",L"视线追踪鼠标",L"持续输入后，咖啡厅角色看向鼠标",0,L"\xE890"},
        {"cafeLookMode",L"视线追踪方式",L"连续转向，或相差 30° 后再转向",2,L"\xE7AD"},
        {"battleWalking",L"允许战斗走动",L"持续输入触发短走，动画保持原速",0,L"\xE7C1"},
        {"look",L"战斗动作朝向鼠标",L"攻击等战斗动作追踪鼠标方向",0,L"\xE890"},
        {"skillProbability",L"EX 技能触发概率",L"持续输入时触发技能的概率 · %",1,L"\xE945"},
        {"clickProbability",L"点击指挥概率",L"点击时触发指挥动作的概率 · %",1,L"\xE8B0"},
        {"keyboardIdle",L"无键盘输入收枪",L"停止键盘输入后的等待时间 · 秒",1,L"\xE823"}
    };
    ~NativeSettings(){closing=true;if(modelMenu)DestroyWindow(modelMenu);if(quickWindow)DestroyWindow(quickWindow);if(window)DestroyWindow(window);}
    int u(int value) const {return ui.px(value);}
    int panelWidth() const {RECT r{};if(panel)GetClientRect(panel,&r);return std::max(320,ui.dip(r.right)-32);}
    bool visible() const {return (window&&IsWindowVisible(window)&&!IsIconic(window))||(quickWindow&&IsWindowVisible(quickWindow));}
    bool pausesPets() const {return quickWindow&&IsWindowVisible(quickWindow);}
    HWND dialogOwner() const {return window&&IsWindowVisible(window)?window:quickWindow;}
    void notify(){
        const bool shown=visible(),paused=pausesPets();
        const bool shownChanged=shown!=notifiedVisible,pausedChanged=paused!=notifiedPaused;
        notifiedVisible=shown;notifiedPaused=paused;if(closing)return;
        if(pausedChanged&&pauseChanged)pauseChanged(paused);
        if(shownChanged&&visibility)visibility(shown);
    }
    HWND control(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h,bool compact=false){
        auto& theme=compact?quickUi:ui;const bool label=std::wcscmp(cls,L"STATIC")==0;
        auto hwnd=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|(label?0:WS_TABSTOP)|style,theme.px(x),theme.px(y),theme.px(w),theme.px(h),parent,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(theme.font),FALSE);theme.themed(hwnd);
        if(!label)SetWindowSubclass(hwnd,NativeFluent::controlProc,1,reinterpret_cast<DWORD_PTR>(&theme));return hwnd;
    }
    void hideQuick(){if(quickWindow)ShowWindow(quickWindow,SW_HIDE);notify();}
    void hideModelMenu(){if(modelMenu)ShowWindow(modelMenu,SW_HIDE);}
    void hide(){hideModelMenu();if(window)ShowWindow(window,SW_HIDE);hideQuick();notify();}
    void placeContent(HWND hwnd,int x,int y,int w,int h,bool stretch=false){content.push_back(hwnd);placements.push_back({hwnd,x,y,w,h,stretch});}
    void scrollTo(int value){
        if(!panel)return;RECT r{};GetClientRect(panel,&r);scroll=std::clamp(value,0,std::max(0,u(contentHeight)-int(r.bottom)));
        // Showing the native scrollbar changes the client width. Set its range
        // before measuring the cards, including after a DPI or page change.
        SCROLLINFO info{sizeof(info),SIF_ALL,0,std::max(0,u(contentHeight)-1),UINT(r.bottom),scroll,0};SetScrollInfo(panel,SB_VERT,&info,TRUE);const int width=panelWidth();
        for(const auto& p:placements)SetWindowPos(p.hwnd,nullptr,u(p.x),u(p.y)-scroll,u(p.stretch?width:p.w),u(p.h),SWP_NOZORDER|SWP_NOACTIVATE);
        for(const auto& c:controls){const int widthInput=c.field->kind==3?184:156;
            if(c.field->kind==0)SetWindowPos(c.input,nullptr,u(16),u(c.y)-scroll,u(width),u(68),SWP_NOZORDER|SWP_NOACTIVATE);
            else{
                SetWindowPos(c.label,nullptr,u(64),u(c.y+12)-scroll,u(std::max(80,width-widthInput-82)),u(22),SWP_NOZORDER|SWP_NOACTIVATE);
                SetWindowPos(c.hint,nullptr,u(64),u(c.y+36)-scroll,u(std::max(80,width-widthInput-82)),u(20),SWP_NOZORDER|SWP_NOACTIVATE);
                SetWindowPos(c.input,nullptr,u(width-widthInput),u(c.y+17)-scroll,u(widthInput),u(c.field->kind==2?260:34),SWP_NOZORDER|SWP_NOACTIVATE);
            }
        }
        InvalidateRect(panel,nullptr,TRUE);
    }
    void layout(){
        if(!window||!panel)return;RECT r{};GetClientRect(window,&r);const int w=ui.dip(r.right),h=ui.dip(r.bottom);
        const auto place=[&](int id,int x,int y,int width,int height){SetWindowPos(GetDlgItem(window,id),nullptr,u(x),u(y),u(std::max(1,width)),u(height),SWP_NOZORDER|SWP_NOACTIVATE);};
        place(12,24,24,176,28);place(13,24,54,180,20);for(int i=0;i<4;++i)place(1000+i,12,106+i*48,200,40);
        place(3,12,h-108,200,40);place(2,12,h-60,200,40);place(10,256,24,w-490,40);place(11,258,70,w-300,22);place(2200,w-186,28,146,36);
        SetWindowPos(panel,nullptr,u(240),u(112),u(std::max(360,w-264)),u(std::max(120,h-174)),SWP_NOZORDER|SWP_NOACTIVATE);
        place(2300,258,h-48,w-300,34);place(2301,258,h-9,w-300,3);scrollTo(scroll);InvalidateRect(window,nullptr,FALSE);
    }
    void build(){
        if(!panel)return;syncing=true;for(auto hwnd:content)DestroyWindow(hwnd);content.clear();placements.clear();controls.clear();list=listEmpty=nullptr;scroll=0;int y=8;
        const auto note=[&](const wchar_t* text,int height=34){auto h=control(panel,L"STATIC",text,0,0,16,y,panelWidth(),height);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(ui.small),FALSE);placeContent(h,16,y,0,height,true);y+=height+12;};
        const auto section=[&](const wchar_t* text){auto h=control(panel,L"STATIC",text,0,0,16,y,panelWidth(),24);SetPropW(h,L"FluentSection",reinterpret_cast<HANDLE>(1));SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(ui.heading),FALSE);placeContent(h,16,y,0,24,true);y+=34;};
        if(page==3){
            section(L"本地模型库");note(L"可多选导入；模型自动保存副本，之后可移动或删除原文件。",26);
            auto folder=control(panel,L"BUTTON",L"打开模型文件夹",BS_OWNERDRAW,2105,16,y,184,36);placeContent(folder,16,y,184,36);
            auto refresh=control(panel,L"BUTTON",L"刷新模型库",BS_OWNERDRAW,2106,212,y,132,36);placeContent(refresh,212,y,132,36);y+=48;
            auto change=control(panel,L"BUTTON",L"更改保存位置…",BS_OWNERDRAW,2107,16,y,184,36);placeContent(change,16,y,184,36);
            auto reset=control(panel,L"BUTTON",L"恢复默认位置",BS_OWNERDRAW,2108,212,y,162,36);placeContent(reset,212,y,162,36);y+=48;
            note(L"默认保存在程序所在文件夹；更改仅影响后续导入，原有模型仍可使用和管理。",40);
            auto folderPath=control(panel,L"STATIC",L"",SS_PATHELLIPSIS,2110,16,y,panelWidth(),24);SendMessageW(folderPath,WM_SETFONT,reinterpret_cast<WPARAM>(ui.small),FALSE);SetPropW(folderPath,L"FluentMuted",reinterpret_cast<HANDLE>(1));placeContent(folderPath,16,y,0,24,true);y+=40;
            section(L"已导入的模型");note(L"勾选显示角色；右键模型可切换、定位或删除。模型库与本地文件同步，同时显示最多 5 个。",40);
            const int rosterHeight=last.value("characters",Json::array()).empty()?128:std::min(4,int(last["characters"].size()))*64;
            list=control(panel,L"LISTBOX",L"已导入的模型",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT|WS_VSCROLL,2000,16,y,panelWidth(),rosterHeight);
            SendMessageW(list,LB_SETITEMHEIGHT,0,u(64));SetWindowSubclass(list,listProc,2,reinterpret_cast<DWORD_PTR>(this));placeContent(list,16,y,0,rosterHeight,true);
            const int emptyY=y+(rosterHeight-58)/2;listEmpty=control(panel,L"STATIC",L"尚未导入模型\n点击右上角「导入模型」添加角色",SS_CENTER,0,16,emptyY,panelWidth(),58);placeContent(listEmpty,16,emptyY,0,58,true);y+=rosterHeight+16;
            int x=16;for(auto [id,text]:{std::pair{2100,L"显示角色"},{2101,L"设为当前"},{2102,L"删除模型"}}){auto h=control(panel,L"BUTTON",text,BS_OWNERDRAW,id,x,y,132,36);placeContent(h,x,y,132,36);x+=144;}y+=50;
            section(L"显示管理");for(auto [id,text]:{std::pair{2103,L"统一角色大小"},{2104,L"关闭当前角色"}}){const int pos=id==2103?16:190;auto h=control(panel,L"BUTTON",text,BS_OWNERDRAW,id,pos,y,162,36);placeContent(h,pos,y,162,36);}y+=52;
            note(L"删除会将本地副本移入回收站并移除列表记录。原始下载文件和自带模型保留；删除最后一个显示中的角色后退出。",40);
        }else{
            note(page==0?L"显示与行为参数由所有角色共享，动作模式只应用于当前角色。":page==1?L"日常陪伴、视线与转向。位置跟随距离可在全局设置中调整。":L"保留按键攻击、换弹、EX、点击指挥与走动的原有规则。");
            for(size_t i=0;i<std::size(fields);++i){
                if((page==0&&i>=14)||(page==1&&(i<14||i>=16))||(page==2&&i<16))continue;const auto& f=fields[i];
                if(i==0)section(L"角色与显示");if(i==4)section(L"移动与跟随");if(i==8)section(L"时间与随机变化");if(i==14)section(L"视线追踪");if(i==16)section(L"战斗动作");
                HWND input{},label{},hint{};
                if(f.kind==0)input=control(panel,L"BUTTON",f.title,BS_OWNERDRAW,100+int(i),16,y,panelWidth(),68);
                else{
                    label=control(panel,L"STATIC",f.title,SS_ENDELLIPSIS,0,64,y+12,270,22);hint=control(panel,L"STATIC",f.detail,SS_ENDELLIPSIS,0,64,y+36,270,20);
                    SendMessageW(hint,WM_SETFONT,reinterpret_cast<WPARAM>(ui.small),FALSE);SetPropW(hint,L"FluentMuted",reinterpret_cast<HANDLE>(1));content.push_back(label);content.push_back(hint);
                    input=control(panel,f.kind==3?TRACKBAR_CLASSW:f.kind==1?L"EDIT":L"COMBOBOX",L"",f.kind==3?TBS_HORZ|TBS_NOTICKS:f.kind==1?WS_BORDER|ES_AUTOHSCROLL:CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL,100+int(i),326,y+17,156,f.kind==2?260:34);
                    if(f.kind==1)SendMessageW(input,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(u(8),u(8)));
                    if(f.kind==3)SendMessageW(input,TBM_SETRANGE,TRUE,MAKELPARAM(10,300));
                    if(f.kind==2){SendMessageW(input,CB_SETITEMHEIGHT,WPARAM(-1),u(30));SendMessageW(input,CB_SETITEMHEIGHT,0,u(32));const auto add=[&](const wchar_t* text){SendMessageW(input,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));};
                        if(std::string(f.key)=="mode"){add(L"咖啡厅");if(last.value("battleAvailable",false))add(L"战斗");}
                        else if(std::string(f.key)=="renderQuality"){add(L"省电 · 1×");add(L"均衡 · 1.5×");add(L"清晰 · 2×");}
                        else if(std::string(f.key)=="outlineMode"){add(L"无");add(L"轮廓");add(L"逐网格线条");}else{add(L"平滑追踪");add(L"30° 后转向");}}
                }
                controls.push_back({&f,input,label,hint,y});content.push_back(input);y+=76;
            }
            if(page==0){y+=8;auto h=control(panel,L"BUTTON",L"恢复默认设置",BS_OWNERDRAW,2201,16,y,160,36);placeContent(h,16,y,160,36);y+=52;}
            if(page==2&&!last.value("battleAvailable",false))note(L"当前模型没有完整的战斗动画。相关选项暂不可用。",38);
        }
        contentHeight=y+16;syncing=false;layout();update();
    }
    static std::wstring rosterName(const Json& row){
        auto name=wide(row.value("name",std::string{}));name+=row.value("loading",false)?L" · 加载中":row.value("failed",false)?L" · 加载失败":!row.value("available",false)?L" · 文件不存在":row.value("active",false)?L" · 已显示":L" · 已隐藏";return name;
    }
    void fillRoster(HWND target,bool compact){
        if(!target)return;const auto rows=last.value("characters",Json::array());bool exists=false;for(const auto& row:rows)exists|=row.value("id",std::string{})==highlighted;if(!exists)highlighted=last.value("selectedId",std::string{});
        const int top=int(SendMessageW(target,LB_GETTOPINDEX,0,0)),caret=int(SendMessageW(target,LB_GETCURSEL,0,0));bool changed=int(SendMessageW(target,LB_GETCOUNT,0,0))!=int(rows.size());
        if(!changed)for(size_t i=0;i<rows.size();++i){const int len=int(SendMessageW(target,LB_GETTEXTLEN,i,0));std::wstring name(std::max(0,len)+1,L'\0');SendMessageW(target,LB_GETTEXT,i,reinterpret_cast<LPARAM>(name.data()));name.resize(std::max(0,len));if(name!=rosterName(rows[i])){changed=true;break;}}
        SendMessageW(target,WM_SETREDRAW,FALSE,0);if(changed){SendMessageW(target,LB_RESETCONTENT,0,0);for(const auto& row:rows){const auto name=rosterName(row);SendMessageW(target,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}}
        int selected=-1;for(size_t i=0;i<rows.size();++i)if(rows[i].value("id",std::string{})==highlighted)selected=int(i);
        if(compact&&GetFocus()==target&&caret>=0&&caret<int(rows.size()))selected=caret;
        SendMessageW(target,LB_SETCURSEL,selected,0);if(top>=0)SendMessageW(target,LB_SETTOPINDEX,top,0);SendMessageW(target,WM_SETREDRAW,TRUE,0);InvalidateRect(target,nullptr,TRUE);
    }
    bool quickEnabled(int id) const {
        if(id==3010)return !last.value("busy",false);
        if(id==3011||id==3012||id==3013)return true;if(last.value("busy",false))return false;
        if(id==3001||id==3005)return last.value("battleAvailable",false);if(id==3002)return last.value("canFollow",false);return true;
    }
    bool quickChecked(int id) const {
        const auto value=last.value("value",Json::object());if(id==3000||id==3001)return (value.value("mode",std::string{})=="battle")==(id==3001);
        const char* key=id==3002?"follow":id==3003?"wandering":id==3004?(value.value("mode",std::string{})=="battle"?"look":"cafeTracking"):"battleWalking";return value.value(key,false);
    }
    std::wstring statusText(bool compact=false) const {
        const auto error=last.value("error",std::string{});if(!error.empty())return wide(error);
        if(last.value("busy",false))return std::to_wstring(last.value("percent",0))+L"%  "+wide(last.value("detail",std::string{}));
        const auto notice=last.value("notice",std::string{});if(!notice.empty())return wide(notice);
        return compact?L"更改自动保存 · Esc 关闭菜单":L"更改已自动保存   ·   角色保持活动";
    }
    void update(){
        if(last.empty())return;syncing=true;const auto value=last.value("value",Json::object());
        if(window){
            SetWindowTextW(GetDlgItem(window,10),pages[page]);const auto sub=wide(last.value("name",std::string{"角色"}))+L"  ·  "+(value.value("mode",std::string{})=="battle"?L"战斗模式":L"咖啡厅模式")+L"  ·  "+std::to_wstring(last.value("activeCount",1))+L" 个角色已显示";SetWindowTextW(GetDlgItem(window,11),sub.c_str());
            for(auto& c:controls){const std::string key=c.field->key;if(!value.contains(key))continue;const auto& v=value[key];
                if(c.field->kind==0)c.checked=v.get<bool>();else if(c.field->kind==3){SendMessageW(c.input,TBM_SETPOS,TRUE,v.get<int>());SetWindowTextW(c.hint,(L"描边宽度 · "+std::to_wstring(v.get<int>())+L"%").c_str());}
                else if(c.field->kind==2){const int index=key=="mode"?(v=="battle"):key=="cafeLookMode"?(v=="threshold"):v.get<int>();SendMessageW(c.input,CB_SETCURSEL,index,0);}
                else if(GetFocus()!=c.input){auto text=v.dump();if(text.ends_with(".0"))text.resize(text.size()-2);SetWindowTextW(c.input,wide(text).c_str());}
                bool enabled=!last.value("busy",false)&&(page!=2||last.value("battleAvailable",false));if(key=="follow")enabled&=last.value("canFollow",false);if(key=="followNear"||key=="followFar")enabled&=last.value("canFollow",false)&&value.value("follow",false);if(key=="cafeLookMode")enabled&=value.value("cafeTracking",true);if(key=="outlineWidth")enabled&=value.value("outlineMode",2)!=0;
                EnableWindow(c.input,enabled);if(c.label)EnableWindow(c.label,enabled);if(c.hint)EnableWindow(c.hint,enabled);InvalidateRect(c.input,nullptr,FALSE);
            }
            SetWindowTextW(GetDlgItem(window,2300),statusText().c_str());EnableWindow(GetDlgItem(window,2200),quickEnabled(3010));ShowWindow(progress,last.value("busy",false)?SW_SHOW:SW_HIDE);InvalidateRect(progress,nullptr,FALSE);
            if(auto path=GetDlgItem(panel,2110))SetWindowTextW(path,wide(last.value("modelsDirectory",std::string{})).c_str());
            EnableWindow(GetDlgItem(panel,2107),!last.value("busy",false));EnableWindow(GetDlgItem(panel,2108),!last.value("busy",false)&&!last.value("modelsDirectoryDefault",false));
            if(list){fillRoster(list,false);const int n=int(SendMessageW(list,LB_GETCURSEL,0,0));const auto rows=last.value("characters",Json::array());ShowWindow(listEmpty,rows.empty()?SW_SHOW:SW_HIDE);const auto row=n>=0&&n<int(rows.size())?rows[n]:Json::object();const bool active=row.value("active",false);
                SetWindowTextW(GetDlgItem(panel,2100),active?L"隐藏角色":L"显示角色");EnableWindow(GetDlgItem(panel,2100),n>=0&&(active||(row.value("available",false)&&last.value("activeCount",0)<5)));EnableWindow(GetDlgItem(panel,2101),n>=0&&active&&row.value("ready",false)&&highlighted!=last.value("selectedId",std::string{}));SetWindowTextW(GetDlgItem(panel,2102),row.value("managed",false)?L"删除模型":L"移除记录");EnableWindow(GetDlgItem(panel,2102),n>=0&&!row.value("loading",false));EnableWindow(GetDlgItem(panel,2103),last.value("activeCount",0)>0);EnableWindow(GetDlgItem(panel,2104),last.value("activeCount",0)>0);}
            for(int i=0;i<4;++i)InvalidateRect(GetDlgItem(window,1000+i),nullptr,FALSE);
        }
        if(quickWindow){
            SetWindowTextW(GetDlgItem(quickWindow,3100),wide(last.value("name",std::string{"桌面角色"})).c_str());SetWindowTextW(GetDlgItem(quickWindow,3101),(L"已导入的模型  ·  "+std::to_wstring(last.value("activeCount",0))+L" / 5 已显示").c_str());
            for(int id:{3000,3001,3002,3003,3004,3005,3010,3011,3012,3013}){EnableWindow(GetDlgItem(quickWindow,id),quickEnabled(id));InvalidateRect(GetDlgItem(quickWindow,id),nullptr,FALSE);}
            SetWindowTextW(GetDlgItem(quickWindow,3004),value.value("mode",std::string{})=="battle"?L"战斗朝向鼠标":L"视线追踪鼠标");SetWindowTextW(GetDlgItem(quickWindow,3102),statusText(true).c_str());fillRoster(quickList,true);
            const bool empty=last.value("characters",Json::array()).empty();ShowWindow(quickList,empty?SW_HIDE:SW_SHOW);ShowWindow(GetDlgItem(quickWindow,3103),empty?SW_SHOW:SW_HIDE);
        }
        updateModelMenu();syncing=false;
    }
    void sync(){
        if(!snapshot)return;const auto next=readSnapshot();
        const bool capabilitiesChanged=last.value("battleAvailable",false)!=next.value("battleAvailable",false);
        const bool rosterChanged=last.value("characters",Json::array()).size()!=next.value("characters",Json::array()).size();
        const auto selected=next.value("selectedId",std::string{});bool known=false;
        for(const auto& row:last.value("characters",Json::array()))known|=row.value("id",std::string{})==selected;
        // Highlight newly imported records. Hiding the current actor also
        // changes the host's current actor, but must leave the hidden list row
        // selected so the same button can show that model again.
        if(last.value("selectedId",std::string{})!=selected&&!known)highlighted=selected;
        last=next;if(window&&IsWindowVisible(window)&&((capabilitiesChanged&&(page==0||page==2))||(rosterChanged&&page==3))){const int before=scroll;build();scrollTo(before);}else update();
    }
    void sendValue(Json value){if(receive)receive({{"type","save"},{"value",std::move(value)}});sync();}
    void save(){
        if(syncing||last.empty())return;auto value=last.at("value");
        for(const auto& c:controls){const auto key=c.field->key;
            if(c.field->kind==0)value[key]=c.checked;else if(c.field->kind==3)value[key]=int(SendMessageW(c.input,TBM_GETPOS,0,0));
            else if(c.field->kind==2){const int n=int(SendMessageW(c.input,CB_GETCURSEL,0,0));if(n<0)continue;if(std::string(key)=="mode")value[key]=n?"battle":"cafe";else if(std::string(key)=="cafeLookMode")value[key]=n?"threshold":"smooth";else value[key]=n;}
            else{wchar_t text[80]{};GetWindowTextW(c.input,text,80);wchar_t* end{};const double n=std::wcstod(text,&end);if(end==text||*end||!std::isfinite(n)){SetWindowTextW(GetDlgItem(window,2300),L"请输入有效数字；该更改尚未保存。");return;}value[key]=n;}
        }sendValue(std::move(value));
    }
    void toggleRoster(int index){
        const auto rows=last.value("characters",Json::array());if(index<0||index>=int(rows.size()))return;const auto row=rows[index];
        if(!row.value("active",false)&&(!row.value("available",false)||last.value("activeCount",0)>=5)){MessageBeep(MB_OK);return;}
        if(receive)receive({{"type","roster_toggle"},{"id",row["id"]},{"active",!row.value("active",false)}});sync();
    }
    void removeModel(const Json& row){
        if(row.value("loading",false))return;
        const bool managed=row.value("managed",false);
        auto text=L"“"+wide(row.value("name",std::string{}))+L"”\n\n";
        text+=managed?L"将本地模型副本移入回收站，并从模型库中移除？原始下载文件保留。":L"从列表中移除该模型？程序自带模型和外部原始文件将保留。";
        if(row.value("active",false)&&last.value("activeCount",0)==1)text+=L"\n\n这是最后一个显示中的角色，操作完成后将退出桌宠。";
        if(nativeMessageBox(window,text.c_str(),managed?L"删除模型":L"移除模型记录",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)==IDYES&&receive)receive({{"type","model_delete"},{"id",row.at("id")}});
    }
    void updateModelMenu(){
        if(!modelMenu||!IsWindowVisible(modelMenu))return;
        const auto rows=last.value("characters",Json::array());
        const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& item){return item.value("id",std::string{})==contextId;});
        if(row==rows.end()){hideModelMenu();return;}
        SetWindowTextW(GetDlgItem(modelMenu,4100),wide(row->value("name",std::string{})).c_str());
        SetWindowTextW(GetDlgItem(modelMenu,4000),row->value("active",false)?L"隐藏角色":L"显示角色");
        EnableWindow(GetDlgItem(modelMenu,4000),row->value("active",false)||(row->value("available",false)&&last.value("activeCount",0)<5));
        EnableWindow(GetDlgItem(modelMenu,4001),row->value("active",false)&&row->value("ready",false)&&contextId!=last.value("selectedId",std::string{}));
        EnableWindow(GetDlgItem(modelMenu,4002),row->value("available",false));
        SetWindowTextW(GetDlgItem(modelMenu,4003),row->value("managed",false)?L"删除模型…":L"移除模型记录…");
        EnableWindow(GetDlgItem(modelMenu,4003),!row->value("loading",false));
        InvalidateRect(modelMenu,nullptr,TRUE);
    }
    void modelCommand(int id){
        const auto rows=last.value("characters",Json::array());
        for(size_t index=0;index<rows.size();++index)if(rows[index].value("id",std::string{})==contextId){
            if(!IsWindowEnabled(GetDlgItem(modelMenu,id)))return;const auto row=rows[index];hideModelMenu();
            if(id==4000)toggleRoster(int(index));else if(id==4001)receive({{"type","roster_select"},{"id",row["id"]}});
            else if(id==4002)receive({{"type","model_locate"},{"id",row["id"]}});else if(id==4003)removeModel(row);
            sync();return;
        }hideModelMenu();
    }
    void showModelMenu(HWND source,LPARAM location){
        POINT point{GET_X_LPARAM(location),GET_Y_LPARAM(location)};const bool keyboard=point.x==-1&&point.y==-1;
        int index=int(SendMessageW(source,LB_GETCURSEL,0,0));
        if(!keyboard){auto local=point;ScreenToClient(source,&local);const auto hit=SendMessageW(source,LB_ITEMFROMPOINT,0,MAKELPARAM(local.x,local.y));if(HIWORD(hit))return;index=LOWORD(hit);RECT item{};if(SendMessageW(source,LB_GETITEMRECT,index,reinterpret_cast<LPARAM>(&item))==LB_ERR||!PtInRect(&item,local))return;}
        const auto rows=last.value("characters",Json::array());if(index<0||index>=int(rows.size()))return;
        highlighted=contextId=rows[index]["id"];SendMessageW(source,LB_SETCURSEL,index,0);update();
        if(keyboard){RECT item{};SendMessageW(source,LB_GETITEMRECT,index,reinterpret_cast<LPARAM>(&item));point={item.left+u(44),item.bottom};ClientToScreen(source,&point);}
        if(!modelMenu){WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BaCafeModelMenu";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);
            modelMenu=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT,wc.lpszClassName,L"模型操作",WS_POPUP|WS_THICKFRAME|WS_CLIPCHILDREN,0,0,280,230,nullptr,nullptr,wc.hInstance,this);
            if(!modelMenu)throw std::runtime_error("Cannot create model context menu");}
        modelUi.load(GetDpiForWindow(source));modelUi.frame(modelMenu,true);
        if(!GetDlgItem(modelMenu,4100)){
            auto title=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_ENDELLIPSIS,0,0,0,0,modelMenu,reinterpret_cast<HMENU>(4100),GetModuleHandleW(nullptr),nullptr);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(modelUi.heading),TRUE);
            for(auto [id,text]:{std::pair{4000,L"显示角色"},{4001,L"设为当前角色"},{4002,L"在文件夹中定位"},{4003,L"删除模型…"}}){auto child=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,modelMenu,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);SetWindowSubclass(child,NativeFluent::controlProc,1,reinterpret_cast<DWORD_PTR>(&modelUi));}
        }
        SetWindowPos(GetDlgItem(modelMenu,4100),nullptr,modelUi.px(16),modelUi.px(12),modelUi.px(240),modelUi.px(26),SWP_NOZORDER|SWP_NOACTIVATE);SendMessageW(GetDlgItem(modelMenu,4100),WM_SETFONT,reinterpret_cast<WPARAM>(modelUi.heading),TRUE);
        for(int id=4000;id<=4003;++id){auto child=GetDlgItem(modelMenu,id);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(modelUi.font),TRUE);SetWindowPos(child,nullptr,modelUi.px(8),modelUi.px(46+(id-4000)*38),modelUi.px(256),modelUi.px(36),SWP_NOZORDER|SWP_NOACTIVATE);}
        MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&info);const int width=modelUi.px(272),height=modelUi.px(208);
        const int x=std::clamp(int(point.x),int(info.rcWork.left),std::max(int(info.rcWork.left),int(info.rcWork.right)-width)),y=std::clamp(int(point.y),int(info.rcWork.top),std::max(int(info.rcWork.top),int(info.rcWork.bottom)-height));
        SendMessageW(modelMenu,WM_CHANGEUISTATE,MAKEWPARAM(keyboard?UIS_CLEAR:UIS_SET,UISF_HIDEFOCUS),0);
        SetWindowPos(modelMenu,HWND_TOPMOST,x,y,width,height,SWP_SHOWWINDOW|SWP_FRAMECHANGED);updateModelMenu();SetForegroundWindow(modelMenu);
        for(int id=4000;id<=4003;++id)if(IsWindowEnabled(GetDlgItem(modelMenu,id))){SetFocus(GetDlgItem(modelMenu,id));break;}
    }
    void quickCommand(int id){
        if(!quickEnabled(id))return;if(id==3011){show(nullptr,defaultsFile.parent_path());return;}if(id==3010){page=3;show(nullptr,defaultsFile.parent_path());receive({{"type","import"}});return;}if(id==3012){receive({{"type","exit"}});return;}if(id==3013){hideQuick();receive({{"type","open_models_folder"}});return;}
        auto value=last.at("value");if(id==3000||id==3001)value["mode"]=id==3000?"cafe":"battle";else{const char* key=id==3002?"follow":id==3003?"wandering":id==3004?(value.value("mode",std::string{})=="battle"?"look":"cafeTracking"):"battleWalking";value[key]=!value.value(key,false);}sendValue(std::move(value));
    }
    void command(int id,int code){
        if(id>=4000&&id<=4003){modelCommand(id);return;}
        if(syncing)return;if(id>=3000&&id<=3013){quickCommand(id);return;}if(id==1){hide();return;}if(id==2){receive({{"type","exit"}});return;}
        if(id==3){nativeMessageBox(window,L"右键：快捷菜单。右键拖动或中键拖动：旋转。\n左键拖拽：移动。滚轮：缩放。\n\n勾选模型显示或隐藏角色；方向键选择，空格勾选，Esc 关闭。\n\n咖啡厅：点击反应；持续输入后追踪鼠标；无输入后散步。\n战斗：按键攻击；句号、回车、分号、问号请求换弹；持续输入可短走或 EX。\n三连击：咖啡厅进入战斗，战斗退场。拖拽优先打断。",L"使用说明",MB_OK);return;}
        if(id>=1000&&id<1004){hideModelMenu();page=id-1000;if(page==3&&receive)receive({{"type","refresh_models"}});build();return;}
        if(id>=100&&id<100+int(std::size(fields))&&(code==BN_CLICKED||code==CBN_SELCHANGE||code==EN_KILLFOCUS)){if(code==BN_CLICKED)for(auto& c:controls)if(c.input==GetDlgItem(panel,id)&&c.field->kind==0)c.checked=!c.checked;save();return;}
        if(id==2000&&code==LBN_SELCHANGE){const int n=int(SendMessageW(list,LB_GETCURSEL,0,0));if(n>=0&&n<int(last["characters"].size()))highlighted=last["characters"][n]["id"];update();return;}
        if(id==2103){receive({{"type","uniform_size"}});return;}if(id==2104){receive({{"type","close_character"}});return;}
        if(id==2105){receive({{"type","open_models_folder"}});return;}
        if(id==2106){receive({{"type","refresh_models"}});return;}
        if(id==2107||id==2108){receive({{"type",id==2107?"change_models_folder":"reset_models_folder"}});return;}
        if(id>=2100&&id<=2102){const int n=int(SendMessageW(list,LB_GETCURSEL,0,0));if(n<0)return;const auto row=last["characters"][n];if(id==2100)toggleRoster(n);else if(id==2101)receive({{"type","roster_select"},{"id",row["id"]}});else removeModel(row);return;}
        if(id==2200){receive({{"type","import"}});return;}
        if(id==2201){auto value=last["value"];NativePreferences defaults;const auto ini=defaultsFile.wstring();defaults.load(ini);value.update(defaults.json());value.update({{"look",GetPrivateProfileIntW(L"Behavior",L"Tracking",1,ini.c_str())!=0},{"idleSeconds",std::clamp(int(GetPrivateProfileIntW(L"Behavior",L"InputIdleSeconds",60,ini.c_str())),1,3600)},{"activeSeconds",std::clamp(int(GetPrivateProfileIntW(L"Behavior",L"ContinuedInputSeconds",30,ini.c_str())),1,3600)}});sendValue(std::move(value));}
    }
    void drawRoster(const DRAWITEMSTRUCT& d,bool compact){
        auto& theme=compact?quickUi:ui;const auto rows=last.value("characters",Json::array());if(d.itemID>=rows.size())return;const auto& row=rows[d.itemID];auto r=d.rcItem;const bool active=row.value("active",false),available=row.value("available",false);
        NativeFluent::fill(d.hDC,r,theme.surface);RECT bg{r.left+theme.px(2),r.top+theme.px(2),r.right-theme.px(2),r.bottom-theme.px(2)};if(d.itemState&ODS_SELECTED)NativeFluent::round(d.hDC,bg,theme.selection,theme.selection,theme.px(6));
        const int cy=(r.top+r.bottom)/2;RECT check{r.left+theme.px(12),cy-theme.px(9),r.left+theme.px(30),cy+theme.px(9)};theme.check(d.hDC,check,active,active||available);
        RECT name{r.left+theme.px(44),compact?r.top:r.top+theme.px(8),r.right-theme.px(14),compact?r.bottom:r.top+theme.px(30)};theme.label(d.hDC,wide(row.value("name",std::string{})),name,theme.font,available?theme.text:theme.disabled);
        if(!compact){std::wstring status=!available?L"文件不存在":row.value("failed",false)?L"加载失败，可重新导入":row.value("loading",false)?L"正在加载…":row.value("mode",std::string{})=="battle"?L"战斗模式":L"咖啡厅模式";status+=row.value("id",std::string{})==last.value("selectedId",std::string{})?L" · 当前角色":active?L" · 已显示":L" · 已隐藏";name.top=r.top+theme.px(32);name.bottom=r.bottom-theme.px(6);theme.label(d.hDC,status,name,theme.small,theme.muted);}
        else if(row.value("loading",false)||row.value("failed",false)){RECT mark{r.right-theme.px(28),r.top,r.right-theme.px(8),r.bottom};theme.glyph(d.hDC,row.value("failed",false)?L"\xE7BA":L"\xE895",mark,theme.muted);}
        theme.focus(d.hDC,bg,d.hwndItem,(d.itemState&ODS_FOCUS)&&!(d.itemState&ODS_NOFOCUSRECT));
    }
    void draw(const DRAWITEMSTRUCT& d){
        const bool context=GetParent(d.hwndItem)==modelMenu,compact=GetParent(d.hwndItem)==quickWindow;auto& theme=context?modelUi:compact?quickUi:ui;const int id=d.CtlID;auto r=d.rcItem;const auto dc=d.hDC;
        const bool disabled=(d.itemState&ODS_DISABLED)!=0,pressed=(d.itemState&ODS_SELECTED)!=0,focused=(d.itemState&ODS_FOCUS)!=0,hover=GetPropW(d.hwndItem,L"FluentHover")!=nullptr;
        NativeFluent::fill(dc,r,context||compact?theme.surface:theme.canvas);if(id==2000||id==3200){drawRoster(d,id==3200);return;}
        if(d.CtlType==ODT_COMBOBOX){
            NativeFluent::fill(dc,r,pressed?theme.selection:theme.surface);if(d.itemID==UINT(-1))return;
            const int len=int(SendMessageW(d.hwndItem,CB_GETLBTEXTLEN,d.itemID,0));std::wstring item(std::max(0,len)+1,L'\0');SendMessageW(d.hwndItem,CB_GETLBTEXT,d.itemID,reinterpret_cast<LPARAM>(item.data()));item.resize(std::max(0,len));r.left+=theme.px(12);r.right-=theme.px(8);theme.label(dc,item,r,theme.font,disabled?theme.disabled:theme.text);return;
        }
        if(id==2301){NativeFluent::fill(dc,r,theme.line);r.right=r.left+(r.right-r.left)*std::clamp(last.value("percent",0),0,100)/100;NativeFluent::fill(dc,r,theme.accent);return;}
        wchar_t title[256]{};GetWindowTextW(d.hwndItem,title,256);COLORREF color=disabled?theme.disabled:theme.text;
        if(context){
            NativeFluent::round(dc,r,pressed?theme.pressed:hover?theme.hover:theme.surface,theme.surface,theme.px(6));if(id==4003&&!disabled)color=theme.danger;
            RECT icon{r.left+theme.px(8),r.top,r.left+theme.px(32),r.bottom};theme.glyph(dc,id==4000?L"\xE890":id==4001?L"\xE73E":id==4002?L"\xE8B7":L"\xE74D",icon,disabled?theme.disabled:color);
            r.left+=theme.px(44);theme.label(dc,title,r,theme.font,disabled?theme.disabled:color);
        }else if(compact){
            const bool mode=id==3000||id==3001,checked=quickChecked(id),isToggle=id>=3002&&id<=3005;auto fill=pressed?theme.pressed:hover?theme.hover:theme.surface;if(mode&&checked)fill=theme.selection;NativeFluent::round(dc,r,fill,mode?theme.line:fill,theme.px(6));if(id==3012)color=theme.danger;
            if(mode)theme.label(dc,title,r,theme.font,disabled?theme.disabled:checked?theme.accent:color,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
            else{const wchar_t* icon=id==3002?L"\xE7C2":id==3003?L"\xE7C1":id==3004?L"\xE890":id==3005?L"\xE7FC":id==3010?L"\xE710":id==3011?L"\xE713":id==3013?L"\xE8B7":L"\xE7E8";RECT glyph{r.left+theme.px(8),r.top,r.left+theme.px(32),r.bottom};theme.glyph(dc,icon,glyph,color);RECT text{r.left+theme.px(44),r.top,r.right-theme.px(isToggle?56:36),r.bottom};theme.label(dc,title,text,theme.font,color);
                if(isToggle){RECT track{r.right-theme.px(46),r.top+theme.px(8),r.right-theme.px(8),r.top+theme.px(28)};theme.toggle(dc,track,checked,!disabled);}if(id==3011){RECT arrow{r.right-theme.px(28),r.top,r.right-theme.px(8),r.bottom};theme.glyph(dc,L"\xE76C",arrow,color);}}
        }else if(id>=1000&&id<1004){
            const bool selected=page==id-1000;const auto fill=selected?theme.pressed:pressed?theme.pressed:hover?theme.line:theme.canvas;NativeFluent::round(dc,r,fill,fill,theme.px(6));RECT icon{r.left+theme.px(10),r.top,r.left+theme.px(36),r.bottom};theme.glyph(dc,pageIcons[id-1000],icon,color);auto text=r;text.left+=theme.px(48);theme.label(dc,title,text,theme.font,color);if(selected){RECT indicator{r.left,r.top+theme.px(12),r.left+theme.px(3),r.bottom-theme.px(12)};NativeFluent::round(dc,indicator,theme.accent,theme.accent,theme.px(3));}
        }else if(id>=100&&id<100+int(std::size(fields))){
            const auto it=std::find_if(controls.begin(),controls.end(),[&](const auto& c){return c.input==d.hwndItem;});if(it==controls.end())return;NativeFluent::round(dc,r,pressed?theme.pressed:hover?theme.hover:theme.surface,theme.line,theme.px(8));RECT icon{r.left+theme.px(14),r.top,r.left+theme.px(40),r.bottom};theme.glyph(dc,it->field->icon,icon,color);RECT text{r.left+theme.px(48),r.top+theme.px(12),r.right-theme.px(76),r.top+theme.px(34)};theme.label(dc,title,text,theme.font,color);text.top+=theme.px(24);text.bottom+=theme.px(24);theme.label(dc,it->field->detail,text,theme.small,disabled?theme.disabled:theme.muted);RECT track{r.right-theme.px(58),r.top+theme.px(24),r.right-theme.px(20),r.top+theme.px(44)};theme.toggle(dc,track,it->checked,!disabled);
        }else{
            const bool nav=id==2||id==3,primary=id==2200;const auto fill=primary?(pressed?theme.muted:theme.accent):pressed?theme.pressed:hover?theme.hover:nav?theme.canvas:theme.surface;NativeFluent::round(dc,r,fill,nav?fill:primary?fill:theme.line,theme.px(6));if(primary)color=disabled?theme.disabled:theme.accentText;else if(id==2||id==2102||id==2104)color=disabled?theme.disabled:theme.danger;
            if(nav){RECT icon{r.left+theme.px(10),r.top,r.left+theme.px(36),r.bottom};theme.glyph(dc,id==2?L"\xE7E8":L"\xE897",icon,color);r.left+=theme.px(48);theme.label(dc,title,r,theme.font,color);}else theme.label(dc,title,r,theme.font,color,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        }
        theme.focus(dc,r,d.hwndItem,focused&&!(d.itemState&ODS_NOFOCUSRECT));
    }
    void paint(HWND hwnd,HDC dc){
        RECT r{};GetClientRect(hwnd,&r);const bool context=hwnd==modelMenu,compact=hwnd==quickWindow;auto& theme=context?modelUi:compact?quickUi:ui;NativeFluent::fill(dc,r,context||compact?theme.surface:theme.canvas);
        if(hwnd==panel){const int width=panelWidth();for(const auto& c:controls)if(c.field->kind!=0){RECT box{u(16),u(c.y)-scroll,u(16+width),u(c.y+68)-scroll};if(box.bottom<0||box.top>r.bottom)continue;NativeFluent::round(dc,box,ui.surface,ui.line,u(8));RECT icon{u(30),u(c.y)-scroll,u(56),u(c.y+68)-scroll};ui.glyph(dc,c.field->icon,icon,IsWindowEnabled(c.input)?ui.text:ui.disabled);}}
        else if(hwnd==window){RECT divider{u(232),0,u(233),r.bottom};NativeFluent::fill(dc,divider,theme.line);}else if(compact||context)NativeFluent::round(dc,r,theme.surface,theme.line,theme.px(8));
    }
    static LRESULT CALLBACK listProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
        auto* self=reinterpret_cast<NativeSettings*>(data);const bool compact=hwnd==self->quickList;
        if(msg==WM_CONTEXTMENU&&!compact){self->showModelMenu(hwnd,l);return 0;}
        if(msg==WM_KEYDOWN&&w==VK_DELETE&&!compact){const int index=int(SendMessageW(hwnd,LB_GETCURSEL,0,0));const auto rows=self->last.value("characters",Json::array());if(index>=0&&index<int(rows.size()))self->removeModel(rows[index]);return 0;}
        if(msg==WM_LBUTTONUP){const auto result=DefSubclassProc(hwnd,msg,w,l);const auto hit=SendMessageW(hwnd,LB_ITEMFROMPOINT,0,l);if(HIWORD(hit)==0&&(compact||GET_X_LPARAM(l)<self->u(42)))self->toggleRoster(LOWORD(hit));return result;}
        if(msg==WM_KEYDOWN&&w==VK_SPACE){self->toggleRoster(int(SendMessageW(hwnd,LB_GETCURSEL,0,0)));return 0;}
        if(msg==WM_KEYDOWN&&w==VK_RETURN&&compact){const auto rows=self->last.value("characters",Json::array());const int n=int(SendMessageW(hwnd,LB_GETCURSEL,0,0));if(n>=0&&n<int(rows.size())&&rows[n].value("ready",false)){self->receive({{"type","roster_select"},{"id",rows[n]["id"]}});self->sync();}return 0;}
        if(msg==WM_NCDESTROY)RemoveWindowSubclass(hwnd,listProc,2);return DefSubclassProc(hwnd,msg,w,l);
    }
    bool route(MSG& message){
        HWND root{};if(modelMenu&&IsWindowVisible(modelMenu)&&(message.hwnd==modelMenu||IsChild(modelMenu,message.hwnd)))root=modelMenu;else if(quickWindow&&IsWindowVisible(quickWindow)&&(message.hwnd==quickWindow||IsChild(quickWindow,message.hwnd)))root=quickWindow;else if(window&&IsWindowVisible(window)&&(message.hwnd==window||IsChild(window,message.hwnd)))root=window;
        if((message.message==WM_LBUTTONDOWN||message.message==WM_RBUTTONDOWN)&&modelMenu&&IsWindowVisible(modelMenu)&&root!=modelMenu)hideModelMenu();
        if(message.message==WM_LBUTTONDOWN||message.message==WM_RBUTTONDOWN||message.message==WM_MBUTTONDOWN){if(quickWindow&&IsWindowVisible(quickWindow)&&root!=quickWindow)hideQuick();}if(!root)return false;
        if(message.message==WM_KEYDOWN){
            if(message.wParam==VK_TAB||message.wParam==VK_LEFT||message.wParam==VK_RIGHT||message.wParam==VK_UP||message.wParam==VK_DOWN)SendMessageW(root,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
            if(message.wParam==VK_ESCAPE){if(root==modelMenu)hideModelMenu();else if(root==quickWindow)hideQuick();else hide();return true;}if(message.wParam==VK_TAB)return IsDialogMessageW(root,&message)!=FALSE;
            if(message.wParam==VK_RETURN){wchar_t cls[32]{};GetClassNameW(message.hwnd,cls,32);if(std::wcscmp(cls,L"EDIT")==0){save();SetFocus(window);return true;}if(std::wcscmp(cls,L"Button")==0){SendMessageW(message.hwnd,BM_CLICK,0,0);return true;}}
            if((root==modelMenu||(root==quickWindow&&message.hwnd!=quickList))&&(message.wParam==VK_DOWN||message.wParam==VK_UP)){const auto next=GetNextDlgTabItem(root,GetFocus(),message.wParam==VK_UP);if(next)SetFocus(next);return true;}}
        return false;
    }
    void reloadTheme(HWND target,UINT dpi,int appearance=-1){
        auto& theme=target==modelMenu?modelUi:target==quickWindow?quickUi:ui;theme.load(dpi,appearance);theme.frame(target,target==quickWindow||target==modelMenu);
        EnumChildWindows(target,[](HWND h,LPARAM data)->BOOL{auto* t=reinterpret_cast<NativeFluent*>(data);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(t->font),FALSE);t->themed(h);return TRUE;},reinterpret_cast<LPARAM>(&theme));
        if(target==modelMenu){hideModelMenu();}
        else if(target==quickWindow){SendMessageW(GetDlgItem(target,3100),WM_SETFONT,reinterpret_cast<WPARAM>(theme.heading),FALSE);for(int id:{3101,3102})SendMessageW(GetDlgItem(target,id),WM_SETFONT,reinterpret_cast<WPARAM>(theme.small),FALSE);layoutQuick();}
        else{SendMessageW(GetDlgItem(window,10),WM_SETFONT,reinterpret_cast<WPARAM>(ui.title),FALSE);SendMessageW(GetDlgItem(window,11),WM_SETFONT,reinterpret_cast<WPARAM>(ui.small),FALSE);const int before=scroll;build();scrollTo(before);}
        RedrawWindow(target,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_ERASE);
    }
    static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
        auto* self=reinterpret_cast<NativeSettings*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<NativeSettings*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}if(!self)return DefWindowProcW(hwnd,msg,w,l);
        const bool context=hwnd==self->modelMenu,compact=hwnd==self->quickWindow;auto& theme=context?self->modelUi:compact?self->quickUi:self->ui;
        if(msg==WM_ENTERSIZEMOVE){self->moveWait=std::make_unique<NativeHealth::Pause>();return 0;}if(msg==WM_EXITSIZEMOVE){self->moveWait.reset();return 0;}
        if(msg==WM_CLOSE){if(context)self->hideModelMenu();else if(compact)self->hideQuick();else self->hide();return 0;}
        if(msg==WM_ACTIVATE&&(compact||context)&&LOWORD(w)==WA_INACTIVE){if(context)self->hideModelMenu();else self->hideQuick();return 0;}
        if(msg==WM_ACTIVATE&&hwnd==self->window&&LOWORD(w)!=WA_INACTIVE&&self->page==3)PostMessageW(hwnd,WM_APP+40,0,0);
        if(msg==WM_APP+40){if(self->receive)self->receive({{"type","refresh_models"}});return 0;}
        if(msg==WM_NCCALCSIZE&&(compact||context)&&w)return 0;if(msg==WM_NCHITTEST&&(compact||context))return HTCLIENT;
        if(msg==WM_SIZE&&hwnd==self->window&&self->panel){if(w==SIZE_MINIMIZED)self->hideModelMenu();self->layout();self->notify();return 0;}if(msg==WM_GETMINMAXINFO&&hwnd==self->window){auto* info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={self->u(800),self->u(540)};return 0;}
        if(msg==WM_DPICHANGED){const auto* r=reinterpret_cast<RECT*>(l);self->reloadTheme(hwnd,HIWORD(w));SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
        if((msg==WM_SETTINGCHANGE||msg==WM_THEMECHANGED)&&hwnd!=self->panel){if(self->panel||compact)self->reloadTheme(hwnd,GetDpiForWindow(hwnd));return 0;}
        if(msg==WM_COMMAND){self->command(LOWORD(w),HIWORD(w));return 0;}if(msg==WM_HSCROLL){self->save();return 0;}
        if(hwnd==self->panel&&(msg==WM_VSCROLL||msg==WM_MOUSEWHEEL)){int next=self->scroll;if(msg==WM_MOUSEWHEEL)next-=GET_WHEEL_DELTA_WPARAM(w)/120*self->u(96);else if(LOWORD(w)==SB_LINEUP)next-=self->u(40);else if(LOWORD(w)==SB_LINEDOWN)next+=self->u(40);else if(LOWORD(w)==SB_PAGEUP)next-=self->u(320);else if(LOWORD(w)==SB_PAGEDOWN)next+=self->u(320);else if(LOWORD(w)==SB_THUMBTRACK){SCROLLINFO info{sizeof(info),SIF_TRACKPOS};GetScrollInfo(hwnd,SB_VERT,&info);next=info.nTrackPos;}self->scrollTo(next);return 0;}
        if(msg==WM_MOUSEWHEEL&&hwnd==self->window){SendMessageW(self->panel,msg,w,l);return 0;}
        if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORBTN||msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX){
            const auto dc=reinterpret_cast<HDC>(w);const auto child=reinterpret_cast<HWND>(l);const bool inPanel=GetParent(child)==self->panel;bool card=inPanel&&GetDlgCtrlID(child)!=0&&GetDlgCtrlID(child)!=2110;
            if(child==self->listEmpty)card=true;
            for(const auto& c:self->controls)if(c.label==child||c.hint==child){card=true;break;}
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,!IsWindowEnabled(child)?theme.disabled:GetPropW(child,L"FluentMuted")?theme.muted:theme.text);SetBkColor(dc,context||compact||card?theme.surface:theme.canvas);return reinterpret_cast<LRESULT>(context||compact||card?theme.surfaceBrush:theme.canvasBrush);
        }
        if(msg==WM_DRAWITEM){self->draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}if(msg==WM_MEASUREITEM){auto* item=reinterpret_cast<MEASUREITEMSTRUCT*>(l);if(item->CtlType==ODT_COMBOBOX){item->itemHeight=theme.px(32);return TRUE;}if(item->CtlID==2000||item->CtlID==3200){item->itemHeight=theme.px(item->CtlID==3200?36:64);return TRUE;}}
        if(msg==WM_ERASEBKGND)return 1;if(msg==WM_PAINT){PAINTSTRUCT ps{};auto dc=BeginPaint(hwnd,&ps);self->paint(hwnd,dc);EndPaint(hwnd,&ps);return 0;}return DefWindowProcW(hwnd,msg,w,l);
    }
    void ensureFull(){
        if(window)return;INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};InitCommonControlsEx(&common);WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BaCafeNativeSettings";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);ui.load(GetDpiForSystem());
        window=CreateWindowExW(WS_EX_APPWINDOW|WS_EX_CONTROLPARENT,wc.lpszClassName,L"BA Cafe · 设置",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,u(1040),u(780),nullptr,nullptr,wc.hInstance,this);if(!window)throw std::runtime_error("Cannot create Fluent settings window");ui.frame(window);
        auto h=control(window,L"STATIC",L"全局设置",SS_ENDELLIPSIS,10,256,24,400,40);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(ui.title),FALSE);h=control(window,L"STATIC",L"",SS_ENDELLIPSIS,11,258,70,600,22);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(ui.small),FALSE);SetPropW(h,L"FluentMuted",reinterpret_cast<HANDLE>(1));
        h=control(window,L"STATIC",L"BA Cafe",0,12,24,24,176,28);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(ui.heading),FALSE);h=control(window,L"STATIC",L"桌面陪伴",0,13,24,54,176,20);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(ui.small),FALSE);SetPropW(h,L"FluentMuted",reinterpret_cast<HANDLE>(1));
        for(int i=0;i<4;++i)control(window,L"BUTTON",pages[i],BS_OWNERDRAW,1000+i,12,106+i*48,200,40);control(window,L"BUTTON",L"使用说明",BS_OWNERDRAW,3,12,660,200,40);control(window,L"BUTTON",L"退出桌宠",BS_OWNERDRAW,2,12,708,200,40);control(window,L"BUTTON",L"＋ 导入模型",BS_OWNERDRAW,2200,840,28,146,36);
        panel=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_CLIPCHILDREN,u(240),u(112),u(750),u(550),window,nullptr,wc.hInstance,this);ui.themed(panel);
        h=control(window,L"STATIC",L"",SS_ENDELLIPSIS,2300,258,712,600,34);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(ui.small),FALSE);SetPropW(h,L"FluentMuted",reinterpret_cast<HANDLE>(1));progress=control(window,L"STATIC",L"",SS_OWNERDRAW,2301,258,760,600,3);
    }
    void show(HWND,const std::filesystem::path& directory){
        if(receive)receive({{"type","refresh_models"}});
        defaultsFile=directory/L"defaults.ini";ensureFull();if(snapshot)last=readSnapshot();highlighted=last.value("selectedId",std::string{});build();
        if(!IsWindowVisible(window)){POINT p{};GetCursorPos(&p);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),&info);const auto area=info.rcWork;
            SetWindowPos(window,nullptr,p.x,p.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);const auto dpi=GetDpiForWindow(window);if(dpi!=ui.dpi)reloadTheme(window,dpi);
            const int width=std::min(u(1040),int(area.right-area.left)-u(32)),height=std::min(u(780),int(area.bottom-area.top)-u(32));SetWindowPos(window,HWND_NOTOPMOST,area.left+(area.right-area.left-width)/2,area.top+(area.bottom-area.top-height)/2,width,height,SWP_SHOWWINDOW);}else ShowWindow(window,SW_RESTORE);
        hideQuick();notify();SetForegroundWindow(window);SetFocus(GetDlgItem(window,1000+page));sync();
    }
    void ensureQuick(){
        if(quickWindow)return;INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BaCafeQuickMenu";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.style=CS_DROPSHADOW;RegisterClassW(&wc);quickUi.load(GetDpiForSystem());
        quickWindow=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT,wc.lpszClassName,L"BA Cafe 快捷菜单",WS_POPUP|WS_THICKFRAME|WS_CLIPCHILDREN,0,0,quickUi.px(344),quickUi.px(600),nullptr,nullptr,wc.hInstance,this);if(!quickWindow)throw std::runtime_error("Cannot create Fluent quick menu");quickUi.frame(quickWindow,true);SetWindowPos(quickWindow,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        auto h=control(quickWindow,L"STATIC",L"桌面角色",SS_ENDELLIPSIS,3100,20,14,300,24,true);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(quickUi.heading),FALSE);h=control(quickWindow,L"STATIC",L"已导入的模型",0,3101,20,44,300,22,true);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(quickUi.small),FALSE);SetPropW(h,L"FluentMuted",reinterpret_cast<HANDLE>(1));
        quickList=control(quickWindow,L"LISTBOX",L"勾选显示模型，空格切换，回车设为当前角色",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT|WS_VSCROLL,3200,8,72,328,144,true);SetWindowSubclass(quickList,listProc,2,reinterpret_cast<DWORD_PTR>(this));SendMessageW(quickList,LB_SETITEMHEIGHT,0,quickUi.px(36));
        control(quickWindow,L"STATIC",L"尚未导入模型",SS_CENTER,3103,20,72,304,36,true);
        for(auto [id,text]:{std::pair{3000,L"咖啡厅"},{3001,L"战斗"},{3002,L"位置跟随鼠标"},{3003,L"允许散步"},{3004,L"视线追踪鼠标"},{3005,L"允许战斗走动"},{3011,L"显示完整界面"},{3013,L"打开模型文件夹"},{3010,L"导入模型…"},{3012,L"退出桌宠"}})control(quickWindow,L"BUTTON",text,BS_OWNERDRAW,id,8,0,328,36,true);
        h=control(quickWindow,L"STATIC",L"",0,3102,20,0,304,34,true);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(quickUi.small),FALSE);SetPropW(h,L"FluentMuted",reinterpret_cast<HANDLE>(1));
    }
    int layoutQuick(){
        RECT rect{};GetWindowRect(quickWindow,&rect);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromRect(&rect,MONITOR_DEFAULTTONEAREST),&info);const int maxDip=quickUi.dip(info.rcWork.bottom-info.rcWork.top)-24;
        quickRows=std::clamp(int(last.value("characters",Json::array()).size()),1,std::clamp((maxDip-484)/36,1,4));
        const auto place=[&](int id,int x,int y,int w,int h){SetWindowPos(GetDlgItem(quickWindow,id),nullptr,quickUi.px(x),quickUi.px(y),quickUi.px(w),quickUi.px(h),SWP_NOZORDER|SWP_NOACTIVATE);};
        place(3100,20,14,300,24);place(3101,20,44,300,22);place(3200,8,72,328,quickRows*36);place(3103,20,72,304,36);SendMessageW(quickList,LB_SETITEMHEIGHT,0,quickUi.px(36));int y=72+quickRows*36+12;place(3000,12,y,156,36);place(3001,176,y,156,36);y+=48;
        for(int id:{3002,3003,3004,3005}){place(id,8,y,328,36);y+=36;}y+=12;for(int id:{3011,3013,3010,3012}){place(id,8,y,328,36);y+=36;}y+=8;place(3102,20,y,304,34);return y+44;
    }
    void showQuick(HWND owner,const std::filesystem::path& directory){
        hideModelMenu();if(receive)receive({{"type","refresh_models"}});
        defaultsFile=directory/L"defaults.ini";if(quickWindow&&IsWindowVisible(quickWindow)){hideQuick();return;}if(snapshot)last=readSnapshot();highlighted=last.value("selectedId",std::string{});ensureQuick();
        POINT cursor{};GetCursorPos(&cursor);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&info);SetWindowPos(quickWindow,nullptr,cursor.x,cursor.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        const auto dpi=GetDpiForWindow(quickWindow);if(dpi!=quickUi.dpi)reloadTheme(quickWindow,dpi);const int height=quickUi.px(layoutQuick()),width=quickUi.px(344);const auto area=info.rcWork;
        const int x=std::clamp(int(cursor.x),int(area.left),std::max(int(area.left),int(area.right)-width)),y=std::clamp(int(cursor.y),int(area.top),std::max(int(area.top),int(area.bottom)-height));
        update();SetWindowPos(quickWindow,HWND_TOPMOST,x,y,width,height,SWP_SHOWWINDOW);notify();SetForegroundWindow(quickWindow);SetFocus(quickList);
    }
};
