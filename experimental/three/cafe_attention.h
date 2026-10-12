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
        if(!chosen||!state.lookTarget){pause();return;}
        if(!threshold){pause();yaw=PetGround::turnToward(yaw,float(state.lookYaw),float(dt));return;}
        if(!turning&&std::abs(std::remainder(float(state.lookYaw)-yaw,360.0f))>=30){
            turning=true;targetYaw=float(state.lookYaw);time=0;++serial;
        }
        if(turning){
            yaw=PetGround::turnToward(yaw,targetYaw,float(dt));time+=dt;
            if(std::abs(std::remainder(targetYaw-yaw,360.0f))<0.5f){yaw=targetYaw;pause();}
        }
    }
};
