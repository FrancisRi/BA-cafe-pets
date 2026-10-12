#pragma once
#include <commctrl.h>
#include <dwmapi.h>
// Small Win32 menu. Uses the same JSON commands as the current desktop host;
// JSON is only an in-process value here, with no browser or server.
struct NativeSettings {
 HWND window{},panel{},list{},progress{};HFONT font{},titleFont{},smallFont{},headingFont{};
 HBRUSH background=CreateSolidBrush(RGB(240,247,252)),paper=CreateSolidBrush(RGB(255,255,255));
 std::function<Json()> snapshot;std::function<void(const Json&)> receive;std::function<void(bool)> visibility;
 struct Field{std::string key;const wchar_t* title;int kind;};
 struct Control{Field field;HWND input{},label{};bool checked=false;};std::vector<Control> controls;std::vector<HWND> content;
 Json last;int page=0,scroll=0,contentHeight=0;bool syncing=false;std::string highlighted;
 static int u(int value){return MulDiv(value,3,2);} // Menu text and spacing +50%.
 inline static const Field fields[]={
  {"mode",L"当前角色模式",2},{"outlineMode",L"角色线条",2},{"outlineWidth",L"线条粗细（10–300%）",3},
  {"walking",L"允许移动",0},{"follow",L"位置跟随鼠标",0},{"followNear",L"最近跟随距离（身位）",1},{"followFar",L"最远跟随距离（身位）",1},
  {"idleSeconds",L"无输入等待（秒）",1},{"activeSeconds",L"持续输入时间（秒）",1},{"moveInterval",L"走动间隔（秒）",1},{"moveBodies",L"走动距离（身位）",1},
  {"walkTimeJitter",L"等待随机幅度（±%）",1},{"walkDistanceJitter",L"距离随机幅度（±%）",1},
  {"cafeTracking",L"咖啡厅视线追踪鼠标",0},{"cafeLookMode",L"视线追踪方式",2},
  {"look",L"战斗动作朝向鼠标",0},{"skillProbability",L"EX 技能触发概率（%）",1},{"clickProbability",L"点击指挥概率（%）",1},{"keyboardIdle",L"无键盘输入收枪（秒）",1}
 };
 ~NativeSettings(){if(window&&IsWindow(window))DestroyWindow(window);for(auto f:{font,titleFont,smallFont,headingFont})if(f)DeleteObject(f);DeleteObject(background);DeleteObject(paper);}
 HWND control(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h){const bool label=std::wcscmp(cls,L"STATIC")==0;if(!std::wcscmp(cls,L"BUTTON")&&(style&BS_TYPEMASK)==BS_PUSHBUTTON)style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;auto child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|(label?0:WS_TABSTOP)|style,u(x),u(y),u(w),u(h),parent,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return child;}
 void hide(){if(window)ShowWindow(window,SW_HIDE);if(visibility)visibility(false);}
 void scrollTo(int value){RECT r{};GetClientRect(panel,&r);const int next=std::clamp(value,0,std::max(0,u(contentHeight)-int(r.bottom)));ScrollWindowEx(panel,0,scroll-next,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE);scroll=next;SCROLLINFO info{sizeof(info),SIF_ALL,0,std::max(0,u(contentHeight)-1),UINT(r.bottom),scroll,0};SetScrollInfo(panel,SB_VERT,&info,TRUE);}
 void layout(){RECT r{};GetClientRect(window,&r);r.right=MulDiv(r.right,2,3);r.bottom=MulDiv(r.bottom,2,3);const auto place=[&](int id,int x,int y,int w,int h){SetWindowPos(GetDlgItem(window,id),nullptr,u(x),u(y),u(std::max(1,w)),u(h),SWP_NOZORDER|SWP_NOACTIVATE);};SetWindowPos(panel,nullptr,u(180),u(96),u(std::max(1,int(r.right)-202)),u(std::max(1,int(r.bottom)-178)),SWP_NOZORDER|SWP_NOACTIVATE);place(10,194,22,r.right-440,34);place(11,196,62,r.right-235,22);place(3,r.right-232,24,64,32);place(2,r.right-154,24,70,32);place(1,r.right-70,24,38,32);place(13,24,r.bottom-82,140,48);place(2300,194,r.bottom-65,r.right-225,40);place(2301,194,r.bottom-20,r.right-225,5);scrollTo(scroll);InvalidateRect(window,nullptr,FALSE);}
 void build(){
  syncing=true;for(auto h:content)DestroyWindow(h);content.clear();controls.clear();list=nullptr;scroll=0;int y=18;
  const auto note=[&](const std::wstring& text,int h=34){auto c=control(panel,L"STATIC",text.c_str(),0,0,18,y,464,h);SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),FALSE);content.push_back(c);y+=h+12;};
  const auto section=[&](const wchar_t* text){auto c=control(panel,L"STATIC",text,0,0,18,y,460,26);SetPropW(c,L"BaCafeSection",reinterpret_cast<HANDLE>(1));SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(headingFont),FALSE);content.push_back(c);y+=36;};
  if(page==3){
   section(L"角色列表");note(L"历史最多 15 条 · 同时显示最多 5 个",24);list=control(panel,L"LISTBOX",L"",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_BORDER,2000,18,y,464,294);SendMessageW(list,LB_SETITEMHEIGHT,0,u(58));content.push_back(list);y+=310;
   int n=0;for(auto [id,text]:{std::pair{2100,L"显示角色"},{2101,L"设为当前"},{2102,L"从历史删除"},{2103,L"统一大小"},{2104,L"关闭当前角色"}}){content.push_back(control(panel,L"BUTTON",text,BS_PUSHBUTTON,id,n==4?18:18+(n%2)*238,y,n==4?464:226,36));if(n%2==1||n==4)y+=48;++n;}
   content.push_back(control(panel,L"BUTTON",L"+  导入 GLB",BS_PUSHBUTTON,2200,18,y,464,36));y+=50;
   note(L"关闭最后一个角色时会确认退出；删除列表不删除 GLB。",40);
  }else{
   note(page==0?L"参数由所有角色共享，模式只应用到当前角色。":page==1?L"持续输入后追踪视线，无输入后散步。":L"按键攻击 · 特定标点换弹 · 持续输入短走或 EX",38);
   for(size_t i=0;i<std::size(fields);i++){if((page==0&&i>=13)||(page==1&&(i<13||i>=15))||(page==2&&i<15))continue;const auto& f=fields[i];
    if(i==0)section(L"角色与显示");if(i==3)section(L"移动与跟随");if(i==7)section(L"触发时间与随机变化");if(i==13)section(L"视线追踪");if(i==15)section(L"战斗动作");
    HWND input{},label{};if(f.kind==0)input=control(panel,L"BUTTON",f.title,BS_OWNERDRAW,100+int(i),18,y,464,38);
    else{label=control(panel,L"STATIC",f.title,0,0,18,y+8,282,26);content.push_back(label);input=control(panel,f.kind==3?TRACKBAR_CLASSW:f.kind==1?L"EDIT":L"COMBOBOX",L"",f.kind==3?TBS_HORZ|TBS_NOTICKS:f.kind==1?WS_BORDER|ES_AUTOHSCROLL:CBS_DROPDOWNLIST|WS_VSCROLL,100+int(i),326,y,156,f.kind==2?180:34);if(f.kind==1)SendMessageW(input,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(u(8),u(6)));if(f.kind==3)SendMessageW(input,TBM_SETRANGE,TRUE,MAKELPARAM(10,300));
     if(f.kind==2){const auto add=[&](const wchar_t* text){SendMessageW(input,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));};if(f.key=="mode"){add(L"咖啡厅");if(last.value("battleAvailable",false))add(L"战斗");}else if(f.key=="outlineMode"){add(L"无");add(L"轮廓模式");add(L"线条模式");}else{add(L"平滑");add(L"30° 再追踪");}}
    }controls.push_back({f,input,label});content.push_back(input);y+=50;
   }
   if(page==0){content.push_back(control(panel,L"BUTTON",L"+  导入 GLB",BS_PUSHBUTTON,2200,18,y,226,36));content.push_back(control(panel,L"BUTTON",L"恢复默认",BS_PUSHBUTTON,2201,256,y,226,36));y+=50;}
   if(page==1)note(L"平滑：连续转身。30°：达到角度阈值后转身。\n位置跟随与距离在「全局」设置。",48);
   if(page==2&&!last.value("battleAvailable",false))note(L"当前角色没有可用战斗动作。",30);
  }
  contentHeight=y+18;syncing=false;layout();update();
 }
 void update(){
  if(!window||last.empty())return;syncing=true;SetWindowTextW(GetDlgItem(window,10),wide(last.value("name",std::string{"角色设置"})).c_str());const auto value=last.value("value",Json::object());SetWindowTextW(GetDlgItem(window,11),value.value("mode",std::string{})=="battle"?L"战斗模式  /  所有角色共享设置":L"咖啡厅模式  /  所有角色共享设置");
  for(auto& c:controls){if(!value.contains(c.field.key))continue;const auto& v=value[c.field.key];if(c.field.kind==0)c.checked=v.get<bool>();
   else if(c.field.kind==3){SendMessageW(c.input,TBM_SETPOS,TRUE,v.get<int>());SetWindowTextW(c.label,(std::wstring(c.field.title)+L"  "+std::to_wstring(v.get<int>())+L"%").c_str());}
   else if(c.field.kind==2){const int index=c.field.key=="mode"?(v=="battle"):c.field.key=="cafeLookMode"?(v=="threshold"):v.get<int>();SendMessageW(c.input,CB_SETCURSEL,index,0);}
   else if(GetFocus()!=c.input){auto text=v.dump();if(text.ends_with(".0"))text.resize(text.size()-2);SetWindowTextW(c.input,wide(text).c_str());}
   bool enabled=!last.value("busy",false)&&(page!=2||last.value("battleAvailable",false));if(c.field.key=="follow")enabled&=last.value("canFollow",false);if(c.field.key=="followNear"||c.field.key=="followFar")enabled&=last.value("canFollow",false)&&value.value("follow",false);if(c.field.key=="cafeLookMode")enabled&=value.value("cafeTracking",true);if(c.field.key=="outlineWidth")enabled&=value.value("outlineMode",2)!=0;EnableWindow(c.input,enabled);if(c.label)EnableWindow(c.label,enabled);InvalidateRect(c.input,nullptr,FALSE);
  }
  if(auto status=GetDlgItem(window,2300)){const auto error=last.value("error",std::string{}),detail=last.value("detail",std::string{});const auto text=!error.empty()?error:last.value("busy",false)?std::to_string(last.value("percent",0))+"%  "+detail:"自动保存  ·  打开菜单时暂停角色行为";SetPropW(status,L"BaCafeError",reinterpret_cast<HANDLE>(!error.empty()?1:0));SetWindowTextW(status,wide(text).c_str());}ShowWindow(progress,last.value("busy",false)?SW_SHOW:SW_HIDE);InvalidateRect(progress,nullptr,FALSE);
  if(auto import=GetDlgItem(panel,2200))EnableWindow(import,!last.value("busy",false)&&last.value("characters",Json::array()).size()<15);
  if(list){const auto rows=last.value("characters",Json::array());bool exists=false;for(const auto& r:rows)exists|=r.value("id",std::string{})==highlighted;if(!exists)highlighted=last.value("selectedId",std::string{});SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);int selected=-1;for(size_t i=0;i<rows.size();i++){const auto name=wide(rows[i].value("name",std::string{}));SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));if(rows[i].value("id",std::string{})==highlighted)selected=int(i);}SendMessageW(list,LB_SETCURSEL,selected,0);SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);const Json r=selected>=0?rows[selected]:Json::object();const bool active=r.value("active",false);SetWindowTextW(GetDlgItem(panel,2100),active?L"隐藏角色":L"显示角色");EnableWindow(GetDlgItem(panel,2100),selected>=0&&(active||(r.value("available",false)&&last.value("activeCount",0)<5)));EnableWindow(GetDlgItem(panel,2101),selected>=0&&active&&r.value("ready",false)&&highlighted!=last.value("selectedId",std::string{}));EnableWindow(GetDlgItem(panel,2102),selected>=0);EnableWindow(GetDlgItem(panel,2103),last.value("activeCount",0)>0);EnableWindow(GetDlgItem(panel,2104),last.value("activeCount",0)>0);}
  for(int i=0;i<4;i++)InvalidateRect(GetDlgItem(window,1000+i),nullptr,FALSE);
  syncing=false;
 }
 void sync(){if(!snapshot)return;const auto value=snapshot();const bool rebuild=!last.empty()&&last.value("battleAvailable",false)!=value.value("battleAvailable",false);last=value;if(window&&IsWindowVisible(window)){if(rebuild&&(page==0||page==2))build();else update();}}
 void save(){
  if(syncing||last.empty())return;auto value=last.at("value");
  for(const auto& c:controls){if(c.field.kind==0)value[c.field.key]=c.checked;
   else if(c.field.kind==3)value[c.field.key]=int(SendMessageW(c.input,TBM_GETPOS,0,0));
   else if(c.field.kind==2){const int selected=int(SendMessageW(c.input,CB_GETCURSEL,0,0));if(selected<0)continue;if(c.field.key=="mode")value[c.field.key]=selected?"battle":"cafe";else if(c.field.key=="cafeLookMode")value[c.field.key]=selected?"threshold":"smooth";else value[c.field.key]=selected;}
   else{wchar_t text[80]{};GetWindowTextW(c.input,text,80);wchar_t* end=nullptr;const double n=std::wcstod(text,&end);if(end==text||*end||!std::isfinite(n))return;value[c.field.key]=n;}
  }receive({{"type","save"},{"value",value}});sync();
 }
 void command(int id,int code){
  if(syncing)return;if(id==1){hide();return;}if(id==2){receive({{"type","exit"}});return;}if(id==3){MessageBoxW(window,L"右键点击设置 · 右键拖动旋转 · 左键拖拽 · 滚轮缩放\n中键拖动也可旋转；右键拖动后松开不会打开菜单。\n\n咖啡厅：点击反应；持续输入后可追踪鼠标；无输入后散步。\n战斗：按键攻击；句号、回车、分号、问号请求换弹；持续输入可短走或 EX。\n三连击：咖啡厅进入战斗，战斗退场。拖拽优先打断。",L"使用说明",MB_OK);return;}
  if(id>=1000&&id<1004){page=id-1000;build();return;}if(id>=100&&id<100+int(std::size(fields))&&(code==BN_CLICKED||code==CBN_SELCHANGE||code==EN_KILLFOCUS)){if(code==BN_CLICKED)for(auto& c:controls)if(c.input==GetDlgItem(panel,id)&&c.field.kind==0)c.checked=!c.checked;save();return;}
  if(id==2000&&code==LBN_SELCHANGE){const int selected=int(SendMessageW(list,LB_GETCURSEL,0,0));if(selected>=0&&selected<int(last["characters"].size()))highlighted=last["characters"][selected]["id"];update();return;}
  if(id==2103){receive({{"type","uniform_size"}});return;}if(id==2104){receive({{"type","close_character"}});return;}
  if(id>=2100&&id<=2102){const int selected=int(SendMessageW(list,LB_GETCURSEL,0,0));if(selected<0)return;const auto row=last["characters"][selected];if(id==2100)receive({{"type","roster_toggle"},{"id",row["id"]},{"active",!row.value("active",false)}});else if(id==2101)receive({{"type","roster_select"},{"id",row["id"]}});else if((row.value("active",false)&&last.value("activeCount",0)==1)||MessageBoxW(window,L"从历史列表移除该角色？原始 GLB 文件保留。",L"删除角色",MB_YESNO|MB_DEFBUTTON2)==IDYES)receive({{"type","roster_remove"},{"id",row["id"]}});return;}
  if(id==2200){receive({{"type","import"}});return;}
  if(id==2201){auto value=last["value"];NativePreferences defaults;value.update(defaults.json());value.update({{"walking",true},{"look",true},{"idleSeconds",60},{"activeSeconds",30}});receive({{"type","save"},{"value",value}});sync();}
 }
 static void rounded(HDC dc,RECT r,COLORREF fill,COLORREF border,int radius=10){auto brush=CreateSolidBrush(fill);auto pen=CreatePen(PS_SOLID,1,border);auto b=SelectObject(dc,brush),p=SelectObject(dc,pen);RoundRect(dc,r.left,r.top,r.right,r.bottom,u(radius),u(radius));SelectObject(dc,b);SelectObject(dc,p);DeleteObject(brush);DeleteObject(pen);}
 void draw(const DRAWITEMSTRUCT& d){
  const auto dc=d.hDC;auto r=d.rcItem;const int id=d.CtlID;const bool disabled=(d.itemState&ODS_DISABLED)!=0,pressed=(d.itemState&ODS_SELECTED)!=0,focused=(d.itemState&ODS_FOCUS)!=0;FillRect(dc,&r,GetParent(d.hwndItem)==panel?paper:background);SetBkMode(dc,TRANSPARENT);SelectObject(dc,font);
  if(id==2301){rounded(dc,r,RGB(223,237,247),RGB(223,237,247),5);r.right=r.left+(r.right-r.left)*std::clamp(last.value("percent",0),0,100)/100;if(r.right>r.left)rounded(dc,r,RGB(41,174,218),RGB(41,174,218),5);return;}
  if(id==2000){const auto rows=last.value("characters",Json::array());if(d.itemID>=rows.size())return;const auto& row=rows[d.itemID];FillRect(dc,&r,pressed?background:paper);const bool available=row.value("available",false),active=row.value("active",false);const int cy=(r.top+r.bottom)/2;RECT dot{r.left+u(10),cy-u(10),r.left+u(30),cy+u(10)};const auto blue=active?RGB(42,177,221):RGB(226,235,242);rounded(dc,dot,blue,blue,20);if(active){SetTextColor(dc,RGB(255,255,255));DrawTextW(dc,L"✓",-1,&dot,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
   auto text=r;text.left+=u(42);text.right-=u(10);text.top+=u(6);text.bottom=text.top+u(23);SelectObject(dc,headingFont);SetTextColor(dc,available?RGB(35,60,81):RGB(167,180,192));const auto name=wide(row.value("name",std::string{}));DrawTextW(dc,name.c_str(),-1,&text,DT_SINGLELINE|DT_END_ELLIPSIS);
   std::wstring detail=!available?L"文件不存在":row.value("failed",false)?L"加载失败":row.value("loading",false)?L"加载中…":row.value("mode",std::string{})=="battle"?L"战斗模式":L"咖啡厅模式";if(row.value("id",std::string{})==last.value("selectedId",std::string{}))detail+=L"  ·  当前角色";else if(!active)detail+=L"  ·  未显示";text.top+=u(24);text.bottom+=u(24);SelectObject(dc,smallFont);SetTextColor(dc,RGB(132,154,170));DrawTextW(dc,detail.c_str(),-1,&text,DT_SINGLELINE|DT_END_ELLIPSIS);return;
  }
  for(const auto& c:controls)if(c.input==d.hwndItem&&c.field.kind==0){FillRect(dc,&r,paper);auto label=r;label.right-=u(64);SetTextColor(dc,disabled?RGB(169,183,194):RGB(51,76,95));DrawTextW(dc,c.field.title,-1,&label,DT_LEFT|DT_VCENTER|DT_SINGLELINE);RECT toggle{r.right-u(44),r.top+u(8),r.right,r.top+u(30)};const auto blue=disabled?RGB(219,230,238):c.checked?RGB(41,174,218):RGB(198,213,224);rounded(dc,toggle,blue,blue,22);const int x=toggle.left+u(c.checked?24:3);RECT dot{x,toggle.top+u(3),x+u(16),toggle.top+u(19)};rounded(dc,dot,RGB(255,255,255),RGB(255,255,255),16);if(focused){InflateRect(&r,-u(2),-u(2));DrawFocusRect(dc,&r);}return;}
  wchar_t text[180]{};GetWindowTextW(d.hwndItem,text,180);
  if(id>=1000&&id<1004){const bool selected=page==id-1000;rounded(dc,r,selected?RGB(221,242,252):RGB(240,247,252),selected?RGB(221,242,252):RGB(240,247,252),14);auto t=r;t.left+=u(16);t.top+=u(7);t.bottom=t.top+u(23);SetTextColor(dc,selected?RGB(19,147,199):RGB(77,102,122));SelectObject(dc,headingFont);DrawTextW(dc,text,-1,&t,DT_SINGLELINE);const wchar_t* descriptions[]{L"显示 · 移动 · 触发",L"视线与日常行为",L"攻击与战斗行为",L"历史与显示管理"};t.top+=u(24);t.bottom+=u(24);SelectObject(dc,smallFont);SetTextColor(dc,RGB(130,155,174));DrawTextW(dc,descriptions[id-1000],-1,&t,DT_SINGLELINE);if(selected){RECT accent{r.left,r.top+u(14),r.left+u(3),r.bottom-u(14)};rounded(dc,accent,RGB(42,177,221),RGB(42,177,221),3);}if(focused)DrawFocusRect(dc,&r);return;}
  const bool danger=id==2||id==2102||id==2104,primary=id==2200||id==2100;COLORREF fill=disabled?RGB(241,245,248):id==2?RGB(224,76,98):primary?RGB(40,169,216):danger?RGB(255,239,242):RGB(245,250,253);if(pressed&&!disabled)fill=id==2?RGB(199,55,79):primary?RGB(23,145,195):RGB(228,241,250);rounded(dc,r,fill,disabled?RGB(229,237,243):primary||id==2?fill:danger?RGB(252,216,224):RGB(214,233,244));SetTextColor(dc,disabled?RGB(171,186,197):id==2||primary?RGB(255,255,255):danger?RGB(208,78,104):RGB(55,121,158));DrawTextW(dc,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);if(focused){InflateRect(&r,-u(4),-u(4));DrawFocusRect(dc,&r);}
 }
 bool route(MSG& message){if(!window||!IsWindowVisible(window)||(message.hwnd!=window&&!IsChild(window,message.hwnd)))return false;if(message.message==WM_KEYDOWN){if(message.wParam==VK_ESCAPE){hide();return true;}if(message.wParam==VK_TAB)return IsDialogMessageW(window,&message)!=FALSE;if(message.wParam==VK_RETURN){wchar_t cls[30]{};GetClassNameW(message.hwnd,cls,30);if(!std::wcscmp(cls,L"EDIT")){save();SetFocus(window);return true;}}}return false;}
 static LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){auto* self=reinterpret_cast<NativeSettings*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(message==WM_NCCREATE){self=static_cast<NativeSettings*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}if(!self)return DefWindowProcW(hwnd,message,w,l);
  if(message==WM_CLOSE){self->hide();return 0;}if(message==WM_SIZE&&hwnd==self->window&&self->panel){self->layout();return 0;}
  if(message==WM_COMMAND){self->command(LOWORD(w),HIWORD(w));return 0;}
  if(message==WM_HSCROLL){self->save();return 0;}
  if(message==WM_NCHITTEST&&hwnd==self->window){const auto hit=DefWindowProcW(hwnd,message,w,l);if(hit!=HTCLIENT)return hit;POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(hwnd,&point);if(point.y<u(90)&&ChildWindowFromPointEx(hwnd,point,CWP_SKIPINVISIBLE|CWP_SKIPDISABLED)==hwnd)return HTCAPTION;return hit;}
  if(message==WM_GETMINMAXINFO&&hwnd==self->window){auto* limits=reinterpret_cast<MINMAXINFO*>(l);limits->ptMinTrackSize={u(740),u(500)};return 0;}
  if((hwnd==self->panel||hwnd==self->window)&&(message==WM_VSCROLL||message==WM_MOUSEWHEEL)){int value=self->scroll;if(message==WM_MOUSEWHEEL)value-=GET_WHEEL_DELTA_WPARAM(w)/120*u(100);else if(LOWORD(w)==SB_LINEUP)value-=u(40);else if(LOWORD(w)==SB_LINEDOWN)value+=u(40);else if(LOWORD(w)==SB_PAGEUP)value-=u(300);else if(LOWORD(w)==SB_PAGEDOWN)value+=u(300);else if(LOWORD(w)==SB_THUMBTRACK){SCROLLINFO info{sizeof(info),SIF_TRACKPOS};GetScrollInfo(hwnd,SB_VERT,&info);value=info.nTrackPos;}self->scrollTo(value);return 0;}
  if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLORBTN||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX){const auto dc=reinterpret_cast<HDC>(w);const auto child=reinterpret_cast<HWND>(l);const bool inPanel=GetParent(child)==self->panel;SetBkMode(dc,TRANSPARENT);SetBkColor(dc,inPanel?RGB(255,255,255):RGB(240,247,252));SetTextColor(dc,!IsWindowEnabled(child)?RGB(167,181,192):GetPropW(child,L"BaCafeError")?RGB(207,73,99):GetPropW(child,L"BaCafeSection")?RGB(33,156,202):RGB(60,87,109));return reinterpret_cast<LRESULT>(inPanel?self->paper:self->background);}
  if(message==WM_DRAWITEM){self->draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
  if(message==WM_MEASUREITEM){auto* item=reinterpret_cast<MEASUREITEMSTRUCT*>(l);if(item->CtlID==2000){item->itemHeight=u(58);return TRUE;}}
  if(message==WM_ERASEBKGND)return 1;
  if(message==WM_PAINT){PAINTSTRUCT ps{};const auto dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);FillRect(dc,&r,hwnd==self->panel?self->paper:self->background);if(hwnd==self->window){RECT stripe{0,0,r.right,u(3)};auto brush=CreateSolidBrush(RGB(48,179,221));FillRect(dc,&stripe,brush);DeleteObject(brush);RECT card{u(176),u(92),r.right-u(18),r.bottom-u(74)};rounded(dc,card,RGB(255,255,255),RGB(224,237,246),18);}EndPaint(hwnd,&ps);return 0;}
  return DefWindowProcW(hwnd,message,w,l);
 }
 void show(HWND,const std::filesystem::path&){
  if(window&&IsWindowVisible(window)){hide();return;}
  if(!window){
   INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&common);
   WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BaCafeNativeSettings";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);
   window=CreateWindowExW(WS_EX_APPWINDOW|WS_EX_TOPMOST|WS_EX_CONTROLPARENT,wc.lpszClassName,L"ba_cafe_lite 设置",WS_POPUP|WS_THICKFRAME|WS_CLIPCHILDREN,0,0,u(800),u(820),nullptr,nullptr,wc.hInstance,this);if(!window)throw std::runtime_error("Cannot create native settings");
   const auto create=[](int size,int weight){return CreateFontW(-u(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");};
   font=create(15,FW_NORMAL);titleFont=create(23,FW_SEMIBOLD);smallFont=create(12,FW_NORMAL);headingFont=create(15,FW_SEMIBOLD);
   const DWORD corners=2;DwmSetWindowAttribute(window,33,&corners,sizeof(corners));
   auto title=control(window,L"STATIC",L"角色设置",SS_ENDELLIPSIS,10,194,22,310,34);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
   auto subtitle=control(window,L"STATIC",L"所有角色共享设置",0,11,196,62,400,22);SendMessageW(subtitle,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);
   auto brand=control(window,L"STATIC",L"BA CAFE",0,12,24,30,140,28);SendMessageW(brand,WM_SETFONT,reinterpret_cast<WPARAM>(headingFont),TRUE);SetPropW(brand,L"BaCafeSection",reinterpret_cast<HANDLE>(1));
   auto footer=control(window,L"STATIC",L"轻量桌面陪伴\n所有角色共享设置",0,13,24,738,140,48);SendMessageW(footer,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);
   control(window,L"BUTTON",L"说明",BS_OWNERDRAW,3,568,24,64,32);control(window,L"BUTTON",L"退出",BS_OWNERDRAW,2,646,24,70,32);control(window,L"BUTTON",L"×",BS_OWNERDRAW,1,730,24,38,32);
   int index=0;for(const auto* text:{L"全局",L"咖啡厅",L"战斗",L"角色"}){control(window,L"BUTTON",text,BS_OWNERDRAW,1000+index,16,120+index*66,148,56);++index;}
   panel=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_CLIPCHILDREN,u(180),u(96),u(598),u(642),window,nullptr,wc.hInstance,this);
   auto status=control(window,L"STATIC",L"",0,2300,194,755,580,40);SendMessageW(status,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);
   progress=control(window,L"STATIC",L"",SS_OWNERDRAW,2301,194,798,580,5);
  }
  if(snapshot)last=snapshot();build();POINT cursor{};GetCursorPos(&cursor);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&info);const auto r=info.rcWork;const int w=std::min(u(800),int(r.right-r.left)),h=std::min(u(820),int(r.bottom-r.top));SetWindowPos(window,HWND_TOPMOST,std::clamp(int(cursor.x+12),int(r.left),int(r.right)-w),std::clamp(int(cursor.y-40),int(r.top),int(r.bottom)-h),w,h,SWP_SHOWWINDOW);if(visibility)visibility(true);sync();SetForegroundWindow(window);
 }
};
