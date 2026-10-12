#pragma once
#include "desktop_geometry.h"
#include "stop_turn.h"
// Validate a whole trip before starting. Internal monitor seams are traversable;
// the resting envelope must fit on one screen and large off-desktop gaps are rejected.
struct DesktopPath {
    static int restingMonitor(PetGround& ground,const DesktopGeometry& geometry,float zoom,Vector3 point){
        const auto saved=ground.position;const auto bounds=ground.bodyOffsets;ground.position=point;geometry.measure(ground,zoom);
        const auto foot=ground.screen(point);const auto b=ground.bodyOffsets;int result=-1;
        const std::vector<PetGround::Area> fallback{{0,0,float(ground.width),float(ground.height)}};
        const auto& areas=ground.monitors.empty()?fallback:ground.monitors;
        for(size_t i=0;i<areas.size();++i){const auto& a=areas[i];
            if(foot.x+b.left>=a.left-0.5f&&foot.x+b.right<=a.right+0.5f&&foot.y+b.top>=a.top-0.5f&&foot.y+b.bottom<=a.bottom+0.5f){result=int(i);break;}}
        ground.position=saved;ground.bodyOffsets=bounds;return result;
    }
    static bool covered(const PetGround& ground,Vector3 endpoint){
        const auto from=ground.screen(ground.position),to=ground.screen(endpoint);
        const double dx=to.x-from.x,dy=to.y-from.y,length=std::hypot(dx,dy);
        const std::vector<PetGround::Area> fallback{{0,0,float(ground.width),float(ground.height)}};
        const auto& areas=ground.monitors.empty()?fallback:ground.monitors;
        std::vector<std::pair<double,double>> intervals;
        for(const auto& a:areas){double lo=0,hi=1;
            auto axis=[&](double start,double delta,double minimum,double maximum){
                if(std::abs(delta)<1e-9)return start>=minimum-0.5&&start<=maximum+0.5;
                const double first=(minimum-start)/delta,last=(maximum-start)/delta;
                lo=std::max(lo,std::min(first,last));hi=std::min(hi,std::max(first,last));return lo<=hi;};
            if(axis(from.x,dx,a.left,a.right)&&axis(from.y,dy,a.top,a.bottom))intervals.push_back({lo,hi});
        }
        if(intervals.empty())return false;std::sort(intervals.begin(),intervals.end());
        // Small work-area/offset-monitor gaps may be crossed without stopping.
        // A path through a large invisible corner is not chosen at all.
        double end=0;for(const auto& span:intervals){if((span.first-end)*length>128)return false;end=std::max(end,span.second);}
        return (1-end)*length<=0.5;
    }
    static bool trip(PetGround& ground,const DesktopGeometry& geometry,float zoom,float heading,double distance,double stopMin=0,double stopMax=0){
        const auto firstLanding=ground.step(heading,float(distance+stopMin)),lastLanding=ground.step(heading,float(distance+stopMax));
        const int monitor=restingMonitor(ground,geometry,zoom,firstLanding);
        return monitor>=0&&restingMonitor(ground,geometry,zoom,lastLanding)==monitor&&covered(ground,lastLanding);
    }
    static bool crossing(PetGround& ground,const DesktopGeometry& geometry,float zoom,Vector3 endpoint){
        const int from=restingMonitor(ground,geometry,zoom,ground.position),to=restingMonitor(ground,geometry,zoom,endpoint);
        return to>=0&&from!=to;
    }
    static bool stepAllowed(const PetGround& ground,float heading,double distance){
        const auto p=ground.screen(ground.step(heading,float(distance)));const auto b=ground.bodyOffsets;
        // Do not clamp a moving foot to each individual monitor. That would
        // stop it at an internal seam/gap after the trip had already been accepted.
        return p.x+b.left>=-0.5f&&p.x+b.right<=ground.width+0.5f&&p.y+b.top>=-0.5f&&p.y+b.bottom<=ground.height+0.5f;
    }
    static bool turnFits(PetGround& ground,const DesktopGeometry& geometry,float zoom,float yaw,float desired,double seconds,double speed,double stopMin,double stopMax){
        auto preview=ground;StopTurn turn;turn.begin(yaw,desired,seconds);
        for(int i=0;i<16;++i){const float angle=float(turn.advance(seconds/16));const float distance=float(speed*seconds/16);
            if(!stepAllowed(preview,angle,distance))return false;preview.position=preview.step(angle,distance);}
        const auto firstLanding=preview.step(desired,float(stopMin)),lastLanding=preview.step(desired,float(stopMax));
        const int monitor=restingMonitor(ground,geometry,zoom,firstLanding);
        return monitor>=0&&restingMonitor(ground,geometry,zoom,lastLanding)==monitor&&covered(ground,lastLanding);
    }
};
