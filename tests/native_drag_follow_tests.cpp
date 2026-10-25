#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
static POINT testCursor{};
static BOOL WINAPI testGetCursorPos(LPPOINT point){*point=testCursor;return TRUE;}
#define GetCursorPos testGetCursorPos
#define PET_NATIVE_NO_ENTRY
#include "../main.cpp"
#undef GetCursorPos
#include <iostream>
#include <limits>

static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){try{
    check(argc>=2,"Pass a fresh isolated profile directory and optional model");
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path(),profile=std::filesystem::absolute(argv[1]);
    check(!std::filesystem::exists(profile),"Never use an existing user profile for drag tests");std::filesystem::create_directories(profile);
    GraphicsContext graphics;auto first=std::make_unique<App>();
    first->directory=std::filesystem::is_regular_file(root/L"build/bin/behavior/template.bt.xml")?root/L"build/bin":root/L"build/bin/Release";first->programDirectory=profile;first->dataDirectory=profile;first->dataDirectorySpecified=true;
    first->model=argc>2?std::filesystem::absolute(argv[2]):root/L"assets/Shiroko.glb";first->modelSpecified=true;first->initialMode=argc>3?utf8(argv[3]):"cafe";first->review=true;first->diagnostics=false;
    first->idleInputSeconds=[] {return 0.0;};
    Scene scene(std::move(first));auto& a=scene.leader();
    // WGL requires the input desktop on some drivers. No frame is presented;
    // keep the actual HWNDs hidden and use only our simulated cursor samples.
    ShowWindow(a.window,SW_HIDE);ShowWindow(a.inputWindow,SW_HIDE);a.depthRequest={};
    const auto tick=[&](int count){for(int i=0;i<count;++i){a.lastTick=preciseSeconds()-.008;scene.tick();}};
    tick(1);check(a.loaded,"Load the real bundled model");
    a.setZoom(argc>4?std::wcstof(argv[4],nullptr):1);
    a.preferences.follow=true;a.preferences.followNear=1;a.preferences.followFar=5;a.preferences.wandering=false;a.preferences.battleWalking=false;a.preferences.cafeTracking=true;a.preferences.cafeLookMode="smooth";
    a.state.activeSecondsOverride=1;a.applyPreferences();a.interactionUntil=0;
    const auto present=[&]{a.presentState(true);a.finishRender();check(!IsWindowVisible(a.window),"The test must never expose a pet window");};present();
    const auto pointer=[&](Vector2 p){testCursor={a.work.left+LONG(std::lround(p.x)),a.work.top+LONG(std::lround(p.y))};};
    int failures=0;const auto follow=[&](const char* name,std::optional<Vector2> chosen=std::nullopt){
        const auto start=a.ground.screen(a.ground.position),center=a.ground.screen({a.ground.position.x,1.15f*a.zoom,a.ground.position.z});
        const auto side=a.ground.screen({a.ground.position.x+a.geometry.bodyWidth*a.zoom,1.15f*a.zoom,a.ground.position.z});
        const auto origin=a.windowPosition;const auto pointerTarget=chosen.value_or(Vector2{start.x<a.ground.width*.5f?float(a.ground.width)*.8f:float(a.ground.width)*.2f,center.y});pointer(pointerTarget);
        const bool needsTravel=DirectedWalk::outsideBand(center,pointerTarget,std::max(1.f,std::abs(side.x-center.x)),a.preferences.followNear,a.preferences.followFar);
        a.interactionUntil=0;for(int i=0;i<2000;++i){if(i%20==0)a.followPlanAt=0;tick(1);}const auto end=a.ground.screen(a.ground.position);
        present();const double progress=((end.x-start.x)*(pointerTarget.x-center.x)+(end.y-start.y)*(pointerTarget.y-center.y))/std::hypot(pointerTarget.x-center.x,pointerTarget.y-center.y);
        std::cout<<name<<": progress="<<progress<<" px; following="<<a.directed.active<<"; crossing="<<a.directed.crossing<<'\n';
        if(needsTravel&&progress<=20){++failures;const auto b=a.ground.bodyOffsets;std::cerr<<"Stalled: foot="<<end.x<<","<<end.y<<" extent="<<end.x+b.left<<","<<end.y+b.top<<".."<<end.x+b.right<<","<<end.y+b.bottom<<" speed="<<a.directed.travelSpeed<<" capture="<<a.left<<" dragging="<<a.state.dragging<<" clip="<<a.state.current<<" idle="<<a.cafeIdle<<'\n';}
        if(needsTravel)check(std::hypot(a.windowPosition.x-origin.x,a.windowPosition.y-origin.y)>20,"The committed window must move with the simulated pet");
        if(!a.battleMode&&!a.directed.active)check(std::abs(std::remainder(a.yaw-float(a.state.lookYaw),360.f))<1,"Idle eye tracking resumes after following");
    };
    follow("before drag");
    for(Vector2 destination:std::vector<Vector2>{{a.ground.width*.5f,a.ground.height*.65f},{4,a.ground.height*.75f},{a.ground.width-4.f,a.ground.height*.75f},{a.ground.width*.5f,4},{a.ground.width*.5f,a.ground.height-4.f}}){
        const auto start=a.ground.screen(a.ground.position);pointer(start);App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);
        for(int i=1;i<=5;++i){pointer({start.x+(destination.x-start.x)*i/5,start.y+(destination.y-start.y)*i/5});App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);tick(1);present();}
        App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick(1);present();
        check(!a.left&&!a.state.dragging,"Drag state must clear on mouse release");follow("after drag");
    }
    // One actor only: keep changing the cursor after a central drop, including
    // targets at screen edges that need clamping and later replanning.
    const Vector2 middle{a.ground.width*.5f,a.ground.height*.65f};
    auto start=a.ground.screen(a.ground.position);pointer(start);App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);pointer(middle);App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);tick(1);present();App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick(1);
    for(Vector2 target:std::vector<Vector2>{{2,a.ground.height*.65f},{a.ground.width-2.f,a.ground.height*.65f},{a.ground.width*.8f,2},{a.ground.width*.2f,a.ground.height-2.f}})follow("single actor cursor retarget",target);
    if(!a.battleMode){
        double inputIdle=0;a.idleInputSeconds=[&]{return inputIdle;};
        a.preferences.follow=false;a.preferences.wandering=true;a.preferences.cafeTracking=false;
        a.preferences.moveInterval=.02;a.preferences.moveBodies=.5;a.preferences.walkTimeJitter=a.preferences.walkDistanceJitter=0;
        a.state.idleSecondsOverride=1;a.state.random.seed(17);a.applyPreferences();
        const auto distance=[](Vector3 p,Vector3 q){return std::hypot(p.x-q.x,p.z-q.z);};
        const auto drop=[&](Vector2 target,bool tickDuringDrag){
            const auto origin=a.ground.screen(a.ground.position);pointer(origin);App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);
            pointer(target);App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);if(tickDuringDrag)tick(1);
            const auto placed=a.ground.position;App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick(1);
            check(a.journey.home&&distance(*a.journey.home,placed)<1e-5,"Mouse release must remember the drop even if no tick observed the drag");
            return placed;
        };
        const auto wander=[&](Vector3 placed){
            inputIdle=100;
            for(int i=0;i<1500&&(!a.state.walking||a.journey.route.empty()||distance(a.ground.position,placed)<.08);++i)tick(1);
            check(a.state.walking&&!a.state.returningHome&&distance(a.ground.position,placed)>=.08,"Real inactive behavior must wander away from the drop");
            check(!a.journey.route.empty()&&distance(a.journey.route.back(),placed)<1e-5,"Wandering must target the latest manual drop");
        };
        const auto returnOnInput=[&](Vector3 placed){
            inputIdle=0;tick(1);check(a.state.returnRequested&&a.state.returningHome,"Resuming input must start returning to the drop");
            for(int i=0;i<3000&&!a.state.returnArrived;++i)tick(1);
            check(a.state.returnArrived&&distance(a.ground.position,placed)<.002,"Input must return to the drop, not a later walking position");tick(2);
        };
        const auto placed=drop(middle,false);wander(placed);
        // Rotation interrupts the excursion but cannot redefine the manual drop.
        App::inputProc(a.inputWindow,WM_MBUTTONDOWN,MK_MBUTTON,0);tick(1);App::inputProc(a.inputWindow,WM_MBUTTONUP,0,0);tick(1);
        wander(placed);returnOnInput(placed);
        const auto replacement=drop({middle.x+50,middle.y},true);wander(replacement);
        // Exercise the actual blocked-movement stop, then resume input before
        // another random walk can repopulate the temporary return route.
        const auto steering=a.avoidHeading;a.avoidHeading=[](float,double){return MovementChoice{std::numeric_limits<float>::quiet_NaN()};};
        tick(1);a.avoidHeading=steering;
        check(a.journey.route.empty()&&!a.state.walking,"A blocked walk clears only its transient route");returnOnInput(replacement);
        a.idleInputSeconds=[] {return 0.0;};a.preferences.follow=true;a.preferences.wandering=false;a.preferences.cafeTracking=true;a.applyPreferences();
        std::cout<<"Manual drop, interrupted wandering, replacement drop and input-triggered return passed.\n";
    }
    // Repeat a central drag into a stationary peer's collision envelope.
    const auto peerFile=profile/L"Peer.glb";std::filesystem::copy_file(a.model,peerFile);const auto peerId=SceneRoster::key(peerFile);
    scene.roster.remember({peerId,utf8(peerFile.wstring()),"Peer","cafe",false});scene.activate(peerId);tick(2);
    auto* peer=scene.find(peerId);check(peer&&peer->loaded,"Load a real peer model");ShowWindow(peer->window,SW_HIDE);ShowWindow(peer->inputWindow,SW_HIDE);peer->depthRequest={};peer->preferences.follow=false;peer->preferences.wandering=false;peer->applyPreferences();peer->state.settingsOpen=true;
    const Vector2 center{a.ground.width*.5f,a.ground.height*.65f};peer->geometry.place(peer->ground,center,peer->zoom);
    start=a.ground.screen(a.ground.position);pointer(start);App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);
    pointer({center.x-12,center.y});App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);tick(1);App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick(1);follow("central peer overlap");
    check(failures==0,"Follow must displace the character after releasing central and edge drags");
    std::cout<<"Native drag/follow lifecycle passed.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
