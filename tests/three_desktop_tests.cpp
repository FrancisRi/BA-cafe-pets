#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#undef PlaySound
#include "pet_battle.h"
#include "pet_ground.h"
#include "pet_battle_input.h"
#include "stop_turn.h"
#include "foot_stop.h"
#include "desktop_geometry.h"
#include "directed_walk.h"
#include "desktop_path.h"
#include "window_boundaries.h"
#include "rlgl.h"
#include <iostream>
#include <stdexcept>
#include <behaviortree_cpp/contrib/json.hpp>
using Json=nlohmann::json;
#include "three_preferences.h"
#include "input_activity.h"
#include "cafe_attention.h"
#include "scene_state.h"
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    pet::Playback typing;typing.idleSecondsOverride=1;typing.inputActive=false;
    for(int frame=0;frame<600;++frame)check(!observeDebouncedInput(typing,(frame%90)/60.0,1.0/60,2),"Typing gaps must not toggle the inactive/resume state");
    check(typing.inputEpoch==1&&typing.activeInputSeconds>9&&!typing.noInputState,"Debounce must retain a single continuous input episode");
    check(!observeDebouncedInput(typing,3,0.01,2)&&typing.noInputState&&!typing.inputActive,"A genuine pause must eventually become inactive");
    check(observeDebouncedInput(typing,0,0.01,2)&&!observeDebouncedInput(typing,0.1,0.01,2),"Input recovery must fire exactly once");
    const auto seconds=typing.activeInputSeconds;observeDebouncedInput(typing,0,2.8,2);
    check(typing.activeInputSeconds>=seconds+2.8,"Input timing must retain elapsed time across slow frames");
    check(modalCharacterZoom({0.8f,1.2f,1.2f,0.8f,1.2f})==1.2f,"Imports must use the most common displayed zoom");
    check(modalCharacterZoom({0.8f,1.2f})==0.8f,"Modal zoom ties must be deterministic");
    SceneRoster historyCap;for(int i=0;i<15;++i)check(historyCap.remember({std::to_string(i),"C:/pet.glb","Pet","cafe",false}),"History must allow fifteen entries");
    check(!historyCap.remember({"16","C:/new.glb","New","cafe",false})&&historyCap.entries.size()==15,"Sixteenth history entry must be rejected without deleting existing history");
    SceneRoster roster;for(int i=0;i<6;++i)roster.entries.push_back({std::to_string(i),"C:/character"+std::to_string(i)+".glb","Character","cafe",false});
    for(int i=0;i<5;++i)check(roster.enable(std::to_string(i)),"Five actors must be selectable");
    check(!roster.enable("5")&&roster.activeCount()==5,"A sixth active actor must be rejected");
    roster.find("2")->active=false;check(roster.enable("5")&&roster.entries.size()==6,"Closed actors stay in history and free a slot");
    for(auto& e:roster.entries)e.active=e.id=="5";
    check(roster.last("5")&&!roster.last("0"),"Confirmation must apply only to the last active actor");
    const auto history=std::filesystem::temp_directory_path()/("three-history-test-"+std::to_string(GetCurrentProcessId())+".json");
    roster.save(history);SceneRoster savedRoster;savedRoster.load(history);std::filesystem::remove(history);
    check(savedRoster.json()==roster.json(),"History, displayed flags and each actor's mode must persist independently of shared parameters");
    check(segmentDistance({0,0,0},{10,0,0},{5,0,2})==2&&segmentDistance({0,0,0},{1,0,0},{2,0,0})==1,"Avoidance must consider the entire swept path, not just its endpoint");
    const float bodyRadius=characterCollisionRadius(0.633f,2.3f,1);
    check(bodyRadius>0.633f*0.55f*1.5f,"Collision volume must include more than the narrow shoulder-width follow unit");
    check(std::abs(characterCollisionRadius(0.633f,2.3f,1.65f)/characterCollisionRadius(0.633f,2.3f,0.45f)-1.65f/0.45f)<0.00001f,"Collision volume must scale with model zoom");
    rlSetClipPlanes(PetGround::NearPlane,PetGround::FarPlane);
    ThreePreferences preferences;auto values=preferences.json();values["follow"]=true;values["skillProbability"]=0;values["followNear"]=2;values["followFar"]=3.5;
    check(preferences.apply(values),"Accept valid web settings including zero percent chance");
    const auto original=preferences.json();values["moveInterval"]=0;
    check(!preferences.apply(values)&&preferences.json()==original,"Invalid web settings must be rejected atomically");
    values=original;values["follow"]=1;check(!preferences.apply(values),"Reject nonboolean web toggles");
    values=original;values["followNear"]=4;check(!preferences.apply(values)&&preferences.json()==original,"Reject inverted near/far limits atomically");
    values=original;values.erase("inputGrace");values["cafeLookMode"]="threshold";
    check(preferences.apply(values)&&preferences.inputGrace==3&&preferences.cafeLookMode=="threshold","Debounce is fixed at 3s and omitted from the form; turn mode is configurable");
    preferences.apply(original);
    pet::Playback attention;attention.inputActive=true;attention.trackingEnabled=true;attention.inputEpoch=1;
    attention.activeSecondsOverride=2;attention.attentionProbabilityOverride=0;attention.lookTarget=true;attention.lookYaw=60;
    CafeAttention gaze;float facing=0;
    attention.activeInputSeconds=1;gaze.update(attention,facing,0.01,true,false);check(facing==0,"Cafe gaze must wait for continuous input");
    attention.activeInputSeconds=2;gaze.update(attention,facing,0.01,true,false);check(facing>0&&!gaze.turning,"Cafe tracking must start after threshold even when legacy probability is zero");
    gaze.update(attention,facing,0.01,true,false,false);check(!gaze.chosen&&!gaze.turning,"Cafe toggle must stop tracking immediately");
    attention.trackingEnabled=false;gaze.update(attention,facing,0.01,true,false,true);
    check(gaze.chosen,"Cafe tracking must work independently of the battle look switch");
    gaze.reset();facing=0;attention.lookYaw=29;gaze.update(attention,facing,0.01,true,true);
    check(facing==0&&!gaze.turning,"Threshold gaze must ignore direction gaps below 30 degrees");
    attention.lookYaw=30;gaze.update(attention,facing,0.01,true,true);
    check(gaze.turning&&facing>0,"A 30 degree gap must start a walking turn");
    attention.lookYaw=-60;for(int frame=0;frame<5;++frame)gaze.update(attention,facing,0.01,true,true);
    check(facing>10,"An active turn must keep its committed target instead of reversing with cursor jitter");
    attention.inputActive=false;gaze.update(attention,facing,0.01,true,true);check(!gaze.turning&&!gaze.chosen,"Input pause must stop gaze and its walk override");
    const auto ini=std::filesystem::temp_directory_path()/("three-prefs-test-"+std::to_string(GetCurrentProcessId())+".ini");
    preferences.save(ini.wstring());ThreePreferences restored;restored.load(ini.wstring());std::filesystem::remove(ini);
    check(restored.json()==original,"Every new web parameter must survive an INI roundtrip");
    PetGround ground;ground.width=3840;ground.height=1080;ground.measuredBounds=true;
    ground.monitors={{0,0,1920,1080},{1920,0,3840,1080}};
    // Presentation must not mutate placement, even across many frames.
    ground.measuredBounds=false;ground.place({960,750});
    const auto resting=ground.position;
    for(int frame=0;frame<100;++frame){
        const auto crop=ground.crop();
        check(crop.x>=0&&crop.y>=0,"Stable viewport must stay within the desktop");
        check(ground.position.x==resting.x&&ground.position.z==resting.z,"Idle presentation must not move the ground anchor");
    }
    ground.measuredBounds=true;DesktopGeometry geometry;
    geometry.minimum={-1.2f,-0.05f,-1.2f};geometry.maximum={1.2f,2.4f,1.2f};
    geometry.renderMinimum={-3,-1.2f,-3};geometry.renderMaximum={3,3.2f,3};
    for(float zoom:{0.45f,1.0f,1.65f}){
        geometry.prepareViewport(ground,zoom);const auto width=geometry.width,height=geometry.height;
        for(Vector2 requested:std::vector<Vector2>{{1920,720},{0,0},{3840,0},{1920,1080},{700,400}}){
            geometry.place(ground,requested,zoom);
            const auto target=geometry.idleFoot(ground,zoom);ground.position=ground.floorAt(target);geometry.measure(ground,zoom);
            const auto foot=ground.screen(ground.position);const auto b=ground.bodyOffsets;
            bool inside=false;for(const auto& a:ground.monitors)inside|=foot.x+b.left>=a.left-1&&foot.x+b.right<=a.right+1&&foot.y+b.top>=a.top-1&&foot.y+b.bottom<=a.bottom+1;
            check(inside,"Resting footprint must fit completely on one monitor at every zoom");
            const auto again=geometry.idleFoot(ground,zoom);
            check(std::hypot(again.x-foot.x,again.y-foot.y)<1,"Docking must converge without repeated corrections");
            geometry.update(ground,zoom);
            const auto low=geometry.minimum,high=geometry.maximum;geometry.minimum=geometry.renderMinimum;geometry.maximum=geometry.renderMaximum;geometry.measure(ground,zoom);
            const auto render=ground.bodyOffsets;
            check(foot.x+render.left>=geometry.crop.x&&foot.x+render.right<=geometry.crop.x+width&&foot.y+render.top>=geometry.crop.y&&foot.y+render.bottom<=geometry.crop.y+height,"Prepared native viewport must contain the animated envelope after zoom");
            geometry.minimum=low;geometry.maximum=high;geometry.measure(ground,zoom);
            const auto anchor=ground.position;
            for(int frame=0;frame<120;++frame)geometry.update(ground,zoom);
            check(geometry.width==width&&geometry.height==height&&ground.position.x==anchor.x&&ground.position.z==anchor.z,"Idle frames must never resize the window or move its ground anchor");
        }
        geometry.place(ground,{960,0},zoom);const auto foot=ground.screen(ground.position);
        check(std::abs(foot.y+ground.bodyOffsets.top)<1,"Dragging must reach the screen top using projected head bounds");
    }
    ground.position=ground.floorAt({1920,750});geometry.measure(ground,1);
    DirectedWalk obstruction;float obstructionYaw=67;const auto stationary=ground.position;
    obstruction.begin(ground.step(90,2),true,1,obstructionYaw);
    obstruction.step(ground,1,0.01,obstructionYaw,[](float,double){return NAN;});
    check(!obstruction.active&&obstruction.blocked&&obstructionYaw==67&&ground.position.x==stationary.x&&ground.position.z==stationary.z,
        "A blocked destination must be abandoned immediately without a facing change");
    DirectedWalk route;float routeYaw=180;route.begin(ground.floorAt(geometry.idleFoot(ground,1)),false,1,routeYaw);
    for(int frame=0;frame<1800&&route.active;++frame)route.step(ground,1.98,1.0/125,routeYaw);
    check(!route.active,"Single-monitor docking must arrive instead of orbiting its endpoint");
    const auto docked=ground.position;for(int i=0;i<100;++i)geometry.update(ground,1);
    check(ground.position.x==docked.x&&ground.position.z==docked.z,"Docked Idle position must stay fixed");
    for(float zoom:{0.45f,1.0f,1.65f})for(double speed:{0.792,2.376})for(double dt:{1.0/60,1.0/125}){
        ground.monitors={{0,0,1920,1080},{1980,0,3840,1080}};ground.position=ground.floorAt({1350,750});geometry.measure(ground,zoom);
        const auto target=ground.floorAt({2350,750});const double dx=target.x-ground.position.x,dz=target.z-ground.position.z;
        const float heading=std::atan2(dx,dz)*RAD2DEG;const double length=std::hypot(dx,dz);
        const auto beforePlan=ground.position;const auto beforeBounds=ground.bodyOffsets;
        check(DesktopPath::trip(ground,geometry,zoom,heading,length),"A complete cross-screen trip with a small monitor gap must be accepted");
        check(DesktopPath::crossing(ground,geometry,zoom,target),"A trip to a different resting monitor must commit to the crossing");
        check(ground.position.x==beforePlan.x&&ground.position.z==beforePlan.z&&ground.bodyOffsets.left==beforeBounds.left,"Planning must never change the model anchor or active bounds");
        routeYaw=heading;route.begin(target,true,2,routeYaw);route.crossing=true;int gapFrames=0;
        for(int frame=0;frame<9000&&route.active;++frame){
            const auto previous=ground.position;
            route.step(ground,speed*zoom,dt,routeYaw,[&](float angle,double travel){return DesktopPath::stepAllowed(ground,angle,travel)?angle:NAN;});
            check(std::hypot(ground.position.x-previous.x,ground.position.z-previous.z)>0,"Accepted Cafe/Battle crossings must not stop at the monitor edge or gap");
            const auto foot=ground.screen(ground.position);gapFrames+=foot.x>1920&&foot.x<1980;
        }
        check(!route.active&&gapFrames>0&&DesktopPath::restingMonitor(ground,geometry,zoom,ground.position)==1,"A crossing must finish already on one monitor without a second docking walk");
        const auto foot=ground.screen(ground.position),settled=geometry.idleFoot(ground,zoom);
        check(std::hypot(foot.x-settled.x,foot.y-settled.y)<1,"No Idle correction may restart movement after crossing");
    }
    ground.position=ground.floorAt({1350,750});geometry.measure(ground,1);
    auto tripTo=[&](Vector2 pixel){const auto p=ground.floorAt(pixel);return DesktopPath::trip(ground,geometry,1,std::atan2(p.x-ground.position.x,p.z-ground.position.z)*RAD2DEG,std::hypot(p.x-ground.position.x,p.z-ground.position.z));};
    check(!tripTo({1940,750})&&!tripTo({2020,750}),"Do not begin trips whose stop would lie in a gap or straddle two screens");
    ground.monitors={{0,0,1920,1080},{2400,0,3840,1080}};
    check(!tripTo({2700,750}),"A large invisible monitor gap must be rejected before walking starts");
    check(!tripTo({3820,750}),"A trip beyond the outer resting envelope must be rejected before walking starts");
    ground.monitors={{0,0,1920,1080},{1920,0,3840,1080}};
    for(float gap:{3.0f,4.0f,5.0f}){
        const auto target=DirectedWalk::followTarget({100,0},{0,0},50,gap);
        check(std::abs(target.x-50*gap)<0.01f&&!DirectedWalk::outsideBand(target,{0,0},50,gap-0.5f,gap+0.5f),"Follow distance must be expressed in visible body widths");
        check(DirectedWalk::outsideBand({100,0},{0,0},50,gap-0.5f,gap+0.5f)&&DirectedWalk::outsideBand({350,0},{0,0},50,gap-0.5f,gap+0.5f),"Following must retreat when too close and approach when too far");
    }
    check(!DirectedWalk::outsideBand({150,0},{0,0},50,3,3),"Equal nearest/farthest values must allow an exact target without jitter");
    for(double velocity:{0.792,1.98})for(double dt:{1.0/60,1.0/125}){
        ground.measuredBounds=false;ground.position=ground.floorAt({1850,750});routeYaw=90;
        route.begin(ground.floorAt({2250,750}),true,1,routeYaw);int jumps=0,crossings=0;bool landed=false;
        for(int frame=0;frame<2400&&route.active;++frame){
            const auto before=ground.screen(ground.position);const bool wasJumping=route.jumping;
            if(route.planJump(ground,velocity,1,routeYaw,dt))++jumps;
            route.step(ground,velocity,dt,routeYaw);const auto after=ground.screen(ground.position);
            if(before.x<1920&&after.x>=1920){++crossings;check(route.jumping&&std::abs(route.jumpTime-0.5)<dt+0.001,"Following must cross the monitor border at the Jump midpoint");}
            if(wasJumping)check(std::hypot(after.x-before.x,after.y-before.y)>0,"Following Jump must keep translating on every update");
            landed|=wasJumping&&!route.jumping;
        }
        check(jumps==1&&crossings==1&&landed&&!route.active,"Following must play a single nonlooping Jump then finish walking at both mode speeds");
    }
    ground.position=ground.floorAt({960,750});route.begin(ground.floorAt({1100,750}),true,1,0);
    bool queried=false;check(!route.planJump(ground,1.98,0,0,0.01,[&](float,float){queried=true;return 0;})&&!queried,"Models without Jump must not query window boundaries");
    route.cancel();ground.measuredBounds=true;
    route.begin(ground.floorAt({800,700}),true,2,0);route.cancel();check(!route.active,"Manual interactions must cancel following immediately");
    auto curvedPath=[&](double dt){
        ground.position=ground.floorAt({960,750});StopTurn turn;turn.begin(0,90);
        double elapsed=0,previousYaw=0;
        while(elapsed<StopTurn::LeadSeconds-1e-9){
            const double step=std::min(dt,StopTurn::LeadSeconds-elapsed);
            const float heading=float(turn.advance(step));
            check(turn.yaw>=previousYaw&&turn.yaw<=90,"Stop turn must progress without oscillating or overshooting");
            ground.position=ground.step(heading,float(1.98*step));
            previousYaw=turn.yaw;elapsed+=step;
        }
        check(std::abs(turn.yaw-90)<1e-8,"The final movement tangent must match the stopping direction");
        const auto ending=ground.position;
        for(int i=0;i<100;++i)check(std::abs(turn.advance(dt)-90)<1e-8,"Stopping must retain its frozen direction");
        check(ground.position.x==ending.x&&ground.position.z==ending.z,"Holding the stopped direction must not move the model");
        return ending;
    };
    const auto slow=curvedPath(1.0/30),fast=curvedPath(1.0/125);
    check(std::hypot(slow.x-fast.x,slow.z-fast.z)<0.002,"Curved stopping must be consistent across update rates");
    StopTurn wrap;wrap.begin(170,-170);wrap.advance(0.45);
    check(std::abs(wrap.yaw-190)<1e-8,"Direction wrap must take the short arc, not spin around");
    wrap.reset();check(!wrap.active,"Drag or other interruptions must cancel the stopping arc");
    FootStopProfile stop;
    check(stop.initialize(0.5,{1.98,1.2,0.6,0.1,0}),"Accept sampled foot-based speed profile");
    const double distance=stop.distanceAt(0.5);
    for(double dt:{1.0/30,1.0/60,1.0/125}){
        double traveled=0,elapsed=0;
        while(elapsed<1.5){const double next=std::min(1.5,elapsed+dt);
            traveled+=stop.distanceAt(next)-stop.distanceAt(elapsed);elapsed=next;}
        check(std::abs(traveled-distance)<1e-8,"Foot-based displacement must not depend on update frequency or clip tail length");
    }
    check(stop.distanceAt(1.5)==distance,"No extra translation after the feet have planted, even in a long Move_End");
    check(std::abs(stop.distanceAt(0.000001)/0.000001-1.98)<0.0001,"Foot profile must preserve incoming speed at its start");
    check(std::abs((stop.distanceAt(0.5)-stop.distanceAt(0.499999))/0.000001)<0.0001,"Foot profile must smoothly reach zero speed at its end");
    check(std::abs((stop.distanceAt(0.5)*1.65)/(stop.distanceAt(0.5)*0.45)-1.65/0.45)<1e-8,"Physical stopping distance must scale with model zoom");
    FootStopProfile invalid;
    check(!invalid.initialize(0.5,{1,2,0})&&!invalid.initialize(0.5,{1,0.5,0.2}),"Reject increasing speeds and nonzero ending speed");
    // Test actual desktop z-order with our own non-activating windows. Top-level
    // test windows use this process, so mark them as candidates explicitly.
    HWND lower=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE,L"STATIC",L"Pet visibility lower",WS_POPUP|WS_VISIBLE,40,40,200,150,nullptr,nullptr,nullptr,nullptr);
    HWND upper=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE,L"STATIC",L"Pet visibility upper",WS_POPUP|WS_VISIBLE,40,40,200,150,nullptr,nullptr,nullptr,nullptr);
    check(lower&&upper,"Create visibility test windows");
    VisibleWindowEdges edges;edges.refresh(nullptr,true);
    size_t lowerIndex=edges.entries.size();
    for(size_t i=0;i<edges.entries.size();++i)if(edges.entries[i].window==lower)lowerIndex=i;
    check(lowerIndex<edges.entries.size(),"Visible background window must be listed");
    check(!edges.exposed(lowerIndex,{100,100}),"Fully covered background window must not cause a jump");
    check(SetWindowPos(upper,HWND_TOPMOST,140,40,100,150,SWP_NOACTIVATE),"Move test covering window");
    UpdateWindow(upper);UpdateWindow(lower);
    MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
    DwmFlush();Sleep(100);edges.refresh(nullptr,true);
    for(size_t i=0;i<edges.entries.size();++i)if(edges.entries[i].window==lower)lowerIndex=i;
    check(edges.exposed(lowerIndex,{60,80})&&!edges.exposed(lowerIndex,{200,80}),"Only exposed portions of a partially covered frame may count");
    for(auto& entry:edges.entries)entry.application=entry.window==lower;
    ground.position=ground.floorAt({220,100});
    auto target=ground.floorAt({260,100});
    float maximum=std::hypot(target.x-ground.position.x,target.z-ground.position.z);
    check(!std::isfinite(edges.distance(ground,90,maximum,0,0,nullptr)),"Crossing a covered background edge must not schedule Jump");
    ground.position=ground.floorAt({20,100});target=ground.floorAt({70,100});maximum=std::hypot(target.x-ground.position.x,target.z-ground.position.z);
    check(std::isfinite(edges.distance(ground,90,maximum,0,0,nullptr)),"Crossing an exposed original frame edge must schedule Jump");
    ShowWindow(upper,SW_HIDE);edges.refresh(nullptr,true);
    bool hidden=false;for(const auto& e:edges.entries)hidden|=e.window==upper;
    check(!hidden,"Hidden windows must be excluded");
    ShowWindow(lower,SW_MINIMIZE);edges.refresh(nullptr,true);
    bool minimized=false;for(const auto& e:edges.entries)minimized|=e.window==lower;
    check(!minimized,"Minimized windows must be excluded");
    DestroyWindow(upper);DestroyWindow(lower);
    std::cout<<"PASS: fixed animated viewport, top dragging, one-monitor docking, following, persisted settings, foot stopping and visible window edges\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
