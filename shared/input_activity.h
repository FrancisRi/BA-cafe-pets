#pragma once
#include "pet_behavior.h"
// A typing/mouse episode survives short quiet gaps. The inactive state cannot
// toggle repeatedly inside that same episode, even with a 1s idle setting.
inline bool observeDebouncedInput(pet::Playback& state,double idle,double dt,double grace){
    const bool active=idle<=grace,resumed=active&&state.noInputState;
    state.inactiveSeconds=active?0:idle;
    if(!active&&idle>=state.inactivityThreshold()&&!state.noInputState){state.noInputState=true;state.trackingCursor=false;}
    if(resumed)state.noInputState=false;
    if(active&&!state.inputActive){++state.inputEpoch;state.activeInputSeconds=0;}
    else if(active)state.activeInputSeconds+=dt;
    else state.activeInputSeconds=0;
    state.inputActive=active;return resumed;
}
