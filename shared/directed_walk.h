#pragma once
#include "pet_ground.h"
#include "screen_boundary.h"
#include "crowd_steering.h"
#ifdef BA_RUST_CROWD_STEERING
struct DirectedWalk;
extern "C" {
    using DirectedSteer = MovementChoice(const void*,float,double);
    using DirectedCanWalk = bool(const void*,float,float);
    using DirectedBoundary = float(const void*,float,float);
    using DirectedClearPath = bool(const void*,float,double);
    void ba_rust_directed_cancel(DirectedWalk*);
    void ba_rust_directed_begin(DirectedWalk*,Vector3,bool,uint64_t,float);
    bool ba_rust_directed_step(DirectedWalk*,Vector3*,double,double,float*,DirectedSteer*,const void*);
    bool ba_rust_directed_plan_jump(DirectedWalk*,Vector3,double,double,DirectedCanWalk*,DirectedBoundary*,DirectedClearPath*,const void*);
    Vector2 ba_rust_follow_target(Vector2,Vector2,float,float);
    bool ba_rust_outside_band(Vector2,Vector2,float,float,float);
    bool ba_rust_continue_follow(Vector2,Vector2,float,float,float,bool,bool);
    bool ba_rust_approaches_cursor(Vector2,Vector2,Vector2);
}
#endif
struct DirectedWalk {
    bool active=false,following=false,jumping=false,crossing=false,blocked=false;
    bool followAway=false;
    Vector3 target{};
    double time=0;uint64_t owner=0,serial=0;
    float travelHeading=0;double travelSpeed=0;
    float finalYaw=0,followHeading=0;bool followHeadingReady=false;
    double jumpTime=0,jumpDuration=0,jumpSpeed=0;float jumpYaw=0;
    void cancel(){
#ifdef BA_RUST_CROWD_STEERING
        ba_rust_directed_cancel(this);
#else
        travelSpeed=0;if(active)++serial;active=jumping=crossing=followHeadingReady=false;
#endif
    }
    void begin(Vector3 destination,bool follow,uint64_t generation,float yaw){
#ifdef BA_RUST_CROWD_STEERING
        ba_rust_directed_begin(this,destination,follow,generation,yaw);
#else
        travelSpeed=0;followHeadingReady=false;target=destination;following=follow;followAway=false;owner=generation;finalYaw=yaw;time=0;active=true;blocked=false;jumping=crossing=false;++serial;
#endif
    }
    bool planJump(const PetGround& ground,double speed,double duration,
                  const std::function<float(float,float)>& visibleDistance={},
                  const std::function<bool(float,double)>& clearPath={}){
#ifdef BA_RUST_CROWD_STEERING
        struct Context {
            const PetGround& ground;
            const std::function<float(float,float)>& visible;
            const std::function<bool(float,double)>& clear;
        } context{ground,visibleDistance,clearPath};
        const auto canWalk=[](const void* data,float heading,float distance){
            return static_cast<const Context*>(data)->ground.canWalk(heading,distance);
        };
        const auto boundary=[](const void* data,float heading,float distance){
            const auto& c=*static_cast<const Context*>(data);
            const float projected=projectedBoundaryDistance(c.ground,heading,distance,c.ground.monitors);
            return c.visible?std::min(projected,c.visible(heading,distance)):projected;
        };
        const auto clear=[](const void* data,float heading,double distance){
            const auto& c=*static_cast<const Context*>(data);
            return !c.clear||c.clear(heading,distance);
        };
        return ba_rust_directed_plan_jump(this,ground.position,speed,duration,canWalk,boundary,clear,&context);
#else
        if(!active||jumping||duration<=0||speed<=0||travelSpeed<=0.01)return false;
        // Use the last collision-checked travel, not the visual facing or the
        // straight line to a cursor target while the pet is turning/avoiding.
        const float heading=travelHeading,angle=heading*DEG2RAD;
        const double forward=(target.x-ground.position.x)*std::sin(angle)+(target.z-ground.position.z)*std::cos(angle);
        if(forward<=0.002)return false;
        const double velocity=std::min({speed,travelSpeed,forward/duration});
        if(!ground.canWalk(heading,float(velocity*duration)))return false;
        const float lookahead=float(velocity*duration*0.5);
        float distance=projectedBoundaryDistance(ground,heading,lookahead,ground.monitors);
        if(visibleDistance)distance=std::min(distance,visibleDistance(heading,lookahead));
        if(!std::isfinite(distance))return false;
        const double offset=std::max(0.0,duration*0.5-distance/velocity);
        if(!ground.canWalk(heading,float(velocity*(duration-offset))))return false;
        if(clearPath&&!clearPath(heading,velocity*(duration-offset)))return false;
        jumping=true;jumpYaw=heading;jumpSpeed=velocity;jumpDuration=duration;
        jumpTime=offset;++serial;return true;
#endif
    }
    bool step(PetGround& ground,double speed,double dt,float& yaw,const std::function<MovementChoice(float,double)>& steer={}){
#ifdef BA_RUST_CROWD_STEERING
        const auto callback=[](const void* context,float heading,double travel){
            const auto& steer=*static_cast<const std::function<MovementChoice(float,double)>*>(context);
            return steer?steer(heading,travel):MovementChoice{heading};
        };
        return ba_rust_directed_step(this,&ground.position,speed,dt,&yaw,callback,&steer);
#else
        if(jumping){
            const double step=std::min(dt,jumpDuration-jumpTime);
            yaw=followHeading=travelHeading=jumpYaw;travelSpeed=jumpSpeed;followHeadingReady=true;ground.position=ground.step(jumpYaw,float(jumpSpeed*step));jumpTime+=step;
            if(jumpTime>=jumpDuration-1e-8){jumping=false;time=0;++serial;}
            else return false;
            dt-=step;if(dt<=1e-9)return false;
        }
        const double dx=target.x-ground.position.x,dz=target.z-ground.position.z,remaining=std::hypot(dx,dz);
        if(remaining<=0.002){ground.position=target;cancel();return true;}
        const float direct=std::atan2(dx,dz)*RAD2DEG;
        float preferred=direct;
        if(following&&!crossing){
            if(!followHeadingReady){followHeading=direct;followHeadingReady=true;}
            // Smooth the 150ms destination updates before collision checks, so
            // avoidance validates the direction actually used by this step.
            // Shorten the response near arrival to avoid orbiting the endpoint.
            const double response=std::clamp(remaining/std::max(speed,0.001)*0.25,0.016,0.12);
            const float blend=float(-std::expm1(-std::max(dt,0.0)/response));
            followHeading=std::remainder(followHeading+std::remainder(direct-followHeading,360.0f)*blend,360.0f);
            preferred=followHeading;
        }else{followHeading=direct;followHeadingReady=true;}
        const auto choice=steer?steer(preferred,speed*dt):MovementChoice{preferred};const float heading=choice.heading;
        if(!std::isfinite(heading)){cancel();blocked=true;return false;}
        if(following){
            const float delta=std::remainder(heading-yaw,360.0f),limit=240.0f*float(std::max(dt,0.0));
            yaw=std::remainder(yaw+std::clamp(delta*float(-std::expm1(-std::max(dt,0.0)/0.10)),-limit,limit),360.0f);
        }else yaw=PetGround::turnToward(yaw,heading,dt);
        // Keep the endpoint fixed, soften the last ~0.35 seconds, and avoid
        // circling a close target while the facing catches up.
        const double alignment=std::max(0.0,double(std::cos(std::remainder(heading-yaw,360.0f)*DEG2RAD)));
        const double movement=std::min(remaining,speed*dt*choice.scale*alignment*std::min(1.0,std::sqrt(2*remaining/(speed*0.35))));
        travelHeading=heading;travelSpeed=dt>0?movement/dt:0;
        ground.position=ground.step(heading,float(movement));time+=dt;
        if(movement>=remaining&&std::abs(std::remainder(heading-direct,360.0f))<0.01f){ground.position=target;cancel();return true;}
        return false;
#endif
    }
    static Vector2 followTarget(Vector2 center,Vector2 cursor,float bodyPixels,float bodies){
#ifdef BA_RUST_CROWD_STEERING
        return ba_rust_follow_target(center,cursor,bodyPixels,bodies);
#else
        float dx=center.x-cursor.x,dy=center.y-cursor.y;float length=std::hypot(dx,dy);
        if(length<1){dx=1;dy=0;length=1;}
        return {cursor.x+dx/length*bodyPixels*bodies,cursor.y+dy/length*bodyPixels*bodies};
#endif
    }
    static bool outsideBand(Vector2 center,Vector2 cursor,float bodyPixels,float nearest,float farthest){
#ifdef BA_RUST_CROWD_STEERING
        return ba_rust_outside_band(center,cursor,bodyPixels,nearest,farthest);
#else
        const float distance=std::hypot(center.x-cursor.x,center.y-cursor.y);
        return distance<nearest*bodyPixels-1||distance>farthest*bodyPixels+1;
#endif
    }
    static bool continueFollow(Vector2 center,Vector2 cursor,float bodyPixels,float nearest,float farthest,bool movingAway,bool blocked=false){
#ifdef BA_RUST_CROWD_STEERING
        return ba_rust_continue_follow(center,cursor,bodyPixels,nearest,farthest,movingAway,blocked);
#else
        if(blocked&&!outsideBand(center,cursor,bodyPixels,nearest,farthest))return false;
        const float distance=std::hypot(center.x-cursor.x,center.y-cursor.y),middle=(nearest+farthest)*0.5f*bodyPixels;
        // A zero lower limit never requests retreat, including a cursor that
        // moves closer while a previously started approach is still running.
        if(nearest<=0&&movingAway)return false;
        return movingAway?distance<middle-1:distance>middle+1;
#endif
    }
    static bool approachesCursor(Vector2 center,Vector2 cursor,Vector2 destination){
#ifdef BA_RUST_CROWD_STEERING
        return ba_rust_approaches_cursor(center,cursor,destination);
#else
        return (cursor.x-center.x)*(destination.x-center.x)+(cursor.y-center.y)*(destination.y-center.y)>0;
#endif
    }
};
#ifdef BA_RUST_CROWD_STEERING
static_assert(sizeof(Vector3)==12 && sizeof(DirectedWalk)==112);
#endif
