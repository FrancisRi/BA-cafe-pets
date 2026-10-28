#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <commctrl.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound
#include "raylib.h"
#include "rlgl.h"
#include "pet_behavior.h"
#include "pet_battle.h"
#include "pet_ground.h"
#include "native_input.h"
#include "pet_settings.h"
#include "stop_turn.h"
#include "foot_stop.h"
#include "desktop_geometry.h"
#include "directed_walk.h"
#include "desktop_path.h"
#include "window_boundaries.h"
#include "input_activity.h"
#include "cafe_attention.h"
#include <behaviortree_cpp/contrib/json.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <cstdio>
#include <climits>
#include <map>

using Json = nlohmann::json;
#include "scene_state.h"
double preciseSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
std::wstring wide(const std::string& s) {
    const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);
    std::wstring out(n,L' ');if(n)MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n);return out;
}
std::string utf8(const std::wstring& s) {
    const int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    std::string out(n,' ');if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr);return out;
}
std::string xmlEscape(std::string s) {
    std::string out;for(char c:s){if(c=='&')out+="&amp;";else if(c=='\"')out+="&quot;";else if(c=='<')out+="&lt;";else out+=c;}return out;
}
void replaceAll(std::string& text,const std::string& from,const std::string& to) {
    size_t p=0;while((p=text.find(from,p))!=std::string::npos){text.replace(p,from.size(),to);p+=to.size();}
}
#include "native_preferences.h"
#include "native_model_library.h"
#include "native_model_storage.h"
#include "native_autostart.h"
#include "native_model_import.h"
#include "native_health.h"
#include "native_session.h"
#include "native_loop.h"
#include "native_frame.h"
#include "native_pet_clicks.h"
#include "native_settings.h"
#include "native_renderer.h"
#include "native_tray.h"
#include "frame_jobs.h"
void nativeLog(int level,const char* text,va_list args){
    if(std::strstr(text,"Pixel data retrieved successfully"))return;
    FILE* out=level>=LOG_WARNING?stderr:stdout;std::fprintf(out,"[%d] ",level);std::vfprintf(out,text,args);std::fputc('\n',out);std::fflush(out);
}
struct GraphicsContext {
    GraphicsContext(){SetTraceLogCallback(nativeLog);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(64,64,"BA Cafe Pets OpenGL");if(!IsWindowReady())throw std::runtime_error("Cannot initialize raylib OpenGL");SetExitKey(KEY_NULL);SetTargetFPS(0);}
    ~GraphicsContext(){CloseWindow();}
};

