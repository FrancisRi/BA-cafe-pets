#pragma once
#include <algorithm>
#include <cmath>

#ifdef BA_RUST_CROWD_STEERING
struct StopTurn;
extern "C" void ba_rust_stop_turn_begin(StopTurn*,double,double,double);
extern "C" double ba_rust_stop_turn_advance(StopTurn*,double);
#endif
// Freeze the desired stopping direction once; later cursor movement must not
// continually bend the path or spin the actor as it comes to rest.
struct StopTurn {
    static constexpr double LeadSeconds=0.45;
    bool active=false;
    double from=0,target=0,yaw=0,elapsed=0,duration=LeadSeconds;
    void reset(){active=false;}
    void begin(double current,double desired,double seconds=LeadSeconds){
#ifdef BA_RUST_CROWD_STEERING
        ba_rust_stop_turn_begin(this,current,desired,seconds);
#else
        active=true;from=current;yaw=current;
        target=current+std::remainder(desired-current,360.0);
        elapsed=0;duration=std::max(0.008,seconds);
#endif
    }
    // The segment's middle heading integrates the curved trajectory; the
    // displayed heading is its endpoint tangent and remains fixed at rest.
    double advance(double dt){
#ifdef BA_RUST_CROWD_STEERING
        return ba_rust_stop_turn_advance(this,dt);
#else
        const double previous=yaw;
        elapsed=std::min(duration,elapsed+std::max(0.0,dt));
        const double t=elapsed/duration,u=t*t*(3-2*t);
        yaw=from+(target-from)*u;
        return std::remainder((previous+yaw)*0.5,360.0);
#endif
    }
};
#ifdef BA_RUST_CROWD_STEERING
static_assert(sizeof(StopTurn)==48);
#endif
