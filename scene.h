#pragma once
#include "native_topmost.h"
// One input observer and settings menu coordinate up to five native render windows.
struct Scene {
    std::vector<std::unique_ptr<App>> actors;
    FrameJobs cpuJobs;
    std::array<CrowdBody,SceneRoster::Maximum> crowdBodies{};size_t crowdCount=0;uint64_t crowdSerial=0;
    HANDLE frameTimer{};FrameDeadline frameClock;
    double frameStatsAt=0,previousFrameAt=0;std::vector<double> frameIntervals,frameCosts;
    SceneRoster roster;
    NativeDesktopInput input;
    std::filesystem::path historyFile;
    std::string error,importNotice;
    NativeTopmostOrder depthOrder;
    bool review=false,stopping=false,broadcasting=false,importingBatch=false,modifyingModels=false;
    App& leader(){return *actors.front();}
    App* find(const std::string& id){for(auto& a:actors)if(a->actorId==id)return a.get();return nullptr;}
    App& selected(){auto* a=find(roster.selected);return a&&a->displayed?*a:leader();}
    void save(){if(!review){for(const auto& a:actors)if(auto* e=roster.find(a->actorId))e->zoom=a->zoom;roster.save(historyFile);}}
    float commonZoom(){std::vector<float> scales;for(const auto& a:actors)if(a->displayed&&a->loaded)scales.push_back(a->zoom);return scales.empty()?leader().zoom:modalCharacterZoom(scales);}
    void sync(){if(!importingBatch&&leader().nativeSettings)leader().nativeSettings->sync();}
    void refreshModels(){
        if(modifyingModels)return;
        try{
            bool changed=false;
            for(const auto& folder:leader().modelStorageSettings->directories())for(const auto& file:NativeModelLibrary(folder).files()){
                const auto id=SceneRoster::key(file);if(!roster.find(id)){roster.remember({id,utf8(file.wstring()),utf8(NativeModelLibrary::displayName(file)),"cafe",false});changed=true;}
            }
            const auto before=roster.entries.size();
            roster.entries.erase(std::remove_if(roster.entries.begin(),roster.entries.end(),[&](const auto& row){return !row.active&&leader().managedLibrary(wide(row.path))&&!std::filesystem::exists(wide(row.path));}),roster.entries.end());
            changed|=before!=roster.entries.size();
            if(!roster.find(roster.selected)){roster.selected.clear();for(const auto& row:roster.entries)if(row.active){roster.selected=row.id;break;}}
            if(changed)save();
        }catch(const std::exception& failure){error=std::string("模型库刷新失败：")+failure.what();}
    }
    Json snapshot(){
        auto result=selected().settingsSnapshot();Json rows=Json::array();
        for(const auto& e:roster.entries){const auto* a=find(e.id);rows.push_back({{"id",e.id},{"name",e.name},{"path",e.path},{"active",e.active},{"managed",bool(leader().managedLibrary(wide(e.path)))},
            {"available",std::filesystem::is_regular_file(wide(e.path))},{"ready",a&&a->loaded&&a->displayed},{"failed",a&&a->done&&!a->loaded},{"loading",e.active&&(!a||(!a->loaded&&a->settings->importBusy))},{"battleAvailable",a&&a->loaded&&a->settings->battleAvailable},{"mode",e.mode}});}
        result["characters"]=rows;result["selectedId"]=roster.selected;result["activeCount"]=roster.activeCount();result["maximum"]=SceneRoster::Maximum;
        if(const auto* entry=roster.find(roster.selected))result["name"]=entry->name;
        if(const auto* e=roster.find(roster.selected);e&&e->active){const auto* a=find(e->id);if(!a){result["name"]=e->name;result["busy"]=true;result["percent"]=0;result["detail"]="等待加载模型";result["battleAvailable"]=false;result["value"]["mode"]=e->mode;}}
        if(!error.empty())result["error"]=error;
        result["notice"]=importNotice;
        bool capable=false;for(const auto& a:actors)capable|=a->displayed&&a->loaded&&(a->cafeWalk>=0||(a->battle&&a->battle->clips.move>=0));
        result["canFollow"]=capable;return result;
    }
    void copySettings(App& target,const App& source){
        if(!target.preferences.follow&&source.preferences.follow){target.blockedCursor.reset();target.followPlanAt=0;target.interactionUntil=0;}
        target.preferences=source.preferences;
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
        // App intent is authoritative: a shell-hidden window still needs to
        // be restored, but a character disabled by the user must stay hidden.
        std::vector<App*> visible;for(auto& a:actors)if(a->displayed&&a->loaded&&!a->done&&a->window)visible.push_back(a.get());
        auto depth=[](const App& a){
            double foot=(a.committedFrame?a.committedFoot:a.ground.screen(a.ground.position)).y+a.work.top;
            foot-=a.retreatDisplacement();
            return std::lround(foot);
        };
        std::sort(visible.begin(),visible.end(),[&](const App* a,const App* b){const auto ay=depth(*a),by=depth(*b);return ay!=by?ay>by:a->actorId<b->actorId;});
        std::vector<HWND> desired;
        // Full settings use normal application z-order; only the pet flyout
        // belongs to the topmost display/input chain.
        const auto* menu=leader().nativeSettings.get();
        if(menu&&menu->quickWindow&&IsWindowVisible(menu->quickWindow))desired.insert(desired.begin(),menu->quickWindow);
        if(menu&&menu->modelMenu&&IsWindowVisible(menu->modelMenu))desired.insert(desired.begin(),menu->modelMenu);
        for(const auto* a:visible){desired.push_back(a->inputWindow);desired.push_back(a->window);}
        // Front-to-back chain: feet lower on the desktop are in front. Native
        // window motion preserves this order, instead of promoting the last mover.
        depthOrder.apply(desired,preciseSeconds());
    }
    void snapshotCrowd(){
        crowdCount=0;
        for(const auto& actor:actors){const auto& a=*actor;if(!a.displayed||!a.loaded)continue;
            CrowdBody b;b.id=a.crowdId;b.position={a.ground.position.x,a.ground.position.z};b.radius=radius(a);
            b.velocity=a.left||a.right||a.state.settingsOpen?Vector2{}:Vector2{a.motionVelocity.x,a.motionVelocity.z};
            if(a.directed.jumping){b.jumping=true;const auto p=a.ground.step(a.directed.jumpYaw,float(a.directed.jumpSpeed*(a.directed.jumpDuration-a.directed.jumpTime)));b.landing={p.x,p.z};}
            else if(a.battleMode&&a.battle&&a.battle->phase==pet::Battle::Phase::Jump){b.jumping=true;
                const double remaining=std::max(0.0,a.state.clips[a.state.current].duration-a.state.time);
                const auto p=a.ground.step(float(a.state.walkYaw),float(a.state.walkSpeed*a.state.movementScale*remaining));b.landing={p.x,p.z};}
            b.movable=(a.state.walking||a.directed.active)&&!a.left&&!a.right&&!a.state.settingsOpen&&!b.jumping;
            crowdBodies[crowdCount++]=b;
        }
    }
    void separateCrowd(){
        if(crowdCount<2)return;
        // Compute from a common post-movement snapshot, then commit corrections.
        // Only moving actors participate; no idle drift or forced model rotation.
        snapshotCrowd();const auto bodies=std::span<const CrowdBody>(crowdBodies.data(),crowdCount);
        for(auto& actor:actors){auto& a=*actor;if(!a.displayed||!a.loaded)continue;
            for(const auto& body:bodies)if(body.id==a.crowdId){
                const auto push=CrowdSteering::separation(body,bodies,a.movementDt);const float distance=std::sqrt(CrowdSteering::norm(push));
                if(distance>0){const float heading=std::atan2(push.x,push.y)*RAD2DEG;
                    if(DesktopPath::stepAllowed(a.ground,heading,distance)){a.ground.position=a.ground.step(heading,distance);
                        a.motionVelocity.x+=push.x/a.movementDt;a.motionVelocity.z+=push.y/a.movementDt;}}
                break;
            }
        }
    }
    MovementChoice steer(App& a,float heading,double travel){
        CrowdBody self;self.id=a.crowdId;self.position={a.ground.position.x,a.ground.position.z};self.radius=radius(a);
        // O(1) cached radii and at most four peers. No vertex scans, allocations,
        // screen enumeration or full-path tests in the local avoidance planner.
        for(size_t i=0;i<crowdCount;++i)if(crowdBodies[i].id==self.id){self.radius=crowdBodies[i].radius;break;}
        const auto choice=a.crowd.step(self,std::span<const CrowdBody>(crowdBodies.data(),crowdCount),heading,travel,a.movementDt,
            [&](float angle,double distance){return DesktopPath::stepAllowed(a.ground,angle,distance);});
        return choice;
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
        if(!first)a.diagnostics=leader().diagnostics;
        a.grouped=true;a.crowdId=++crowdSerial;
        a.syncRequest=[this]{sync();};a.menuRequest=[this,&a]{roster.selected=a.actorId;save();leader().nativeSettings->show(leader().window,leader().directory);};
        a.quickMenuRequest=[this,&a]{roster.selected=a.actorId;save();leader().nativeSettings->showQuick(a.window,leader().directory);};
        a.closeRequest=[this,&a]{close(a.displayed?a.actorId:roster.selected);};
        a.tickCallback=[this]{tick();};a.settingsChanged=[this,&a]{changed(a);};
        a.followRequest=[this]{auto& a=selected();a.preferences.follow=!a.preferences.follow;if(a.preferences.follow){a.blockedCursor.reset();a.followPlanAt=0;a.interactionUntil=0;}a.preferences.save(a.settings->ini);changed(a);};
        a.avoidHeading=[this,&a](float yaw,double distance){return steer(a,yaw,distance);};
        a.depthRequest=[this]{orderDepth();};
        if(first)a.sessionChanged=[this]{frameClock.reset(preciseSeconds());frameStatsAt=previousFrameAt=0;frameIntervals.clear();frameCosts.clear();armFrame();input.clear();input.lastKeyboard=0;const double now=preciseSeconds();for(auto& actor:actors){actor->lastTick=now;actor->nextRender=0;actor->pendingRender=false;actor->motionVelocity={};actor->motionRemaining=0;if(actor->renderer)actor->renderer->resume();}};
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
        if(!review){try{first->modelStorageSettings->recover(roster,first->directory/L"models/Shiroko.glb");}catch(const std::exception& e){error=std::string("旧模型记录恢复失败：")+e.what();}}
        if(!review){try{first->modelLibrary().ensureDirectory();}catch(const std::exception& e){error=std::string("无法创建模型保存目录，请在模型管理中更改位置：")+e.what();}}
        for(auto& e:roster.entries)if(e.active&&!std::filesystem::is_regular_file(wide(e.path)))e.active=false;
        bool explicitAccepted=false;
        if(!review&&first->modelSpecified){
            const auto id=SceneRoster::key(first->model);
            if(!roster.find(id))roster.remember({id,utf8(first->model.wstring()),utf8(first->model.stem().wstring()),"cafe",false});
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
        if(!roster.find(id))roster.remember({id,utf8(first->model.wstring()),utf8(first->model.stem().wstring()),first->initialMode.empty()?"cafe":first->initialMode,false});
        if(!review){std::vector<float> scales;for(const auto& e:roster.entries)if(e.active)scales.push_back(e.zoom);first->zoom=modalCharacterZoom(scales);for(auto& e:roster.entries)if(e.active)e.zoom=first->zoom;}
        roster.enable(id);first->actorId=id;actors.push_back(std::move(first));attach(leader(),true);
        auto& menu=*leader().nativeSettings;
        menu.snapshot=[this]{return snapshot();};menu.receive=[this](const Json& j){message(j);};
        menu.visibility=[this](bool visible){
            if(visible){leader().dismissMenuHint();for(auto& a:actors){a->menuHintVisible=false;a->presentState(true);}}
        };
        menu.pauseChanged=[this](bool paused){
            for(auto& a:actors){a->state.settingsOpen=paused;if(!paused){a->placementNeeded=true;a->interactionUntil=a->preferences.follow?0:preciseSeconds()+0.5;}}
        };
        // Load sequentially: allocating five GLBs simultaneously would stall the desktop.
        refreshModels();input.pollKeyboard=false;input.enable(true);save();
        frameTimer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
        if(!frameTimer)frameTimer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
        frameClock.reset(preciseSeconds());armFrame();
    }
    void activate(const std::string& id){
        auto* e=roster.find(id);if(!e)return;
        if(!std::filesystem::is_regular_file(wide(e->path))){error="找不到模型文件，请重新导入。";sync();return;}
        if(!roster.enable(id)){error="最多同时显示 5 个角色。";sync();return;}
        if(auto* a=find(id)){
            const bool reopening=!a->displayed;a->displayed=true;a->done=false;a->lastTick=preciseSeconds();copySettings(*a,leader());
            if(!a->loaded&&!a->settings->importBusy)a->requestModel(wide(e->path));
            if(reopening&&a->loaded){a->setZoom(commonZoom());a->selectMode(a->battleMode,false);}
            ShowWindow(a->window,SW_SHOWNOACTIVATE);ShowWindow(a->inputWindow,SW_SHOWNOACTIVATE);placeNew(*a);
        }
        error.clear();save();sync();
    }
    void close(const std::string& id,bool confirmLast=true){
        const auto* entry=roster.find(id);if(!entry||!entry->active)return;
        if(roster.last(id)){
            if(confirmLast&&!review&&nativeMessageBox(leader().nativeSettings->dialogOwner()?leader().nativeSettings->dialogOwner():leader().window,
                L"这是最后一个角色。关闭后将退出桌宠程序，确定继续吗？",L"关闭角色",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
            roster.find(id)->active=false;save();stopping=true;PostQuitMessage(0);return;
        }
        roster.find(id)->active=false;
        if(auto* a=find(id)){a->displayed=false;a->releaseMouse();a->directed.cancel();a->state.walking=false;
            a->post({{"type","suspend"}});ShowWindow(a->window,SW_HIDE);ShowWindow(a->inputWindow,SW_HIDE);}
        for(const auto& e:roster.entries)if(e.active){roster.selected=e.id;break;}
        save();sync();
    }
    NativeImportBatch::Outcome importModel(const std::filesystem::path& source){
        NativeHealth::Pause saving;
        try{
            auto library=leader().modelLibrary();const auto plan=library.prepare(source);const auto id=SceneRoster::key(plan.file);
            const auto oldId=SceneRoster::key(std::filesystem::absolute(source));
            const bool replaceLegacy=!roster.find(id)&&roster.find(oldId);
            const bool reused=roster.find(id)||replaceLegacy;
            const auto file=library.commit(plan);
            // Reimporting a legacy external file upgrades that history entry in
            // place, including its live actor, mode, zoom and current selection.
            if(replaceLegacy){
                auto* entry=roster.find(oldId);entry->id=id;entry->path=utf8(file.wstring());
                if(auto* actor=find(oldId)){actor->actorId=id;actor->model=file;if(actor->pendingImport)actor->pendingModel=file;}
                if(roster.selected==oldId)roster.selected=id;
            }
            if(!roster.find(id))roster.entries.push_back({id,utf8(file.wstring()),utf8(plan.name),"cafe",false});
            const bool alreadyActive=roster.find(id)->active;
            // Files beyond the display limit are still imported into history.
            if(alreadyActive||roster.activeCount()<SceneRoster::Maximum)activate(id);
            if(alreadyActive)if(auto* actor=find(id)){actor->initialMode=roster.find(id)->mode;actor->requestModel(file);}
            save();sync();return {true,reused,roster.find(id)->active,{}};
        }catch(const std::exception& failure){return {false,false,false,std::string("模型保存失败：")+failure.what()};}
    }
    void importModels(const std::vector<std::filesystem::path>& files){
        if(files.empty())return;
        error.clear();importNotice.clear();NativeImportBatch::Report report;
        {
            struct Guard {bool& flag;Guard(bool& value):flag(value){flag=true;}~Guard(){flag=false;}} guard(importingBatch);
            report=NativeImportBatch::run(files,[this](const auto& file){return importModel(file);});
        }
        importNotice="导入完成：新增 "+std::to_string(report.added)+" 个，复用 "+std::to_string(report.reused)+" 个";
        if(report.hidden)importNotice+="；"+std::to_string(report.hidden)+" 个保存在列表中未显示（同时显示上限 5 个）";
        if(!report.failures.empty()){
            error=importNotice+"；失败 "+std::to_string(report.failures.size())+" 个。";
            const auto& first=report.failures.front();error+=utf8(first.source.filename().wstring())+"："+first.error;
        }
        sync();
    }
    void message(const Json& j){
        const auto type=j.value("type",std::string{});
        if(type!="import"&&type!="refresh_models")importNotice.clear();
        if(type=="exit"){save();stopping=true;PostQuitMessage(0);return;}
        if(type=="refresh_models"){refreshModels();sync();return;}
        if(type=="change_models_folder"||type=="reset_models_folder"){
            if(modifyingModels)return;NativeHealth::Pause choosing;
            try{
                auto& storage=*leader().modelStorageSettings;
                const auto folder=type=="reset_models_folder"?std::optional<std::filesystem::path>(storage.defaultDirectory()):NativeModelStorage::choose(leader().nativeSettings->dialogOwner(),storage.current);
                if(!folder)return;storage.change(*folder);error.clear();refreshModels();sync();
            }catch(const std::exception& failure){error=failure.what();sync();}return;
        }
        if(type=="model_locate"){
            if(const auto* row=roster.find(j.value("id",std::string{}))){
                const auto file=wide(row->path);const auto args=L"/select,\""+std::filesystem::absolute(file).wstring()+L"\"";
                if(reinterpret_cast<INT_PTR>(ShellExecuteW(leader().nativeSettings->dialogOwner(),L"open",L"explorer.exe",args.c_str(),nullptr,SW_SHOWNORMAL))<=32)error="无法在文件夹中定位模型";
            }sync();return;
        }
        if(type=="open_models_folder"){
            try{leader().openModelsFolder();error.clear();}catch(const std::exception& failure){error=failure.what();}sync();return;
        }
        if(type=="roster_select"){const auto id=j.value("id",std::string{});if(auto* e=roster.find(id);e&&e->active){roster.selected=id;save();sync();}return;}
        if(type=="roster_toggle"){const auto id=j.value("id",std::string{});if(j.at("active").get<bool>())activate(id);else close(id);return;}
        if(type=="roster_retry"){if(auto* a=find(j.value("id",std::string{}));a&&a->displayed&&!a->settings->importBusy)a->requestModel(a->model);return;}
        if(type=="model_delete"){
            if(modifyingModels)return;
            const auto id=j.value("id",std::string{});if(!roster.find(id))return;
            struct Guard {bool& flag;Guard(bool& value):flag(value){flag=true;}~Guard(){flag=false;}} guard(modifyingModels);
            try{
                if(auto* actor=find(id);actor&&actor->settings->importBusy)throw std::runtime_error("请等待模型加载完成后再删除");
                const auto file=wide(roster.find(id)->path);const auto library=leader().managedLibrary(file);
                if(library)library->recycle(file,leader().nativeSettings->dialogOwner());
                if(auto* row=roster.find(id);row&&row->active)close(id,false);
                roster.entries.erase(std::remove_if(roster.entries.begin(),roster.entries.end(),[&](const auto& e){return e.id==id;}),roster.entries.end());
                if(roster.selected==id)roster.selected=roster.entries.empty()?"":roster.entries.front().id;
                error.clear();save();sync();
            }catch(const std::exception& failure){error=failure.what();sync();}return;
        }
        if(type=="uniform_size"){
            const float size=commonZoom();broadcasting=true;
            for(auto& a:actors)if(a->displayed){if(a->loaded)a->setZoom(size);else a->zoom=size;}
            broadcasting=false;save();sync();return;
        }
        if(type=="close_character"){close(roster.selected);return;}
        if(type=="import"){
            if(modifyingModels)return;
            NativeHealth::Pause dialogWait;
            try{importModels(NativeModelPicker::choose(leader().nativeSettings->dialogOwner()));}
            catch(const std::exception& failure){error=failure.what();sync();}return;
        }
        if(type=="save"){
            const auto* target=find(roster.selected);if(!target||!target->loaded){error="请等待当前角色加载完成后修改设置。";sync();return;}
            error.clear();selected().settingsMessage(j);
        }else leader().settingsMessage(j);
    }
    void tick(){
        if(stopping||leader().session.paused())return;
        const bool loading=std::any_of(actors.begin(),actors.end(),[](const auto& a){return a->displayed&&!a->loaded&&!a->done&&(!a->renderer||a->settings->importBusy);});
        if(!loading)for(const auto& entry:roster.entries)if(entry.active&&!find(entry.id)){
            auto child=std::make_unique<App>();child->directory=leader().directory;child->programDirectory=leader().programDirectory;child->dataDirectory=leader().dataDirectory;child->modelStorageSettings=leader().modelStorageSettings;child->model=wide(entry.path);child->actorId=entry.id;
            child->initialMode=entry.mode;child->smooth=leader().smooth;child->zoom=commonZoom();
            auto& a=*child;actors.push_back(std::move(child));try{attach(a,false);}catch(const std::exception& e){a.done=true;a.displayed=false;roster.find(entry.id)->active=false;error=e.what();save();sync();}break;
        }
        snapshotCrowd();
        auto events=input.take(false,false,false);
        if(leader().nativeSettings->visible())events.keyPressed=events.reload=events.mouseClick=events.windowClosed=false;
        const bool calling=std::any_of(actors.begin(),actors.end(),[](const auto& a){return a->displayed&&a->battleMode&&a->battle&&a->battle->phase==pet::Battle::Phase::Callsign;});
        std::vector<App*> candidates;
        if(events.mouseClick&&!calling)for(auto& a:actors)if(a->displayed&&a->loaded&&a->battleMode&&a->battle->canCallsign()&&!a->directed.jumping&&!a->state.settingsOpen)candidates.push_back(a.get());
        App* caller=candidates.empty()?nullptr:candidates[std::uniform_int_distribution<size_t>(0,candidates.size()-1)(leader().state.random)];
        bool modesChanged=false;
        for(auto& a:actors)if(a->displayed){a->deferRendering=true;a->sharedEvents=events;a->sharedEvents.mouseClick=a.get()==caller;a->tick();
            if(a->loaded)if(auto* e=roster.find(a->actorId)){const auto mode=a->battleMode?"battle":"cafe";if(e->mode!=mode){e->mode=mode;modesChanged=true;}}
            if(stopping)break;}
        if(!stopping)separateCrowd();
        if(modesChanged){save();sync();}
        // Keep the leader's hidden host/menu; release other closed renderers.
        for(size_t i=1;i<actors.size();)if(!actors[i]->displayed){actors[i]->destroyedCallback=actors[i]->depthRequest={};actors.erase(actors.begin()+i);}else ++i;
        if(!frameTimer&&frameClock.due(preciseSeconds()))renderScene();
    }
    void armFrame(){
        if(!frameTimer)return;LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,std::llround((frameClock.next-preciseSeconds())*10000000));
        if(!SetWaitableTimer(frameTimer,&due,0,nullptr,nullptr,FALSE))throw std::runtime_error("Cannot arm scene frame timer");
    }
    void renderScene(){
        if(stopping||leader().session.paused())return;
        const double sampleAt=preciseSeconds();std::vector<App*> renderActors;std::vector<std::function<void()>> jobs;
        for(auto& a:actors)if(a->window&&a->displayed&&a->loaded){
            a->presentState(true,sampleAt);renderActors.push_back(a.get());
            jobs.emplace_back([renderer=a->renderer.get(),sampleAt]{renderer->prepareCpu(sampleAt);});
        }
        const unsigned beforeWorkers=cpuJobs.count();NativeHealth::mark("CPU animation batch");
        if(leader().serialCpu)for(const auto& job:jobs)job();else cpuJobs.run(jobs);
        if(cpuJobs.count()!=beforeWorkers)TraceLog(LOG_INFO,"FRAME CPU workers: %u; OpenGL and Win32 remain on the main thread",cpuJobs.count());
        // Submit all GPU draws before the first synchronous readback. Each
        // actor retains its own FBO, so no extra frame buffers or frame latency.
        for(auto* a:renderActors)a->finishRender(true);
        bool drew=false;for(auto* a:renderActors){const auto before=a->renderedFrames;a->commitRender();drew|=a->renderedFrames!=before;}
        NativeHealth::mark("depth order");orderDepth();if(drew)rlDrawRenderBatchActive();
        const double finished=preciseSeconds();
        if(leader().diagnostics&&!renderActors.empty()){
            if(previousFrameAt)frameIntervals.push_back((finished-previousFrameAt)*1000);previousFrameAt=finished;
            frameCosts.push_back((finished-sampleAt)*1000);if(!frameStatsAt)frameStatsAt=finished;
            if(finished-frameStatsAt>=3&&!frameIntervals.empty()){
                auto intervals=frameIntervals,costs=frameCosts;std::sort(intervals.begin(),intervals.end());std::sort(costs.begin(),costs.end());
                const auto p95=[](const auto& v){return v[std::min(v.size()-1,size_t(std::ceil(v.size()*0.95)-1))];};
                TraceLog(LOG_INFO,"PERF scene: actors=%zu FPS=%.1f interval p95=%.2f max=%.2f ms; frame CPU+GPU+Win32 p95=%.2f ms",renderActors.size(),frameIntervals.size()/(finished-frameStatsAt),p95(intervals),intervals.back(),p95(costs));
                frameStatsAt=finished;frameIntervals.clear();frameCosts.clear();
            }
        }
        frameClock.advance(finished);armFrame();
    }
    int run(){
        auto& host=leader();
        while(!stopping){
            NativeHealth::mark("message wait");
            const bool paused=host.session.paused();DWORD wake;HANDLE waits[2]{};DWORD count=0;int motion=-1,frame=-1;
            if(!paused){if(host.motionTimer){motion=int(count);waits[count++]=host.motionTimer;}if(frameTimer){frame=int(count);waits[count++]=frameTimer;}}
            {NativeHealth::Pause idleWait;
                wake=MsgWaitForMultipleObjectsEx(count,waits,500,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}
            if(wake==WAIT_FAILED)throw std::runtime_error("Scene timer wait failed");
            // Drain input before expensive rendering, as in the stable raylib host.
            if(!drainNativeMessages([&](MSG& message){NativeHealth::mark("message dispatch");
                if(host.nativeSettings&&host.nativeSettings->route(message))return;TranslateMessage(&message);DispatchMessageW(&message);
            }))stopping=true;
            if(!stopping&&!host.session.paused()){
                if(motion>=0&&(wake==WAIT_OBJECT_0+DWORD(motion)||WaitForSingleObject(host.motionTimer,0)==WAIT_OBJECT_0))tick();
                if(!stopping&&frame>=0&&(wake==WAIT_OBJECT_0+DWORD(frame)||WaitForSingleObject(frameTimer,0)==WAIT_OBJECT_0))renderScene();
            }
        }return host.exitCode;
    }
    ~Scene(){if(frameTimer){CancelWaitableTimer(frameTimer);CloseHandle(frameTimer);}cpuJobs.shutdown();input.enable(false);for(auto& a:actors){a->syncRequest=a->menuRequest=a->closeRequest=a->loadedCallback=a->destroyedCallback=a->tickCallback=a->settingsChanged=a->followRequest=a->depthRequest=a->sessionChanged={};}actors.clear();}
};
