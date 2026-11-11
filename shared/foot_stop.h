#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#ifdef BA_RUST_CROWD_STEERING
#include <array>
extern "C" bool ba_rust_foot_stop_initialize(double,const double*,size_t,double*);
extern "C" double ba_rust_foot_stop_distance(double,const double*,const double*,size_t,double);
#endif

struct FootStopProfile {
    double stopTime=0.28;
    std::vector<double> speeds;
    std::vector<double> distances;
    bool initialize(double seconds,std::vector<double> values){
#ifdef BA_RUST_CROWD_STEERING
        std::array<double,256> integrated{};
        if(!ba_rust_foot_stop_initialize(seconds,values.data(),values.size(),integrated.data()))return false;
        stopTime=seconds;speeds=std::move(values);distances.assign(integrated.begin(),integrated.begin()+speeds.size());
        return true;
#else
        if(!std::isfinite(seconds)||seconds<=0||seconds>10||values.size()<2||values.size()>256)return false;
        for(size_t i=0;i<values.size();++i)
            if(!std::isfinite(values[i])||values[i]<0||values[i]>5||(i&&values[i]>values[i-1]+1e-6))return false;
        if(values.back()>1e-6)return false;
        stopTime=seconds;speeds=std::move(values);distances.assign(speeds.size(),0);
        const double dt=stopTime/(speeds.size()-1);
        for(size_t i=1;i<speeds.size();++i)distances[i]=distances[i-1]+(speeds[i-1]+speeds[i])*dt*0.5;
        return true;
#endif
    }
    double distanceAt(double time) const {
#ifdef BA_RUST_CROWD_STEERING
        return ba_rust_foot_stop_distance(stopTime,speeds.data(),distances.data(),speeds.size(),time);
#else
        if(speeds.empty()){
            const double t=std::clamp(time/stopTime,0.0,1.0);
            // Short fallback when there are no unambiguous feet/Move_End.
            return 2.376*stopTime*(t-t*t*t+0.5*t*t*t*t);
        }
        const double dt=stopTime/(speeds.size()-1),t=std::clamp(time,0.0,stopTime);
        const size_t i=std::min(size_t(t/dt),speeds.size()-2);
        const double within=t-i*dt;
        return distances[i]+speeds[i]*within+(speeds[i+1]-speeds[i])*within*within/(2*dt);
#endif
    }
};
