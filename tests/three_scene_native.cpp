#define PET_THREE_NO_ENTRY
#include "../experimental/three/main.cpp"
// Drives the actual native Scene, WebViews and menu routes, never system input.
static int answer=IDNO;
static void CALLBACK answerDialog(HWND,UINT,UINT_PTR timer,DWORD){
    bool found=false;EnumWindows([](HWND hwnd,LPARAM data)->BOOL{
        DWORD process=0;GetWindowThreadProcessId(hwnd,&process);wchar_t cls[80]{};GetClassNameW(hwnd,cls,80);
        if(process==GetCurrentProcessId()&&std::wcscmp(cls,L"#32770")==0){PostMessageW(hwnd,WM_COMMAND,answer,0);*reinterpret_cast<bool*>(data)=true;}
        return TRUE;
    },reinterpret_cast<LPARAM>(&found));if(found)KillTimer(nullptr,timer);
}
static void requireScene(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int result=1;
    try{
        int argc=0;auto** argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(argc!=2)throw std::runtime_error("Provide a prepared fixture directory");
        const auto folder=std::filesystem::absolute(argv[1]);LocalFree(argv);
        auto app=std::make_unique<App>();app->directory=folder;app->model=folder/L"models/Azusa.glb";
        Scene scene(std::move(app));const double started=preciseSeconds();
        auto pump=[&]{MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}scene.tick();Sleep(8);};
        while(scene.actors.size()<5||!std::all_of(scene.actors.begin(),scene.actors.end(),[](const auto& a){return a->loaded&&a->frameReady;})){
            if(preciseSeconds()-started>45)throw std::runtime_error("Five real GLBs did not load and render in time");pump();
        }
        requireScene(scene.roster.activeCount()==5,"Expected five active actors");
        {
            auto& pet=scene.leader();const auto monitor=pet.ground.monitors.front();
            pet.ground.position=pet.ground.floorAt({(monitor.left+monitor.right)*0.5f,monitor.top+(monitor.bottom-monitor.top)*0.8f});pet.presentState(true);const auto anchor=pet.ground.position;const auto scale=pet.zoom;
            pet.yaw=43;pet.motionVelocity={1,0,1};pet.motionRemaining=1;
            pet.state.time=0.37;const auto clip=pet.state.current;const auto generation=pet.state.generation;
            const auto window=pet.windowPosition;const int width=pet.windowWidth,height=pet.windowHeight;
            pet.setZoom(scale*1.1f);
            requireScene(pet.yaw==43&&!pet.directed.active&&pet.motionRemaining==0&&std::hypot(pet.ground.position.x-anchor.x,pet.ground.position.z-anchor.z)<0.002,
                "Zoom must preserve feet/facing and clear stale movement prediction");
            requireScene(pet.state.current==clip&&pet.state.generation==generation&&pet.state.time==0.37,
                "Stationary zoom must preserve the animation pose and clock without restarting the clip");
            requireScene(pet.windowPosition.x==window.x&&pet.windowPosition.y==window.y&&pet.windowWidth==width&&pet.windowHeight==height,
                "Wheel zoom must not move or resize the native crop while its anchor fits the screen");
            for(float value:{0.45f,1.65f,1.0f}){
                pet.setZoom(value);
                std::printf("ZOOM %.2f anchor %.6f %.6f -> %.6f %.6f crop %ld %ld -> %ld %ld size %d %d -> %d %d clock %.6f generation %llu -> %llu\n",value,anchor.x,anchor.z,pet.ground.position.x,pet.ground.position.z,window.x,window.y,pet.windowPosition.x,pet.windowPosition.y,width,height,pet.windowWidth,pet.windowHeight,pet.state.time,(unsigned long long)generation,(unsigned long long)pet.state.generation);std::fflush(stdout);
                requireScene(pet.ground.position.x==anchor.x&&pet.ground.position.z==anchor.z
                    &&pet.windowWidth==width&&pet.windowHeight==height&&pet.windowPosition.x==window.x&&pet.windowPosition.y==window.y
                    &&pet.state.generation==generation&&pet.state.time==0.37,"Repeated wheel zoom across its supported range must preserve crop, anchor and animation");
            }
            const auto revision=pet.maskRevision;
            pet.applyRegion({{"type","mask"},{"width",width},{"height",height},{"generation",pet.displayGeneration()},
                {"maskRevision",revision-1},{"spans",Json::array({0,0,10})}});
            HRGN region=CreateRectRgn(0,0,0,0);requireScene(GetWindowRgn(pet.window,region)==ERROR,"A pre-zoom asynchronous mask must never clip the resized character");DeleteObject(region);
            pet.applyRegion({{"type","mask"},{"width",width},{"height",height},{"generation",pet.displayGeneration()},
                {"maskRevision",revision},{"spans",Json::array({0,0,10})}});
            region=CreateRectRgn(0,0,0,0);
            requireScene(GetWindowRgn(pet.window,region)==ERROR&&SendMessageW(pet.window,WM_NCHITTEST,0,0)==HTTRANSPARENT,"Rendering must always remain unclipped and outside native input hit testing");
            requireScene((GetWindowLongPtrW(pet.window,GWL_EXSTYLE)&(WS_EX_LAYERED|WS_EX_TRANSPARENT))==(WS_EX_LAYERED|WS_EX_TRANSPARENT),"Display must have the layered transparent flags that let real input reach other processes");
            requireScene(GetWindowRgn(pet.inputWindow,region)!=ERROR&&PtInRegion(region,5,0)&&!PtInRegion(region,50,0),"Alpha shape must apply exclusively to the separate input window");
            POINT hit{window.x+5,window.y};requireScene(WindowFromPoint(hit)==pet.inputWindow,"Visible alpha must receive input through the invisible input window");
            hit.x=window.x+50;requireScene(WindowFromPoint(hit)!=pet.inputWindow&&WindowFromPoint(hit)!=pet.window,"Transparent alpha must pass through both windows");
            DeleteObject(region);
            pet.state.activeInputSeconds=32;const auto zoomAnchor=pet.ground.position;
            for(bool mode:{true,false,true}){pet.selectMode(mode,false);
                requireScene(pet.yaw==43&&pet.state.activeInputSeconds==32&&pet.motionRemaining==0&&pet.ground.position.x==zoomAnchor.x&&pet.ground.position.z==zoomAnchor.z,
                    "Mode changes must preserve position, facing and continuous input timing");}
            pet.setZoom(scale);pet.yaw=0;pet.state.activeInputSeconds=0;
            std::printf("PASS: zoom/mode anchor and facing stability, unclipped rendering, independent alpha input and transparent passthrough\n");std::fflush(stdout);
        }
        scene.actors[0]->zoom=0.8f;for(size_t i=1;i<scene.actors.size();++i)scene.actors[i]->zoom=1.2f;
        scene.message({{"type","uniform_size"}});
        for(const auto& actor:scene.actors)requireScene(actor->zoom==1.2f,"The real uniform-size route must apply the modal zoom to all actors");
        for(auto& actor:scene.actors)actor->setZoom(1);
        auto above=[](HWND front,HWND back){for(HWND p=GetWindow(back,GW_HWNDPREV);p;p=GetWindow(p,GW_HWNDPREV))if(p==front)return true;return false;};
        {
            auto& low=*scene.actors[0];auto& high=*scene.actors[1];const auto lowGround=low.ground,highGround=high.ground;
            low.ground.position=low.ground.floorAt({low.ground.width*0.6f,low.ground.height*0.8f});
            high.ground.position=high.ground.floorAt({high.ground.width*0.6f,high.ground.height*0.6f});
            low.presentState(true);high.presentState(true);scene.orderDepth();
            requireScene(above(low.window,high.window),"Lower feet must place the native render window in front");
            high.ground.position.x+=0.05f;high.presentState(true);
            requireScene(above(low.window,high.window),"Moving an upper character must not promote it over a lower character");
            low.ground.position=low.ground.floorAt({low.ground.width*0.6f,low.ground.height*0.5f});low.presentState(true);
            requireScene(above(high.window,low.window),"Depth order must reverse immediately when feet swap screen height");
            const auto nearBody=Scene::radius(low);low.zoom*=1.3f;
            requireScene(std::abs(Scene::radius(low)/nearBody-1.3f)<0.00001f,"Actual model collision footprint must grow with zoom");low.zoom/=1.3f;
            low.ground=lowGround;high.ground=highGround;low.presentState(true);high.presentState(true);
            std::printf("PASS: native foot-depth layering, immediate height swaps, movement preserves order, enlarged zoom-aware collision footprint\n");std::fflush(stdout);
        }
        {
            auto& pet=scene.leader();const auto savedGround=pet.ground;const auto savedWork=pet.work;const float savedYaw=pet.yaw;
            const bool walking=pet.state.walkEnabled;std::vector<bool> shown;for(auto& a:scene.actors){shown.push_back(a->displayed);if(a.get()!=&pet)a->displayed=false;}
            const auto inputSource=pet.idleInputSeconds;pet.idleInputSeconds=[](){return 0.0;};
            pet.selectMode(true,false);pet.state.walkEnabled=true;pet.state.settingsOpen=false;
            pet.ground=PetGround{};pet.ground.width=3840;pet.ground.height=1080;pet.ground.measuredBounds=true;
            pet.ground.monitors={{0,0,1920,1080},{1980,0,3840,1080}};pet.work={0,0,3840,1080};
            pet.ground.position=pet.ground.floorAt({1450,750});pet.geometry.measure(pet.ground,pet.zoom);
            const double originalNear=pet.preferences.followNear,originalFar=pet.preferences.followFar;
            const auto activeWait=pet.state.activeSecondsOverride;const bool allowLook=pet.state.trackingEnabled;
            pet.preferences.follow=false;pet.preferences.followNear=pet.preferences.followFar=0.5;
            for(bool battleMode:{false,true}){
                pet.preferences.follow=false;pet.selectMode(battleMode,false);pet.placementNeeded=false;pet.placementGeneration=pet.state.generation;
                pet.state.trackingEnabled=true;pet.state.activeSecondsOverride=600;pet.state.noInputState=false;pet.yaw=37;
                const auto anchor=pet.ground.position;
                for(int frame=0;frame<150;++frame){pet.state.trackingCursor=true;pet.lastTick=preciseSeconds()-0.008;pet.tick();}
                requireScene(!pet.directed.active&&pet.ground.position.x==anchor.x&&pet.ground.position.z==anchor.z&&pet.yaw==37,
                    "Cafe/Battle Idle must hold its facing even with mouse-look enabled and an old tracking request");
                pet.state.activeSecondsOverride=activeWait;pet.interactionUntil=preciseSeconds()+100;pet.followPlanAt=preciseSeconds()+100;
                pet.followRequest();pet.lastTick=preciseSeconds()-0.008;pet.tick();
                requireScene(pet.directed.active&&pet.directed.following,"Enabling follow must start on the next tick without waiting for input or walk timers");
                const auto owner=pet.directed.owner;
                for(int frame=0;frame<5;++frame){pet.lastTick=preciseSeconds()-0.008;pet.tick();}
                requireScene(pet.directed.active&&pet.state.generation==owner,"Immediate follow must remain active in both modes");
                pet.directed.cancel();pet.preferences.follow=false;pet.restFacing.reset();
            }
            const auto idleWait=pet.state.idleSecondsOverride;pet.preferences.follow=true;pet.idleInputSeconds=[](){return 1000.0;};
            for(bool mode:{false,true}){
                pet.selectMode(mode,false);pet.state.walkEnabled=true;pet.state.idleSecondsOverride=1;
                pet.state.walkIntervalOverride=0;pet.state.idleTime=1000;pet.state.walkTimeJitter=0;
                if(mode){pet.battle->walkTimeJitter=0;pet.battle->setMoveInterval(0);pet.battle->start();}
                pet.lastTick=preciseSeconds()-0.008;pet.tick();
                requireScene(pet.state.walking&&!pet.directed.active,"Mouse follow enabled must still permit wandering after input becomes idle");
                pet.stopLocomotion();
            }
            pet.idleInputSeconds=[](){return 0.0;};pet.state.idleSecondsOverride=idleWait;pet.applyPreferences();
            pet.state.trackingEnabled=allowLook;pet.state.activeSecondsOverride=activeWait;pet.selectMode(true,false);
            pet.preferences.follow=false;pet.preferences.followNear=originalNear;pet.preferences.followFar=originalFar;
            std::printf("PASS: Cafe/Battle Idle hold facing; enabling follow starts immediately and continues\n");std::fflush(stdout);
            {
                const auto saved=pet.ground.position;const bool look=pet.state.trackingEnabled;
                pet.ground.position=pet.ground.floorAt({1100,700});pet.state.trackingEnabled=false;
                pet.state.inputActive=true;pet.state.noInputState=false;pet.state.activeInputSeconds=600;pet.sharedEvents={};
                POINT cursor{};GetCursorPos(&cursor);const Vector2 pixel{float(cursor.x-pet.work.left),float(cursor.y-pet.work.top)};
                const auto goal=pet.ground.planeAt(pixel,1.15f*pet.zoom);
                pet.yaw=std::remainder(std::atan2(goal.x-pet.ground.position.x,goal.z-pet.ground.position.z)*RAD2DEG+180.0f,360.0f);
                pet.state.walkYaw=pet.yaw;pet.state.walkSpeed=0.792;pet.state.movementScale=pet.zoom*3;
                pet.state.start(pet.battle->clips.move,true,true);pet.state.walkRemaining=0.8;
                pet.walkGeneration=pet.state.generation;pet.battle->phase=pet::Battle::Phase::Moving;
                pet.battle->moveDuration=0.8;pet.battle->activeInputMove=true;pet.stopTurn.reset();
                const float awayFacing=pet.yaw;bool curved=false;double targetYaw=0;
                const auto center=pet.ground.screen({pet.ground.position.x,1.15f*pet.zoom,pet.ground.position.z});double previousDistance=std::hypot(center.x-pixel.x,center.y-pixel.y);
                for(int frame=0;frame<700&&pet.battle->phase!=pet::Battle::Phase::Idle;++frame){
                    pet.lastTick=preciseSeconds()-0.008;pet.tick();
                    if(frame<50){requireScene(std::abs(std::remainder(pet.yaw-awayFacing,360.0f))<0.01f,"An active-input walk must keep its away heading in the initial straight section");
                        const auto body=pet.ground.screen({pet.ground.position.x,1.15f*pet.zoom,pet.ground.position.z});const double distance=std::hypot(body.x-pixel.x,body.y-pixel.y);
                        requireScene(distance>=previousDistance-0.01,"The projected body center must move away from the mouse, not just its feet");previousDistance=distance;}

                    if(pet.stopTurn.active){curved=true;targetYaw=pet.stopTurn.target;}
                }
                requireScene(curved&&pet.battle->phase==pet::Battle::Phase::Idle&&!pet.state.walking
                    &&std::abs(std::remainder(pet.yaw-float(targetYaw),360.0f))<0.5f,"The final curve must finish facing its frozen cursor target and stop, even with battle action-look disabled");
                pet.ground.position=saved;pet.state.trackingEnabled=look;pet.battle->start();
                std::printf("PASS: active-input away segment, late curved turn, planted stop facing cursor\n");std::fflush(stdout);
            }
            const auto target=pet.ground.floorAt({2450,750});const float dx=target.x-pet.ground.position.x,dz=target.z-pet.ground.position.z;
            const double length=std::hypot(dx,dz);const float heading=std::atan2(dx,dz)*RAD2DEG;
            requireScene(pet.canStartWalk(heading,length),"The actual loaded model must accept a cross-screen trip with a safe stop");
            const auto blocked=pet.ground.floorAt({1940,750});
            requireScene(!pet.canStartWalk(heading,std::hypot(blocked.x-pet.ground.position.x,blocked.z-pet.ground.position.z)),"The actual loaded model must reject a trip ending on the seam");
            pet.state.movementScale=pet.zoom*3;pet.state.walkSpeed=0.792;pet.state.walkYaw=heading;pet.yaw=heading;
            pet.state.start(pet.battle->clips.move,true,true);pet.state.walkRemaining=length/(0.792*pet.zoom*3);pet.walkGeneration=pet.state.generation;
            pet.battle->phase=pet::Battle::Phase::Moving;pet.sharedEvents={};int gapFrames=0;
            for(int frame=0;frame<2500&&pet.battle->phase!=pet::Battle::Phase::Idle;++frame){
                const auto before=pet.ground.position;const auto phase=pet.battle->phase;pet.lastTick=preciseSeconds()-0.008;pet.tick();
                const auto foot=pet.ground.screen(pet.ground.position);if(foot.x>1920&&foot.x<1980){++gapFrames;requireScene(std::hypot(pet.ground.position.x-before.x,pet.ground.position.z-before.z)>0,"Actual model movement must never pause in the monitor gap");}
                if(phase==pet::Battle::Phase::Moving||phase==pet::Battle::Phase::Jump)requireScene(std::hypot(pet.ground.position.x-before.x,pet.ground.position.z-before.z)>0,"Actual Moving/Jump must keep translating through the edge");
            }
            requireScene(gapFrames>0&&pet.battle->phase==pet::Battle::Phase::Idle&&!pet.directed.active&&DesktopPath::restingMonitor(pet.ground,pet.geometry,pet.zoom,pet.ground.position)==1,
                "Actual Move_End must arrive on the other monitor without starting a second docking walk");
            pet.idleInputSeconds=inputSource;pet.ground=savedGround;pet.work=savedWork;pet.yaw=savedYaw;pet.state.walkEnabled=walking;pet.selectMode(false,false);
            for(size_t i=0;i<scene.actors.size();++i)scene.actors[i]->displayed=shown[i];pet.presentState(true);
            std::printf("PASS: actual model crosses monitor gap continuously and stops once on the destination screen\n");std::fflush(stdout);
        }
        const auto snapshot=scene.snapshot();requireScene(snapshot["characters"].size()==6,"History must include the inactive sixth actor");
        const auto sixth=scene.roster.entries.back().id;scene.activate(sixth);
        requireScene(!scene.roster.find(sixth)->active&&scene.actors.size()==5,"A sixth renderer must not start");
        scene.error.clear();
        auto value=scene.selected().settingsSnapshot()["value"];value["outlineMode"]=1;value["outlineWidth"]=175;value["inputGrace"]=2.5;
        value["walking"]=false;value["follow"]=false;value["skillProbability"]=0;value["clickProbability"]=100;
        scene.message({{"type","save"},{"value",value}});
        for(const auto& a:scene.actors)requireScene(a->preferences.outlineMode==1&&a->preferences.outlineWidth==175&&a->preferences.inputGrace==3,"All actors must share settings immediately");
        ThreePreferences restored;restored.load((folder/L"settings.ini").wstring());requireScene(restored.outlineMode==1&&restored.outlineWidth==175,"One shared INI must persist the parameters");
        for(const auto& a:scene.actors)if(a->settings->battleAvailable)a->selectMode(true,false);
        auto& cafe=scene.leader();cafe.selectMode(false,false);
        for(int i=0;i<3;++i){cafe.click=true;cafe.tick();}
        requireScene(cafe.battleMode,"Triple clicking a combat-capable Cafe actor must enter battle");
        for(auto& a:scene.actors)if(a->battleMode)a->battle->start();
        scene.input.clear();scene.input.mouseClick=true;scene.tick();
        const auto calls=std::count_if(scene.actors.begin(),scene.actors.end(),[](const auto& a){return a->battleMode&&a->battle->phase==pet::Battle::Phase::Callsign;});
        requireScene(calls==1,"One global click must select exactly one Callsign at 100 percent chance");
        scene.input.mouseClick=true;scene.tick();
        requireScene(std::count_if(scene.actors.begin(),scene.actors.end(),[](const auto& a){return a->battleMode&&a->battle->phase==pet::Battle::Phase::Callsign;})==1,"No second Callsign may start while the first is active");
        // Collision math against actual loaded body widths, including zoom.
        auto& moving=*scene.actors[1];auto& obstacle=*scene.actors[2];
        moving.ground.position=moving.ground.floorAt({moving.ground.width/2.0f,moving.ground.height*0.7f});moving.yaw=90;
        obstacle.ground.position=moving.ground.step(90,Scene::radius(moving)+Scene::radius(obstacle)+0.15f);
        const auto heading=scene.steer(moving,90,0.01);
        requireScene(!std::isfinite(heading)&&moving.yaw==90,"A blocked route must stop without a detour or rotation");
        const auto movingInput=moving.idleInputSeconds;moving.idleInputSeconds=[](){return 0.0;};
        moving.selectMode(true,false);moving.state.walkEnabled=true;moving.preferences.follow=true;
        moving.directed.begin(moving.ground.step(90,2),true,moving.state.generation,90);moving.placementNeeded=false;
        moving.interactionUntil=preciseSeconds()+100;
        moving.lastTick=preciseSeconds()-0.008;moving.tick();
        requireScene(!moving.directed.active&&moving.blockedCursor.has_value(),"Blocked follow must abandon its destination");
        const auto stopped=moving.ground.position;
        moving.interactionUntil=moving.followPlanAt=0;
        for(int frame=0;frame<50;++frame){moving.lastTick=preciseSeconds()-0.008;moving.tick();}
        requireScene(!moving.directed.active&&moving.yaw==90&&moving.ground.position.x==stopped.x&&moving.ground.position.z==stopped.z,
            "Unchanged cursor must not restart the blocked destination or rotate the character");
        moving.idleInputSeconds=movingInput;moving.preferences.follow=false;moving.state.walkEnabled=false;moving.blockedCursor.reset();
        const auto clear=scene.steer(moving,-90,0.01);requireScene(std::isfinite(clear),"A free escape direction must remain usable");
        for(auto& a:scene.actors)a->placementNeeded=false;
        for(int i=0;i<400;++i)pump(); // Compile and render the contour shader in every WebView.
        scene.leader().showSettings();for(int i=0;i<100;++i)pump();
        requireScene(scene.leader().webSettings->browser!=nullptr,"Shared web settings must open");
        scene.orderDepth();for(const auto& a:scene.actors)requireScene(above(scene.leader().webSettings->window,a->window),"The settings menu must remain above all depth-sorted pets");
        auto script=[&](const wchar_t* text){
            bool completed=false;Json value;HRESULT status=E_FAIL;
            auto* callback=new Callback<ICoreWebView2ExecuteScriptCompletedHandler,HRESULT,LPCWSTR>([&](HRESULT hr,LPCWSTR json){status=hr;value=Json::parse(utf8(json));completed=true;return S_OK;});
            const auto hr=scene.leader().webSettings->browser->ExecuteScript(text,callback);callback->Release();check(hr,"Test settings DOM");
            const double deadline=preciseSeconds()+8;while(!completed&&preciseSeconds()<deadline)pump();requireScene(completed&&SUCCEEDED(status),"Settings script must complete");return value;
        };
        const double menuDeadline=preciseSeconds()+8;
        while(script(L"!!document.querySelector('#help')?.onclick")!=Json(true)){
            requireScene(preciseSeconds()<menuDeadline,"Settings module must finish attaching its controls");
            for(int i=0;i<10;++i)pump();
        }
        script(L"document.querySelector('#help').click();true");
        requireScene(script(L"({open:document.querySelector('#help-dialog').open,debounce:!!document.querySelector('[name=inputGrace]'),turn:!!document.querySelector('[name=cafeLookMode]'),size:!!document.querySelector('#uniform-size')})")==Json({{"open",true},{"debounce",false},{"turn",true},{"size",true}}),"Help dialog, hidden fixed debounce, cafe turn mode and uniform size must exist in the live menu");
        requireScene(script(L"({pages:Array.from(document.querySelectorAll('nav button')).map(b=>b.textContent),cafe:document.querySelector('[name=cafeTracking]').closest('section').dataset.section,battle:document.querySelector('[name=look]').closest('section').dataset.section,probability:!!document.querySelector('[name=attentionProbability]')})")==Json({{"pages",Json::array({"全局","咖啡厅","战斗","角色"})},{"cafe","cafe"},{"battle","battle"},{"probability",false}}),"Settings must separate global/cafe/battle and remove the cafe probability field");
        script(L"document.querySelector('#help-close').click();true");
        Json dom;
        for(int attempt=0;attempt<12;++attempt){dom=script(L"({rows:document.querySelectorAll('.character').length,range:document.querySelector('[name=outlineWidth]')?.type,mode:document.querySelector('[name=outlineMode]')?.value})");if(dom["rows"]==6)break;for(int i=0;i<25;++i)pump();}
        requireScene(dom["rows"]==6&&dom["range"]=="range"&&dom["mode"]=="1","Real menu DOM must contain history and the new outline controls");
        script(L"document.querySelector('[name=outlineWidth]').value=125;document.querySelector('[name=outlineWidth]').dispatchEvent(new Event('change',{bubbles:true}));true");
        for(int i=0;i<30;++i)pump();
        for(const auto& a:scene.actors)requireScene(a->preferences.outlineWidth==125,"An actual web form change must propagate to every actor");
        script(L"document.querySelector('[name=cafeTracking]').checked=false;document.querySelector('[name=cafeTracking]').dispatchEvent(new Event('change',{bubbles:true}));true");
        for(int frame=0;frame<30;++frame)pump();
        for(const auto& actor:scene.actors)requireScene(!actor->preferences.cafeTracking,"Cafe tracking toggle must apply to all actors through the real form");
        ThreePreferences trackingReload;trackingReload.load((folder/L"settings.ini").wstring());requireScene(!trackingReload.cafeTracking,"Cafe tracking toggle must persist");
        scene.close(scene.leader().actorId);pump();
        requireScene(scene.roster.activeCount()==4&&!scene.leader().displayed,"Closing the leader must retain a hidden timer host for the other actors");
        scene.activate(scene.leader().actorId);pump();
        requireScene(scene.roster.activeCount()==5&&scene.leader().displayed&&scene.leader().preferences.outlineWidth==125,"Reopening a hidden leader must retain shared settings and resume a valid state");
        scene.message({{"type","roster_remove"},{"id",sixth}});requireScene(!scene.roster.find(sixth)&&scene.roster.entries.size()==5,"Removing history must not delete or affect displayed actors");
        // Close non-leaders, keep each path in history, then exercise both answers to the real confirmation.
        std::vector<std::string> ids;for(const auto& e:scene.roster.entries)if(e.active&&e.id!=scene.leader().actorId)ids.push_back(e.id);
        for(const auto& id:ids){scene.close(id);pump();}
        requireScene(scene.roster.activeCount()==1&&scene.actors.size()==1&&scene.roster.entries.size()==5,"Closing actors must release renderers and retain history");
        answer=IDNO;SetTimer(nullptr,0,50,answerDialog);scene.close(scene.leader().actorId);
        requireScene(!scene.stopping&&scene.roster.activeCount()==1,"Canceling the last-character confirmation must keep the program running");
        answer=IDYES;SetTimer(nullptr,0,50,answerDialog);scene.close(scene.leader().actorId);
        requireScene(scene.stopping&&scene.roster.activeCount()==0,"Confirming closure of the last actor must exit");
        std::printf("PASS: five GLBs, six-entry history, cap, shared settings/menu, Cafe triple click, single Callsign, avoidance, contour render, close/cancel/exit\n");std::fflush(stdout);result=0;
    }catch(const std::exception& e){std::fprintf(stderr,"SCENE TEST: %s\n",e.what());std::fflush(stderr);}
    CoUninitialize();return result;
}
