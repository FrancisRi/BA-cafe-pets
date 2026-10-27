#pragma once
#include <fstream>
// Small native, owned settings popup. Requires windows.h and pet_behavior.h.
struct PetSettings {
    pet::Playback& state;
    HWND window{}, idleEdit{}, activeEdit{};
    HFONT font{}, titleFont{};
    HBRUSH white = CreateSolidBrush(RGB(255,255,255));
    std::wstring title, ini;
    std::wstring outlineTitle=L"模型描边", outlineDescription=L"突出角色外轮廓";
    // Opt-in: the separate Three.js prototype keeps its existing menu.
    bool modesEnabled=false, battleAvailable=false, battleMode=false,formationMode=false;
    std::optional<bool> modeRequest;
    int bodyOffset() const { return modesEnabled ? 64 : 0; }
    int height() const { return 568+bodyOffset(); }
    bool initializing = false, invalid = false;
    bool importBusy = false;
    int importPercent = 0;
    std::wstring progressTitle, progressDetail;
    std::filesystem::path progressFile;
    ULONGLONG importStarted = 0, nextProgressPoll = 0;
    void beginProgress(const std::filesystem::path& file) {
        progressFile=file;importPercent=0;progressTitle=L"准备导入";progressDetail.clear();
        importStarted=GetTickCount64();nextProgressPoll=0;
        std::error_code error;std::filesystem::remove(file,error);
    }
    void pollProgress() {
        const auto now=GetTickCount64();if(now<nextProgressPoll)return;nextProgressPoll=now+250;
        std::ifstream file(progressFile,std::ios::binary);
        std::string number,stage,detail;
        if(std::getline(file,number)&&std::getline(file,stage)&&std::getline(file,detail)&&!file.eof()) {
            char* end=nullptr;const long percent=std::strtol(number.c_str(),&end,10);
            if(end!=number.c_str()&&!*end&&percent>=0&&percent<=100) {
                const wchar_t* label=nullptr;
                if(stage=="prepare")label=L"1/6  检查模型与准备资源";
                else if(stage=="sampling")label=L"2/6  采样动画";
                else if(stage=="converting")label=L"3/6  Blender 转换模型";
                else if(stage=="validating")label=L"4/6  校验骨架、嘴型与材质";
                else if(stage=="rendering")label=L"5/6  检查动作加载";
                else if(stage=="comparing")label=L"6/6  对照动画数据";
                else if(stage=="ready")label=L"检查完成，准备显示";
                else if(stage=="launching")label=L"正在启动新角色";
                else if(stage=="complete")label=L"导入完成";
                else if(stage=="failed")label=L"导入失败，原角色保留";
                if(label) {
                    if(importPercent!=percent || progressTitle!=label)
                        TraceLog(LOG_INFO,"IMPORT progress: %ld%% %s",percent,stage.c_str());
                    importPercent=static_cast<int>(percent);progressTitle=label;
                    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,detail.data(),static_cast<int>(detail.size()),nullptr,0);
                    progressDetail.assign(size,L' ');
                    if(size)MultiByteToWideChar(CP_UTF8,0,detail.data(),static_cast<int>(detail.size()),progressDetail.data(),size);
                }
            }
        }
        if(window)InvalidateRect(window,nullptr,FALSE);
    }
    std::wstring importSource, importMessage;
    static constexpr COLORREF Ink = RGB(37,57,81), Blue = RGB(30,174,230);

    PetSettings(pet::Playback& s, std::wstring name, std::wstring file)
        : state(s), title(std::move(name)), ini(std::move(file)) {
        state.walkEnabled = GetPrivateProfileIntW(L"Behavior", L"Walking", 1, ini.c_str()) != 0;
        state.trackingEnabled = GetPrivateProfileIntW(L"Behavior", L"Tracking", 1, ini.c_str()) != 0;
        state.outlineEnabled = GetPrivateProfileIntW(L"Behavior", L"Outline", 0, ini.c_str()) != 0;
        auto read = [&](const wchar_t* key, std::optional<double>& target) {
            wchar_t value[24]{};
            GetPrivateProfileStringW(L"Behavior", key, L"", value, 24, ini.c_str());
            wchar_t* end = nullptr;
            const long seconds = std::wcstol(value, &end, 10);
            if (end != value && *end == 0 && seconds >= 1 && seconds <= 3600) target = seconds;
        };
        read(L"InputIdleSeconds", state.idleSecondsOverride);
        read(L"ContinuedInputSeconds", state.activeSecondsOverride);
    }
    ~PetSettings() {
        if (window) DestroyWindow(window);
        if (font) DeleteObject(font);
        if (titleFont) DeleteObject(titleFont);
        DeleteObject(white);
    }
    void save() {
        bool ok = WritePrivateProfileStringW(L"Behavior", L"Walking", state.walkEnabled ? L"1" : L"0", ini.c_str()) != 0;
        ok &= WritePrivateProfileStringW(L"Behavior", L"Tracking", state.trackingEnabled ? L"1" : L"0", ini.c_str()) != 0;
        ok &= WritePrivateProfileStringW(L"Behavior", L"Outline", state.outlineEnabled ? L"1" : L"0", ini.c_str()) != 0;
        auto write = [&](const wchar_t* key, std::optional<double> value) {
            if (value) ok &= WritePrivateProfileStringW(L"Behavior", key, std::to_wstring(static_cast<int>(*value)).c_str(), ini.c_str()) != 0;
        };
        write(L"InputIdleSeconds", state.idleSecondsOverride);
        write(L"ContinuedInputSeconds", state.activeSecondsOverride);
        if(modesEnabled)ok &= WritePrivateProfileStringW(L"Behavior",L"Mode",formationMode?L"Formation":battleMode?L"Battle":L"Cafe",ini.c_str())!=0;
        if (!ok) TraceLog(LOG_WARNING, "SETTINGS could not save preferences (%lu)", GetLastError());
    }
    bool applyTimes() {
        if (initializing) return true;
        bool valid = true;
        auto read = [&](HWND edit, std::optional<double>& target) {
            wchar_t text[24]{};
            GetWindowTextW(edit, text, 24);
            wchar_t* end = nullptr;
            const long value = std::wcstol(text, &end, 10);
            if (end == text || *end || value < 1 || value > 3600) valid = false;
            else target = value;
        };
        read(idleEdit, state.idleSecondsOverride);
        read(activeEdit, state.activeSecondsOverride);
        invalid = !valid;
        InvalidateRect(window, nullptr, FALSE);
        if (valid) save();
        return valid;
    }
    void hide() {
        // Invalid intermediate edits do not replace the last valid setting.
        applyTimes();
        ShowWindow(window, SW_HIDE);
        state.settingsOpen = false;
    }
    void show(HWND owner) {
        if (window && IsWindowVisible(window)) { hide(); return; }
        if (!window) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = L"DesktopPetSettings";
            wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            RegisterClassW(&wc);
            window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, wc.lpszClassName, title.c_str(),
                WS_POPUP | WS_CLIPCHILDREN, 0, 0, 432, height(), owner, nullptr, wc.hInstance, this);
            if (!window) throw std::runtime_error("Cannot create settings menu");
            font = CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
            titleFont = CreateFontW(-19,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
            auto control = [&](const wchar_t* type, const wchar_t* text, DWORD style, int id, int x,int y,int w,int h) {
                HWND child = CreateWindowExW(0,type,text,WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                    x,y+(id>=2 && id<=7?bodyOffset():0),w,h,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),wc.hInstance,nullptr);
                SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
                return child;
            };
            control(L"BUTTON",L"Close",BS_OWNERDRAW,1,380,12,36,36);
            if(modesEnabled) {
                control(L"BUTTON",L"咖啡厅模式",BS_OWNERDRAW,8,16,73,192,34);
                EnableWindow(control(L"BUTTON",L"战斗模式",BS_OWNERDRAW,9,224,73,192,34),battleAvailable);
            }
            control(L"BUTTON",L"走动",BS_OWNERDRAW,2,336,86,64,28);
            control(L"BUTTON",L"追踪鼠标",BS_OWNERDRAW,3,336,204,64,28);
            control(L"BUTTON",L"模型描边",BS_OWNERDRAW,6,336,322,64,28);
            control(L"BUTTON",L"导入 GLB 模型…",BS_OWNERDRAW,7,16,404,400,38);
            idleEdit = control(L"EDIT",L"",ES_NUMBER | ES_CENTER | ES_AUTOHSCROLL,4,308,131,66,23);
            activeEdit = control(L"EDIT",L"",ES_NUMBER | ES_CENTER | ES_AUTOHSCROLL,5,308,249,66,23);
            SendMessageW(idleEdit,EM_SETLIMITTEXT,4,0); SendMessageW(activeEdit,EM_SETLIMITTEXT,4,0);
        }
        initializing = true;
        SetWindowTextW(idleEdit,std::to_wstring(static_cast<int>(state.inactivityThreshold())).c_str());
        SetWindowTextW(activeEdit,std::to_wstring(static_cast<int>(state.activeSecondsOverride.value_or(30))).c_str());
        initializing = false; invalid = false;
        POINT cursor{}; GetCursorPos(&cursor);
        MONITORINFO monitor{sizeof(MONITORINFO),{},{},0};
        GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&monitor);
        const RECT r = monitor.rcWork;
        SetWindowPos(window,HWND_TOPMOST,std::clamp(cursor.x+12,r.left,std::max(r.left,r.right-432)),
            std::clamp(cursor.y-40,r.top,std::max(r.top,r.bottom-height())),432,height(),SWP_SHOWWINDOW);
        state.settingsOpen = true;
        SetForegroundWindow(window); SetFocus(window);
        InvalidateRect(window,nullptr,TRUE);
    }
    void paint(HDC dc) {
        RECT all{0,0,432,height()};
        HBRUSH backdrop=CreateSolidBrush(RGB(247,250,253)); FillRect(dc,&all,backdrop); DeleteObject(backdrop);
        auto card = [&](RECT rect, COLORREF color, COLORREF border, int radius) {
            HBRUSH brush=CreateSolidBrush(color); HPEN pen=CreatePen(PS_SOLID,1,border);
            const auto previousBrush=SelectObject(dc,brush); const auto previousPen=SelectObject(dc,pen);
            RoundRect(dc,rect.left,rect.top,rect.right,rect.bottom,radius,radius);
            SelectObject(dc,previousBrush); SelectObject(dc,previousPen); DeleteObject(brush); DeleteObject(pen);
        };
        RECT header{0,0,432,61}; FillRect(dc,&header,white);
        card({0,0,432,4},Blue,Blue,0);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,Ink);
        auto text = [&](const wchar_t* value, RECT rect, HFONT face) {
            const auto previous=SelectObject(dc,face);
            DrawTextW(dc,value,-1,&rect,DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(dc,previous);
        };
        text(title.c_str(),{22,10,370,54},titleFont);
        if(modesEnabled) {
            SetTextColor(dc,RGB(101,125,148));
            text(battleAvailable?L"战斗：输入触发动作 · 连击角色三次退场":L"当前模型缺少战斗动作，战斗模式不可用",{22,109,412,132},font);
            SetTextColor(dc,Ink);
        }
        SetViewportOrgEx(dc,0,bodyOffset(),nullptr);
        for (RECT rect : {RECT{16,76,416,180},RECT{16,194,416,298},RECT{16,312,416,392}})
            card(rect,RGB(255,255,255),RGB(225,236,245),14);
        card({298,125,384,160},RGB(255,255,255),RGB(184,206,225),10);
        card({298,243,384,278},RGB(255,255,255),RGB(184,206,225),10);
        text(L"走动",{30,82,280,116},titleFont);
        text(L"Input idle 时间",{30,125,267,157},font);
        text(L"秒",{390,126,414,158},font);
        text(L"追踪鼠标",{30,200,280,234},titleFont);
        text(L"Continued input 时间",{30,243,267,275},font);
        text(L"秒",{390,244,414,276},font);
        text(outlineTitle.c_str(),{30,318,280,352},titleFont);
        SetTextColor(dc,RGB(101,125,148));
        text(outlineDescription.c_str(),{30,354,300,380},font);
        SetTextColor(dc,Ink);
        text(progressTitle.empty()?L"模型导入":progressTitle.c_str(),{22,450,412,478},font);
        SetTextColor(dc,RGB(101,125,148));
        const wchar_t* detail=progressDetail.empty()?(importBusy?L"正在处理，请稍候…":L"选择 GLB 文件开始导入"):progressDetail.c_str();
        text(detail,{22,478,412,502},font);
        card({22,507,410,519},RGB(222,234,244),RGB(222,234,244),10);
        if(importPercent>0)card({22,507,22+388*importPercent/100,519},Blue,Blue,10);
        if(importBusy) {
            const int x=22+static_cast<int>((GetTickCount64()/12)%358);
            card({x,507,x+30,519},RGB(133,220,249),RGB(133,220,249),8);
        }
        std::wstring info=L"步骤进度 "+std::to_wstring(importPercent)+L"%";
        if(importBusy) {
            const auto seconds=(GetTickCount64()-importStarted)/1000;
            info+=L" · 已用 "+std::to_wstring(seconds/60)+L"分"+std::to_wstring(seconds%60)+L"秒";
        }
        text(info.c_str(),{22,521,412,545},font);
        SetTextColor(dc,invalid?RGB(200,68,68):RGB(101,125,148));
        text(invalid?L"请输入 1–3600 的整数秒，保留上次有效值":
             !importMessage.empty()?importMessage.c_str():L"自动保存 · 修改立即生效",{22,543,412,568},font);
        SetViewportOrgEx(dc,0,0,nullptr);
    }
    void drawButton(const DRAWITEMSTRUCT& item) {
        HDC dc=item.hDC; RECT r=item.rcItem;
        HBRUSH background=CreateSolidBrush(RGB(255,255,255));
        FillRect(dc,&r,background); DeleteObject(background);
        if(item.CtlID==8 || item.CtlID==9) {
            const bool selected=(item.CtlID==9)==battleMode;
            HBRUSH fill=CreateSolidBrush(selected?Blue:RGB(234,242,249));FillRect(dc,&r,fill);DeleteObject(fill);
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,(item.itemState&ODS_DISABLED)?RGB(161,176,191):selected?RGB(255,255,255):Ink);
            const auto old=SelectObject(dc,font);
            DrawTextW(dc,item.CtlID==8?L"咖啡厅模式":L"战斗模式",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            SelectObject(dc,old);
        } else if (item.CtlID==7) {
            HBRUSH blue=CreateSolidBrush(Blue); FillRect(dc,&r,blue); DeleteObject(blue);
            SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
            const auto old=SelectObject(dc,font);
            DrawTextW(dc,importBusy ? L"导入处理中…" : L"＋  导入 GLB 模型…",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            SelectObject(dc,old);
        } else if (item.CtlID==1) {
            HPEN pen=CreatePen(PS_SOLID,2,Ink); const auto old=SelectObject(dc,pen);
            MoveToEx(dc,12,11,nullptr); LineTo(dc,24,23); MoveToEx(dc,24,11,nullptr); LineTo(dc,12,23);
            SelectObject(dc,old); DeleteObject(pen);
        } else {
            const bool enabled=item.CtlID==2 ? state.walkEnabled : item.CtlID==3 ? state.trackingEnabled : state.outlineEnabled;
            HBRUSH track=CreateSolidBrush(enabled ? Blue : RGB(174,190,205));
            const auto oldBrush=SelectObject(dc,track); const auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));
            RoundRect(dc,0,0,64,28,28,28);
            SelectObject(dc,white); const int x=enabled ? 39 : 3;
            Ellipse(dc,x,3,x+22,25);
            SelectObject(dc,oldBrush); SelectObject(dc,oldPen); DeleteObject(track);
        }
        if (item.itemState & ODS_FOCUS) { InflateRect(&r,-2,-2); DrawFocusRect(dc,&r); }
    }
    static LRESULT CALLBACK proc(HWND window,UINT message,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<PetSettings*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if (message==WM_NCCREATE) {
            self=static_cast<PetSettings*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(window,message,w,l);
        switch(message) {
        case WM_PAINT: { PAINTSTRUCT ps{}; HDC dc=BeginPaint(window,&ps); self->paint(dc); EndPaint(window,&ps); return 0; }
        case WM_ERASEBKGND: return 1;
        case WM_DRAWITEM: self->drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(l)); return TRUE;
        case WM_CTLCOLOREDIT: SetTextColor(reinterpret_cast<HDC>(w),Ink); SetBkColor(reinterpret_cast<HDC>(w),RGB(255,255,255)); return reinterpret_cast<LRESULT>(self->white);
        case WM_COMMAND:
            if (LOWORD(w)==1 && HIWORD(w)==BN_CLICKED) self->hide();
            else if (LOWORD(w)==2 && HIWORD(w)==BN_CLICKED) { self->state.walkEnabled=!self->state.walkEnabled; self->save(); InvalidateRect(GetDlgItem(window,2),nullptr,TRUE); }
            else if (LOWORD(w)==3 && HIWORD(w)==BN_CLICKED) { self->state.trackingEnabled=!self->state.trackingEnabled; if (!self->state.trackingEnabled) self->state.trackingCursor=false; self->save(); InvalidateRect(GetDlgItem(window,3),nullptr,TRUE); }
            else if (LOWORD(w)==6 && HIWORD(w)==BN_CLICKED) { self->state.outlineEnabled=!self->state.outlineEnabled; self->save(); InvalidateRect(GetDlgItem(window,6),nullptr,TRUE); }
            else if ((LOWORD(w)==8 || LOWORD(w)==9) && HIWORD(w)==BN_CLICKED && self->modesEnabled) {
                if(LOWORD(w)==8 || self->battleAvailable) self->modeRequest=LOWORD(w)==9;
            }
            else if (LOWORD(w)==7 && HIWORD(w)==BN_CLICKED && !self->importBusy) {
                wchar_t file[32768]{};
                OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(OPENFILENAMEW);
                dialog.hwndOwner=window; dialog.lpstrFilter=L"GLB 模型 (*.glb)\0*.glb\0\0";
                dialog.lpstrFile=file; dialog.nMaxFile=32768;
                dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
                if (GetOpenFileNameW(&dialog)) {
                    self->importSource=file; self->importBusy=true; self->importMessage.clear();
                    InvalidateRect(window,nullptr,TRUE); InvalidateRect(GetDlgItem(window,7),nullptr,TRUE);
                }
            }
            else if ((LOWORD(w)==4 || LOWORD(w)==5) && HIWORD(w)==EN_CHANGE) self->applyTimes();
            return 0;
        case WM_CLOSE: self->hide(); return 0;
        case WM_KEYDOWN: if (w==VK_ESCAPE || w==VK_RETURN) { self->hide(); return 0; } break;
        case WM_NCHITTEST: { POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)}; ScreenToClient(window,&p); if(p.y<60 && p.x<374) return HTCAPTION; break; }
        }
        return DefWindowProcW(window,message,w,l);
    }
};
