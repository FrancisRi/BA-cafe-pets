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
    check(gesture.release(300,0,0,500,8,8).tripleClick,"Third click emits the mode cycle gesture");
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
    {
        auto& model=*a.renderer->model;const int idle=model.catalog["cafe"]["idle"],pickup=model.catalog["cafe"]["pickup"];
        const auto collapsed=[](Vector3 value,Vector3 base){return std::abs(value.x)<=std::abs(base.x)*.0011f&&std::abs(value.y)<=std::abs(base.y)*.0011f&&std::abs(value.z)<=std::abs(base.z)*.0011f;};
        model.sample(idle,0,0,false);std::vector<nativepet::Pose> idlePose;for(const auto& n:model.nodes)idlePose.push_back(n.current);
        model.sample(pickup,0,0,false);std::vector<nativepet::Pose> pickedPose;for(const auto& n:model.nodes)pickedPose.push_back(n.current);
        std::vector<size_t> switches;for(size_t n=0;n<model.nodes.size();++n)if(collapsed(idlePose[n].s,model.nodes[n].rest.s)!=collapsed(pickedPose[n].s,model.nodes[n].rest.s))switches.push_back(n);
        for(bool reverse:{false,true}){
            model.sample(reverse?pickup:idle,0,0,false);model.sample(reverse?idle:pickup,0,.09,true);
            const auto& expected=reverse?idlePose:pickedPose;
            for(size_t n:switches)check(Vector3Distance(model.nodes[n].current.t,expected[n].t)<1e-6&&Vector3Distance(model.nodes[n].current.s,expected[n].s)<1e-6,"A bone-scaled hidden prop must switch in place instead of flying between its hiding position and the hand");
        }
        model.sample(idle,0,0,false);model.updateGeometry(false);model.currentClip=-1;model.displayed=false;
        std::cout<<"Hidden prop pickup/drop transitions: "<<switches.size()<<" scale visibility switches passed.\n";
    }
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
    {
        const auto savedPreferences=a.preferences;const auto savedInput=a.idleInputSeconds;
        const auto savedActive=a.state.activeSecondsOverride,savedIdle=a.state.idleSecondsOverride;
        double idle=0;a.idleInputSeconds=[&]{return idle;};int failures=0;
        for(const std::string mode:{"smooth","threshold"}){
            a.selectMode(false);a.preferences.follow=a.preferences.wandering=false;a.preferences.cafeTracking=true;a.preferences.cafeLookMode=mode;
            a.state.activeSecondsOverride=5;a.state.idleSecondsOverride=30;a.applyPreferences();a.interactionUntil=0;
            a.geometry.place(a.ground,{a.ground.width*.5f,a.ground.height*.65f},a.zoom);const auto anchor=a.ground.position;
            const auto eye=a.ground.eyePixel(a.eyeHeight*a.zoom);
            const auto point=[&](float x,float y){interactionCursor={a.work.left+LONG(std::lround(x)),a.work.top+LONG(std::lround(y))};};
            idle=0;a.state.inputActive=true;a.state.activeInputSeconds=6;a.state.noInputState=false;++a.state.inputEpoch;
            point(eye.x+200,eye.y);tick(100);check(std::abs(a.yaw-90)<.1f,"Stationary cafe initially looks at the cursor");
            idle=4;tick(100);point(eye.x-200,eye.y);idle=0;tick(100);
            const bool retargeted=std::abs(std::remainder(a.yaw+90,360.f))<.1f;
            idle=31;tick(10);const float beforeWake=a.yaw;point(eye.x,eye.y);idle=0;tick();
            const bool keptFacing=std::abs(std::remainder(a.yaw-beforeWake,360.f))<.01f;
            idle=120;tick(1000);const float beforeKeyboard=a.yaw;point(eye.x-200,eye.y);idle=0;tick();
            const bool keyboardKeptFacing=std::abs(std::remainder(a.yaw-beforeKeyboard,360.f))<.01f;
            std::cout<<"Stationary cafe "<<mode<<": retarget after quiet="<<retargeted<<"; hover wake without snap="<<keptFacing<<"; keyboard wake without snap="<<keyboardKeptFacing<<"; yaw="<<a.yaw<<'\n';
            failures+=!retargeted||!keptFacing||!keyboardKeptFacing;
            check(Vector3Distance(anchor,a.ground.position)<1e-6&&!a.state.walking&&!a.directed.active,"Stationary eye tracking never changes the ground position");
            a.preferences.cafeTracking=false;point(eye.x+200,eye.y);const float disabledYaw=a.yaw;tick(100);
            check(a.yaw==disabledYaw,"Disabling cafe eye tracking still prevents rotation");
        }
        a.preferences=savedPreferences;a.idleInputSeconds=savedInput;a.state.activeSecondsOverride=savedActive;a.state.idleSecondsOverride=savedIdle;
        a.selectMode(false);a.applyPreferences();check(failures==0,"Stationary cafe eye tracking must survive quiet gaps and input resumption without snapping forward");
    }
    a.preferences.follow=true;a.preferences.wandering=true;a.preferences.battleWalking=true;a.preferences.cafeTracking=true;a.state.trackingEnabled=true;a.applyPreferences();
    a.geometry.place(a.ground,{a.ground.width*.5f,a.ground.height*.65f},a.zoom);
    const auto pointer=[&](float x,float y){interactionCursor={a.work.left+LONG(x),a.work.top+LONG(y)};};
    auto foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);
    const auto click=[&]{App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick();};
    std::vector<std::string> modes{"cafe"};if(a.settings->battleAvailable)modes.push_back("battle");if(a.formationIdle>=0)modes.push_back("formation");
    for(size_t round=0;round<2;++round)for(size_t current=0;current<modes.size();++current){
        check(a.modeName()==modes[current],"Mode cycle starts in the expected supported mode");
        a.clickSequence.reset();foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);const auto before=a.ground.position;
        click();click();click();check(a.modeName()==modes[(current+1)%modes.size()],"Triple click cycles cafe -> battle -> formation -> cafe and skips unavailable modes");
        check(Vector3Distance(before,a.ground.position)<1e-6,"Cycling modes keeps the character at its current position");
        if(modes.size()>1)check(!a.manualAction,"Third click interrupts Exs before switching mode");
    }
    // Exercise all capability combinations independently of the supplied GLB.
    const auto missing=a.battle->clips.missing;const int formation=a.formationIdle;
    a.selectMode(false);a.battle->clips.missing="test: no battle";a.formationIdle=-1;
    a.clickSequence.reset();click();click();click();check(a.modeName()==std::string("cafe"),"A model with only cafe mode stays in cafe on triple click");
    a.battle->clips.missing=missing;a.formationIdle=formation;a.selectMode(false);
    if(a.manualSkill>=0){
        a.yaw=70;
        a.clickSequence.reset();click();click();check(a.manualAction&&a.state.current==a.manualSkill,"Double click starts Exs even without battle capability");
        if(argc>3){
            const auto out=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(out);
            a.smooth=false;
            for(int percent:{10,35,60,85}){a.state.time=a.state.clips[a.manualSkill].duration*percent/100;a.presentState(true);
                a.renderer->render(a.window,a.inputWindow,true,out/(L"exs-"+std::to_wstring(percent)+L".png"));a.commitRender();}
            a.state.time=0;
        }
        const auto anchor=a.ground.position;pointer(1,1);a.yaw=90;tick();a.presentState(true);
        check(a.yaw==70&&a.renderer->state["yaw"]==160,"Manual Exs retains its trigger heading, adds the authored +90 compensation and ignores the cursor");
        check(Vector3Distance(a.ground.position,anchor)<1e-6&&!a.directed.active,"Manual Exs pauses positional follow and wandering");
        // Keep the original click position; the third click must override Exs.
        pointer(foot.x,foot.y);click();
        check(a.modeName()==modes[1%modes.size()],"Cafe triple click interrupts Exs and selects the next supported mode");
        if(modes.size()==1)check(a.manualAction,"No alternative mode leaves a valid Exs playing");
        if(a.battleMode){
            a.clickSequence.reset();click();click();check(a.manualAction,"Battle double click also starts Exs");click();
            check(!a.manualAction&&a.modeName()==std::string(a.formationIdle>=0?"formation":"cafe"),"Battle third click interrupts Exs and switches to formation or cafe directly");
        }
        a.selectMode(false);a.yaw=-30;a.startManualSkill();a.state.time=a.state.clips[a.manualSkill].duration-.001;tick();
        check(!a.manualAction&&!a.battleMode&&a.state.current==a.cafeIdle,"Completed Exs returns to the previous mode idle");
        a.presentState(true);check(a.renderer->state["yaw"]==-30,"Exs facing compensation ends with the skill while retaining its trigger heading");
        a.startManualSkill();foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);
        App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);interactionCursor.x+=12;App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);tick();a.presentState(true);
        check(!a.manualAction&&a.state.dragging&&a.renderer->state["yaw"]==0,"Pickup interrupts Exs without inheriting its release compensation");
        App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick();
        if(a.settings->battleAvailable&&a.battle->clips.skill>=0){
            a.selectMode(true);a.state.inputActive=true;a.battle->skillProbability=1;const auto active=a.state.activeSecondsOverride;a.state.activeSecondsOverride=0;a.yaw=-45;tick();a.presentState(true);a.state.activeSecondsOverride=active;
            check(a.battle->phase==pet::Battle::Phase::Skill&&a.yaw==-45&&a.renderer->state["yaw"]==45,"Automatic battle Exs captures the current heading and uses the same relative compensation");
            pointer(200,200);tick(5);a.presentState(true);check(a.yaw==-45&&a.renderer->state["yaw"]==45,"Automatic Exs ignores cursor changes during the skill");
            a.battle->start();a.presentState(true);check(a.renderer->state["yaw"]==-45,"Ordinary battle idle keeps its own heading");
        }
        for(float heading:{-150.f,-45.f,0.f,60.f,179.f}){
            a.selectMode(false);a.yaw=heading;a.clickSequence.reset();foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);
            click();check(std::abs(std::remainder(a.yaw-heading,360.f))<.01,"The first click preserves the direction intended for a double-click Exs");click();a.presentState(true);
            check(a.manualAction&&std::abs(std::remainder(a.renderer->state["yaw"].get<float>()-heading-90,360.f))<.01,"Exs follows arbitrary trigger headings across angle wrap");
            if(heading==60){
                const auto rotate=[&]{App::inputProc(a.inputWindow,WM_RBUTTONDOWN,MK_RBUTTON,0);interactionCursor.x+=40;App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_RBUTTON,0);App::inputProc(a.inputWindow,WM_RBUTTONUP,0,0);tick();a.presentState(true);};
                rotate();check(a.manualAction&&std::abs(a.yaw-88)<.01&&std::abs(a.renderer->state["yaw"].get<float>()-178)<.01,"Right drag rotates an active Exs without restarting or interrupting it");
                a.preferences.rightDragRotation=false;rotate();check(std::abs(a.yaw-88)<.01,"Disabling the global switch blocks right-drag rotation during Exs");
                a.preferences.rightDragRotation=true;heading=88;
            }
            a.state.time=a.state.clips[a.manualSkill].duration-.001;tick();a.presentState(true);
            check(!a.manualAction&&std::abs(std::remainder(a.yaw-heading,360.f))<.01,"Exs completion retains the trigger heading without leaving the authored compensation on idle");
        }
        const auto savedRotationPreferences=a.preferences;
        for(const auto& mode:modes){
            a.selectActionMode(mode);a.preferences.follow=a.preferences.wandering=a.preferences.cafeTracking=false;a.state.trackingEnabled=false;a.preferences.rightDragRotation=true;a.applyPreferences();a.yaw=13;
            a.startManualSkill();const auto generation=a.state.generation;const auto location=a.ground.position;
            const auto dragRight=[&]{App::inputProc(a.inputWindow,WM_RBUTTONDOWN,MK_RBUTTON,0);interactionCursor.x+=40;App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_RBUTTON,0);App::inputProc(a.inputWindow,WM_RBUTTONUP,0,0);tick();a.presentState(true);};
            dragRight();check(a.manualAction&&a.state.generation==generation&&std::abs(a.yaw-41)<.01&&std::abs(a.renderer->state["yaw"].get<float>()-131)<.01&&Vector3Distance(location,a.ground.position)<1e-6,"Global rotation overrides every mode's active Exs without restarting or moving the pet");
            a.preferences.rightDragRotation=false;a.applyPreferences();const float disabled=a.yaw;dragRight();check(a.manualAction&&a.yaw==disabled&&a.state.generation==generation,"Disabling global rotation blocks Exs rotation in every mode");
        }
        a.preferences=savedRotationPreferences;a.state.trackingEnabled=true;a.applyPreferences();
    }else{a.clickSequence.reset();click();click();check(!a.manualAction,"A missing Exs never selects an unrelated clip");}
    a.selectMode(false);
    if(a.formationIdle<0){check(!a.selectActionMode("formation")&&!a.formationMode,"A missing Formation_Idle cannot enter a fake formation mode");std::cout<<"Double-click/triple-click interactions and unavailable formation capability passed.\n";return 0;}
    a.preferences.rightDragRotation=false;a.applyPreferences();
    check(a.selectActionMode("formation",true),"Enter formation mode");
    check(a.formationMode&&!a.battleMode&&a.state.current==a.formationIdle,"Formation starts its actual Formation_Idle clip");
    if(argc>3){
        const auto out=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(out);const auto saved=a.ground.position;const float savedZoom=a.zoom;
        if(argc>5)a.setZoom(std::wcstof(argv[5],nullptr));
        for(const auto& mode:modes){
            a.selectActionMode(mode);foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);
            App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);pointer(a.ground.width*.5f,a.ground.height-3.f);App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);tick();
            App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick();
            const auto foot=a.ground.screen(a.ground.position);const auto offset=a.ground.bodyOffsets;
            std::cout<<"EDGE "<<mode<<" foot="<<foot.x<<","<<foot.y<<" desktop="<<a.ground.width<<","<<a.ground.height<<" offsets="<<offset.left<<","<<offset.top<<","<<offset.right<<","<<offset.bottom<<"\n";
            float nearestBottom=INFINITY;
            for(double fraction:{0.,.25,.5,.75,1.}){
                a.state.time=a.state.clips[a.state.current].duration*fraction;a.presentState(true);
                const auto file=out/(wide(mode)+L"-edge-"+std::to_wstring(int(fraction*100))+L".png");
                a.renderer->render(a.window,a.inputWindow,true,file);a.commitRender();
                const auto path=utf8(file.wstring());Image image=LoadImage(path.c_str());check(image.data,"Read rendered edge regression frame");auto* pixels=LoadImageColors(image);int bottom=-1;
                // Ignore isolated subpixel outline remnants of collapsed props;
                // measure a visible row of the character instead of a stray dot.
                for(int y=0;y<image.height;++y){int count=0;for(int x=0;x<image.width;++x)count+=pixels[y*image.width+x].a>=16;if(count>=8)bottom=y;}
                UnloadImageColors(pixels);UnloadImage(image);check(bottom>=0,"Edge frame contains the actual visible character");
                const float gap=a.work.bottom-(a.renderer->state["surfaceOrigin"][1].get<int>()+a.renderer->surfaceOffset.y+bottom+1);
                nearestBottom=std::min(nearestBottom,gap);std::cout<<"EDGE visible "<<mode<<" time="<<fraction<<" gap="<<gap<<"\n";
            }
            if(argc>4)check(nearestBottom>=-3&&nearestBottom<=std::wcstof(argv[4],nullptr),"Dragging to the bottom must not reserve invisible collapsed geometry below the character");
        }
        a.setZoom(savedZoom);a.ground.position=saved;a.selectActionMode("formation",true);
    }
    const auto anchor=a.ground.position;int firstFormationIdle=a.formationIdle;pointer(1,1);a.yaw=90;tick(2500);a.presentState(true);
    check(Vector3Distance(anchor,a.ground.position)<1e-6&&!a.state.walking&&!a.directed.active&&!a.state.trackingCursor,"Formation never wanders, follows, returns home or tracks the cursor");
    check(a.yaw==0&&a.renderer->state["yaw"]==0,"Formation remains screen-facing");
    a.clickSequence.reset();click();check(a.formationMode&&!a.manualAction&&a.state.current==firstFormationIdle,"A formation single click keeps the selected idle set");click();
    if(a.manualSkill>=0){
        check(a.manualAction&&a.formationMode&&a.state.current==a.manualSkill,"Formation double click starts Exs while retaining its mode");
        pointer(20,20);tick(10);a.presentState(true);
        check(a.renderer->state["yaw"]==90&&!a.state.trackingCursor&&Vector3Distance(anchor,a.ground.position)<1e-6,"Formation Exs retains its release heading and never follows the cursor");
        a.state.time=a.state.clips[a.manualSkill].duration-.001;tick();a.presentState(true);
        check(!a.manualAction&&a.formationMode&&a.state.current==firstFormationIdle&&a.renderer->state["yaw"]==0,"Completed formation Exs restores the same idle set and its front-facing heading");
        a.startManualSkill();foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);
        App::inputProc(a.inputWindow,WM_LBUTTONDOWN,MK_LBUTTON,0);interactionCursor.x+=12;App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_LBUTTON,0);tick();
        check(!a.manualAction&&a.state.dragging&&a.state.current==(a.formationPickup>=0?a.formationPickup:a.formationIdle),"Formation pickup interrupts Exs with the matching pickup clip");
        App::inputProc(a.inputWindow,WM_LBUTTONUP,0,0);tick();
    }else check(!a.manualAction&&a.state.current==firstFormationIdle,"Formation without Exs keeps its idle on double click");
    a.clickSequence.reset();foot=a.ground.screen(a.ground.position);pointer(foot.x,foot.y);click();click();click();
    check(a.modeName()==std::string("cafe")&&!a.manualAction,"Formation triple click interrupts Exs and wraps back to cafe");
    check(a.selectActionMode("formation",true),"Return to formation after the gesture cycle");firstFormationIdle=a.formationIdle;
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
    a.preferences.rightDragRotation=true;a.applyPreferences();
    const auto rotateFormation=[&]{App::inputProc(a.inputWindow,WM_RBUTTONDOWN,MK_RBUTTON,0);interactionCursor.x+=40;App::inputProc(a.inputWindow,WM_MOUSEMOVE,MK_RBUTTON,0);App::inputProc(a.inputWindow,WM_RBUTTONUP,0,0);tick();a.presentState(true);};
    const auto rotationAnchor=a.ground.position;rotateFormation();tick(100);a.presentState(true);
    check(std::abs(a.yaw-28)<.01&&std::abs(a.renderer->state["yaw"].get<float>()-28)<.01&&Vector3Distance(a.ground.position,rotationAnchor)<1e-6,"Enabled global rotation overrides formation's front lock without enabling movement or tracking");
    if(a.manualSkill>=0){a.startManualSkill();rotateFormation();check(a.manualAction&&a.formationMode&&std::abs(a.yaw-56)<.01&&std::abs(a.renderer->state["yaw"].get<float>()-146)<.01,"Formation Exs can rotate while playing");a.state.time=a.state.clips[a.manualSkill].duration-.001;tick();check(!a.manualAction&&std::abs(a.yaw-56)<.01,"Formation idle retains the manually rotated Exs heading");}
    a.preferences.rightDragRotation=false;a.applyPreferences();tick();check(a.yaw==0,"Disabling rotation restores formation's default front-facing idle");
    const auto zoom=a.zoom;const int zoomIdle=a.formationIdle;const auto zoomAnchor=a.ground.position;
    App::inputProc(a.inputWindow,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);a.presentState(true);
    check(a.zoom>zoom&&a.formationMode&&a.state.current==zoomIdle&&a.renderer->state["yaw"]==0,"Formation wheel up enlarges the current idle without changing mode or heading");
    check(Vector3Distance(zoomAnchor,a.ground.position)<1e-6,"Formation zoom preserves its placement away from screen boundaries");
    App::inputProc(a.inputWindow,WM_MOUSEWHEEL,MAKEWPARAM(0,SHORT(-WHEEL_DELTA)),0);check(std::abs(a.zoom-zoom)<1e-5,"Formation wheel down restores the previous size");
    for(int i=0;i<20;++i)App::inputProc(a.inputWindow,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);
    check(std::abs(a.zoom-1.65f)<1e-5,"Formation wheel uses the standard maximum zoom");
    for(int i=0;i<30;++i)App::inputProc(a.inputWindow,WM_MOUSEWHEEL,MAKEWPARAM(0,SHORT(-WHEEL_DELTA)),0);
    check(std::abs(a.zoom-.45f)<1e-5,"Formation wheel uses the standard minimum zoom");a.setZoom(zoom);
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