struct App {
    static constexpr int Size=480;
    static constexpr double WalkSpeedMultiplier=1.2;
    HWND window{},inputWindow{}; std::filesystem::path directory,programDirectory,dataDirectory,model,pendingModel,snapshot;
    std::shared_ptr<NativeModelStorage> modelStorageSettings;
    std::shared_ptr<NativeAutostart> autostartSettings;
    std::string autostartError;
    NativeSession session;std::function<void()> sessionChanged;
    std::filesystem::path storage() const {return dataDirectory.empty()?directory:dataDirectory;}
    NativeModelLibrary modelLibrary() const {return NativeModelLibrary(modelStorageSettings->current);}
    std::filesystem::path bundledResource() const {
        const auto file=directory/L"model/Shiroko.glb";
        return std::filesystem::is_regular_file(file)?file:directory/L"models/Shiroko.glb";
    }
    std::optional<NativeModelLibrary> managedLibrary(const std::filesystem::path& file) const {
        if(modelStorageSettings->bundled(file,bundledResource()))return {};
        for(const auto& folder:modelStorageSettings->directories()){NativeModelLibrary library(folder);if(library.owns(file))return library;}return {};
    }
    void openModelsFolder(){
        const auto folder=modelLibrary().ensureDirectory();const auto result=ShellExecuteW(nativeSettings->dialogOwner(),L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        if(reinterpret_cast<INT_PTR>(result)<=32)throw std::runtime_error("无法打开模型文件夹，请检查文件夹是否可访问");
    }
    std::string reviewClip,initialMode;double reviewTime=0,runSeconds=0;
    bool review=false,fixedPose=false,sampleSpecified=false,done=false,loaded=false,frameReady=false,capturing=false;
    bool smooth=true,serialCpu=false,diagnostics=false;
    bool grouped=false,displayed=true,modelSpecified=false,dataDirectorySpecified=false,deferRendering=false,pendingRender=false;std::string actorId;
    std::shared_ptr<bool> alive=std::make_shared<bool>(true);
    std::function<void()> menuRequest,quickMenuRequest,closeRequest,loadedCallback,destroyedCallback,tickCallback,settingsChanged,syncRequest,followRequest,depthRequest;
    std::function<MovementChoice(float,double)> avoidHeading;
    CrowdSteering crowd;uint64_t crowdId=0;double movementDt=0.008;float walkIntentYaw=0;
    std::function<bool(float,double)> clearJump;
    pet::BattleInput sharedEvents{};
    NativePetClicks clickSequence;bool doubleClickRequest=false,tripleClickRequest=false,manualAction=false;
    std::unique_ptr<nativepet::NativeRenderer> renderer;bool pendingImport=false;double nextRender=0;uint64_t renderedFrames=0;
    pet::Playback state;std::unique_ptr<pet::Behavior> behavior;std::unique_ptr<PetSettings> settings;
    std::unique_ptr<pet::Battle> battle;NativeDesktopInput battleInput;
    bool battleMode=false,formationMode=false;
    PetGround ground;PetJourney journey;RECT work{};float primaryHeight=1,eyeHeight=1.7f,yaw=0,zoom=1;
    POINT down{},previous{};bool left=false,right=false,menuPressed=false,rightDragged=false,dragged=false,wasDragging=false,click=false,dragStarted=false;
    bool lookHotkey=false,quitHotkey=false;uint64_t walkGeneration=0;
    ULONGLONG started=0,readyAt=0;int exitCode=0;
    double lastTick=0,lastPost=0;HANDLE motionTimer{};
    std::function<double()> idleInputSeconds=[](){LASTINPUTINFO input{sizeof(input),0};return GetLastInputInfo(&input)?static_cast<DWORD>(GetTickCount()-input.dwTime)/1000.0:0.0;};
    Vector3 motionVelocity{};double motionRemaining=0;
    uint64_t retreatLogged=UINT64_MAX;
    uint64_t victoryLogged=UINT64_MAX,maskRevision=0;
    StopTurn stopTurn;VisibleWindowEdges visibleWindows;
    std::map<int,FootStopProfile> footStops;
    FootStopProfile fallbackStop;
    DesktopGeometry geometry;DirectedWalk directed;NativePreferences preferences;CafeAttention cafeAttention;
    std::unique_ptr<NativeSettings> nativeSettings;
    bool placementNeeded=true,settingsOnLoad=false,menuHintVisible=false;uint64_t placementGeneration=UINT64_MAX;
    double interactionUntil=0,followPlanAt=0;POINT followPlannedCursor{};bool hasFollowPlan=false;
    std::optional<Vector2> blockedCursor;
    std::optional<float> restFacing;
    pet::BattleInput deferredInput{};bool deferredCafeClick=false;
    std::string settingsError;
    int windowWidth=Size,windowHeight=Size,cafeIdle=-1,cafeWalk=-1,cafeJump=-1,formationIdle=-1,formationPickup=-1,manualSkill=-1;
    struct FormationPose {int idle=-1,pickup=-1;};std::vector<FormationPose> formationPoses;size_t formationPoseIndex=0;
    const char* modeName() const {return formationMode?"formation":battleMode?"battle":"cafe";}
    bool exsPlaying() const {return manualAction||(battleMode&&battle&&battle->phase==pet::Battle::Phase::Skill);}
    bool frontLocked() const {return formationMode||exsPlaying();}
    POINT windowPosition{LONG_MIN,LONG_MIN};
    POINT committedOrigin{};Vector2 committedFoot{};bool committedFrame=false;

    void post(const Json& value){if(renderer&&value.value("type",std::string{})=="state"){renderer->state=value;renderer->state["hintBelow"]=geometry.below;}}
    void renderFrame(bool force=false){
        if(!renderer||!renderer->model||!loaded||!displayed||!window)return;
        if(grouped&&deferRendering)return; // Scene owns the shared 60 Hz render deadline.
        const double now=preciseSeconds();if(!force&&now<nextRender)return;
        nextRender+=1.0/60;if(nextRender<=now)nextRender=now+1.0/60;
        presentState(true); // The image and HWND must use the same latest pose/anchor.
        if(deferRendering){pendingRender=true;return;}
        finishRender();
    }
    void finishRender(bool deferred=false){
        pendingRender=false;
        if(!renderer||!loaded||!displayed||!window)return;
        const auto before=renderer->sampling();
        renderer->render(window,inputWindow,false,{},deferred);if(!deferred)commitRender();
        if(renderer->sampling()!=before)syncSettings();
    }
    void commitRender(){
        if(!renderer||!loaded||!displayed||!window)return;
        renderer->present();frameReady=true;if(renderer->didDraw)++renderedFrames;
        RECT rect{};if(GetWindowRect(window,&rect)){windowPosition=committedOrigin={rect.left,rect.top};
            const auto& p=renderer->state.at("position");committedFoot=ground.screen({p[0],p[1],p[2]});committedFrame=true;}
    }
    const FootStopProfile& stopProfile(int clip) const {
        const int source=clip>=0?state.clips[clip].modelIndex:-1;
        const auto found=footStops.find(source);return found==footStops.end()?fallbackStop:found->second;
    }
    uint64_t displayGeneration() const {return state.generation*100000+directed.serial*2+(directed.active?1:0)+cafeAttention.serial*1000000000ULL;}
    void applyPreferences(){
        state.walkEnabled=preferences.wandering;
        state.walkIntervalOverride=preferences.moveInterval;state.walkTimeJitter=preferences.walkTimeJitter/100;state.walkDistanceJitter=preferences.walkDistanceJitter/100;
        state.walkDurationOverride=geometry.bodyWidth*preferences.moveBodies/(0.792*WalkSpeedMultiplier);
        state.attentionProbabilityOverride=1; // Legacy BT chance is disabled in this host; cafe tracking uses its toggle.
        if(battle){battle->configuredTimers=true;battle->activeWalkDistanceScale=0.5;battle->activeWalkAway=true;battle->bufferReloadRequests=true;battle->walkTimeJitter=state.walkTimeJitter;battle->walkDistanceJitter=state.walkDistanceJitter;battle->setMoveInterval(preferences.moveInterval);
            battle->moveSeconds=geometry.bodyWidth*preferences.moveBodies/(0.792*WalkSpeedMultiplier*3);
            battle->callsignProbability=preferences.clickProbability/100;battle->skillProbability=preferences.skillProbability/100;battle->keyboardIdleSeconds=preferences.keyboardIdle;}
    }
    Json settingsSnapshot() const {
        auto value=preferences.json();value.update({{"look",state.trackingEnabled},{"outline",state.outlineEnabled},
            {"mode",modeName()},{"idleSeconds",state.inactivityThreshold()},{"activeSeconds",state.activeSecondsOverride.value_or(30)}});
        const auto sampling=renderer?renderer->sampling():nativepet::NativeRenderer::Sampling{-1,1,0};
        Json startup={{"enabled",false},{"available",bool(autostartSettings)}};
        if(!autostartError.empty())startup["error"]=autostartError;
        try{if(autostartSettings)startup["enabled"]=autostartSettings->enabled();}catch(const std::exception& e){startup["error"]=e.what();}
        return {{"type","settings"},{"name",utf8(settings->title)},{"value",value},{"startup",startup},{"battleAvailable",settings->battleAvailable},
            {"renderSampling",{{"visible",displayed},{"ready",loaded&&sampling.quality==preferences.renderQuality},{"scale",sampling.scale},{"msaa",sampling.msaa}}},
            {"formationAvailable",formationIdle>=0},{"manualSkillAvailable",manualSkill>=0},
            {"canFollow",!formationMode&&(battleMode?battle&&battle->clips.move>=0:cafeWalk>=0)},{"busy",settings->importBusy},{"percent",settings->importPercent},
            {"detail",utf8(settings->progressDetail)},{"error",settingsError},{"modelsDirectory",utf8(modelLibrary().directory.wstring())},{"modelsDirectoryDefault",NativeModelStorage::same(modelLibrary().directory,modelStorageSettings->defaultDirectory())}};
    }
    void syncSettings(){if(syncRequest)syncRequest();else if(nativeSettings)nativeSettings->sync();}
    void dismissMenuHint(){
        menuHintVisible=false;
        if(!WritePrivateProfileStringW(L"UI",L"MenuHintDismissed",L"1",settings->ini.c_str()))
            state.log("WARNING: Cannot persist menu hint dismissal");
        presentState(true);
    }
    void stopLocomotion(){
        crowd.reset();
        directed.cancel();cafeAttention.reset();restFacing.reset();stopTurn.reset();journey.route.clear();
        state.walking=false;state.walkRemaining=0;
        state.returnRequested=state.returningHome=state.returnArrived=false;
        motionVelocity={};motionRemaining=0;
    }
    void stopBlocked(){
        stopLocomotion();
        POINT cursor{};GetCursorPos(&cursor);blockedCursor=Vector2{float(cursor.x),float(cursor.y)};
        placementNeeded=false;state.resetCooldown();
        if(battleMode&&battle)battle->stopMovement();
        else if(behavior&&cafeIdle>=0){behavior->halt();state.start(cafeIdle,true);}
        placementGeneration=state.generation;
        state.log("MOVEMENT blocked: destination abandoned");
    }
    void setZoom(float value){
        const auto anchor=ground.position;const float facing=yaw;
        const bool moving=state.walking||directed.active||state.returningHome;
        if(moving){stopLocomotion();if(battleMode&&battle)battle->stopMovement();
            else if(behavior&&cafeIdle>=0){behavior->halt();state.start(cafeIdle,true);}}
        motionVelocity={};motionRemaining=0;blockedCursor.reset();
        zoom=std::clamp(value,0.45f,1.65f);ground.modelZoom=zoom;++maskRevision;
        yaw=facing;state.resetCooldown();updatePlacementShape();
        ground.position=anchor;
        const auto foot=ground.screen(anchor),target=geometry.idleFoot(ground,zoom);
        if(std::hypot(target.x-foot.x,target.y-foot.y)>1)ground.position=ground.floorAt(target);
        geometry.measure(ground,zoom);placementNeeded=false;placementGeneration=state.generation;
        interactionUntil=preciseSeconds()+0.5;presentState(true);
        if(settingsChanged)settingsChanged();
    }
    bool moveGround(float heading,float distance){
        const auto choice=avoidHeading?avoidHeading(heading,distance):MovementChoice{heading};const float steered=choice.heading;
        if(!std::isfinite(steered))return false;
        if(choice.avoiding)yaw=steered;
        distance*=float(choice.scale);
        if(!DesktopPath::stepAllowed(ground,steered,distance))return false;
        ground.position=ground.step(steered,distance);return true;
    }
    std::pair<double,double> brakeRange() const {
        double minimum=INFINITY,maximum=0;
        if(battle)for(const auto& p:battle->clips.poses){const auto& profile=stopProfile(p.moveEnd);const double d=profile.distanceAt(profile.stopTime)*zoom;minimum=std::min(minimum,d);maximum=std::max(maximum,d);}
        return {std::isfinite(minimum)?minimum:0,maximum};
    }
    double activeTurnLead() const {return battle&&battle->activeInputMove?std::min(StopTurn::LeadSeconds,battle->moveDuration*0.35):StopTurn::LeadSeconds;}
    double earliestFootPlant() const {
        double result=INFINITY;if(battle)for(const auto& pose:battle->clips.poses)result=std::min(result,stopProfile(pose.moveEnd).stopTime);
        return std::isfinite(result)?result:pet::Battle::StopFallbackSeconds;
    }
    bool activeCurveFits(float heading,float desired,double lead,double turnSeconds) const {
        if(!battle)return false;
        for(const auto& pose:battle->clips.poses){
            auto preview=ground;const auto& profile=stopProfile(pose.moveEnd);StopTurn turn;turn.begin(heading,desired,turnSeconds);
            const double seconds=lead+profile.stopTime;double time=0;
            while(time<seconds-1e-9){const double next=std::min(seconds,time+1.0/60);
                const double straight=std::max(0.0,std::min(next,lead)-std::min(time,lead));
                const double braking=profile.distanceAt(std::max(0.0,next-lead))-profile.distanceAt(std::max(0.0,time-lead));
                const float distance=float(straight*0.792*WalkSpeedMultiplier*zoom*3+braking*zoom);
                const float angle=float(turn.advance(next-time));if(!DesktopPath::stepAllowed(preview,angle,distance))return false;
                preview.position=preview.step(angle,distance);time=next;
            }
            if(DesktopPath::restingMonitor(preview,geometry,zoom,preview.position)<0||!DesktopPath::covered(ground,preview.position))return false;
        }return true;
    }
    bool canStartWalk(double heading,double requested){
        if(!loaded)return ground.canWalk(float(heading),float(requested));
        double distance=requested,minimum=0,maximum=0;
        if(!battleMode&&smooth){distance=std::max(0.0,requested-0.792*WalkSpeedMultiplier*zoom*(pet::Battle::StopFallbackSeconds*0.25+0.02));maximum=requested-distance;}
        if(battleMode&&battle){const double seconds=battle->moveSeconds.value_or(4)*(state.inputActive?battle->activeWalkDistanceScale:1)*(1+state.walkDistanceJitter);
            distance=std::max(distance,0.792*WalkSpeedMultiplier*zoom*3*seconds);const auto brakes=brakeRange();minimum=brakes.first;maximum=brakes.second;}
        if(!DesktopPath::trip(ground,geometry,zoom,float(heading),distance,minimum,maximum))return false;
        if(battleMode&&battle&&battle->activeWalkAway&&state.inputActive&&state.lookTarget&&battle->moveDuration>0){
            const double lead=std::min(StopTurn::LeadSeconds,battle->moveDuration*0.35);
            const auto saved=ground.position;ground.position=ground.step(float(heading),float(std::max(0.0,distance-0.792*WalkSpeedMultiplier*zoom*3*lead)));
            const bool safe=activeCurveFits(float(heading),float(state.lookYaw),lead,lead+earliestFootPlant());ground.position=saved;
            if(!safe)return false;
        }
        return true; // Moving peers are handled locally; only Jump reserves its entire path.
    }
    bool beginDirected(Vector3 target,bool following){
        const double dx=target.x-ground.position.x,dz=target.z-ground.position.z,remaining=std::hypot(dx,dz);
        const float heading=std::atan2(dx,dz)*RAD2DEG;
        if(!DesktopPath::trip(ground,geometry,zoom,heading,remaining))return false;
        directed.begin(target,following,state.generation,yaw);directed.crossing=DesktopPath::crossing(ground,geometry,zoom,target);return true;
    }
    void settingsMessage(const Json& message){
        if(message.value("type","")=="drag"){ReleaseCapture();SendMessageW(nativeSettings->window,WM_NCLBUTTONDOWN,HTCAPTION,0);return;}
        const auto type=message.value("type",std::string{});
        if(type=="ready"){syncSettings();state.log("SETTINGS web ready");return;}
        if(type=="close"){nativeSettings->hide();return;}
        if(type=="exit"){DestroyWindow(window);return;}
        if(type=="open_models_folder"){
            try{openModelsFolder();settingsError.clear();
            }catch(const std::exception& error){settingsError=error.what();}syncSettings();return;
        }
        if(type=="import"){
            NativeHealth::Pause choosing;
            try{for(const auto& file:NativeModelPicker::choose(nativeSettings->dialogOwner()))importLocalModel(file);}
            catch(const std::exception& failure){settingsError=failure.what();syncSettings();}return;
        }
        if(type!="save")return;
        const auto& value=message.at("value");auto candidate=preferences;
        bool valid=candidate.apply(value);
        for(const char* key:{"look"})valid&=value.contains(key)&&value[key].is_boolean();
        for(const char* key:{"idleSeconds","activeSeconds"})valid&=value.contains(key)&&value[key].is_number()&&std::isfinite(value[key].get<double>())&&value[key].get<double>()>=1&&value[key].get<double>()<=3600;
        const auto mode=value.value("mode",std::string{});valid&=mode=="cafe"||(mode=="battle"&&settings->battleAvailable)||(mode=="formation"&&formationIdle>=0);
        if(!valid){settingsError="设置值超出允许范围，未保存";syncSettings();return;}
        const bool startFollow=!preferences.follow&&candidate.follow;
        if(candidate.cafeLookMode!=preferences.cafeLookMode)cafeAttention.pause();
        preferences=candidate;if(startFollow){blockedCursor.reset();followPlanAt=0;interactionUntil=0;}state.trackingEnabled=value["look"];state.outlineEnabled=preferences.outlineMode!=0;
        state.idleSecondsOverride=value["idleSeconds"];state.activeSecondsOverride=value["activeSeconds"];
        if(mode!=modeName())selectActionMode(mode,false);
        if(!directed.jumping)directed.cancel();restFacing.reset();placementNeeded=true;state.resetCooldown();applyPreferences();settings->save();preferences.save(settings->ini);
        settingsError.clear();syncSettings();state.log("SETTINGS saved: follow="+std::to_string(preferences.follow)+" interval="+std::to_string(preferences.moveInterval));
        if(settingsChanged)settingsChanged();
    }
    void showSettings(){if(menuRequest){menuRequest();return;}nativeSettings->show(window,directory);settings->window=nativeSettings->window;}
    void showQuickMenu(){if(quickMenuRequest){quickMenuRequest();return;}nativeSettings->showQuick(window,directory);}
    void refreshDisplays(){
        std::vector<RECT> areas;EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM p)->BOOL{
            MONITORINFO i{sizeof(i),{},{},0};if(GetMonitorInfoW(m,&i))reinterpret_cast<std::vector<RECT>*>(p)->push_back(i.rcWork);return TRUE;
        },reinterpret_cast<LPARAM>(&areas));
        if(areas.empty())throw std::runtime_error("No desktop monitor");work=areas.front();
        if(primaryHeight<=1)primaryHeight=static_cast<float>(work.bottom-work.top);
        for(const auto& a:areas){work.left=std::min(work.left,a.left);work.top=std::min(work.top,a.top);work.right=std::max(work.right,a.right);work.bottom=std::max(work.bottom,a.bottom);}
        ground.width=work.right-work.left;ground.height=work.bottom-work.top;
        const float scale=ground.height/primaryHeight;ground.camera.position={0,29.789228f*scale,140.147298f*scale};
        // Keep raylib's unprojection as precise as the Three.js camera. Its
        // default 0.01 near plane loses precision at our ~143-unit distance.
        rlSetClipPlanes(PetGround::NearPlane,PetGround::FarPlane*std::max(1.0f,scale));
        ground.monitors.clear();for(const auto& a:areas)ground.monitors.push_back({float(a.left-work.left),float(a.top-work.top),float(a.right-work.left),float(a.bottom-work.top)});
    }
    void progress(int percent,const std::wstring& label){
        settings->importPercent=percent;settings->progressTitle=L"直接加载 GLB";settings->progressDetail=label;
        syncSettings();
    }
    void requestModel(const std::filesystem::path& file){
        if(!std::filesystem::is_regular_file(file))throw std::runtime_error("Model file does not exist");
        pendingModel=std::filesystem::absolute(file);done=false;settingsError.clear();settings->importBusy=true;settings->importStarted=GetTickCount64();progress(0,L"读取原始 GLB");pendingImport=true;
        state.log("LOAD original GLB: "+utf8(pendingModel.wstring()));
    }
    void importLocalModel(const std::filesystem::path& file){
        NativeHealth::Pause saving;
        try{requestModel(modelLibrary().store(file));}catch(const std::exception& error){settingsError=error.what();syncSettings();}
    }
    bool selectMode(bool enabled,bool persist=false){return selectActionMode(enabled?"battle":"cafe",persist);}
    void chooseFormationPose(bool different){
        if(formationPoses.empty())return;
        const size_t size=formationPoses.size();
        formationPoseIndex=different&&size>1?(formationPoseIndex+std::uniform_int_distribution<size_t>(1,size-1)(state.random))%size:std::uniform_int_distribution<size_t>(0,size-1)(state.random);
        formationIdle=formationPoses[formationPoseIndex].idle;formationPickup=formationPoses[formationPoseIndex].pickup;
        state.start(formationIdle,true);
    }
    bool selectActionMode(const std::string& mode,bool persist=false){
        if(!behavior || !battle)return false;
        const bool enabled=mode=="battle",formation=mode=="formation";
        if(mode!="cafe"&&!enabled&&!formation)return false;
        if(formation&&formationIdle<0)return false;
        if(enabled && !battle->clips.supported()){
            state.log("MODE battle unavailable: "+battle->clips.missing);return false;
        }
        const auto anchor=ground.position;const float facing=yaw;
        behavior->halt();stopLocomotion();blockedCursor.reset();placementNeeded=false;manualAction=false;
        clickSequence.reset();doubleClickRequest=tripleClickRequest=wasDragging=dragStarted=false;
        deferredInput={};deferredCafeClick=false;
        state.pendingClick=false;state.dragging=false;state.trackingCursor=false;
        state.returnRequested=state.returningHome=state.returnArrived=false;
        state.resetCooldown();
        battleMode=enabled;formationMode=formation;settings->battleMode=enabled;settings->formationMode=formation;
        battleInput.pollKeyboard=false;if(!grouped)battleInput.enable(enabled);
        if(formation)chooseFormationPose(false);
        else if(enabled)battle->start();else if(!fixedPose){const bool walk=state.walkEnabled,look=state.trackingEnabled;state.walkEnabled=state.trackingEnabled=false;behavior->tick(0);state.walkEnabled=walk;state.trackingEnabled=look;}
        ground.position=anchor;yaw=formation?0:facing;updatePlacementShape();placementGeneration=state.generation;interactionUntil=preciseSeconds()+0.5;
        if(persist)settings->save();
        syncSettings();
        state.log(std::string("MODE ")+modeName()+"; battle supported="+(battle->clips.supported()?"1":"0"));
        return true;
    }
    bool cycleActionMode(bool persist=false){
        static constexpr std::array<const char*,3> modes{"cafe","battle","formation"};
        const size_t current=formationMode?2:battleMode?1:0;
        for(size_t step=1;step<modes.size();++step)
            if(selectActionMode(modes[(current+step)%modes.size()],persist))return true;
        return false;
    }
    bool startManualSkill(){
        if(manualSkill<0||!loaded||state.settingsOpen)return false;
        behavior->halt();stopLocomotion();if(battleMode)battle->start();
        manualAction=true;state.pendingClick=false;state.facingRequest.reset();state.trackingCursor=false;
        state.start(manualSkill,false);state.reaction=0;yaw=0;placementNeeded=false;return true;
    }
    void finishManualSkill(){
        manualAction=false;stopLocomotion();state.pendingClick=false;state.resetCooldown();
        if(battleMode)battle->start();else{behavior->halt();state.start(formationMode?formationIdle:cafeIdle,true);}
        yaw=0;interactionUntil=preciseSeconds()+0.5;placementNeeded=false;
    }
    void initializeBehavior(const Json& message){
        if(!grouped)battleInput.enable(false);battle.reset();behavior.reset();state.clips.clear();state.active=false;state.current=-1;
        for(const auto& c:message.at("clips")){
            const double duration=c.at("duration").get<double>();
            if(std::isfinite(duration)&&duration>0)state.clips.push_back({c.at("name").get<std::string>(),duration,c.at("index").get<int>()});
        }
        if(state.clips.empty())throw std::runtime_error("No usable animation clips");
        const auto& catalog=message.at("catalog");
        auto index=[&](const Json& object,const char* role) {
            const int source=object.value(role,-1);
            for(size_t i=0;i<state.clips.size();++i)if(state.clips[i].modelIndex==source)return static_cast<int>(i);
            return -1;
        };
        auto name=[&](const char* role) {const int i=index(catalog.at("cafe"),role);return i<0?std::string{}:state.clips[i].name;};
        const auto idle=name("idle"),walk=name("walk"),reaction=name("reaction"),pickup=name("pickup");
        cafeIdle=index(catalog.at("cafe"),"idle");cafeWalk=index(catalog.at("cafe"),"walk");
        formationIdle=index(catalog.value("formation",Json::object()),"idle");formationPickup=index(catalog.value("formation",Json::object()),"pickup");
        formationPoses.clear();for(const auto& pose:catalog.value("formation",Json::object()).value("poses",Json::array())){const int idle=index(pose,"idle");if(idle>=0)formationPoses.push_back({idle,index(pose,"pickup")});}
        if(formationPoses.empty()&&formationIdle>=0)formationPoses.push_back({formationIdle,formationPickup});
        manualSkill=index(catalog.value("manual",catalog.at("battle")),"skill");
        if(idle.empty())throw std::runtime_error("No usable default clip");
        for(const auto& warning:catalog.at("warnings"))state.log("ANIMATION mapping: "+warning.get<std::string>());
        for(const auto& note:catalog.value("notes",Json::array()))state.log("ANIMATION selection: "+note.get<std::string>());
        pet::BattleClips mapped(state);mapped.poses.clear();
        const auto& combat=catalog.at("battle");
        for(const auto& item:combat.at("poses")) {
            pet::BattleClips::Pose p; p.name=item.at("name").get<std::string>();
            p.idle=index(item,"idle");p.start=index(item,"start");p.delay=index(item,"delay");p.attack=index(item,"attack");
            p.end=index(item,"end");p.reload=index(item,"reload");p.callsign=index(item,"callsign");p.moveEnd=index(item,"moveEnd");
            if(p.idle>=0 && p.attack>=0)mapped.poses.push_back(p);
        }
        mapped.move=index(combat,"move");mapped.jump=index(combat,"jump");mapped.pickup=index(combat,"pickup");
        cafeJump=mapped.jump;
        mapped.skill=index(combat,"skill");mapped.death=index(combat,"death");mapped.retreat=index(combat,"retreat");
        mapped.victoryStart=index(combat,"victoryStart");mapped.victoryEnd=index(combat,"victoryEnd");
        mapped.missing=mapped.poses.empty()?"No posture with Idle + Attack keywords":"";
        footStops.clear();
        for(const auto& item:message.value("stopProfiles",Json::array())){
            const int source=item.value("clip",-1);FootStopProfile profile;
            auto clip=std::find_if(state.clips.begin(),state.clips.end(),[&](const auto& c){return c.modelIndex==source;});
            const double seconds=item.value("stopTime",0.0);
            if(clip!=state.clips.end()&&seconds<=clip->duration&&profile.initialize(seconds,item.at("speeds").get<std::vector<double>>())){
                footStops[source]=profile;
                state.log("BRAKE feet: "+clip->name+" plant="+std::to_string(seconds)+"s distance="+std::to_string(profile.distanceAt(seconds))
                    +" old="+std::to_string(1.98*clip->duration*0.75));
            }
        }
        for(const auto& p:mapped.poses)if(p.moveEnd<0||!footStops.count(state.clips[p.moveEnd].modelIndex))
            state.log("BRAKE fallback: "+p.name+"; no reliable planted-foot sample, 0.28s stop");
        state.log("ANIMATION cafe: idle="+idle+" walk="+walk);
        for(const auto& p:mapped.poses)state.log("ANIMATION posture="+p.name+" attack="+state.clips[p.attack].name+
            " start="+std::to_string(p.start)+" end="+std::to_string(p.end)+" moveEnd="+std::to_string(p.moveEnd));
        std::ifstream file(directory/"behavior/template.bt.xml");std::string xml((std::istreambuf_iterator<char>(file)),{});
        const auto removeBlock=[&](const std::string& first,const std::string& last){const auto start=xml.find(first);if(start!=std::string::npos){const auto end=xml.find(last,start);if(end!=std::string::npos)xml.erase(start,end+last.size()-start);}};
        if(reaction.empty())removeBlock("<Sequence name=\"Click reaction\">","</Sequence>");
        if(walk.empty()){removeBlock("<ReturnHome ","/>");removeBlock("<ReactiveSequence name=\"Only wander when user is inactive\">","</ReactiveSequence>");}
        replaceAll(xml,"clip=\"Cafe_Idle\"","clip=\""+xmlEscape(idle)+"\"");
        replaceAll(xml,"clip=\"Formation_Pickup\"","clip=\""+xmlEscape(pickup)+"\"");
        if(!walk.empty())replaceAll(xml,"clip=\"Cafe_Walk\"","clip=\""+xmlEscape(walk)+"\"");
        if(!reaction.empty())replaceAll(xml,"clip=\"Cafe_Reaction\"","clip=\""+xmlEscape(reaction)+"\"");
        const auto tree=storage()/"behavior"/(pendingModel.stem().wstring()+L".bt.xml");
        std::ofstream(storage()/"behavior"/(pendingModel.stem().wstring()+L".animations.json"))
            << Json{{"source",utf8(pendingModel.wstring())},{"clips",message.at("clips")},{"catalog",catalog},{"stopProfiles",message.value("stopProfiles",Json::array())}}.dump(2);
        if(!std::filesystem::exists(tree))std::ofstream(tree)<<xml;
        behavior=std::make_unique<pet::Behavior>(state,tree);behavior->start(idle);
        battle=std::make_unique<pet::Battle>(state,mapped);settings->battleAvailable=battle->clips.supported();
        battle->attackOnKeyPress=true;
        battle->bufferKeyPress=true;
        battle->ignoreReloadKeyboard=true;
        battle->endBeforeReload=true;battle->smoothStops=smooth;
        applyPreferences();
        wchar_t savedMode[24]{};GetPrivateProfileStringW(L"Behavior",L"Mode",L"Cafe",savedMode,24,settings->ini.c_str());
        std::string requested=initialMode.empty()?(std::wcscmp(savedMode,L"Battle")==0?"battle":std::wcscmp(savedMode,L"Formation")==0?"formation":"cafe"):initialMode;
        if(!fixedPose && initialMode=="battle" && !settings->battleAvailable)throw std::runtime_error("Model has no battle animations: "+battle->clips.missing);
        if(!fixedPose&&initialMode=="formation"&&formationIdle<0)throw std::runtime_error("Model has no Formation_Idle animation");
        if(fixedPose||!selectActionMode(requested))selectMode(false);
        if(fixedPose){const auto selected=reviewClip.empty()?idle:reviewClip;state.start(state.find(selected),true);state.time=reviewTime;}
        model=pendingModel;loaded=true;frameReady=false;readyAt=GetTickCount64();journey={};yaw=0;stopTurn.reset();
        directed.cancel();restFacing.reset();geometry=DesktopGeometry{};
        const auto& spatial=message.at("spatial");
        auto box=[&](const Json& values,Vector3& low,Vector3& high){if(!values.is_array()||values.size()!=6)throw std::runtime_error("Invalid spatial bounds");for(const auto& v:values)if(!v.is_number()||!std::isfinite(v.get<float>())||std::abs(v.get<float>())>100)throw std::runtime_error("Invalid model envelope");low={values[0],values[1],values[2]};high={values[3],values[4],values[5]};};
        box(spatial.at("idle"),geometry.minimum,geometry.maximum);box(spatial.at("envelope"),geometry.renderMinimum,geometry.renderMaximum);
        auto radial=[](Vector3& low,Vector3& high){const float radius=std::hypot(std::max(std::abs(low.x),std::abs(high.x)),std::max(std::abs(low.z),std::abs(high.z)));low.x=low.z=-radius;high.x=high.z=radius;};
        radial(geometry.renderMinimum,geometry.renderMaximum);
        for(const auto& row:spatial.at("placement")){
            auto& shapes=geometry.restingBoxes[row.at("clip").get<int>()];
            for(const auto& values:row.at("boxes")){BoundingBox bounds{};box(values,bounds.min,bounds.max);shapes.push_back(bounds);}
        }
        updatePlacementShape();
        state.log("PLACEMENT per-primitive idle bounds: "+std::to_string(geometry.restingBoxes.size())+" clips; clearance=2 px");
        geometry.bodyWidth=spatial.at("bodyWidth");ground.measuredBounds=true;geometry.prepareViewport(ground,zoom);
        state.log("FOLLOW body unit="+std::to_string(geometry.bodyWidth)+" source="+spatial.value("bodyWidthSource",std::string{"unknown"}));
        geometry.place(ground,ground.screen(ground.position),zoom);placementNeeded=true;placementGeneration=UINT64_MAX;applyPreferences();
        eyeHeight=message.value("eyeHeight",1.7f);settings->title=model.stem().wstring();settings->importBusy=false;progress(100,L"原始模型加载完成，无 Blender 转换");
        if(!review&&!grouped){const std::wstring text=L"[Model]\nPath="+model.wstring()+L"\n";std::ofstream out(storage()/"active-model.ini",std::ios::binary);const unsigned char bom[]{0xff,0xfe};out.write(reinterpret_cast<const char*>(bom),2);out.write(reinterpret_cast<const char*>(text.data()),text.size()*sizeof(wchar_t));}
        std::printf("READY %s: %zu clips, %d skins, %d mesh hulls, %d ms\n",utf8(model.stem().wstring()).c_str(),state.clips.size(),message.value("skins",0),message.value("outlineMeshes",0),message.value("elapsedMs",0));std::fflush(stdout);
        std::printf("CAFE tree: %s\n",utf8(tree.wstring()).c_str());std::fflush(stdout);
        if(loadedCallback)loadedCallback();
        if(settingsOnLoad){settingsOnLoad=false;showSettings();}
    }
    void capture(){if(capturing||snapshot.empty()||!renderer||!loaded)return;capturing=true;renderer->render(window,inputWindow,true,snapshot);DestroyWindow(window);}
    void tick(){
        NativeHealth::mark("behavior / import");
        const auto now=GetTickCount64();const double preciseNow=preciseSeconds();
        const double elapsed=std::max(0.0,preciseNow-lastTick);
        const float dt=static_cast<float>(std::min(elapsed,0.1));lastTick=preciseNow;
        movementDt=dt;
        if(pendingImport){
            pendingImport=false;const auto started=preciseSeconds();
            try{renderer->load(pendingModel,[this](int p,const std::string& text){progress(p,wide(text));if(nativeSettings->window)UpdateWindow(nativeSettings->window);});
                auto metadata=renderer->model->metadata;metadata["elapsedMs"]=int((preciseSeconds()-started)*1000);initializeBehavior(metadata);
            }catch(const std::exception& error){settingsError=error.what();done=!loaded;settings->importBusy=false;settings->progressDetail=wide(settingsError);state.log("IMPORT failed: "+settingsError);syncSettings();if(!loaded)showSettings();}
        }
        const auto before=ground.position;motionVelocity={};motionRemaining=0;
        int endingClip=-1;double endingFrom=0,endingTo=0;
        if(!settings->importSource.empty()){const auto file=settings->importSource;settings->importSource.clear();importLocalModel(file);}
        if(review&&(fixedPose||runSeconds>0||!snapshot.empty())&&now-started>25000){std::fprintf(stderr,"Preview timed out\n");exitCode=1;DestroyWindow(window);return;}
        if(!loaded||!behavior||!displayed)return;
        if(settings->modeRequest){selectMode(*settings->modeRequest,true);settings->modeRequest.reset();}
        if(runSeconds>0&&now>=readyAt&&now-readyAt>=runSeconds*1000){std::printf("PROFILE completed %.1f seconds\n",runSeconds);DestroyWindow(window);return;}
        ground.modelZoom=zoom;state.movementScale=zoom*WalkSpeedMultiplier*(battleMode?3.0:1.0);updatePlacementShape();geometry.measure(ground,zoom);
        const double idle=idleInputSeconds();
        if(!fixedPose){
            const bool dragging=left&&dragged;
            if(tripleClickRequest){
                doubleClickRequest=false;
                if(cycleActionMode(!review)&&settingsChanged)settingsChanged();
                click=false;
            }else if(doubleClickRequest&&!dragging){startManualSkill();click=false;}
            doubleClickRequest=tripleClickRequest=false;
            if(manualAction&&dragging)finishManualSkill();
            if(manualAction){
                yaw=0;state.pendingClick=false;state.trackingCursor=false;state.facingRequest.reset();
                if(!state.settingsOpen)state.advance(dt);
                if(state.time>=state.clips[state.current].duration)finishManualSkill();
                click=dragStarted=false;wasDragging=dragging;renderFrame();return;
            }
            if(formationMode){
                if(!dragging&&(wasDragging||dragStarted))chooseFormationPose(true);
                const int target=dragging&&formationPickup>=0?formationPickup:formationIdle;
                if(!state.active||state.current!=target)state.start(target,true);
                state.dragging=dragging;state.walking=false;state.pendingClick=false;state.trackingCursor=false;state.facingRequest.reset();
                state.returnRequested=state.returningHome=state.returnArrived=false;
                if(!state.settingsOpen)state.advance(dt);yaw=0;wasDragging=dragging;
                click=dragStarted=false;renderFrame();return;
            }
            if(observeDebouncedInput(state,idle,elapsed,preferences.inputGrace) && !battleMode&&!preferences.follow&&!directed.active){state.trackingCursor=false;state.returnRequested=state.walkEnabled&&journey.requestReturn(ground.position);if(!state.returnRequested)yaw=0;}
            POINT cursor{};GetCursorPos(&cursor);const Vector2 pixel{float(cursor.x-work.left),float(cursor.y-work.top)};
            const auto goal=battleMode&&state.inputActive?ground.planeAt(pixel,1.15f*zoom):ground.floorAt(ground.clampFoot(pixel));const float dx=goal.x-ground.position.x,dz=goal.z-ground.position.z;
            state.cursorTarget=dx*dx+dz*dz>0.04f;state.cursorYaw=std::atan2(dx,dz)*RAD2DEG;
            float lookYaw=0;state.lookTarget=ground.faceCursor(pixel,eyeHeight*zoom,lookYaw);state.lookYaw=lookYaw;
            const bool front=click||dragStarted||wasDragging!=dragging;
            if((front&&!directed.jumping)||dragging||right){blockedCursor.reset();cafeAttention.pause();directed.cancel();deferredInput={};deferredCafeClick=false;restFacing.reset();interactionUntil=preciseNow+2;placementNeeded=true;}
            if(front&&!directed.jumping)yaw=0;if(front||dragging||right||!state.walkEnabled){journey.route.clear();state.returnRequested=false;}
            if(battleMode){
                auto events=grouped?sharedEvents:battleInput.take(false,dragging,false);events.petClick=false;events.dragging=dragging;
                if(directed.jumping){
                    deferredInput.keyPressed|=events.keyPressed;deferredInput.reload|=events.reload;
                    deferredInput.mouseClick|=events.mouseClick;deferredInput.windowClosed|=events.windowClosed;
                    events.keyPressed=events.reload=events.mouseClick=events.windowClosed=false;
                }else{
                    events.keyPressed|=deferredInput.keyPressed;events.reload|=deferredInput.reload;
                    events.mouseClick|=deferredInput.mouseClick;events.windowClosed|=deferredInput.windowClosed;deferredInput={};
                }
                const auto phaseBefore=battle->phase;
                const int clipBefore=state.current;const double remainingBefore=state.walkRemaining;const bool loopBefore=state.loop;
                const double turnLead=activeTurnLead();
                const bool turnPhase=phaseBefore==pet::Battle::Phase::Moving||phaseBefore==pet::Battle::Phase::MoveEnd;
                if(!turnPhase||front||dragging||right)stopTurn.reset();
                double stoppingDistance=0;
                for(const auto& pose:battle->clips.poses){const auto& profile=stopProfile(pose.moveEnd);stoppingDistance=std::max(stoppingDistance,profile.distanceAt(profile.stopTime));}
                const double stoppingTravel=stoppingDistance*zoom+state.walkSpeed*state.movementScale*StopTurn::LeadSeconds;
                if(smooth&&phaseBefore==pet::Battle::Phase::Moving&&state.walkRemaining<=turnLead+dt&&!front&&!dragging&&!right){
                    const auto brakes=brakeRange();const double speed=state.walkSpeed*state.movementScale;
                    if(!DesktopPath::trip(ground,geometry,zoom,yaw,speed*state.walkRemaining,brakes.first,brakes.second)){
                        // Finish crossing in the same Moving state, before Move_End/Idle.
                        for(int extra=1;extra<=30;++extra)if(DesktopPath::trip(ground,geometry,zoom,yaw,speed*(state.walkRemaining+extra*0.1),brakes.first,brakes.second)){
                            state.walkRemaining+=extra*0.1;stopTurn.reset();state.log("BOUNDARY continue through to one-screen stop");break;}
                    }
                }
                if(smooth && phaseBefore==pet::Battle::Phase::Moving && !DesktopPath::stepAllowed(ground,yaw,stoppingTravel))
                    state.walkRemaining=std::min(state.walkRemaining,StopTurn::LeadSeconds);
                if(phaseBefore==pet::Battle::Phase::Moving&&!stopTurn.active&&!front&&!dragging&&!right
                    &&(state.walkRemaining<=turnLead||!preferences.automaticWalk(true,state.inputActive))) {
                    const double lead=std::max(0.0,std::min(state.walkRemaining,turnLead));const auto brakes=brakeRange();
                    const bool activeRetreat=battle->activeInputMove;
                    const double turnSeconds=activeRetreat?std::max(0.008,lead+earliestFootPlant()):state.walkRemaining>0?std::min(state.walkRemaining,turnLead):0.18;
                    const bool curveSafe=activeRetreat?activeCurveFits(yaw,lookYaw,lead,turnSeconds)
                        :DesktopPath::turnFits(ground,geometry,zoom,yaw,lookYaw,lead,state.walkSpeed*state.movementScale,brakes.first,brakes.second);
                    const double desired=(activeRetreat||state.inputActive&&state.trackingEnabled)&&state.lookTarget&&curveSafe?lookYaw:yaw;
                    stopTurn.begin(yaw,desired,turnSeconds);
                    state.log("MOTION stop turn: from="+std::to_string(yaw)+" target="+std::to_string(stopTurn.target));
                }
                if(turnPhase&&stopTurn.active&&!state.settingsOpen) {
                    state.walkYaw=stopTurn.advance(dt);yaw=float(std::remainder(stopTurn.yaw,360.0));
                } else if(phaseBefore==pet::Battle::Phase::Moving)state.walkYaw=walkIntentYaw;
                const float boundaryHeading=crowd.active?crowd.heading:float(state.walkYaw);
                battleInput.planJump(events,*battle,state,ground,boundaryHeading,work.left,work.top,[&](float maximum){
                    return visibleWindows.distance(ground,boundaryHeading,maximum,work.left,work.top,window);
                });
                if(events.boundary&&clearJump&&battle->clips.jump>=0&&!clearJump(boundaryHeading,state.walkSpeed*state.movementScale*(state.clips[battle->clips.jump].duration-events.jumpOffset)))events.boundary=false;
                if(events.boundary)state.walkYaw=boundaryHeading;
                if(dragStarted&&!dragging){auto drag=events;drag.dragging=true;drag.petClick=drag.mouseClick=drag.keyPressed=drag.windowClosed=false;battle->tick(0,drag);}
                const bool allowWalk=state.walkEnabled;state.walkEnabled=preferences.automaticWalk(true,state.inputActive)&&(!preferences.follow||!state.inputActive)&&!directed.active;
                const bool menuOpen=state.settingsOpen;if(directed.jumping)state.settingsOpen=true;
                battle->activityDelta=elapsed;battle->tick(dt,events);state.settingsOpen=menuOpen;state.walkEnabled=allowWalk;
                if(phaseBefore==pet::Battle::Phase::MoveEnd){
                    endingClip=clipBefore;
                    endingFrom=std::max(0.0,(loopBefore?pet::Battle::StopFallbackSeconds:state.clips[clipBefore].duration)-remainingBefore);
                    endingTo=endingFrom+battle->movementSeconds;
                }else if(battle->phase==pet::Battle::Phase::MoveEnd){
                    const double duration=state.loop?pet::Battle::StopFallbackSeconds:state.clips[state.current].duration;
                    if(duration-state.walkRemaining>1e-8){
                        endingClip=state.current;endingTo=duration-state.walkRemaining;
                        endingFrom=std::max(0.0,endingTo-battle->movementSeconds);
                    }
                }
                if(state.walking&&walkGeneration!=state.generation){
                    const bool resumedWalk=phaseBefore==pet::Battle::Phase::Skill&&battle->phase==pet::Battle::Phase::Moving;
                    if(!stopTurn.active&&!resumedWalk)yaw=float(state.walkYaw);
                    if(battle->phase==pet::Battle::Phase::Moving&&!resumedWalk)walkIntentYaw=float(state.walkYaw);
                    walkGeneration=state.generation;
                }
                if(battle->returnToCafe)selectMode(false,!review);
                else if(state.facingRequest&&battle->phase!=pet::Battle::Phase::Idle&&!dragging&&!right
                    &&!(stopTurn.active&&(phaseBefore==pet::Battle::Phase::MoveEnd||battle->phase==pet::Battle::Phase::MoveEnd)))
                    yaw=float(*state.facingRequest);
                if(battle->phase!=pet::Battle::Phase::Moving&&battle->phase!=pet::Battle::Phase::MoveEnd)stopTurn.reset();
            }else{
                if(dragStarted&&!dragging)behavior->tick(0,false,true);
                const bool allowWalk=state.walkEnabled;state.walkEnabled=allowWalk&&(!preferences.follow||!state.inputActive)&&!directed.active;
                const bool allowLook=state.trackingEnabled;state.trackingEnabled=false;
                if(directed.jumping)deferredCafeClick|=click;
                else {
                    const bool slowerIdle=state.active&&state.loop&&!state.walking&&state.current==cafeIdle;
                    const auto generation=state.generation;const double beforeTime=state.time;
                    behavior->tick(dt,click||deferredCafeClick,dragging);deferredCafeClick=false;
                    // Keep behavior/input timers in real seconds; only the
                    // Cafe Idle animation clock runs at 75% speed.
                    if(slowerIdle&&state.generation==generation&&state.current==cafeIdle)
                        state.time=std::fmod(beforeTime+(state.settingsOpen?0:dt*0.75),state.clips[cafeIdle].duration);
                }
                state.walkEnabled=allowWalk;state.trackingEnabled=allowLook;
            }
            state.trackingCursor=false;
            state.facingRequest.reset();wasDragging=dragging;dragStarted=click=false;
            if(state.returningHome){if(!state.settingsOpen&&!state.returnArrived){
                double speed=state.walkSpeed*state.movementScale;
                if(smooth&&!journey.route.empty()) {const auto home=journey.route.back();const double distance=std::hypot(home.x-ground.position.x,home.z-ground.position.z);speed*=std::min(1.0,std::sqrt(2*distance/(speed*0.28)));}
                if(avoidHeading&&!journey.route.empty()){
                    const auto home=journey.route.back();const double dx=home.x-ground.position.x,dz=home.z-ground.position.z,remaining=std::hypot(dx,dz);
                    if(remaining<0.002){ground.position=home;journey.route.clear();state.returnArrived=true;}
                    else {const float direct=std::atan2(dx,dz)*RAD2DEG;const double travel=std::min(remaining,speed*dt);const auto choice=avoidHeading(direct,travel);const float heading=choice.heading;
                        if(std::isfinite(heading)){yaw=PetGround::turnToward(yaw,heading,dt);ground.position=ground.step(heading,float(travel*choice.scale));
                            if(travel*choice.scale>=remaining&&std::abs(std::remainder(heading-direct,360.0f))<0.01f){ground.position=home;journey.route.clear();state.returnArrived=true;}}
                        else stopBlocked();
                    }
                }else state.returnArrived=journey.stepHome(ground,float(speed*dt),yaw);
                if(state.returnArrived)yaw=0;
            }}
            else if(battleMode && battle->movementSeconds>0){
                const auto& profile=stopProfile(endingClip);
                const float distance=endingClip>=0?float((profile.distanceAt(endingTo)-profile.distanceAt(endingFrom))*zoom)
                    :float(battle->movementSpeed*state.movementScale*battle->movementSeconds);
                if(battle->jumpMotion){yaw=float(battle->movementYaw);ground.position=ground.step(yaw,distance);}
                else if(!moveGround(float(battle->movementYaw),distance))stopBlocked();
            }
            else if(!battleMode&&state.walking&&!state.settingsOpen){if(walkGeneration!=state.generation){yaw=walkIntentYaw=float(state.walkYaw);walkGeneration=state.generation;journey.begin(ground.position);state.returnArrived=false;}
                state.walkYaw=walkIntentYaw;
                const double duration=pet::Battle::StopFallbackSeconds;
                if(smooth&&!DesktopPath::stepAllowed(ground,yaw,state.walkSpeed*state.movementScale*duration*0.75))
                    state.walkRemaining=std::min(state.walkRemaining,duration);
                const double remaining=std::min<double>(dt,state.walkRemaining);
                const double travel=smooth?pet::Battle::stopDistance(duration-state.walkRemaining+remaining,duration)
                    -pet::Battle::stopDistance(duration-state.walkRemaining,duration):remaining;
                const double movingTime=state.walkRemaining>=duration?remaining:std::max(0.0,travel);
                if(!moveGround(walkIntentYaw,float(state.walkSpeed*state.movementScale*movingTime)))stopBlocked();
            }
            const bool idlePose=battleMode?battle->phase==pet::Battle::Phase::Idle:state.current==cafeIdle;
            if(directed.active&&(!idlePose||directed.owner!=state.generation||(left&&!directed.jumping)||right||(directed.following&&!preferences.follow&&!directed.jumping))){directed.cancel();deferredInput={};deferredCafeClick=false;}
            if(idlePose&&placementGeneration!=state.generation){placementNeeded=true;placementGeneration=state.generation;}
            const int travelClip=battleMode?battle->clips.move:cafeWalk;
            if(idlePose&&(!left||directed.jumping)&&!right&&(!state.settingsOpen||directed.jumping)&&!state.returningHome){
                if(placementNeeded&&!directed.active){
                    const auto foot=ground.screen(ground.position),target=geometry.idleFoot(ground,zoom);placementNeeded=false;
                    if(std::hypot(target.x-foot.x,target.y-foot.y)>2){
                        if(travelClip>=0&&state.walkEnabled){if(beginDirected(ground.floorAt(target),false)){restFacing.reset();state.log("PLACEMENT one-monitor destination");}}
                        else ground.position=ground.floorAt(target);
                    }
                }
                if(blockedCursor&&std::hypot(cursor.x-blockedCursor->x,cursor.y-blockedCursor->y)>16)blockedCursor.reset();
                if(directed.active&&directed.following&&!directed.jumping
                    &&(!directed.crossing||DesktopPath::restingMonitor(ground,geometry,zoom,ground.position)>=0)){
                    const auto center=ground.screen({ground.position.x,1.15f*zoom,ground.position.z});
                    const auto side=ground.screen({ground.position.x+geometry.bodyWidth*zoom,1.15f*zoom,ground.position.z});
                    const float bodyPixels=std::max(1.0f,std::abs(side.x-center.x));
                    const auto destination=ground.screen({directed.target.x,1.15f*zoom,directed.target.z});
                    if(!DirectedWalk::continueFollow(center,pixel,bodyPixels,preferences.followNear,preferences.followFar,directed.followAway,crowd.active&&crowd.scale<0.08)
                        ||(preferences.followNear==0&&!DirectedWalk::approachesCursor(center,pixel,destination))){
                        directed.cancel();motionVelocity={};motionRemaining=0;restFacing.reset();followPlanAt=0;
                        state.log("FOLLOW distance satisfied: stop");
                    }
                }
                // Replan at up to 20Hz while the cursor moves, but retain the
                // 150ms stationary cadence for projection/path work.
                const bool cursorMoved=!hasFollowPlan||std::hypot(double(cursor.x)-followPlannedCursor.x,double(cursor.y)-followPlannedCursor.y)>=2;
                const bool followDue=preciseNow>=followPlanAt||(cursorMoved&&preciseNow>=followPlanAt-0.10);
                if(!blockedCursor&&preferences.follow&&(state.inputActive||directed.active&&directed.following)&&travelClip>=0&&!directed.jumping&&!directed.crossing&&preciseNow>=interactionUntil&&(!directed.active||directed.following)&&followDue){
                    const auto foot=ground.screen(ground.position);
                    const auto center=ground.screen({ground.position.x,1.15f*zoom,ground.position.z});
                    const auto side=ground.screen({ground.position.x+geometry.bodyWidth*zoom,1.15f*zoom,ground.position.z});
                    const float bodyPixels=std::max(1.0f,std::abs(side.x-center.x));
                    const bool outside=DirectedWalk::outsideBand(center,pixel,bodyPixels,preferences.followNear,preferences.followFar);
                    if(outside||directed.active){
                        const auto destination=DirectedWalk::followTarget(center,pixel,bodyPixels,float((preferences.followNear+preferences.followFar)*0.5));
                        auto target=ground.clampFoot({destination.x+foot.x-center.x,destination.y+foot.y-center.y});
                        const auto saved=ground.position;ground.position=ground.floorAt(target);target=geometry.idleFoot(ground,zoom);ground.position=saved;geometry.measure(ground,zoom);
                        const auto world=ground.floorAt(target);
                        const auto targetCenter=ground.screen({world.x,1.15f*zoom,world.z});
                        if(preferences.followNear==0&&!DirectedWalk::approachesCursor(center,pixel,targetCenter)){stopBlocked();}
                        else if(!directed.active&&std::hypot(target.x-foot.x,target.y-foot.y)>8){if(beginDirected(world,true)){directed.followAway=std::hypot(center.x-pixel.x,center.y-pixel.y)<bodyPixels*(preferences.followNear+preferences.followFar)*0.5;restFacing.reset();state.log(std::string("FOLLOW begin: ")+(battleMode?"battle":"cafe"));}}
                        else if(directed.active){const double dx=world.x-ground.position.x,dz=world.z-ground.position.z;
                            if(DesktopPath::trip(ground,geometry,zoom,std::atan2(dx,dz)*RAD2DEG,std::hypot(dx,dz))){directed.target=world;directed.crossing=DesktopPath::crossing(ground,geometry,zoom,world);}}
                    }
                    followPlanAt=preciseNow+0.15;followPlannedCursor=cursor;hasFollowPlan=true;
                }
                if(directed.active){
                    const int jump=battleMode?battle->clips.jump:-1;
                    if(jump>=0&&directed.planJump(ground,0.792*state.movementScale,state.clips[jump].duration,
                        [&](float heading,float maximum){return visibleWindows.distance(ground,heading,maximum,work.left,work.top,window,true);},clearJump))state.log("FOLLOW JUMP continuous: "+state.clips[jump].name);
                    const bool following=directed.following;const float oldFacing=directed.finalYaw;
                    const auto steering=[&](float heading,double travel){
                        const auto choice=avoidHeading?avoidHeading(heading,travel):MovementChoice{heading};
                        if(directed.following&&preferences.follow&&!std::isfinite(choice.heading)){
                            const auto center=ground.screen({ground.position.x,1.15f*zoom,ground.position.z});
                            const auto side=ground.screen({ground.position.x+geometry.bodyWidth*zoom,1.15f*zoom,ground.position.z});
                            const float bodyPixels=std::max(1.0f,std::abs(side.x-center.x));
                            if(DirectedWalk::outsideBand(center,pixel,bodyPixels,preferences.followNear,preferences.followFar)){
                                // Outside the band, wait for a route instead of
                                // abandoning follow. Other movement keeps its timeout.
                                crowd.blockedTime=0;return MovementChoice{yaw,0,true};
                            }
                        }
                        return choice;
                    };
                    if(directed.step(ground,0.792*state.movementScale,dt,yaw,steering)){if(following)restFacing.reset();else restFacing=oldFacing;placementNeeded=false;state.log("DIRECTED arrival");}
                    else if(directed.blocked)stopBlocked();
                }else if(restFacing){yaw=PetGround::turnToward(yaw,*restFacing,dt);if(std::abs(std::remainder(yaw-*restFacing,360.0f))<0.1f)restFacing.reset();}
            }
            // A skill can suspend a walk. Keep its avoidance direction so the
            // resumed walk turns smoothly instead of snapping to its intent.
            if(!state.walking&&!directed.active&&!(battleMode&&battle->phase==pet::Battle::Phase::Skill))crowd.reset();
            if(!battleMode)cafeAttention.update(state,yaw,dt,
                idlePose&&!directed.active&&!restFacing&&!state.returningHome&&!left&&!right&&!front&&!state.settingsOpen
                    &&preciseNow>=interactionUntil,preferences.cafeLookMode=="threshold",preferences.cafeTracking);
            else cafeAttention.reset();
            if(battleMode&&(battle->phase==pet::Battle::Phase::VictoryStart||battle->phase==pet::Battle::Phase::VictoryEnd))yaw=0;
            if(dt>0 && (state.walking||directed.active) && (!state.settingsOpen || directed.jumping || (battleMode && battle->phase==pet::Battle::Phase::Jump))) {
                motionVelocity={(ground.position.x-before.x)/dt,0,(ground.position.z-before.z)/dt};
                motionRemaining=directed.active?(directed.jumping?std::min(0.032,directed.jumpDuration-directed.jumpTime):0.032):state.returningHome ? 0 : state.walkRemaining;
                if(endingClip>=0)motionRemaining=std::max(0.0,stopProfile(endingClip).stopTime-endingTo);
                if(directed.active&&!directed.jumping){
                    const float speed=std::hypot(motionVelocity.x,motionVelocity.z);
                    if(speed>0)motionRemaining=std::min(motionRemaining,std::hypot(directed.target.x-ground.position.x,directed.target.z-ground.position.z)/double(speed));
                }
                if(state.returningHome && !journey.route.empty()) {
                    const auto home=journey.route.back();const float speed=std::hypot(motionVelocity.x,motionVelocity.z);
                    if(speed>0)motionRemaining=std::hypot(home.x-ground.position.x,home.z-ground.position.z)/speed;
                }
            }
        }
        renderFrame();
        if(frameReady&&!snapshot.empty()&&now>=readyAt&&now-readyAt>600)capture();
    }
    int retreatDisplacement()const{
        if(!battleMode||!battle||battle->phase!=pet::Battle::Phase::Retreat)return 0;
        const double t=std::clamp(battle->retreatProgress,0.0,1.0);
        // Ease in: preserve the animation duration and total travel, accelerate upward.
        return int(t*t*(geometry.crop.y+geometry.height+20));
    }
    void updatePlacementShape(){
        geometry.yaw=yaw;int idle=cafeIdle;
        if(formationMode)idle=formationIdle;
        if(battleMode&&battle)idle=battle->idleClip();
        const int clip=idle>=0&&idle<int(state.clips.size())?state.clips[idle].modelIndex:-1;
        if(loaded&&geometry.restingClip!=clip)placementNeeded=true;
        geometry.restingClip=clip;
    }
    void moveCachedDrag(){
        if(!committedFrame||!left||!dragged)return;
        const auto destination=draggedFrameOrigin(committedOrigin,committedFoot,ground.screen(ground.position));
        if(destination.x==windowPosition.x&&destination.y==windowPosition.y)return;
        for(HWND hwnd:{window,inputWindow})SetWindowPos(hwnd,nullptr,destination.x,destination.y,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
        windowPosition=destination;if(depthRequest)depthRequest();
    }
    void presentState(bool force=false,double sampleAt=0){
        if(!loaded)return;
        updatePlacementShape();
        if(!force)return;
        const double now=sampleAt?sampleAt:preciseSeconds();
        const bool paused=state.settingsOpen&&!directed.jumping&&!(battleMode&&battle->phase==pet::Battle::Phase::Jump);
        const auto displayPosition=framePosition(ground.position,motionVelocity,motionRemaining,lastTick,now,paused||left||right);
        const auto foot=ground.screen(displayPosition);geometry.crop={std::round(foot.x-geometry.width*0.5f),std::round(foot.y-geometry.above)};const auto crop=geometry.crop;
        const int retreatOffset=retreatDisplacement();
        const POINT surfaceOrigin{work.left+int(crop.x),work.top+int(crop.y)-retreatOffset};
        // Normal movement/rotation/zoom commits the image AND origin together
        // in UpdateLayeredWindow. Only an explicit drag translates a cached frame.
        windowWidth=geometry.width;windowHeight=geometry.height;lastPost=now;
        const bool turningWalk=!formationMode&&!manualAction&&!battleMode&&cafeAttention.turning&&cafeWalk>=0;
        const int index=turningWalk?cafeWalk:directed.active?(directed.jumping?(battleMode?battle->clips.jump:cafeJump):(battleMode?battle->clips.move:cafeWalk)):state.current;
        const int clip=index>=0?state.clips[index].modelIndex:0;
        const double playbackRate=!battleMode&&index==cafeIdle&&state.loop&&!state.walking&&!turningWalk&&!directed.active?0.75
            :1.0;
        const bool retreat=battle&&state.current==battle->clips.retreat;
        const bool victory=battleMode&&battle&&(battle->phase==pet::Battle::Phase::VictoryStart||battle->phase==pet::Battle::Phase::VictoryEnd);
        // Exs releases toward screen-left in the authored animation. Rotate the
        // complete skill (including its props) toward the viewer; keep logical
        // movement yaw independent so this correction cannot affect other clips.
        const float displayYaw=exsPlaying()?90.0f:formationMode?0:std::remainder(yaw+(retreat?180.0f:0),360.0f);
        if(retreat && retreatLogged!=state.generation) {
            retreatLogged=state.generation;std::printf("RETREAT facing: base=%.1f render=%.1f offset=180\n",yaw,displayYaw);std::fflush(stdout);
        }
        if(victory&&victoryLogged!=state.generation){victoryLogged=state.generation;state.log("VICTORY facing: render=0");}
        post({{"type","state"},{"maskRevision",maskRevision},{"generation",displayGeneration()},{"clip",clip},{"time",fixedPose?reviewTime:turningWalk?cafeAttention.time:directed.active?(directed.jumping?directed.jumpTime:directed.time):state.time},{"loop",turningWalk|| (directed.active?!directed.jumping:state.loop)},{"review",fixedPose},
            {"mode",modeName()},{"paused",paused},
            {"camera",{ground.camera.position.x,ground.camera.position.y,ground.camera.position.z}},{"fov",ground.camera.fovy},{"far",500*std::max(1.0f,ground.height/primaryHeight)},
            {"desktop",{ground.width,ground.height}},{"viewport",{windowWidth,windowHeight}},{"crop",{crop.x,crop.y}},{"surfaceOrigin",{surfaceOrigin.x,surfaceOrigin.y}},{"position",{displayPosition.x,0,displayPosition.z}},
            {"velocity",{motionVelocity.x,0,motionVelocity.z}},{"motionRemaining",motionRemaining},{"sampleTime",lastTick*1000},{"playbackRate",playbackRate},
            {"smooth",smooth},{"menuHint",menuHintVisible&&!state.settingsOpen},
            {"zoom",zoom},{"yaw",victory?0:displayYaw},{"outline",state.outlineEnabled},{"outlineMode",preferences.outlineMode},{"renderQuality",preferences.renderQuality},{"outlineWidth",preferences.outlineWidth},{"interacting",left||right},
            {"resting",!manualAction&&!left&&!right&&!state.walking&&!directed.active&&!turningWalk&&(formationMode?state.current==formationIdle:battleMode?battle->phase==pet::Battle::Phase::Idle:state.current==cafeIdle)}});
    }
    void captureMouse(){
        SetCapture(inputWindow);
        // Capture keeps the gesture active outside the input silhouette.
        presentState(true);
    }
    void releaseMouse(){
        if(left&&dragged)journey.place(ground.position);
        if(right)placementNeeded=true;
        left=right=menuPressed=rightDragged=false;ReleaseCapture();presentState(true);
    }
    static LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
        App* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
        if(!app)return DefWindowProcW(hwnd,message,w,l);
        try{
            POINT cursor{};GetCursorPos(&cursor);
            switch(message){
            case WM_ERASEBKGND:return 1;
            case WM_NCHITTEST:return HTTRANSPARENT; // Only the separate alpha-shaped window accepts input.
            case WM_PAINT:{PAINTSTRUCT p{};BeginPaint(hwnd,&p);EndPaint(hwnd,&p);return 0;}
            case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
            case WM_SYSCOMMAND:if((w&0xfff0)==SC_MINIMIZE)return 0;return DefWindowProcW(hwnd,message,w,l);
            case WM_POWERBROADCAST:case WM_WTSSESSION_CHANGE:
                if(app->session.event(message,w,l)){
                    if(app->sessionChanged)app->sessionChanged();
                    else {app->lastTick=preciseSeconds();app->nextRender=0;if(app->renderer)app->renderer->resume();}
                    app->state.log(app->session.paused()?"DESKTOP paused: sleep / lock / display off":"DESKTOP resumed: reset frame clock");
                }return message==WM_POWERBROADCAST?TRUE:0;
            case WM_DISPLAYCHANGE:app->refreshDisplays();app->geometry.prepareViewport(app->ground,app->zoom);app->geometry.place(app->ground,app->ground.screen(app->ground.position),app->zoom);app->placementNeeded=true;app->presentState(true);return 0;
            case WM_TIMER:if(!app->session.paused()){if(app->tickCallback)app->tickCallback();else app->tick();}return 0;
            case WM_LBUTTONDOWN:if(app->right||app->menuPressed)return 0;app->left=true;app->dragged=false;app->down=app->previous=cursor;app->captureMouse();return 0;
            case WM_RBUTTONDOWN:if(app->left||app->right)return 0;app->right=app->menuPressed=true;app->rightDragged=false;app->down=app->previous=cursor;app->captureMouse();return 0;
            case WM_MBUTTONDOWN:if(app->left||app->menuPressed||app->frontLocked())return 0;app->right=true;app->previous=cursor;app->captureMouse();return 0;
            case WM_MOUSEMOVE:
                if(app->left){const bool before=app->dragged;app->dragged|=std::abs(cursor.x-app->down.x)>4||std::abs(cursor.y-app->down.y)>4;
                    if(app->dragged){app->clickSequence.reset();app->doubleClickRequest=app->tripleClickRequest=false;app->dragStarted|=!before;auto foot=app->ground.screen(app->ground.position);foot.x+=cursor.x-(before?app->previous.x:app->down.x);foot.y+=cursor.y-(before?app->previous.y:app->down.y);app->updatePlacementShape();app->geometry.place(app->ground,foot,app->zoom);}app->previous=cursor;}
                if(app->right){
                    if(app->menuPressed&&!app->rightDragged){app->rightDragged=std::abs(cursor.x-app->down.x)>4||std::abs(cursor.y-app->down.y)>4;if(!app->rightDragged)return 0;}
                    if(!app->frontLocked())app->yaw=std::remainder(app->yaw+(cursor.x-app->previous.x)*0.7f,360.0f);app->previous=cursor;
                }
                if(app->left)app->moveCachedDrag();return 0;
            case WM_LBUTTONUP:if(app->left){app->click=!app->dragged;if(app->click){const auto gesture=app->clickSequence.release(GetTickCount64(),cursor.x,cursor.y,GetDoubleClickTime(),GetSystemMetrics(SM_CXDOUBLECLK),GetSystemMetrics(SM_CYDOUBLECLK));app->doubleClickRequest|=gesture.doubleClick;app->tripleClickRequest|=gesture.tripleClick;}app->releaseMouse();}return 0;
            case WM_RBUTTONUP:if(app->menuPressed){const bool openMenu=!app->rightDragged;app->releaseMouse();if(BattleDesktopInput::instance)BattleDesktopInput::instance->mouseClick=false;if(openMenu)app->showQuickMenu();}return 0;
            case WM_MBUTTONUP:if(app->right&&!app->menuPressed)app->releaseMouse();return 0;
            case WM_CAPTURECHANGED:app->left=app->right=app->menuPressed=app->rightDragged=false;app->presentState(true);return 0;
            case WM_CANCELMODE:app->releaseMouse();return 0;
            case WM_MOUSEWHEEL:app->setZoom(app->zoom*std::pow(1.1f,GET_WHEEL_DELTA_WPARAM(w)/120.0f));return 0;
            case WM_HOTKEY:if(w==1){if(app->closeRequest)app->closeRequest();else DestroyWindow(hwnd);}else if(w==2&&!app->formationMode){if(app->followRequest)app->followRequest();else{app->preferences.follow=!app->preferences.follow;app->preferences.save(app->settings->ini);app->syncSettings();}}return 0;
            case WM_CLOSE:if(app->closeRequest)app->closeRequest();else DestroyWindow(hwnd);return 0;
            case WM_DESTROY:KillTimer(hwnd,1);if(app->quitHotkey)UnregisterHotKey(hwnd,1);if(app->lookHotkey)UnregisterHotKey(hwnd,2);app->done=true;app->window=nullptr;if(app->destroyedCallback)app->destroyedCallback();else if(!app->grouped)PostQuitMessage(0);return 0;
            default:return DefWindowProcW(hwnd,message,w,l);
            }
        }catch(const std::exception& e){std::fprintf(stderr,"Host: %s\n",e.what());std::fflush(stderr);app->exitCode=1;DestroyWindow(hwnd);return 0;}
    }
    static LRESULT CALLBACK inputProc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
        auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
        if(app&&app->window){
            switch(message){
            case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
            case WM_SYSCOMMAND:if((w&0xfff0)==SC_MINIMIZE)return 0;return DefWindowProcW(hwnd,message,w,l);
            case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_RBUTTONDOWN:case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:case WM_MBUTTONUP:case WM_MOUSEMOVE:case WM_MOUSEWHEEL:case WM_CAPTURECHANGED:case WM_CANCELMODE:
                return proc(app->window,message,w,l);
            }
        }
        return DefWindowProcW(hwnd,message,w,l);
    }
    void initialize(bool leader=true){
        if(!modelStorageSettings)modelStorageSettings=std::make_shared<NativeModelStorage>(programDirectory.empty()?directory:programDirectory,storage(),!dataDirectorySpecified);
        if(!autostartSettings)try{autostartSettings=std::make_shared<NativeAutostart>(NativeAutostart::launchExecutable(),dataDirectorySpecified?storage():std::filesystem::path{});}catch(const std::exception& e){autostartError=e.what();}
        state.log=[this](const std::string& text){if(!diagnostics)return;std::printf("%s\n",text.c_str());std::fflush(stdout);};
        state.log(smooth?"VARIANT smooth transitions + braking":"VARIANT regular");
        state.canWalk=[this](double y,double d){return canStartWalk(y,d);};
        std::filesystem::create_directories(storage()/L"behavior");
        const auto ini=storage()/L"settings.ini";
        if(!std::filesystem::exists(ini)&&std::filesystem::is_regular_file(directory/L"defaults.ini"))std::filesystem::copy_file(directory/L"defaults.ini",ini);
        settings=std::make_unique<PetSettings>(state,model.stem().wstring(),ini.wstring());
        menuHintVisible=!review&&leader&&GetPrivateProfileIntW(L"UI",L"MenuHintDismissed",0,ini.c_str())==0;
        settings->outlineTitle=L"角色线条";settings->outlineDescription=L"Wiki 网格描边，保留内部贴图线条";
        settings->modesEnabled=true;preferences.load(settings->ini);state.outlineEnabled=preferences.outlineMode!=0;
        nativeSettings=std::make_unique<NativeSettings>();nativeSettings->snapshot=[this]{return settingsSnapshot();};nativeSettings->receive=[this](const Json& j){settingsMessage(j);};
        nativeSettings->visibility=[this](bool visible){if(visible)dismissMenuHint();};
        nativeSettings->pauseChanged=[this](bool paused){state.settingsOpen=paused;if(!paused){placementNeeded=true;interactionUntil=preciseSeconds()+0.5;}};
        settings->importBusy=true;settings->progressDetail=L"初始化模型显示窗口";
        if(!std::filesystem::exists(ini))state.outlineEnabled=true;
        refreshDisplays();ground.modelZoom=zoom;ground.measuredBounds=true;geometry.prepareViewport(ground,zoom);geometry.place(ground,{ground.width*0.72f,ground.height*0.83f},zoom);geometry.update(ground,zoom,false);const auto crop=geometry.crop;windowWidth=geometry.width;windowHeight=geometry.height;
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BaCafeNativePet";wc.lpfnWndProc=proc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);
        // Independent layered display and alpha-shaped input windows.
        window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED|WS_EX_TRANSPARENT,wc.lpszClassName,L"BA Cafe Pets",WS_POPUP,
            work.left+int(crop.x),work.top+int(crop.y),windowWidth,windowHeight,nullptr,nullptr,wc.hInstance,this);
        if(!window)throw std::runtime_error("Cannot create desktop pet window");
        wc.lpszClassName=L"BaCafeNativeInput";wc.lpfnWndProc=inputProc;RegisterClassW(&wc);
        // No redirected bitmap, background brush or composition visual: this
        // window has no visible surface. Its region is exclusively for input.
        inputWindow=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_NOREDIRECTIONBITMAP,wc.lpszClassName,L"BA Cafe Pets input",WS_POPUP,
            work.left+int(crop.x),work.top+int(crop.y),windowWidth,windowHeight,nullptr,nullptr,wc.hInstance,this);
        if(!inputWindow)throw std::runtime_error("Cannot create desktop pet input window");
        const BOOL excludePeek=TRUE;
        for(HWND hwnd:{window,inputWindow})DwmSetWindowAttribute(hwnd,DWMWA_EXCLUDED_FROM_PEEK,&excludePeek,sizeof(excludePeek));
        SetWindowRgn(inputWindow,CreateRectRgn(0,0,0,0),FALSE);
        // High-resolution waitable timer drives motion at 125 Hz on the message
        // thread. No global timer-resolution change and no extra worker thread.
        if(displayed){ShowWindow(window,SW_SHOWNOACTIVATE);ShowWindow(inputWindow,SW_SHOWNOACTIVATE);}started=GetTickCount64();lastTick=preciseSeconds();
        if(leader)session.attach(window);
        if(leader)motionTimer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
        if(motionTimer) {
            LARGE_INTEGER due{};due.QuadPart=-80000;
            if(!SetWaitableTimer(motionTimer,&due,8,nullptr,nullptr,FALSE)){CloseHandle(motionTimer);motionTimer=nullptr;}
        }
        if(leader&&!motionTimer)SetTimer(window,1,10,nullptr);
        std::printf("HOST motion timer: %s; clock=steady\n",motionTimer?"high resolution 125 Hz":"Win32 fallback");std::fflush(stdout);
        if(leader){quitHotkey=RegisterHotKey(window,1,MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_NOREPEAT,'Q');lookHotkey=RegisterHotKey(window,2,MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_NOREPEAT,'L');}
        renderer=std::make_unique<nativepet::NativeRenderer>();renderer->diagnostics=diagnostics;requestModel(model);
    }
    int run(){
        initialize();
        while(!done){
            NativeHealth::mark("message wait");
            const bool paused=session.paused();
            DWORD wake;{NativeHealth::Pause idleWait;wake=MsgWaitForMultipleObjectsEx(motionTimer&&!paused?1:0,motionTimer&&!paused?&motionTimer:nullptr,500,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}
            if(wake==WAIT_FAILED)throw std::runtime_error("Motion/message wait failed");
            if(!drainNativeMessages([&](MSG& message){NativeHealth::mark("message dispatch");
                if(nativeSettings&&nativeSettings->route(message))return;
                TranslateMessage(&message);DispatchMessageW(&message);
            }))done=true;
            if(!done&&!session.paused()&&motionTimer&&(wake==WAIT_OBJECT_0||WaitForSingleObject(motionTimer,0)==WAIT_OBJECT_0)) {
                try{tick();}catch(const std::exception& e){std::fprintf(stderr,"Host: %s\n",e.what());exitCode=1;DestroyWindow(window);}
            }
        }return exitCode;
    }
    ~App(){
        *alive=false;destroyedCallback=depthRequest=sessionChanged={};session.detach();if(motionTimer){CancelWaitableTimer(motionTimer);CloseHandle(motionTimer);}
        if(settings)settings->window=nullptr;nativeSettings.reset();if(!grouped)battleInput.enable(false);
        battle.reset();behavior.reset();renderer.reset();
        if(inputWindow&&IsWindow(inputWindow))DestroyWindow(inputWindow);
        if(window&&IsWindow(window))DestroyWindow(window);
    }
};

