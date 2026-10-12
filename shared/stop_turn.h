#pragma once
#include <algorithm>
#include <cmath>

// Freeze the desired stopping direction once; later cursor movement must not
// continually bend the path or spin the actor as it comes to rest.
struct StopTurn {
    static constexpr double LeadSeconds=0.45;
    bool active=false;
    double from=0,target=0,yaw=0,elapsed=0,duration=LeadSeconds;
    void reset(){active=false;}
    void begin(double current,double desired,double seconds=LeadSeconds){
        active=true;from=current;yaw=current;
        target=current+std::remainder(desired-current,360.0);
        elapsed=0;duration=std::max(0.008,seconds);
    }
    // The segment's middle heading integrates the curved trajectory; the
    // displayed heading is its endpoint tangent and remains fixed at rest.
    double advance(double dt){
        const double previous=yaw;
        elapsed=std::min(duration,elapsed+std::max(0.0,dt));
        const double t=elapsed/duration,u=t*t*(3-2*t);
        yaw=from+(target-from)*u;
        return std::remainder((previous+yaw)*0.5,360.0);
    }
};
