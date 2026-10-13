#pragma once
#include "pet_ground.h"
#include "screen_boundary.h"
#include "crowd_steering.h"
struct DirectedWalk {
    bool active=false,following=false,jumping=false,crossing=false,blocked=false;
    bool followAway=false;
    Vector3 target{};
    double time=0;uint64_t owner=0,serial=0;
    float finalYaw=0;
    double jumpTime=0,jumpDuration=0,jumpSpeed=0;float jumpYaw=0;
    void cancel(){if(active)++serial;active=jumping=crossing=false;}
    void begin(Vector3 destination,bool follow,uint64_t generation,float yaw){
        target=destination;following=follow;followAway=false;owner=generation;finalYaw=yaw;time=0;active=true;blocked=false;jumping=crossing=false;++serial;
    }
    bool planJump(const PetGround& ground,double speed,double duration,float yaw,double dt,
                  const std::function<float(float,float)>& visibleDistance={}){
        if(!active||jumping||duration<=0||speed<=0)return false;
        const double remaining=std::hypot(target.x-ground.position.x,target.z-ground.position.z);
        const float heading=std::atan2(target.x-ground.position.x,target.z-ground.position.z)*RAD2DEG;
        if(remaining<=0.002||std::abs(std::remainder(heading-yaw,360.0f))>3)return false;
        // Fit a short final crossing without overshooting the fixed destination.
        const double velocity=std::min(speed,remaining/duration);
        if(!ground.canWalk(heading,float(velocity*duration)))return false;
        const float lookahead=float(velocity*duration*0.5);
        float distance=projectedBoundaryDistance(ground,heading,lookahead,ground.monitors);
        if(visibleDistance)distance=std::min(distance,visibleDistance(heading,lookahead));
        if(!std::isfinite(distance))return false;
        const double offset=std::max(0.0,duration*0.5-distance/velocity);
        if(!ground.canWalk(heading,float(velocity*(duration-offset))))return false;
        jumping=true;jumpYaw=heading;jumpSpeed=velocity;jumpDuration=duration;
        jumpTime=offset;++serial;return true;
    }
    bool step(PetGround& ground,double speed,double dt,float& yaw,const std::function<MovementChoice(float,double)>& steer={}){
        if(jumping){
            const double step=std::min(dt,jumpDuration-jumpTime);
            yaw=jumpYaw;ground.position=ground.step(jumpYaw,float(jumpSpeed*step));jumpTime+=step;
            if(jumpTime>=jumpDuration-1e-8){jumping=false;time=0;++serial;}
            else return false;
            dt-=step;if(dt<=1e-9)return false;
        }
        const double dx=target.x-ground.position.x,dz=target.z-ground.position.z,remaining=std::hypot(dx,dz);
        if(remaining<=0.002){ground.position=target;cancel();return true;}
        const float direct=std::atan2(dx,dz)*RAD2DEG;
        const auto choice=steer?steer(direct,speed*dt):MovementChoice{direct};const float heading=choice.heading;
        if(!std::isfinite(heading)){cancel();blocked=true;return false;}
        yaw=PetGround::turnToward(yaw,heading,dt);
        // Keep the endpoint fixed, soften the last ~0.35 seconds, and avoid
        // circling a close target while the facing catches up.
        const double alignment=std::max(0.0,double(std::cos(std::remainder(heading-yaw,360.0f)*DEG2RAD)));
        const double movement=std::min(remaining,speed*dt*choice.scale*alignment*std::min(1.0,std::sqrt(2*remaining/(speed*0.35))));
        ground.position=ground.step(heading,float(movement));time+=dt;
        if(movement>=remaining&&std::abs(std::remainder(heading-direct,360.0f))<0.01f){ground.position=target;cancel();return true;}
        return false;
    }
    static Vector2 followTarget(Vector2 center,Vector2 cursor,float bodyPixels,float bodies){
        float dx=center.x-cursor.x,dy=center.y-cursor.y;float length=std::hypot(dx,dy);
        if(length<1){dx=1;dy=0;length=1;}
        return {cursor.x+dx/length*bodyPixels*bodies,cursor.y+dy/length*bodyPixels*bodies};
    }
    static bool outsideBand(Vector2 center,Vector2 cursor,float bodyPixels,float nearest,float farthest){
        const float distance=std::hypot(center.x-cursor.x,center.y-cursor.y);
        return distance<nearest*bodyPixels-1||distance>farthest*bodyPixels+1;
    }
    static bool continueFollow(Vector2 center,Vector2 cursor,float bodyPixels,float nearest,float farthest,bool movingAway,bool blocked=false){
        if(blocked&&!outsideBand(center,cursor,bodyPixels,nearest,farthest))return false;
        const float distance=std::hypot(center.x-cursor.x,center.y-cursor.y),middle=(nearest+farthest)*0.5f*bodyPixels;
        // A zero lower limit never requests retreat, including a cursor that
        // moves closer while a previously started approach is still running.
        if(nearest<=0&&movingAway)return false;
        return movingAway?distance<middle-1:distance>middle+1;
    }
    static bool approachesCursor(Vector2 center,Vector2 cursor,Vector2 destination){
        return (cursor.x-center.x)*(destination.x-center.x)+(cursor.y-center.y)*(destination.y-center.y)>0;
    }
};