#include "scene.h"

#ifndef PET_NATIVE_NO_ENTRY
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    int result=1;
    try{
        auto owned=std::make_unique<App>();auto& app=*owned;wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);app.directory=std::filesystem::path(executable).parent_path();
        wchar_t dataRoot[32768]{};if(GetEnvironmentVariableW(L"BA_NATIVE_DATA_DIR",dataRoot,32768)>0)app.dataDirectory=std::filesystem::absolute(dataRoot);
        if(app.dataDirectory.empty())app.dataDirectory=app.directory/L"data";
        wchar_t programRoot[32768]{};if(GetEnvironmentVariableW(L"BA_NATIVE_PROGRAM_DIR",programRoot,32768)>0)app.programDirectory=std::filesystem::absolute(programRoot);else app.programDirectory=app.directory;
        app.model=app.bundledResource();
        wchar_t saved[32768]{};GetPrivateProfileStringW(L"Model",L"Path",L"",saved,32768,(app.storage()/L"active-model.ini").c_str());
        if(*saved&&std::filesystem::is_regular_file(saved))app.model=saved;
        int argc=0;auto** argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        for(int i=1;i<argc;i++){
            const std::wstring flag=argv[i];
            if(flag==L"--data-dir"&&i+1<argc){app.dataDirectory=std::filesystem::absolute(argv[++i]);app.dataDirectorySpecified=true;}
            else if(flag==L"--model"&&i+1<argc){app.model=std::filesystem::absolute(argv[++i]);app.modelSpecified=true;}
            else if(flag==L"--clip"&&i+1<argc){app.reviewClip=utf8(argv[++i]);app.review=true;}
            else if(flag==L"--sample"&&i+1<argc){app.reviewTime=std::wcstod(argv[++i],nullptr);app.sampleSpecified=true;app.review=true;}
            else if(flag==L"--mode"&&i+1<argc){app.initialMode=utf8(argv[++i]);app.review=true;}
            else if(flag==L"--smooth")app.smooth=true;
            else if(flag==L"--serial-cpu")app.serialCpu=true;
            else if(flag==L"--diagnostics")app.diagnostics=true;
            else if(flag==L"--settings")app.settingsOnLoad=true;
            else if(flag==L"--zoom"&&i+1<argc)app.zoom=std::clamp(float(std::wcstod(argv[++i],nullptr)),0.45f,1.65f);
            else if(flag==L"--snapshot"&&i+1<argc){app.snapshot=std::filesystem::absolute(argv[++i]);app.review=true;}
            else if(flag==L"--duration"&&i+1<argc){app.runSeconds=std::clamp(std::wcstod(argv[++i],nullptr),1.0,20.0);app.review=true;}
            else if(flag==L"--exit-after"&&i+1<argc){app.runSeconds=std::clamp(std::wcstod(argv[++i],nullptr),1.0,20.0);}
        }LocalFree(argv);
        if(!app.initialMode.empty()&&app.initialMode!="cafe"&&app.initialMode!="battle"&&app.initialMode!="formation")throw std::runtime_error("Mode must be cafe, battle or formation");
        app.fixedPose=app.review&&(app.initialMode.empty()||!app.reviewClip.empty()||app.sampleSpecified);
        app.modelStorageSettings=std::make_shared<NativeModelStorage>(app.programDirectory,app.storage(),!app.dataDirectorySpecified);
        std::unique_ptr<NativeHealth> health;
        if(app.diagnostics){
            std::filesystem::create_directories(app.storage()/L"logs");
            _wfreopen((app.storage()/L"logs/pet.log").c_str(),L"a",stdout);_wfreopen((app.storage()/L"logs/pet.err").c_str(),L"a",stderr);
            health=std::make_unique<NativeHealth>(app.storage()/L"logs");
        }else{_wfreopen(L"NUL",L"w",stdout);_wfreopen(L"NUL",L"w",stderr);}
        SetTraceLogLevel(app.diagnostics?LOG_INFO:LOG_NONE);
        GraphicsContext graphics;
        Scene scene(std::move(owned));result=scene.run();
    }catch(const std::exception& e){std::fprintf(stderr,"Startup: %s\n",e.what());MessageBoxW(nullptr,wide(e.what()).c_str(),L"BA Cafe Pets",MB_ICONERROR);}
    return result;
}
#endif
