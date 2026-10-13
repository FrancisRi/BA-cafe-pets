#pragma once
#include "pet_behavior.h"
#include <cctype>

namespace pet {
inline std::string lowerClip(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// Discover capabilities from the loaded animations, never from the file name.
struct BattleClips {
    struct Pose {
        std::string name;
        int idle=-1, start=-1, delay=-1, attack=-1, end=-1, reload=-1, callsign=-1, moveEnd=-1;
    };
    std::vector<Pose> poses;
    int move=-1, jump=-1, pickup=-1, skill=-1, death=-1, retreat=-1, victoryStart=-1, victoryEnd=-1;
    std::string missing;
    int find(const Playback& state, const std::string& name) const {
        for (size_t i=0;i<state.clips.size();++i)
            if (lowerClip(state.clips[i].name)==lowerClip(name)) return static_cast<int>(i);
        return -1;
    }
    explicit BattleClips(const Playback& state) {
        for (const char* name : {"Normal", "Stand", "Kneel"}) {
            const std::string n=name;
            Pose p{n,find(state,n+"_Idle"),find(state,n+"_Attack_Start"),find(state,n+"_Attack_Delay"),
                find(state,n+"_Attack_Ing"),find(state,n+"_Attack_End"),find(state,n+"_Attack_Reload"),
                find(state,n+"_Callsign"),find(state,"Move_End_"+n)};
            if(p.reload<0)p.reload=find(state,n+"_Reload");
            // Only complete poses participate in random selection.
            if(p.idle>=0 && p.start>=0 && p.delay>=0 && p.attack>=0 && p.end>=0 && p.moveEnd>=0) poses.push_back(p);
        }
        move=find(state,"Move_Ing"); jump=find(state,"Move_Jump"); pickup=find(state,"Formation_Pickup");
        skill=find(state,"Exs"); death=find(state,"Vital_Death"); retreat=find(state,"Vital_Retreat");
        victoryStart=find(state,"Victory_Start"); victoryEnd=find(state,"Victory_End");
        if(poses.empty() || poses.front().name!="Normal") missing="Normal combat animations / Move_End_Normal";
        else if(move<0)missing="Move_Ing";
    }
    bool supported() const { return missing.empty(); }
};

struct BattleInput {
    bool dragging=false, petClick=false, mouseClick=false;
    bool keyPressed=false, keyboardActive=false, reload=false, windowClosed=false, boundary=false;
    double jumpOffset=0; // Advance into Jump so the crossing is exactly at its midpoint.
};

class Battle {
public:
    enum class Phase { Idle, Pickup, Moving, Jump, MoveEnd, AttackStart, AttackDelay, Attack,
        AttackEnd, Reload, Callsign, Skill, VictoryStart, VictoryEnd, Death, Retreat };
    BattleClips clips;
    double callsignProbability=0.10, skillProbability=0.05;
    bool attackOnKeyPress=false; // Opt-in for the Three.js host; raylib keeps its existing rules.
    bool bufferKeyPress=false,bufferReloadRequests=false,activeWalkAway=false;
    bool activeInputMove=false;double moveDuration=0,activeWalkDistanceScale=0.35;
    double walkTimeJitter=0,walkDistanceJitter=0;
    bool ignoreReloadKeyboard=false; // Three.js: reload plays without keyboard decisions or queued attacks.
    bool endBeforeReload=false, smoothStops=false;
    bool configuredTimers=false;
    std::optional<double> activityDelta; // Three.js wall time; animation dt stays bounded.
    void stopMovement(){movementSeconds=0;jumpMotion=false;idle();}
    double moveInterval=8,keyboardIdleSeconds=10;
    void setMoveInterval(double value){if(moveInterval!=value){moveInterval=value;moveWait=value*(walkTimeJitter>0?std::uniform_real_distribution<double>(1-walkTimeJitter,1+walkTimeJitter)(state.random):1);}}
    std::optional<double> moveSeconds;
    static constexpr double StopFallbackSeconds=0.7;
    // Distance under a speed curve: full speed in the first half, smoothstep
    // to zero in the second half. Integrating avoids frame-rate-dependent stops.
    static double stopDistance(double elapsed,double duration) {
        if(duration<=0)return 0;
        const double t=std::clamp(elapsed/duration,0.0,1.0);
        if(t<=0.5)return duration*t;
        const double u=2*t-1;
        return duration*(0.5+0.5*(u-u*u*u+0.5*u*u*u*u));
    }
    bool returnToCafe=false;
    Phase phase=Phase::Idle;
    double retreatProgress=0;
    double movementSeconds=0, movementYaw=0, movementSpeed=0;
    bool jumpMotion=false;
private:
    Playback& state;
    size_t pose=0;
    bool ready=false, wasDragging=false, skillSaved=false, pendingWindowClose=false;
    double clock=0, phaseTime=0, idleLimit=20, keyboardTime=0, skillTime=0;
    double clickStart=-10;
    double keyboardIdle=0;
    bool attackPending=false;
    bool bufferedAttack=false;double lastAttackRequest=-10;
    bool reloadPending=false,bufferedReload=false;double moveWait=8;
    double brakeRemaining=0, brakeDuration=0;
    int clicks=0, victoryLoops=0;
    struct Saved {
        Phase phase; int current; double time, phaseTime, remaining; bool loop, walking, ready;
    } saved{};
    void play(Phase next, int clip, bool loop=false, bool moving=false) {
        if(next==Phase::Reload)bufferedReload=false;
        if(next==Phase::Reload&&ignoreReloadKeyboard){bufferedAttack=false;keyboardIdle=skillTime=0;}
        phase=next;phaseTime=0;keyboardTime=0;
        if(clip<0) { idle();return; }
        state.start(clip,loop,moving);
        state.reaction=0; // Combat clips keep their original size and root motion.
        state.trackingCursor=false;
        state.log("BATTLE state="+std::to_string(static_cast<int>(next))+" pose="+clips.poses[pose].name);
    }
    bool complete() const { return !state.loop && state.time>=state.clips[state.current].duration; }
    void faceMouse() {
        if(state.trackingEnabled && state.lookTarget) state.facingRequest=state.lookYaw;
    }
    void idle(bool normal=false) {
        reloadPending=false;bufferedReload=false;brakeRemaining=0;activeInputMove=false;
        if(normal) { pose=0;ready=false; }
        phase=Phase::Idle;phaseTime=0;keyboardTime=0;
        state.start(clips.poses[pose].idle,true);state.reaction=0;
        state.trackingCursor=false;
        idleLimit=std::uniform_real_distribution<double>(10,30)(state.random);
        moveWait=moveInterval*(walkTimeJitter>0?std::uniform_real_distribution<double>(1-walkTimeJitter,1+walkTimeJitter)(state.random):1);
    }
    void move() {
        if(!state.walkEnabled || clips.move<0)return;
        // Short walks during input; longer excursions when no input is present.
        double duration=moveSeconds?*moveSeconds*(state.inputActive?activeWalkDistanceScale:1):state.inputActive ? std::uniform_real_distribution<double>(0.6,1.2)(state.random)
            : std::uniform_real_distribution<double>(2,4)(state.random);
        if(moveSeconds&&walkDistanceJitter>0)duration*=std::uniform_real_distribution<double>(1-walkDistanceJitter,1+walkDistanceJitter)(state.random);
        moveDuration=duration;
        double yaw=0;
        bool found=false;
        if(activeWalkAway&&state.inputActive){
            if(state.cursorTarget)for(double offset:{0.0,15.0,-15.0,30.0,-30.0,45.0,-45.0}){
                yaw=std::remainder(state.cursorYaw+180+offset,360.0);
                if(state.canWalk(yaw,0.18*state.movementScale)){found=true;break;}
            }
        }else for(int i=0;i<20;++i) {
            yaw=std::uniform_real_distribution<double>(-180,180)(state.random);
            if(state.cursorTarget && std::bernoulli_distribution(0.5)(state.random))yaw=state.cursorYaw+(configuredTimers&&state.inputActive?180:0);
            if(state.canWalk(yaw,0.18*state.movementScale)){found=true;break;}
        }
        if(!found) {idle();idleLimit=2;return;}
        activeInputMove=activeWalkAway&&state.inputActive;moveDuration=duration;
        play(Phase::Moving,clips.move,true,true);
        state.walkYaw=yaw;state.walkSpeed=0.792;state.walkRemaining=duration;
    }
    void endMove() {
        pose=std::uniform_int_distribution<size_t>(0,clips.poses.size()-1)(state.random);
        ready=true;
        if(smoothStops) {
            const int ending=clips.poses[pose].moveEnd;
            brakeDuration=brakeRemaining=ending>=0?state.clips[ending].duration:StopFallbackSeconds;
            play(Phase::MoveEnd,ending>=0?ending:clips.move,ending<0,true);
            state.walkRemaining=brakeRemaining;
        } else {play(Phase::MoveEnd,clips.poses[pose].moveEnd);if(state.inputActive)faceMouse();}
    }
    void prepareAttack(const BattleClips::Pose& p) {
        ready=true;attackPending=attackOnKeyPress;
        if(p.start>=0)play(Phase::AttackStart,p.start);
        else {attackPending=false;play(Phase::Attack,p.attack);}
    }
    void skill() {
        saved={phase,state.current,state.time,phaseTime,state.walkRemaining,state.loop,state.walking,ready};
        skillSaved=true;play(Phase::Skill,clips.skill);
    }
    void resumeSkill() {
        if(!skillSaved) {idle();return;}
        skillSaved=false;phase=saved.phase;phaseTime=saved.phaseTime;ready=saved.ready;
        state.start(saved.current,saved.loop,saved.walking);state.time=saved.time;
        state.walkRemaining=saved.remaining;state.reaction=0;
    }
public:
    explicit Battle(Playback& playback,std::optional<BattleClips> catalog={}):clips(catalog?*catalog:BattleClips(playback)),state(playback) {}
    int idleClip() const {return pose<clips.poses.size()?clips.poses[pose].idle:-1;}
    bool canCallsign() const {return phase==Phase::Idle&&!clips.poses.empty()&&(clips.poses[pose].callsign>=0||clips.poses.front().callsign>=0);}
    bool start() {
        if(!clips.supported())return false;
        returnToCafe=false;retreatProgress=0;clock=phaseTime=keyboardTime=skillTime=keyboardIdle=0;
        pendingWindowClose=false;movementSeconds=0;
        attackPending=false;
        bufferedAttack=false;lastAttackRequest=-10;
        reloadPending=false;bufferedReload=false;brakeRemaining=0;
        clicks=0;clickStart=-10;wasDragging=false;skillSaved=false;
        state.pendingClick=false;state.returnRequested=state.returningHome=state.returnArrived=false;
        state.dragging=false;state.trackingCursor=false;
        idle(true);return true;
    }
    bool exiting() const { return phase==Phase::Death || phase==Phase::Retreat; }
    void tick(double dt, const BattleInput& events={}) {
        const double activityDt=activityDelta.value_or(dt);activityDelta.reset();
        auto input=events;
        const bool reloading=ignoreReloadKeyboard&&phase==Phase::Reload;
        if(reloading){input.keyPressed=input.keyboardActive=input.reload=false;bufferedAttack=false;}
        if(bufferReloadRequests&&!state.settingsOpen&&!input.dragging&&input.reload&&!reloading
            &&(phase==Phase::AttackStart||phase==Phase::Attack||phase==Phase::AttackDelay||phase==Phase::AttackEnd))bufferedReload=true;
        movementSeconds=0;jumpMotion=false;
        if(returnToCafe)return;
        clock+=dt;
        if(bufferKeyPress&&attackOnKeyPress&&!state.settingsOpen&&!input.dragging&&input.keyPressed&&clock-lastAttackRequest>=0.12){bufferedAttack=true;lastAttackRequest=clock;}
        if(!reloading)keyboardIdle=input.keyPressed ? 0 : keyboardIdle+activityDt;
        if(input.petClick) {
            if(clock-clickStart>1.2) {clicks=0;clickStart=clock;}
            if(++clicks>=3) {
                clicks=0;skillSaved=false;state.dragging=false;
                if(clips.death>=0)play(Phase::Death,clips.death);
                else if(clips.retreat>=0)play(Phase::Retreat,clips.retreat);
                else returnToCafe=true;
            }
        }
        // Drag wins over every animation, including a skill or a retirement.
        if(input.dragging) {
            if(!wasDragging) {skillSaved=false;bufferedReload=false;retreatProgress=0;returnToCafe=false;play(Phase::Pickup,clips.pickup>=0?clips.pickup:clips.poses[0].idle,true);}
            wasDragging=true;state.dragging=true;state.advance(dt);return;
        }
        state.dragging=false;
        if(wasDragging) {wasDragging=false;skillSaved=false;idle(true);}
        pendingWindowClose|=input.windowClosed;
        if(pendingWindowClose && phase!=Phase::Jump && !exiting() && phase!=Phase::VictoryStart && phase!=Phase::VictoryEnd) {
            pendingWindowClose=false;
            skillSaved=false;bufferedReload=false;victoryLoops=0;
            if(clips.victoryStart>=0)play(Phase::VictoryStart,clips.victoryStart);
            else if(clips.victoryEnd>=0)play(Phase::VictoryEnd,clips.victoryEnd);
        }
        if(state.settingsOpen && phase!=Phase::Jump)return;
        if(smoothStops && phase==Phase::Moving && (!state.walkEnabled || (state.walkRemaining<=0 && !(input.boundary && clips.jump>=0))))endMove();
        // Plan before advancing time/motion, never after crossing the boundary.
        if(phase==Phase::Moving && state.walkEnabled && input.boundary && clips.jump>=0) {
            const double duration=state.clips[clips.jump].duration;
            play(Phase::Jump,clips.jump,false,true);
            state.time=phaseTime=std::clamp(input.jumpOffset,0.0,duration*0.5);
            state.walkRemaining=std::max(state.walkRemaining,duration-state.time);
            state.log("JUMP continuous: duration="+std::to_string(duration)+" offset="+std::to_string(state.time));
        }
        if(state.walking) {
            movementSeconds=std::min(dt,state.walkRemaining);
            movementYaw=state.walkYaw;movementSpeed=state.walkSpeed;
            jumpMotion=phase==Phase::Jump;
            if(smoothStops && phase==Phase::MoveEnd) {
                movementSeconds=std::min(dt,brakeRemaining);
                const double next=std::max(0.0,brakeRemaining-movementSeconds);
                if(movementSeconds>0)movementSpeed*= (stopDistance(brakeDuration-next,brakeDuration)
                    -stopDistance(brakeDuration-brakeRemaining,brakeDuration))/movementSeconds;
                brakeRemaining=next;
            }
        }
        state.advance(dt);phaseTime+=(configuredTimers&&phase==Phase::Idle)?activityDt:dt;
        if(smoothStops && phase==Phase::MoveEnd && state.walking && brakeRemaining<=0) {
            state.walking=false;
            if(state.inputActive)faceMouse();
        }
        if(phase==Phase::Retreat)retreatProgress=std::clamp(phaseTime/std::max(0.1,state.clips[state.current].duration),0.0,1.0);
        if(phase==Phase::Skill) {if(complete())resumeSkill();return;}
        if(exiting()) {
            if(complete()) {
                if(phase==Phase::Death && clips.retreat>=0)play(Phase::Retreat,clips.retreat);
                else {returnToCafe=true;state.walking=false;state.log("BATTLE retirement complete: cafe mode");}
            }
            return;
        }
        if(phase==Phase::VictoryStart || phase==Phase::VictoryEnd) {
            if(complete()) {
                if(phase==Phase::VictoryStart && clips.victoryEnd>=0)play(Phase::VictoryEnd,clips.victoryEnd);
                else if(phase==Phase::VictoryEnd && ++victoryLoops<3)play(Phase::VictoryEnd,clips.victoryEnd);
                else idle(true);
            }
            return;
        }
        if(!reloading){if(state.inputActive)skillTime+=activityDt;else skillTime=0;}
        if(!reloading&&skillTime>=(configuredTimers?state.activeSecondsOverride.value_or(30):30) && phase!=Phase::Jump) {
            skillTime=0;
            if(clips.skill>=0 && std::bernoulli_distribution(skillProbability)(state.random)) {skill();return;}
        }
        const auto& p=clips.poses[pose];
        switch(phase) {
        case Phase::Idle:
            if((ready || attackOnKeyPress) && (input.keyPressed||bufferedAttack)) {
                bufferedAttack=false;
                prepareAttack(p);
            }
            else if(input.mouseClick && std::bernoulli_distribution(callsignProbability)(state.random)) {
                int call=p.callsign;
                if(call<0) {call=clips.poses[0].callsign;if(call>=0)pose=0;}
                if(call>=0) {play(Phase::Callsign,call);faceMouse();}
            } else if(state.walkEnabled && (configuredTimers?
                (phaseTime>=moveWait&&((state.inputActive&&state.activeInputSeconds>=state.activeSecondsOverride.value_or(30))
                    ||(!state.inputActive&&state.inactiveSeconds>=state.inactivityThreshold()))):
                ((state.inputActive && phaseTime>=3 && state.activeInputSeconds>=3)
                || (!state.inputActive && phaseTime>=idleLimit && state.inactiveSeconds>=idleLimit))))move();
            break;
        case Phase::Moving:
            if(!state.walkEnabled)endMove();
            else if(state.walkRemaining<=0)endMove();
            break;
        case Phase::Jump:
            if(complete()) {
                if(state.walkEnabled && state.walkRemaining>0)play(Phase::Moving,clips.move,true,true);
                else endMove();
            }
            break;
        case Phase::MoveEnd:
            if(!state.walking && (complete() || state.loop))idle();
            break;
        case Phase::AttackEnd:
            if(complete()) {
                if(reloadPending||(bufferReloadRequests&&bufferedReload&&p.reload>=0)) {reloadPending=false;play(Phase::Reload,p.reload);}
                else idle();
            }
            break;
        case Phase::Reload: case Phase::Callsign:
            if(complete())idle();
            break;
        case Phase::AttackStart:
            if(complete()) {
                if(attackOnKeyPress && attackPending) {attackPending=false;play(Phase::Attack,p.attack);}
                else play(Phase::AttackDelay,p.delay,true);
            }
            break;
        case Phase::AttackDelay:
            if((input.reload||(bufferReloadRequests&&bufferedReload)) && p.reload>=0) {
                bufferedReload=false;
                if(endBeforeReload && p.end>=0){reloadPending=true;play(Phase::AttackEnd,p.end);}
                else play(Phase::Reload,p.reload);
            }
            else if(keyboardIdle>=(configuredTimers?keyboardIdleSeconds:10))play(Phase::AttackEnd,p.end);
            else if(attackOnKeyPress && (input.keyPressed||bufferedAttack)){bufferedAttack=false;play(Phase::Attack,p.attack);}
            else {
                keyboardTime=input.keyboardActive ? keyboardTime+dt : 0;
                if(!attackOnKeyPress && keyboardTime>=3)play(Phase::Attack,p.attack);
            }
            break;
        case Phase::Attack:
            if(complete())play(Phase::AttackDelay,p.delay,true);
            break;
        default: break;
        }
    }
};
} // namespace pet
