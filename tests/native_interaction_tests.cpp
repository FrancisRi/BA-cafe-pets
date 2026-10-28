// Copyright (C) 2026 FrancisRi
// SPDX-License-Identifier: GPL-3.0-only
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
static POINT interactionCursor{};
static BOOL WINAPI interactionGetCursorPos(LPPOINT p){*p=interactionCursor;return TRUE;}
#define GetCursorPos interactionGetCursorPos
#define PET_NATIVE_NO_ENTRY
#include "../main.cpp"
#undef GetCursorPos
#include "animation_scale_transition.h"
#include <iostream>

static void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int wmain(int argc,wchar_t** argv){try{
    std::vector<nativepet::Node> scaleNodes(2);std::vector<nativepet::Pose> scaleFrom(2);scaleNodes[1].parent=0;
    scaleFrom[0].s={.01f,.01f,.01f};scaleNodes[0].current.s={.0001f,.0001f,.0001f};scaleFrom[1].s={100,100,100};scaleNodes[1].current.s={10000,10000,10000};
    check(compensatingScaleTransition(scaleNodes,scaleFrom),"Opposite parent/child unit conversions require an atomic pose switch");
    scaleNodes[0].current.s=scaleFrom[0].s;check(!compensatingScaleTransition(scaleNodes,scaleFrom),"An authored scale change without ancestor compensation keeps its ordinary blend");
    scaleNodes[0].current.s={.011f,.011f,.011f};scaleNodes[1].current.s={101,101,101};check(!compensatingScaleTransition(scaleNodes,scaleFrom),"Ordinary breathing/pose scale interpolation stays smooth");
    NativePetClicks gesture;
    check(!gesture.release(100,0,0,500,8,8).doubleClick,"First click remains a single click");
    check(gesture.release(200,0,0,500,8,8).doubleClick,"Second click emits the manual skill gesture");
    check(gesture.release(300,0,0,500,8,8).tripleClick,"Third click preserves the legacy mode/retirement gesture");
    gesture.release(1000,0,0,500,8,8);check(!gesture.release(1700,0,0,500,8,8).doubleClick,"Slow clicks cannot start a double-click skill");
    gesture.reset();gesture.release(2000,0,0,500,8,8);check(!gesture.release(2100,30,0,500,8,8).doubleClick,"A different click location starts a new sequence");
    gesture.reset();gesture.release(3000,0,0,500,8,8);check(!gesture.release(4400,0,0,500,8,8).tripleClick&&gesture.count==1,"An expired sequence cannot switch modes");
    check(argc>=2,"Pass a fresh isolated profile path and optional model path");
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path(),profile=std::filesystem::absolute(argv[1]);
    check(!std::filesystem::exists(profile),"Never use user data for interaction tests");std::filesystem::create_directories(profile);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    GraphicsContext graphics;auto app=std::make_unique<App>();app->directory=root/L"build/bin";app->programDirectory=app->dataDirectory=profile;app->dataDirectorySpecified=true;
    app->model=argc>2?std::filesystem::absolute(argv[2]):root/L"assets/Shiroko.glb";app->modelSpecified=true;app->initialMode="cafe";app->review=true;app->idleInputSeconds=[] {return 0.0;};
    Scene scene(std::move(app));auto& a=scene.leader();ShowWindow(a.window,SW_HIDE);ShowWindow(a.inputWindow,SW_HIDE);a.depthRequest={};
    scene.frameClock.reset(preciseSeconds()+3600);
    const auto tick=[&](int count=1){for(int i=0;i<count;++i){a.lastTick=preciseSeconds()-.016;scene.tick();}};tick();
    check(a.loaded,"Load actual GLB with current animation discovery");
    if(a.formationIdle>=0&&a.formationPickup>=0){
        auto& model=*a.renderer->model;
        const auto extent=[&]{model.updateGeometry(false);float result=0;for(const auto& part:model.parts){bool visible=model.nodes[part->node].inScene;for(int n=part->node;n>=0;n=model.nodes[n].parent)visible&=model.nodes[n].visible;if(!visible)continue;const auto b=part->posedBounds;result=std::max(result,std::max({b.max.x-b.min.x,b.max.y-b.min.y,b.max.z-b.min.z}));}return result;};
        const int from=a.state.clips[a.formationIdle].modelIndex,to=a.state.clips[a.formationPickup].modelIndex;
        model.sample(from,0,0,false);const float initial=extent();model.sample(to,0,0,false);const float picked=extent();
        model.sample(from,0,0,false);model.sample(to,0,.09,true);const float transition=extent();
        std::cout<<"Formation pickup size: idle="<<initial<<" pickup="<<picked<<" transition="<<transition<<'\n';
        check(transition<=2*std::max(initial,picked),"Compensating parent/child scales must not inflate a pickup transition");
        model.sample(from,0,.09,true);check(extent()<=2*std::max(initial,picked),"Dropping must not inflate the reverse transition");
        model.sample(a.state.clips[a.cafeIdle].modelIndex,0,0,false);model.updateGeometry(false);model.currentClip=-1;model.displayed=false;
    }
    a.preferences.follow=true;a.preferences.wandering=true;a.preferences.battleWalking=true;a.preferences.cafeTracking=true;a.state.trackingEnabled=true;a.applyPreferences();
    a.geometry.place(a.ground,{a.ground.width*.5f,a.ground.height*.65f},a.zoom);
    const auto pointer=[&](float x,float y){interactionCursor={a.work.left+LONG(x),a.work.top+LONG(y)};};
    auto foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);
    const auto click=[&]{App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick();};
    if(a.manualSkill>=0){
        a.clickSequence.reset();click();click();check(a.manualAction&&a.state.current==a.manualSkill,"Double click starts Exs even without battle capability");
        if(argc>3){
            const auto out=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(out);
            a.smooth=false;
            for(int percent:{10,35,60,85}){a.state.time=a.state.clips[a.manualSkill].duration*percent/100;a.presentState(true);
                a.renderer->render(a.window,a.inputWindow,true,out/(L"exs-"+std::to_wstring(percent)+L".png"));a.commitRender();}
            a.state.time=0;
        }
        const auto anchor=a.ground.position;pointer(1,1);a.yaw=90;tick();a.presentState(true);
        check(a.yaw==0&&a.renderer->state["yaw"]==90,"Manual Exs compensates the authored left-facing release by +90 degrees while ignoring the cursor");
        check(Vector3Distance(a.ground.position,anchor)<1e-6&&!a.directed.active,"Manual Exs pauses positional follow and wandering");
        // Keep the original click position; the third click must override Exs.
        pointer(foot.x,foot.y);click();
        if(a.settings->battleAvailable)check(a.battleMode&&!a.manualAction,"Cafe triple click interrupts Exs and enters battle");
        else check(a.manualAction,"Unavailable battle mode does not interrupt a valid Exs");
        if(a.battleMode){
            a.clickSequence.reset();click();click();check(a.manualAction,"Battle double click also starts Exs");click();
            check(!a.manualAction&&(a.battle->exiting()||a.battle->returnToCafe),"Battle third click interrupts Exs and retires");
        }
        a.selectMode(false);a.startManualSkill();a.state.time=a.state.clips[a.manualSkill].duration-.001;tick();
        check(!a.manualAction&&!a.battleMode&&a.state.current==a.cafeIdle,"Completed Exs returns to the previous mode idle");
        a.presentState(true);check(a.renderer->state["yaw"]==0,"Exs facing compensation ends with the skill");
        a.startManualSkill();foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);
        App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);interactionCursor.x+=12;App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);tick();a.presentState(true);
        check(!a.manualAction&&a.state.dragging&&a.renderer->state["yaw"]==0,"Pickup interrupts Exs without inheriting its release compensation");
        App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick();
        if(a.settings->battleAvailable&&a.battle->clips.skill>=0){
            a.selectMode(true);a.state.inputActive=true;a.battle->skillProbability=1;a.battle->activityDelta=1000;a.battle->tick(.016);a.yaw=-45;a.presentState(true);
            check(a.battle->phase==pet::Battle::Phase::Skill&&a.renderer->state["yaw"]==90,"Automatic battle Exs uses the same authored release compensation");
            a.battle->start();a.presentState(true);check(a.renderer->state["yaw"]==-45,"Ordinary battle idle keeps its own heading");
        }
    }else{a.clickSequence.reset();click();click();check(!a.manualAction,"A missing Exs never selects an unrelated clip");}
    a.selectMode(false);
    if(a.formationIdle<0){check(!a.selectActionMode("formation")&&!a.formationMode,"A missing Formation_Idle cannot enter a fake formation mode");std::cout<<"Double-click/triple-click interactions and unavailable formation capability passed.\n";return 0;}
    check(a.selectActionMode("formation",true),"Enter formation mode");
    check(a.formationMode&&!a.battleMode&&a.state.current==a.formationIdle,"Formation starts its actual Formation_Idle clip");
    const auto anchor=a.ground.position;const int firstFormationIdle=a.formationIdle;pointer(1,1);a.yaw=90;tick(2500);a.presentState(true);
    check(Vector3Distance(anchor,a.ground.position)<1e-6&&!a.state.walking&&!a.directed.active&&!a.state.trackingCursor,"Formation never wanders, follows, returns home or tracks the cursor");
    check(a.yaw==0&&a.renderer->state["yaw"]==0,"Formation remains screen-facing");
    click();click();click();check(a.formationMode&&!a.manualAction&&a.state.current==a.formationIdle&&a.formationIdle==firstFormationIdle,"Formation ignores single, double and triple clicks without changing the selected formation set");
    foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);pointer(foot.x+50,foot.y-20);App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);tick();
    check(a.state.current==(a.formationPickup>=0?a.formationPickup:a.formationIdle)&&a.state.dragging,"Formation drag plays its pickup animation");
    check(Vector3Distance(anchor,a.ground.position)>0.01f,"Formation can still be manually placed");
    App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick();check(a.state.current==a.formationIdle&&!a.state.dragging,"Dropping restores Formation_Idle");
    if(a.formationPoses.size()>1)check(a.formationIdle!=firstFormationIdle,"A completed pickup/drop randomly changes to a different formation idle when multiple sets exist");
    for(int repetition=0;repetition<6&&a.formationPoses.size()>1;++repetition){
        const auto previous=a.formationIdle;foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);
        App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);interactionCursor.x+=8;App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);tick();
        check(a.state.current==(a.formationPickup>=0?a.formationPickup:a.formationIdle),"Each formation set uses its own compatible pickup");
        App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick();check(a.formationIdle!=previous&&a.state.current==a.formationIdle,"Repeated drops keep choosing a different valid formation set");
    }
    const auto dropped=a.ground.position;tick(120);check(Vector3Distance(dropped,a.ground.position)<1e-6,"The dropped formation character stays at the manual placement");
    App::inputProc(a.inputWindow,WM_RBUTTONDOWN,MK_RBUTTON,0);interactionCursor.x+=40;App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_RBUTTON,0);App::inputProc(a.inputWindow,WM_RBUTTONUP,0,0);tick();
    check(a.yaw==0,"Right drag cannot rotate formation");
    App::inputProc(a.inputWindow,WM_MBUTTONDOWN,MK_MBUTTON,0);check(!a.right,"Middle drag cannot rotate formation");
    const auto zoom=a.zoom;App::inputProc(a.inputWindow,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);check(a.zoom==zoom,"Formation allows pickup/drop rather than wheel interaction");
    App::inputProc(a.inputWindow,WM_HOTKEY,2,0);check(a.preferences.follow,"Formation hotkeys preserve the saved follow preference");
    check(a.preferences.follow&&a.preferences.wandering&&a.preferences.cafeTracking&&a.state.trackingEnabled,"Formation does not erase shared behavior preferences");
    check(a.settingsSnapshot()["value"]["mode"]=="formation","Settings snapshot exposes the third mode");
    SceneRoster saved;SceneEntry entry{"formation-model",utf8(a.model.wstring()),"Formation test","formation",true};saved.remember(entry);saved.selected=entry.id;saved.save(profile/L"formation-roster.json");
    SceneRoster restored;restored.load(profile/L"formation-roster.json");check(restored.find(entry.id)->mode=="formation","Per-character formation mode survives roster restart");
    a.settings->save();a.initialMode.clear();a.initializeBehavior(a.renderer->model->metadata);check(a.formationMode,"Persisted standalone formation mode restores after model initialization");
    check(a.selectMode(false)&&!a.formationMode&&a.state.current==a.cafeIdle,"Switch back to cafe normally");
    a.presentState(true);a.finishRender();check(!IsWindowVisible(a.window),"Integration tests keep the rendered pet hidden");
    std::cout<<"Double-click Exs, triple-click priority, formation interactions and persistence passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
