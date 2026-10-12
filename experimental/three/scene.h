#pragma once
// One input observer and settings menu coordinate up to five native render windows.
struct Scene {
    std::vector<std::unique_ptr<App>> actors;
    SceneRoster roster;
    BattleDesktopInput input;
    std::filesystem::path historyFile;
    std::string error;
    std::vector<HWND> depthOrder;
    bool review=false,stopping=false,broadcasting=false;
    App& leader(){return *actors.front();}
    App* find(const std::string& id){for(auto& a:actors)if(a->actorId==id)return a.get();return nullptr;}
    App& selected(){auto* a=find(roster.selected);return a&&a->displayed?*a:leader();}
    void save(){if(!review){for(const auto& a:actors)if(auto* e=roster.find(a->actorId))e->zoom=a->zoom;roster.save(historyFile);}}
    float commonZoom(){std::vector<float> scales;for(const auto& a:actors)if(a->displayed&&a->loaded)scales.push_back(a->zoom);return scales.empty()?leader().zoom:modalCharacterZoom(scales);}
    void sync(){if(leader().webSettings)leader().webSettings->sync();}
    Json snapshot(){
        auto result=selected().settingsSnapshot();Json rows=Json::array();
        for(const auto& e:roster.entries){const auto* a=find(e.id);rows.push_back({{"id",e.id},{"name",e.name},{"path",e.path},{"active",e.active},
            {"available",std::filesystem::is_regular_file(wide(e.path))},{"ready",a&&a->loaded&&a->displayed},{"failed",a&&a->done&&!a->loaded},{"loading",e.active&&(!a||(!a->loaded&&a->settings->importBusy))},{"battleAvailable",a&&a->loaded&&a->settings->battleAvailable},{"mode",e.mode}});}
        result["characters"]=rows;result["selectedId"]=roster.selected;result["activeCount"]=roster.activeCount();result["maximum"]=SceneRoster::Maximum;result["historyMaximum"]=SceneRoster::HistoryMaximum;
        if(const auto* e=roster.find(roster.selected);e&&e->active){const auto* a=find(e->id);if(!a){result["name"]=e->name;result["busy"]=true;result["percent"]=0;result["detail"]="等待加载模型";result["battleAvailable"]=false;result["value"]["mode"]=e->mode;}}
        if(!error.empty())result["error"]=error;
        bool capable=false;for(const auto& a:actors)capable|=a->displayed&&a->loaded&&(a->cafeWalk>=0||(a->battle&&a->battle->clips.move>=0));
        result["canFollow"]=capable;return result;
    }
    void copySettings(App& target,const App& source){
        if(!target.preferences.follow&&source.preferences.follow){target.blockedCursor.reset();target.followPlanAt=0;target.interactionUntil=0;}
        target.preferences=source.preferences;target.state.walkEnabled=source.state.walkEnabled;
        target.state.trackingEnabled=source.state.trackingEnabled;target.state.outlineEnabled=source.state.outlineEnabled;
        target.state.idleSecondsOverride=source.state.idleSecondsOverride;target.state.activeSecondsOverride=source.state.activeSecondsOverride;
        target.state.settingsOpen=source.state.settingsOpen;target.applyPreferences();
    }
    void changed(App& source){
        if(broadcasting)return;broadcasting=true;
        for(auto& a:actors){if(a.get()!=&source)copySettings(*a,source);if(auto* e=roster.find(a->actorId))e->mode=a->battleMode?"battle":"cafe";}
        // The shared INI contains parameters. Per-character modes live only in the roster.
        save();broadcasting=false;sync();
    }
    static float radius(const App& a){return characterCollisionRadius(a.geometry.bodyWidth,a.geometry.maximum.y-a.geometry.minimum.y,a.zoom);}
    void orderDepth(){
        std::vector<App*> visible;for(auto& a:actors)if(a->displayed&&a->loaded&&a->window&&IsWindowVisible(a->window))visible.push_back(a.get());
        auto depth=[](const App& a){
            double foot=a.ground.screen(a.ground.position).y+a.work.top;
            if(a.battleMode&&a.battle&&a.battle->phase==pet::Battle::Phase::Retreat)
                foot-=int(a.battle->retreatProgress*(a.geometry.crop.y+a.geometry.height+20));
            return std::lround(foot);
        };
        std::sort(visible.begin(),visible.end(),[&](const App* a,const App* b){const auto ay=depth(*a),by=depth(*b);return ay!=by?ay>by:a->actorId<b->actorId;});
        std::vector<HWND> desired;
        const auto* menu=leader().webSettings.get();if(menu&&menu->window&&IsWindowVisible(menu->window))desired.push_back(menu->window);
        for(const auto* a:visible){desired.push_back(a->inputWindow);desired.push_back(a->window);}
        if(desired==depthOrder)return;
        // Front-to-back chain: feet lower on the desktop are in front. Native
        // window motion preserves this order, instead of promoting the last mover.
        HWND anchor=HWND_TOPMOST;
        for(HWND hwnd:desired){SetWindowPos(hwnd,anchor,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER);anchor=hwnd;}
        depthOrder=std::move(desired);
    }
    float steer(App& a,float heading,double travel){
        if(travel<=0)return heading;
        const double lookahead=std::max(travel,0.792*a.zoom*(a.battleMode?3:1)*0.3);
        auto clear=[&](float angle){
            if(!DesktopPath::stepAllowed(a.ground,angle,travel))return false;
            if(std::abs(std::remainder(angle-heading,360.0f))>0.1f&&a.state.walking&&!a.state.returningHome&&!a.directed.active
                &&(!a.battleMode||a.battle->phase==pet::Battle::Phase::Moving)){
                double distance=a.state.walkSpeed*a.state.movementScale*a.state.walkRemaining,minimum=0,maximum=0;
                if(a.battleMode){const auto brakes=a.brakeRange();minimum=brakes.first;maximum=brakes.second;}
                else if(a.smooth){const double reduction=std::min(distance,a.state.walkSpeed*a.zoom*(pet::Battle::StopFallbackSeconds*0.25+0.02));distance-=reduction;maximum=reduction;}
                if(!DesktopPath::trip(a.ground,a.geometry,a.zoom,angle,distance,minimum,maximum))return false;
            }
            const auto endpoint=a.ground.step(angle,float(lookahead));
            for(const auto& other:actors){if(other.get()==&a||!other->displayed||!other->loaded)continue;
                const auto p=other->ground.position;const double separation=radius(a)+radius(*other)+0.04;
                const double before=std::hypot(p.x-a.ground.position.x,p.z-a.ground.position.z);
                // Dragging can overlap characters: allow only an escape that increases separation.
                if(before<separation){const auto next=a.ground.step(angle,float(travel));if(std::hypot(p.x-next.x,p.z-next.z)<=before+1e-7)return false;}
                else if(segmentDistance(a.ground.position,endpoint,p)<separation)return false;
                if(other->directed.jumping){const auto landing=other->ground.step(other->directed.jumpYaw,float(other->directed.jumpSpeed*(other->directed.jumpDuration-other->directed.jumpTime)));
                    if(segmentDistance(a.ground.position,endpoint,landing)<separation)return false;}
            }return true;
        };
        // An obstructed route stops immediately; no repeated detour/heading changes.
        return clear(heading)?heading:std::numeric_limits<float>::quiet_NaN();
    }
    void placeNew(App& a){
        // Place newly loaded characters beside existing ones; keep the first actor's anchor.
        if(actors.size()==1)return;
        const auto anchor=a.ground.screen(a.ground.position);
        for(int ring=0;ring<12;++ring)for(int side:{1,-1}){
            const Vector2 pixel{anchor.x+side*ring*150.0f,anchor.y+(ring%3-1)*90.0f};
            a.geometry.place(a.ground,pixel,a.zoom);a.ground.position=a.ground.floorAt(a.geometry.idleFoot(a.ground,a.zoom));
            bool free=true;for(const auto& b:actors)if(b.get()!=&a&&b->displayed&&b->loaded)
                free&=std::hypot(b->ground.position.x-a.ground.position.x,b->ground.position.z-a.ground.position.z)>radius(a)+radius(*b)+0.12;
            if(free){a.presentState(true);return;}
        }
    }
    void attach(App& a,bool first){
        a.grouped=true;
        a.syncRequest=[this]{sync();};a.menuRequest=[this,&a]{roster.selected=a.actorId;save();leader().webSettings->show(leader().window,leader().environment,leader().directory);};
        a.closeRequest=[this,&a]{close(a.displayed?a.actorId:roster.selected);};
        a.tickCallback=[this]{tick();};a.settingsChanged=[this,&a]{changed(a);};
        a.followRequest=[this]{auto& a=selected();a.preferences.follow=!a.preferences.follow;if(a.preferences.follow){a.blockedCursor.reset();a.followPlanAt=0;a.interactionUntil=0;}a.preferences.save(a.settings->ini);changed(a);};
        a.avoidHeading=[this,&a](float yaw,double distance){return steer(a,yaw,distance);};
        a.depthRequest=[this]{orderDepth();};
        a.clearJump=[this,&a](float yaw,double distance){
            const auto end=a.ground.step(yaw,float(distance));
            for(const auto& b:actors)if(b.get()!=&a&&b->displayed&&b->loaded){
                const auto p=b->directed.jumping?b->ground.step(b->directed.jumpYaw,float(b->directed.jumpSpeed*(b->directed.jumpDuration-b->directed.jumpTime))):b->ground.position;
                if(segmentDistance(a.ground.position,end,p)<radius(a)+radius(*b)+0.04)return false;
            }return true;
        };
        a.loadedCallback=[this,&a]{copySettings(a,leader());placeNew(a);if(auto* e=roster.find(a.actorId))e->mode=a.battleMode?"battle":"cafe";save();sync();};
        a.destroyedCallback=[this,&a]{if(&a==&leader()){stopping=true;PostQuitMessage(a.exitCode);}else{a.displayed=false;if(auto* e=roster.find(a.actorId))e->active=false;save();sync();}};
        a.initialize(first);
        if(!first)copySettings(a,leader());
    }
    explicit Scene(std::unique_ptr<App> first):review(first->review){
        historyFile=first->storage()/L"characters.json";
        if(!review){try{roster.load(historyFile);}catch(const std::exception& e){error=std::string("角色历史读取失败：")+e.what();}}
        for(auto& e:roster.entries)if(e.active&&!std::filesystem::is_regular_file(wide(e.path)))e.active=false;
        bool explicitAccepted=false;
        if(!review&&first->modelSpecified){
            const auto id=SceneRoster::key(first->model);
            if(!roster.find(id)){if(roster.entries.size()<SceneRoster::HistoryMaximum)roster.entries.push_back({id,utf8(first->model.wstring()),utf8(first->model.stem().wstring()),"cafe",false});else error="历史列表已满（15 条），请先删除一个角色。";}
            explicitAccepted=roster.enable(id);
            if(explicitAccepted&&first->initialMode.empty())first->initialMode=roster.find(id)->mode;
            if(!explicitAccepted&&roster.find(id))error="模型已加入历史，最多同时显示 5 个角色。";
        }
        if(!review&&!explicitAccepted&&roster.activeCount()){
            auto* e=roster.find(roster.selected);if(!e||!e->active)e=&*std::find_if(roster.entries.begin(),roster.entries.end(),[](const auto& e){return e.active;});
            first->model=wide(e->path);first->initialMode=e->mode;
        }else if(!review&&!explicitAccepted){
            const auto* last=roster.find(roster.selected);if(last&&std::filesystem::is_regular_file(wide(last->path))){first->model=wide(last->path);first->initialMode=last->mode;}
        }
        const auto id=SceneRoster::key(first->model);
        if(!roster.find(id)&&!roster.remember({id,utf8(first->model.wstring()),utf8(first->model.stem().wstring()),first->initialMode.empty()?"cafe":first->initialMode,false}))error="历史列表已满（15 条），请先删除一个角色。";
        if(!review){std::vector<float> scales;for(const auto& e:roster.entries)if(e.active)scales.push_back(e.zoom);first->zoom=modalCharacterZoom(scales);for(auto& e:roster.entries)if(e.active)e.zoom=first->zoom;}
        roster.enable(id);first->actorId=id;actors.push_back(std::move(first));attach(leader(),true);
        auto& menu=*leader().webSettings;
        menu.snapshot=[this]{return snapshot();};menu.receive=[this](const Json& j){message(j);};
        menu.visibility=[this](bool visible){
            if(visible){leader().dismissMenuHint();for(auto& a:actors){a->menuHintVisible=false;a->presentState(true);}}
            for(auto& a:actors){a->state.settingsOpen=visible;if(!visible){a->placementNeeded=true;a->interactionUntil=a->preferences.follow?0:preciseSeconds()+0.5;}}
        };
        // Load sequentially: allocating five GLBs simultaneously would stall the desktop.
        input.pollKeyboard=true;input.enable(true);save();
    }
    void activate(const std::string& id){
        auto* e=roster.find(id);if(!e)return;
        if(!std::filesystem::is_regular_file(wide(e->path))){error="找不到模型文件，请重新导入。";sync();return;}
        if(!roster.enable(id)){error="最多同时显示 5 个角色。";sync();return;}
        if(auto* a=find(id)){
            const bool reopening=!a->displayed;a->displayed=true;a->done=false;a->lastTick=preciseSeconds();copySettings(*a,leader());
            if(!a->loaded&&!a->settings->importBusy)a->requestModel(wide(e->path));
            if(reopening&&a->loaded){a->setZoom(commonZoom());a->selectMode(a->battleMode,false);}
            if(a->controller)a->controller->put_IsVisible(TRUE);ShowWindow(a->window,SW_SHOWNOACTIVATE);ShowWindow(a->inputWindow,SW_SHOWNOACTIVATE);placeNew(*a);
        }
        error.clear();save();sync();
    }
    void close(const std::string& id){
        const auto* entry=roster.find(id);if(!entry||!entry->active)return;
        if(roster.last(id)){
            if(!review&&MessageBoxW(leader().webSettings->window?leader().webSettings->window:leader().window,
                L"这是最后一个角色。关闭后将退出桌宠程序，确定继续吗？",L"关闭角色",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
            roster.find(id)->active=false;save();stopping=true;PostQuitMessage(0);return;
        }
        roster.find(id)->active=false;
        if(auto* a=find(id)){a->displayed=false;a->releaseMouse();a->directed.cancel();a->state.walking=false;
            a->post({{"type","suspend"}});ShowWindow(a->window,SW_HIDE);ShowWindow(a->inputWindow,SW_HIDE);if(a->controller)a->controller->put_IsVisible(FALSE);}
        for(const auto& e:roster.entries)if(e.active){roster.selected=e.id;break;}
        save();sync();
    }
    void message(const Json& j){
        const auto type=j.value("type",std::string{});
        if(type=="exit"){save();stopping=true;PostQuitMessage(0);return;}
        if(type=="roster_select"){const auto id=j.value("id",std::string{});if(auto* e=roster.find(id);e&&e->active){roster.selected=id;save();sync();}return;}
        if(type=="roster_toggle"){const auto id=j.value("id",std::string{});if(j.at("active").get<bool>())activate(id);else close(id);return;}
        if(type=="roster_retry"){if(auto* a=find(j.value("id",std::string{}));a&&a->displayed&&!a->settings->importBusy)a->requestModel(a->model);return;}
        if(type=="roster_remove"){
            const auto id=j.value("id",std::string{});if(!roster.find(id))return;
            if(roster.find(id)->active){close(id);if(roster.find(id)->active)return;}
            roster.entries.erase(std::remove_if(roster.entries.begin(),roster.entries.end(),[&](const auto& e){return e.id==id;}),roster.entries.end());
            if(roster.selected==id)roster.selected=roster.entries.empty()?"":roster.entries.front().id;
            save();sync();return;
        }
        if(type=="uniform_size"){
            const float size=commonZoom();broadcasting=true;
            for(auto& a:actors)if(a->displayed){if(a->loaded)a->setZoom(size);else a->zoom=size;}
            broadcasting=false;save();sync();return;
        }
        if(type=="close_character"){close(roster.selected);return;}
        if(type=="import"){
            wchar_t path[32768]{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=leader().webSettings->window;
            dialog.lpstrFilter=L"GLB 模型\0*.glb\0\0";dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
            if(GetOpenFileNameW(&dialog)){
                const auto file=std::filesystem::absolute(path);const auto id=SceneRoster::key(file);
                if(!roster.find(id)){if(roster.entries.size()>=SceneRoster::HistoryMaximum){error="历史列表已满（15 条），请先删除一个角色。";sync();return;}roster.entries.push_back({id,utf8(file.wstring()),utf8(file.stem().wstring()),"cafe",false});}
                const bool alreadyActive=roster.find(id)->active;
                activate(id);if(alreadyActive)if(auto* a=find(id))a->requestModel(file);
                save();sync();
            }return;
        }
        if(type=="save"){
            const auto* target=find(roster.selected);if(!target||!target->loaded){error="请等待当前角色加载完成后修改设置。";sync();return;}
            error.clear();selected().settingsMessage(j);
        }else leader().settingsMessage(j);
    }
    void tick(){
        if(stopping)return;
        const bool loading=std::any_of(actors.begin(),actors.end(),[](const auto& a){return a->displayed&&!a->loaded&&!a->done&&(!a->browser||a->settings->importBusy);});
        if(!loading)for(const auto& entry:roster.entries)if(entry.active&&!find(entry.id)){
            auto child=std::make_unique<App>();child->directory=leader().directory;child->dataDirectory=leader().dataDirectory;child->model=wide(entry.path);child->actorId=entry.id;
            child->initialMode=entry.mode;child->smooth=leader().smooth;child->zoom=commonZoom();
            auto& a=*child;actors.push_back(std::move(child));try{attach(a,false);}catch(const std::exception& e){a.done=true;a.displayed=false;roster.find(entry.id)->active=false;error=e.what();save();sync();}break;
        }
        auto events=input.take(false,false,false);
        if(leader().state.settingsOpen)events.keyPressed=events.reload=events.mouseClick=events.windowClosed=false;
        const bool calling=std::any_of(actors.begin(),actors.end(),[](const auto& a){return a->displayed&&a->battleMode&&a->battle&&a->battle->phase==pet::Battle::Phase::Callsign;});
        std::vector<App*> candidates;
        if(events.mouseClick&&!calling)for(auto& a:actors)if(a->displayed&&a->loaded&&a->battleMode&&a->battle->canCallsign()&&!a->directed.jumping&&!a->state.settingsOpen)candidates.push_back(a.get());
        App* caller=candidates.empty()?nullptr:candidates[std::uniform_int_distribution<size_t>(0,candidates.size()-1)(leader().state.random)];
        bool modesChanged=false;
        for(auto& a:actors)if(a->displayed){a->sharedEvents=events;a->sharedEvents.mouseClick=a.get()==caller;a->tick();
            if(a->loaded)if(auto* e=roster.find(a->actorId)){const auto mode=a->battleMode?"battle":"cafe";if(e->mode!=mode){e->mode=mode;modesChanged=true;}}
            if(stopping)break;}
        if(modesChanged){save();sync();}
        orderDepth();
        // Keep the leader's hidden host timer/menu; release other closed renderers.
        for(size_t i=1;i<actors.size();)if(!actors[i]->displayed){actors[i]->destroyedCallback=actors[i]->depthRequest={};actors.erase(actors.begin()+i);}else ++i;
    }
    int run(){
        MSG message{};auto& host=leader();
        while(!stopping){
            const auto wake=MsgWaitForMultipleObjectsEx(host.motionTimer?1:0,host.motionTimer?&host.motionTimer:nullptr,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(wake==WAIT_FAILED)throw std::runtime_error("Scene timer wait failed");
            if(host.motionTimer&&wake==WAIT_OBJECT_0)tick();
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                if(message.message==WM_QUIT){stopping=true;break;}TranslateMessage(&message);DispatchMessageW(&message);
            }
        }return host.exitCode;
    }
    ~Scene(){input.enable(false);for(auto& a:actors){a->syncRequest=a->menuRequest=a->closeRequest=a->loadedCallback=a->destroyedCallback=a->tickCallback=a->settingsChanged=a->followRequest=a->depthRequest={};}actors.clear();}
};
