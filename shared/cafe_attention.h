#pragma once
#include "pet_ground.h"
#include "pet_behavior.h"
// Host-only orientation tracking. A turning walk is a presentation override,
// so it never changes the ground anchor or steals the behavior tree's action.
struct CafeAttention {
    bool chosen=false,turning=false;
    uint64_t epoch=UINT64_MAX,serial=0;
    double time=0;
    float targetYaw=0;
    void pause(){if(turning){turning=false;++serial;}time=0;}
    void reset(){pause();chosen=false;epoch=UINT64_MAX;}
    void update(pet::Playback& state,float& yaw,double dt,bool eligible,bool threshold,bool enabled=true){
        if(!state.inputActive||!enabled){reset();return;}
        if(state.inputEpoch!=epoch){pause();chosen=false;epoch=state.inputEpoch;}
        if(!eligible){pause();return;}
        chosen=state.activeInputSeconds>=state.activeSecondsOverride.value_or(30);
        if(!chosen||!state.lookTarget||!std::isfinite(state.lookYaw)||!std::isfinite(dt)){pause();return;}
        dt=std::max(0.0,dt);
        if(!threshold){pause();
            const float delta=std::remainder(float(state.lookYaw)-yaw,360.0f);
            yaw=std::remainder(yaw+delta*float(-std::expm1(-std::max(0.0,dt)/0.08)),360.0f);return;
        }
        if(!turning&&std::abs(std::remainder(float(state.lookYaw)-yaw,360.0f))>=30){
            turning=true;targetYaw=float(state.lookYaw);time=0;++serial;
        }
        if(turning){
            // Keep the thirty-degree entry threshold, but do not finish an
            // obsolete turn after the pointer has already moved elsewhere.
            targetYaw=float(state.lookYaw);
            yaw=PetGround::turnToward(yaw,targetYaw,float(dt));time+=dt;
            if(std::abs(std::remainder(targetYaw-yaw,360.0f))<0.5f){yaw=targetYaw;pause();}
        }
    }
};
