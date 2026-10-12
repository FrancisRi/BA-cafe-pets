#pragma once
// Reuses the pet's WebView2 environment; the independent menu never resizes
// the transparent renderer or replaces its alpha hit region.
struct WebSettings {
    HWND window{};ICoreWebView2Controller* controller{};ICoreWebView2* browser{};
    std::function<Json()> snapshot;
    std::function<void(const Json&)> receive;
    std::function<void(bool)> visibility;
    ~WebSettings(){if(controller)controller->Close();release(browser);release(controller);if(window)DestroyWindow(window);}
    void sync(){if(browser){const auto text=wide(snapshot().dump());browser->PostWebMessageAsJson(text.c_str());}}
    void hide(){if(window)ShowWindow(window,SW_HIDE);if(controller)controller->put_IsVisible(FALSE);visibility(false);}
    static LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
        auto* self=reinterpret_cast<WebSettings*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){self=static_cast<WebSettings*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(self){
            if(message==WM_CLOSE){self->hide();return 0;}
            if(message==WM_SIZE&&self->controller){RECT rect{};GetClientRect(hwnd,&rect);self->controller->put_Bounds(rect);return 0;}
            if(message==WM_DPICHANGED){const auto r=*reinterpret_cast<RECT*>(l);SetWindowPos(hwnd,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE|SWP_NOZORDER);return 0;}
            if(message==WM_GETMINMAXINFO){auto* limits=reinterpret_cast<MINMAXINFO*>(l);const float scale=GetDpiForWindow(hwnd)/96.0f;limits->ptMinTrackSize={LONG(460*scale),LONG(400*scale)};return 0;}
        }
        return DefWindowProcW(hwnd,message,w,l);
    }
    void show(HWND owner,ICoreWebView2Environment* environment,const std::filesystem::path& directory){
        if(window&&IsWindowVisible(window)){hide();return;}
        if(!window){
            WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"ThreePetWebSettings";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
            window=CreateWindowExW(WS_EX_APPWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"ba_cafe_lite 设置",WS_POPUP|WS_THICKFRAME|WS_CLIPCHILDREN,0,0,520,730,nullptr,nullptr,wc.hInstance,this);
            if(!window)throw std::runtime_error("Cannot create web settings window");
            auto* complete=new Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,HRESULT,ICoreWebView2Controller*>([this,directory](HRESULT result,ICoreWebView2Controller* value){
                check(result,"Create settings WebView");controller=value;controller->AddRef();check(controller->get_CoreWebView2(&browser),"Get settings browser");
                RECT rect{};GetClientRect(window,&rect);controller->put_Bounds(rect);controller->put_IsVisible(IsWindowVisible(window));
                ICoreWebView2_3* mapping=nullptr;check(browser->QueryInterface(uuid<ICoreWebView2_3>(),reinterpret_cast<void**>(&mapping)),"Settings folder mapping");
                check(mapping->SetVirtualHostNameToFolderMapping(L"pet.local",(directory/L"app").c_str(),COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS),"Map settings folder");mapping->Release();
                ICoreWebView2Settings* options=nullptr;browser->get_Settings(&options);options->put_AreDefaultContextMenusEnabled(FALSE);options->put_IsStatusBarEnabled(FALSE);options->Release();
                auto* messages=new Callback<ICoreWebView2WebMessageReceivedEventHandler,ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs*>([this](ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs* args){
                    LPWSTR source=nullptr;args->get_Source(&source);const bool local=source&&std::wstring(source)==L"https://pet.local/settings.html";CoTaskMemFree(source);if(!local)return S_OK;
                    LPWSTR text=nullptr;check(args->get_WebMessageAsJson(&text),"Read settings message");const auto json=utf8(text);CoTaskMemFree(text);
                    try{receive(Json::parse(json));}catch(const std::exception& error){std::fprintf(stderr,"Settings: %s\n",error.what());}return S_OK;
                });EventRegistrationToken token{};browser->add_WebMessageReceived(messages,&token);messages->Release();
                check(browser->Navigate(L"https://pet.local/settings.html"),"Open settings page");return S_OK;
            });
            const auto result=environment->CreateCoreWebView2Controller(window,complete);complete->Release();check(result,"Start settings browser");
        }
        POINT cursor{};GetCursorPos(&cursor);MONITORINFO info{sizeof(info),{},{},0};GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&info);
        SetWindowPos(window,nullptr,cursor.x,cursor.y,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
        const float scale=GetDpiForWindow(window)/96.0f;
        const auto r=info.rcWork;const int width=std::min(int(520*scale),int(r.right-r.left)),height=std::min(int(730*scale),int(r.bottom-r.top-24*scale));
        SetWindowPos(window,HWND_TOPMOST,std::clamp(int(cursor.x+12),int(r.left),std::max(int(r.left),int(r.right)-width)),std::clamp(int(cursor.y-40),int(r.top),std::max(int(r.top),int(r.bottom)-height)),width,height,SWP_SHOWWINDOW);
        if(controller)controller->put_IsVisible(TRUE);visibility(true);sync();SetForegroundWindow(window);
    }
};
