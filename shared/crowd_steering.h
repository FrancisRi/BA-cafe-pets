#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>

struct MovementChoice {float heading;double scale=1;bool avoiding=false;};
struct CrowdBody {
    uint64_t id=0;Vector2 position{},velocity{};float radius=0;
    bool jumping=false;Vector2 landing{};
    bool movable=false;
};
// Fixed-size local steering, not pathfinding. The scene supplies one coherent
// snapshot; stationary bodies never move because another actor approaches.
struct CrowdSteering {
    bool active=false,laneLocked=false;float heading=0,offset=0;int side=1;
    uint64_t obstacle=0;
    double scale=1,planWait=0,blockedTime=0,clearTime=0,yieldTime=0;
    void reset(){*this=CrowdSteering{};}
    static Vector2 direction(float yaw){const float a=yaw*DEG2RAD;return {std::sin(a),std::cos(a)};}
    static float dot(Vector2 a,Vector2 b){return a.x*b.x+a.y*b.y;}
    static float norm(Vector2 a){return dot(a,a);}
    static Vector2 subtract(Vector2 a,Vector2 b){return {a.x-b.x,a.y-b.y};}
    static Vector2 separation(const CrowdBody& self,std::span<const CrowdBody> bodies,double dt){
        if(!self.movable||self.jumping||dt<=0)return {};
        Vector2 correction{};
        for(const auto& other:bodies){if(other.id==self.id||other.jumping)continue;
            const auto q=subtract(self.position,other.position);const float squared=norm(q),r=self.radius+other.radius;
            if(squared>=r*r)continue;
            const float distance=std::sqrt(squared),share=other.movable?0.5f:1.0f;
            const Vector2 normal=distance>1e-6f?Vector2{q.x/distance,q.y/distance}:Vector2{self.id<other.id?-1.0f:1.0f,0};
            const float push=(r-distance)*0.35f*share;correction.x+=normal.x*push;correction.y+=normal.y*push;
        }
        const float length=std::sqrt(norm(correction)),limit=float(self.radius*0.5*dt);
        if(length>limit){correction.x*=limit/length;correction.y*=limit/length;}return correction;
    }
    static bool safe(Vector2 delta,Vector2 relative,float radius,double horizon){
        const float d=norm(delta),r=radius*radius;
        // User dragging can overlap bodies. Allow an escape, never push them.
        if(d<r&&dot(delta,relative)>=0)return true;
        const float v=norm(relative);const float t=v>1e-10f?float(std::clamp(-double(dot(delta,relative))/v,0.0,horizon)):0;
        const Vector2 q{delta.x+relative.x*t,delta.y+relative.y*t};return norm(q)>=r;
    }
    template<class Allowed>
    MovementChoice step(const CrowdBody& self,std::span<const CrowdBody> bodies,float preferred,double travel,double dt,Allowed allowed){
        if(travel<=0||dt<=0)return {preferred,1,false};
        if(!allowed(preferred,travel))return {std::numeric_limits<float>::quiet_NaN(),0,false};
        if(bodies.size()<2){reset();return {preferred,1,false};}
        const float speed=float(travel/dt);constexpr double horizon=0.6;
        const auto clear=[&](float angle,double future,double factor=1){const auto u=direction(angle);const Vector2 velocity{float(u.x*speed*factor),float(u.y*speed*factor)};
            for(const auto& other:bodies){if(other.id==self.id)continue;const float r=self.radius+other.radius+0.04f;
                const auto delta=subtract(other.position,self.position);const float range=r+(speed+std::sqrt(norm(other.velocity)))*float(future);
                if(norm(delta)>range*range&&!other.jumping)continue;
                if(!safe(delta,subtract(other.velocity,velocity),r,future))return false;
                if(other.jumping&&!safe(subtract(other.landing,self.position),{-velocity.x,-velocity.y},r,future))return false;
            }return true;};
        const bool directClear=clear(preferred,horizon);
        bool yielding=false;
        if(!directClear){const auto u=direction(preferred);const Vector2 velocity{u.x*speed,u.y*speed};
            for(const auto& other:bodies){if(other.id==self.id||other.id>self.id)continue;
                const float otherSpeed=std::sqrt(norm(other.velocity));if(otherSpeed<0.1f||std::abs(dot(u,other.velocity))>otherSpeed*0.65f)continue;
                if(!safe(subtract(other.position,self.position),subtract(other.velocity,velocity),self.radius+other.radius+0.04f,horizon)){yielding=true;break;}
            }
        }
        if(directClear&&!active){heading=preferred;scale=1;blockedTime=0;return {preferred,1,false};}
        if(!active){active=true;heading=preferred;offset=0;side=1;planWait=0;}
        // Passing has memory: one clear prediction cannot send us back into the
        // same encounter. Keep the lane until the blocker is behind or distant.
        bool passed=true;const auto intent=direction(preferred);
        for(const auto& other:bodies)if(other.id==obstacle){const auto q=subtract(other.position,self.position);
            const float range=self.radius+other.radius+speed*0.8f;
            passed=dot(q,intent)<=0||norm(q)>range*range;break;}
        const bool edgeRecovery=laneLocked&&directClear&&!allowed(preferred+offset,std::max(travel,double(speed)*0.22));
        clearTime=directClear&&(passed||edgeRecovery)?clearTime+dt:0;
        const bool returning=clearTime>=0.24;
        yieldTime=yielding?0.24:std::max(0.0,yieldTime-dt);
        planWait-=dt;
        if(returning){offset=0;laneLocked=false;obstacle=0;}
        else if(planWait<=0&&yieldTime==0){planWait=0.08;float best=INFINITY,selected=offset;uint64_t selectedObstacle=obstacle;
            const auto consider=[&](float angle,uint64_t id){
                if(std::abs(angle)>100||(laneLocked&&side*angle<=0))return;
                // Don't straighten while still approaching the held blocker.
                if(laneLocked&&!passed&&std::abs(angle)<std::abs(offset))angle=offset;
                const float candidate=preferred+angle;
                if(!allowed(candidate,std::max(travel,double(speed)*0.22))||!clear(candidate,horizon))return;
                const float score=std::abs(angle)+std::abs(angle-offset)*0.5f+(angle*side<0?8.0f:0.0f);
                if(score<best){best=score;selected=angle;selectedObstacle=id;}
            };
            if(laneLocked)consider(offset,obstacle);
            // Continuous boundaries of the relative-velocity collision cone.
            // Intersect each tangent ray with our speed circle; no angle buckets.
            for(const auto& other:bodies){if(other.id==self.id)continue;
                const auto tangent=[&](Vector2 position,Vector2 velocity){const auto q=subtract(position,self.position);
                    const float r=self.radius+other.radius+0.07f,d=std::sqrt(norm(q));
                    if(dot(q,intent)<-r)return;
                    const float center=std::atan2(q.x,q.y);
                    if(d<r){
                        // A drag can place us inside the padded collision cone,
                        // where external tangents do not exist. Try either side
                        // with an outward component; clear() still checks every
                        // peer and never allows the existing overlap to deepen.
                        if(d>1e-6f)for(int sign:{1,-1})consider(std::remainder(center*RAD2DEG+sign*100.0f-preferred,360.0f),other.id);
                        return;
                    }
                    const float spread=std::asin(std::clamp(r/d,0.0f,1.0f));
                    for(int sign:{1,-1}){const auto ray=direction((center+sign*spread)*RAD2DEG);const float along=dot(velocity,ray);
                        const float discriminant=along*along+speed*speed-norm(velocity);if(discriminant<0)continue;
                        const float length=-along+std::sqrt(discriminant);if(length<=0)continue;
                        const Vector2 v{velocity.x+ray.x*length,velocity.y+ray.y*length};
                        const float angle=std::remainder(std::atan2(v.x,v.y)*RAD2DEG-preferred,360.0f);
                        consider(angle,other.id);}
                };
                tangent(other.position,other.velocity);if(other.jumping)tangent(other.landing,{});
            }
            if(std::isfinite(best)){offset=selected;obstacle=selectedObstacle;side=offset>0?1:-1;laneLocked=true;}
            // A blocked held side yields instead of alternating left/right.
        }
        const float desired=preferred+offset,delta=std::remainder(desired-heading,360.0f);
        heading=std::remainder(heading+std::clamp(delta,float(-240*dt),float(240*dt)),360.0f);
        double target=1;
        const bool laneFits=allowed(heading,std::max(travel,double(speed)*0.22));
        if(yieldTime>0||!laneFits)target=0;
        else if(!clear(heading,0.32)){
            target=0;
            if(clear(heading,0.32,0)){double lo=0,hi=1;for(int i=0;i<6;++i){const double mid=(lo+hi)*0.5;
                if(clear(heading,0.32,mid))lo=mid;else hi=mid;}target=lo;}
        }
        // When recovering toward a nearby goal, don't run a full-speed circle
        // while the limited turn catches up with the original direction.
        if(returning)target*=std::max(0.0f,std::cos(std::remainder(heading-preferred,360.0f)*DEG2RAD));
        // A ray/circle contact bound prevents overshoot while a smooth turn or
        // braking is still in progress. The reservation covers the peer's next step.
        const auto u=direction(heading);double safeTravel=allowed(heading,travel)?travel:0;
        for(const auto& other:bodies){if(other.id==self.id)continue;const auto q=subtract(other.position,self.position);
            const float r=self.radius+other.radius+0.04f+std::sqrt(norm(other.velocity))*float(dt),d=norm(q),projection=dot(q,u);
            if(d<r*r){if(d>1e-10f&&projection>0)safeTravel=0;continue;}
            const float across=d-projection*projection;if(projection>0&&across<r*r)safeTravel=std::min(safeTravel,double(std::max(0.0f,projection-std::sqrt(r*r-across))));
            if(other.jumping){const auto landing=subtract(other.landing,self.position);const float along=dot(landing,u),acrossLanding=norm(landing)-along*along;
                if(along>0&&acrossLanding<r*r)safeTravel=std::min(safeTravel,double(std::max(0.0f,along-std::sqrt(r*r-acrossLanding))));}
        }
        scale+=std::clamp(target-scale,-4*dt,3*dt);scale=std::clamp(std::min(scale,safeTravel/travel),0.0,1.0);
        if(scale<0.08&&(!directClear||!laneFits))blockedTime+=dt;else blockedTime=0;
        if(blockedTime>=(yieldTime>0?1.6:0.9))return {std::numeric_limits<float>::quiet_NaN(),0,true};
        if(returning&&std::abs(std::remainder(heading-preferred,360.0f))<0.1f&&scale>0.999){reset();return {preferred,1,false};}
        return {heading,scale,true};
    }
};
