#include "pet_battle.h"
#include "pet_ground.h"
#include "rlgl.h"
#include <iostream>

void require(bool result,const char* text) {if(!result)throw std::runtime_error(text);}
pet::Playback model() {
    pet::Playback s;s.random.seed(17);
    for(const char* name:{"Cafe_Idle","Formation_Pickup","Normal_Idle","Normal_Attack_Start","Normal_Attack_Delay",
        "Normal_Attack_Ing","Normal_Attack_End","Normal_Reload","Normal_Callsign","Move_End_Normal","Move_Ing","Move_Jump",
        "Stand_Idle","Stand_Attack_STart","Stand_Attack_Delay","Stand_Attack_Ing","Stand_Attack_End","Stand_Reload","Move_End_Stand",
        "Exs","Vital_Death","Vital_Retreat","Victory_Start","Victory_End"})s.clips.push_back({name,1,static_cast<int>(s.clips.size())});
    return s;
}
using Phase=pet::Battle::Phase;
void settled(pet::Battle& b,pet::Playback& s) {
    s.inputActive=true;s.activeInputSeconds=10;
    b.tick(3.1);require(b.phase==Phase::Moving,"Continuous input must start a short walk");
    b.tick(1.3);require(b.phase==Phase::MoveEnd,"Walk must select a complete ending pose");
    b.tick(1.1);require(b.phase==Phase::Idle,"Ending must return to posture Idle");
}
int main() {
 try {
    pet::Playback hibiki;hibiki.clips={{"Cafe_Idle",1,0},{"Exs",1,1},{"Formation_Pickup",1,2}};
    pet::Battle unsupported(hibiki);require(!unsupported.start(),"An Exs clip alone must not enable battle (Hibiki)");
    auto s=model();pet::Battle b(s);b.skillProbability=b.callsignProbability=0;
    require(b.clips.poses.size()==2,"Mixed-case Stand_Attack_STart must be recognized");
    require(b.start(),"Complete battle model must start");
    require(s.clips[s.current].name=="Normal_Idle","Entry must use Normal_Idle");
    settled(b,s);
    pet::BattleInput key;key.keyPressed=true;key.keyboardActive=true;b.tick(0,key);
    require(b.phase==Phase::AttackStart,"Keyboard input from posture Idle must prepare attack");
    b.tick(1.1);require(b.phase==Phase::AttackDelay && s.loop,"Preparation must enter looping Attack_Delay");
    b.tick(2.9,key);require(b.phase==Phase::AttackDelay,"Must wait for three seconds of keyboard input");
    b.tick(0.2,key);require(b.phase==Phase::Attack,"Three seconds of keyboard input must attack");
    b.tick(1.1);require(b.phase==Phase::AttackDelay,"Attack must return to Delay");
    pet::BattleInput reload;reload.reload=true;reload.keyboardActive=true;b.tick(0,reload);
    require(b.phase==Phase::Reload,"Punctuation must reload using the model's actual Reload clip");
    b.tick(1.1);require(b.phase==Phase::Idle,"Reload must return to posture Idle");
    b.tick(0,key);b.tick(1.1);s.inactiveSeconds=0;
    pet::BattleInput mouseOnly;mouseOnly.mouseClick=true;
    b.tick(8.8,mouseOnly);require(b.phase==Phase::AttackDelay,"Keyboard idle threshold must not end early");
    b.tick(0.2,mouseOnly);
    require(b.phase==Phase::AttackEnd,"Ten seconds without keyboard must end attack even with mouse input");
    b.tick(1.1);require(b.phase==Phase::Idle,"Attack_End must return to posture Idle");

    b.start();s.inactiveSeconds=0;settled(b,s);
    b.callsignProbability=1;s.lookTarget=true;s.lookYaw=73;
    pet::BattleInput click;click.mouseClick=true;b.tick(0,click);
    require(b.phase==Phase::Callsign && s.facingRequest==73,"Callsign must face the eye-height cursor target");
    b.tick(1.1);require(b.phase==Phase::Idle,"Callsign must finish in Idle");

    b.start();b.skillProbability=1;b.tick(30.1);
    require(b.phase==Phase::Skill,"Sustained input must provide a low-probability skill opportunity");
    b.tick(1.1);require(b.phase==Phase::Idle,"Skill must restore its interrupted state");
    pet::BattleInput drag;drag.dragging=true;b.tick(0,drag);
    require(b.phase==Phase::Pickup && !s.walking,"Drag must interrupt all animations");
    const auto owner=s.generation;b.tick(5,drag);
    require(owner==s.generation,"Holding drag must not restart Pickup each frame");
    b.tick(0);require(b.phase==Phase::Idle && s.clips[s.current].name=="Normal_Idle","Drag release must force Normal_Idle");

    b.skillProbability=0;b.start();pet::BattleInput closed;closed.windowClosed=true;b.tick(0,closed);
    require(b.phase==Phase::VictoryStart,"Closing an application must trigger Victory_Start");
    b.tick(1.1);require(b.phase==Phase::VictoryEnd,"Victory_Start must be followed by Victory_End");
    b.tick(1.1);require(b.phase==Phase::VictoryEnd,"First Victory_End must repeat");
    b.tick(1.1);require(b.phase==Phase::VictoryEnd,"Second Victory_End must repeat");
    b.tick(1.1);require(b.phase==Phase::Idle,"Exactly three Victory_End cycles must return to Normal_Idle");
    b.start();pet::BattleInput petClick;petClick.petClick=true;
    b.tick(0.1,petClick);b.tick(0.1,petClick);b.tick(0.1,petClick);
    require(b.phase==Phase::Death,"Three quick pet clicks must force Death");
    b.tick(1.1);require(b.phase==Phase::Retreat,"Death must be followed by Retreat");
    b.tick(0.5);require(b.retreatProgress>0 && b.retreatProgress<1,"Retreat must expose an ascent progress");
    b.tick(0.6);require(b.returnToCafe,"Retreat must finish in cafe mode");
    b.start();b.tick(0.1,petClick);b.tick(1.3,petClick);b.tick(0.1,petClick);
    require(!b.exiting(),"Separated clicks must not be counted as a triple");

    b.start();settled(b,s);b.tick(3.1);require(b.phase==Phase::Moving,"Input walk must repeat after idle pause");
    pet::BattleInput edge;edge.boundary=true;b.tick(0,edge);
    require(b.phase==Phase::Jump && s.walking && !s.loop,"Predicted boundary must play Jump once while moving");
    const auto jumpGeneration=s.generation;const auto jumpYaw=s.walkYaw;const auto speed=s.walkSpeed;
    s.walkEnabled=false;s.settingsOpen=true;edge.windowClosed=true;
    b.tick(0.4,edge);
    require(b.phase==Phase::Jump && s.generation==jumpGeneration && b.movementSeconds==0.4
        && s.walkYaw==jumpYaw && s.walkSpeed==speed,"Jump must keep moving without restarting, turning or pausing");
    edge.windowClosed=false;b.tick(0.61,edge);s.settingsOpen=false;
    require(b.phase==Phase::MoveEnd,"Turning walking off during Jump must finish movement");
    b.tick(0);require(b.phase==Phase::VictoryStart,"Window-close reaction must be deferred until landing");
    b.start();s.walkEnabled=false;
    b.tick(1.1);b.tick(60);require(b.phase==Phase::Idle,"Walking toggle must suppress new walks");

    s.walkEnabled=true;b.start();s.inputActive=true;s.activeInputSeconds=10;b.tick(3.1);
    s.walkRemaining=3;edge.windowClosed=false;edge.jumpOffset=0.1;b.tick(0.05,edge);
    require(std::abs(s.time-0.15)<1e-8 && b.movementSeconds==0.05,"Jump offset must align animation with crossing time");
    const auto direction=s.walkYaw;b.tick(0.86,edge);
    require(b.phase==Phase::Moving && s.walking && s.walkYaw==direction,"Landing must resume the same walking direction");

    auto noJump=model();noJump.clips.erase(noJump.clips.begin()+noJump.find("Move_Jump"));
    pet::Battle noJumpBattle(noJump);noJumpBattle.skillProbability=0;noJumpBattle.start();
    noJump.inputActive=true;noJump.activeInputSeconds=10;noJumpBattle.tick(3.1);
    const auto noJumpGeneration=noJump.generation;noJumpBattle.tick(0.1,edge);
    require(noJumpBattle.phase==Phase::Moving && noJump.walking && noJump.generation==noJumpGeneration,
        "A model without Jump must ignore boundary events and continue moving");

    // Run the same prediction + ground integration used by both native hosts.
    // Crossing must happen halfway through one Jump, with no lost movement frame.
    rlSetClipPlanes(PetGround::NearPlane,PetGround::FarPlane);
    for(float heading:{90.0f,135.0f,45.0f}) {
        PetGround ground;ground.width=3840;ground.height=1080;
        ground.monitors={{0,0,1920,1080},{1920,0,3840,1080}};
        ground.place({1850,750});
        auto actor=model();pet::Battle machine(actor);machine.skillProbability=0;machine.start();
        actor.inputActive=true;actor.activeInputSeconds=10;machine.tick(3.1);
        actor.walkYaw=heading;actor.walkRemaining=4;actor.movementScale=pet::MovementSpeedMultiplier*0.85;
        const double velocity=actor.walkSpeed*actor.movementScale;
        int jumps=0,crossings=0;double traveled=0;
        for(int frame=0;frame<210;++frame) {
            pet::BattleInput predicted;
            if(machine.phase==Phase::Moving) {
                const float d=ground.boundaryDistance(heading,float(velocity*0.5),ground.monitors);
                if(std::isfinite(d)){predicted.boundary=true;predicted.jumpOffset=std::max(0.0,0.5-d/velocity);}
            }
            const auto oldPhase=machine.phase;const auto before=ground.screen(ground.position);
            machine.tick(1.0/60,predicted);
            if(oldPhase!=Phase::Jump && machine.phase==Phase::Jump)++jumps;
            const double step=velocity*machine.movementSeconds;
            ground.position=ground.step(float(machine.movementYaw),float(step));traveled+=step;
            const auto after=ground.screen(ground.position);
            if(before.x<1920 && after.x>=1920) {
                ++crossings;
                require(machine.phase==Phase::Jump && std::abs(actor.time-0.5)<0.02,
                    "Monitor crossing must be at the Jump animation midpoint");
            }
        }
        require(jumps==1 && crossings==1,"One crossing must produce exactly one Jump");
        require(std::abs(traveled-velocity*3.5)<0.0001,"Jump must not lose movement time at transitions");
    }

    s.walkEnabled=true;s.inputActive=false;s.inactiveSeconds=90;b.start();b.tick(9);
    require(b.phase==Phase::Idle,"No-input movement must not begin before ten seconds");
    b.tick(21);require(b.phase==Phase::Moving,"No-input movement must begin within thirty seconds");
    b.tick(4.1);b.tick(1.1);b.tick(1);
    require(b.phase==Phase::Idle,"Each no-input excursion must include an idle pause");
    s.settingsOpen=true;const auto time=s.time;b.tick(60,key);
    require(s.time==time,"Settings must pause battle animation and decisions");
    auto three=model();pet::Battle immediate(three);immediate.attackOnKeyPress=true;
    immediate.skillProbability=0;immediate.start();
    immediate.tick(0,key);
    require(immediate.phase==Phase::AttackStart,"Three.js must accept the first key from initial Normal_Idle");
    immediate.tick(1.1);
    require(immediate.phase==Phase::Attack,"A single Three.js keypress must prepare then execute an attack");
    immediate.tick(1.1);
    require(immediate.phase==Phase::AttackDelay,"Attack must return to looping Delay");
    immediate.tick(0.001,key);
    require(immediate.phase==Phase::Attack,"A single key in Three.js Delay must attack without a three-second hold");
    auto queued=model();pet::Battle buffered(queued);buffered.attackOnKeyPress=true;buffered.bufferKeyPress=true;buffered.skillProbability=0;buffered.start();
    buffered.tick(0,key);buffered.tick(0.3,key);buffered.tick(0.3,key);buffered.tick(0.5);
    require(buffered.phase==Phase::Attack,"Buffered keys during preparation must preserve the first attack");
    buffered.tick(1.1);buffered.tick(0.01);
    require(buffered.phase==Phase::Attack,"A key during preparation must produce one pending attack at the next Delay");
    buffered.tick(1.1);buffered.tick(0.01);
    require(buffered.phase==Phase::AttackDelay,"Repeated keys must coalesce, not accumulate a long queue");
    queued.settingsOpen=true;buffered.tick(0.3,key);queued.settingsOpen=false;buffered.tick(0.01);
    require(buffered.phase==Phase::AttackDelay,"Settings input must not enqueue an attack");
    auto legacy=model();pet::Battle legacyBattle(legacy);legacyBattle.start();legacyBattle.tick(0,key);
    require(legacyBattle.phase==Phase::Idle,"Raylib must keep its original default input rules");
    auto adapted=model();pet::BattleClips sparse(adapted);sparse.poses={sparse.poses[1]};
    sparse.poses[0].start=sparse.poses[0].end=sparse.poses[0].moveEnd=-1;
    sparse.poses[0].reload=adapted.find("Normal_Reload"); // Fixture's authored Stand Reload replacement.
    sparse.missing.clear();
    pet::Battle flexible(adapted,sparse);flexible.attackOnKeyPress=true;flexible.endBeforeReload=true;
    flexible.skillProbability=0;require(flexible.start(),"A model with only a partial Stand group must remain usable");
    flexible.tick(0,key);require(flexible.phase==Phase::Attack,"Missing attack start must skip to the actual Attack");
    flexible.tick(1.1);flexible.tick(0,reload);
    require(flexible.phase==Phase::Reload,"Missing attack end must skip safely to Reload");
    flexible.tick(1.1);
    auto chain=model();pet::Battle reloadChain(chain);reloadChain.attackOnKeyPress=true;reloadChain.endBeforeReload=true;
    reloadChain.skillProbability=0;reloadChain.start();reloadChain.tick(0,key);reloadChain.tick(1.1);reloadChain.tick(1.1);
    reloadChain.tick(0,reload);require(reloadChain.phase==Phase::AttackEnd,"Three.js reload must play Attack_End first");
    reloadChain.tick(1.1);require(reloadChain.phase==Phase::Reload,"Attack_End must lead to the queued Reload");
    reloadChain.tick(1.1);require(reloadChain.phase==Phase::Idle,"Reload chain must finish in posture Idle");
    auto protectedReload=model();pet::Battle uninterrupted(protectedReload);
    uninterrupted.attackOnKeyPress=true;uninterrupted.bufferKeyPress=true;uninterrupted.endBeforeReload=true;uninterrupted.ignoreReloadKeyboard=true;
    uninterrupted.skillProbability=0;uninterrupted.start();uninterrupted.tick(0,key);uninterrupted.tick(1.1);uninterrupted.tick(1.1);
    uninterrupted.tick(0,reload);uninterrupted.tick(1.1,key);
    require(uninterrupted.phase==Phase::Reload,"Reload must clear keys queued during its preceding Attack_End");
    protectedReload.inputActive=true;protectedReload.activeSecondsOverride=0.1;
    uninterrupted.configuredTimers=true;uninterrupted.skillProbability=1;
    for(int i=0;i<5;++i){uninterrupted.tick(0.1,key);require(uninterrupted.phase==Phase::Reload,"Keyboard input must neither interrupt Reload nor trigger EX during it");}
    uninterrupted.tick(0.6,reload);require(uninterrupted.phase==Phase::Idle,"Even punctuation on Reload's final frame must only finish the reload");
    protectedReload.inputActive=false;uninterrupted.skillProbability=0;uninterrupted.tick(0.01);
    require(uninterrupted.phase==Phase::Idle,"Reload input must not trigger a buffered attack after completion");
    uninterrupted.tick(0.2,key);require(uninterrupted.phase==Phase::AttackStart,"Fresh keyboard input after Reload must restore normal attack decisions");
    auto braking=model();pet::Battle eased(braking);eased.smoothStops=true;eased.skillProbability=0;eased.start();
    braking.inputActive=true;braking.activeInputSeconds=10;eased.tick(3.1);braking.walkRemaining=0;
    const double fullSpeed=braking.walkSpeed;double lastSpeed=fullSpeed,brakeDistance=0;
    for(int frame=0;frame<100;++frame) {
        eased.tick(0.01);require(eased.movementSpeed<=lastSpeed+1e-8,"Braking speed must decrease monotonically");
        if(frame<50)require(std::abs(eased.movementSpeed-fullSpeed)<1e-8,"First half of Move_End must retain full movement speed");
        brakeDistance+=eased.movementSpeed*eased.movementSeconds;lastSpeed=eased.movementSpeed;
    }
    require(!braking.walking && std::abs(brakeDistance-fullSpeed*0.75)<1e-8,"Second-half smoothstep braking must integrate to 75 percent of full-speed travel");
    auto timed=model();pet::Battle configured(timed);configured.configuredTimers=true;
    configured.moveInterval=2;configured.moveSeconds=6;configured.keyboardIdleSeconds=4;
    configured.skillProbability=0;configured.attackOnKeyPress=true;timed.idleSecondsOverride=5;timed.activeSecondsOverride=7;
    configured.start();timed.inputActive=false;timed.inactiveSeconds=4;configured.tick(2.1);
    require(configured.phase==Phase::Idle,"Configured walking must wait for the global input idle threshold");
    timed.inactiveSeconds=5;configured.tick(0);
    require(configured.phase==Phase::Moving&&std::abs(timed.walkRemaining-6)<1e-8,"Configured movement distance must set excursion duration");
    configured.start();timed.inputActive=true;timed.activeInputSeconds=6;configured.tick(2.1);
    require(configured.phase==Phase::Idle,"Configured active walking must wait for the continued input threshold");
    timed.activeInputSeconds=7;configured.tick(0);
    require(configured.phase==Phase::Moving&&std::abs(timed.walkRemaining-2.1)<1e-8,"Active-input movement must be 35 percent of the configured distance");
    configured.activeWalkDistanceScale=0.5;configured.start();configured.tick(2.1);
    require(configured.phase==Phase::Moving&&std::abs(timed.walkRemaining-3)<1e-8,"Three.js active walk must be 50 percent of configured normal distance");
    configured.start();configured.tick(0,key);configured.tick(1.1);configured.tick(1.1);
    configured.tick(1.7);
    require(configured.phase==Phase::AttackDelay,"Configured attack end must not run before the keyboard idle timeout");
    configured.tick(0.2);require(configured.phase==Phase::AttackEnd,"Configured keyboard idle time must control attack end");
    configured.start();timed.inputActive=true;timed.activeInputSeconds=8;
    configured.activityDelta=8;configured.tick(0.01);
    require(configured.phase==Phase::Moving,"Continuous input must trigger a move using wall time even after a slow frame");
    for(const auto phase:{Phase::AttackStart,Phase::Attack,Phase::AttackEnd}){
        auto queuedState=model();pet::Battle queued(queuedState);queued.attackOnKeyPress=true;queued.endBeforeReload=true;
        queued.ignoreReloadKeyboard=true;queued.bufferReloadRequests=true;queued.skillProbability=0;queuedState.walkEnabled=false;queued.start();
        queued.phase=phase;queuedState.start(queuedState.find(phase==Phase::AttackStart?"Normal_Attack_Start":phase==Phase::Attack?"Normal_Attack_Ing":"Normal_Attack_End"),false);
        pet::BattleInput request;request.reload=true;queued.tick(0.01,request);
        for(int frame=0;frame<40&&queued.phase!=Phase::Reload;++frame)queued.tick(0.1);
        require(queued.phase==Phase::Reload,"Reload requested in any attack stage must survive until Reload");
        queued.tick(0.1,request);queued.tick(1.1);
        require(queued.phase==Phase::Idle,"Reload input during Reload must not queue another reload");
    }
    auto varied=model();pet::Battle variation(varied);variation.configuredTimers=true;variation.moveSeconds=4;variation.moveInterval=10;
    variation.walkDistanceJitter=0.15;variation.walkTimeJitter=0.1;variation.skillProbability=0;varied.inputActive=false;varied.inactiveSeconds=1000;varied.idleSecondsOverride=1;
    bool changed=false;
    for(int trial=0;trial<12;++trial){variation.start();variation.tick(11.01);require(variation.phase==Phase::Moving&&varied.walkRemaining>=3.4&&varied.walkRemaining<=4.6,"Random movement distance must stay in configured bounds");changed|=std::abs(varied.walkRemaining-4)>0.001;}
    require(changed,"Configured distance variation must actually vary movement");
    auto retreating=model();pet::Battle away(retreating);away.activeWalkAway=true;away.configuredTimers=true;
    away.setMoveInterval(0);away.moveSeconds=2;away.skillProbability=0;retreating.activeSecondsOverride=0;
    retreating.inputActive=true;retreating.cursorTarget=true;retreating.cursorYaw=37;
    for(int trial=0;trial<30;++trial){away.start();away.tick(0);require(away.phase==Phase::Moving&&away.activeInputMove,
        "Every active-input walk must use the away route");require(std::abs(std::remainder(retreating.walkYaw-217,360.0))<1e-8,"Active walk must begin directly away from the cursor, without a probability roll");}
    retreating.canWalk=[](double heading,double){return std::abs(std::remainder(heading-37,360.0))<30;};away.start();away.tick(0);
    require(away.phase==Phase::Idle,"When every away heading is blocked, do not fall back to walking toward the cursor");
    std::cout<<"PASS: battle capabilities, case aliases, moves, input attacks/reload, cancellation, skill, victory, retirement, configurable timers\n";
    return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
