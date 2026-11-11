#include "shared/directed_walk.h"
#include "shared/foot_stop.h"
#include "shared/stop_turn.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

static void check(bool result,const char* message){
    if(!result)throw std::runtime_error(message);
}

static void near(double actual,double expected,const char* message,double tolerance=1e-6){
    check(std::abs(actual-expected)<=tolerance,message);
}

static void stopTurn(bool trace){
    StopTurn turn;
    turn.begin(179,-179);
    check(turn.active&&turn.target==181,"Stop turn takes the short arc across 180 degrees");
    near(turn.advance(-.01),179,"A negative time step leaves the turn at its starting yaw");
    for(int frame=0;frame<64;++frame){
        const double segment=turn.advance(.008);
        if(trace)std::cout<<"turn "<<frame<<' '<<segment<<' '<<turn.yaw<<' '<<turn.elapsed<<'\n';
    }
    near(turn.yaw,181,"The display settles at the target angle");
    near(turn.advance(.008),-179,"The trajectory heading wraps after the turn");
    turn.reset();
    check(!turn.active,"Reset releases a frozen stopping angle");
}

static void footStop(bool trace){
    FootStopProfile profile;
    near(profile.distanceAt(1),.33264,"Missing foot data uses the fallback braking curve");
    check(profile.initialize(.4,{2,1,0}),"Accept a measured deceleration profile");
    near(profile.distanceAt(-.1),0,"Distance before the stop starts is zero");
    near(profile.distanceAt(.1),.175,"Integrate the first half of a sample interval");
    near(profile.distanceAt(.25),.34375,"Continue integrating from the previous interval");
    near(profile.distanceAt(1),.4,"Stop distance stays fixed after the last sample");
    check(!profile.initialize(.2,{1,2,0})&&!profile.initialize(.2,{1,.5}),
          "Reject acceleration and profiles that end while moving");
    check(!profile.initialize(.2,std::vector<double>(257,0)),"Reject oversized sample lists");
    near(profile.distanceAt(1),.4,"A rejected profile preserves the previous curve");
    if(trace)for(int frame=0;frame<=64;++frame)
        std::cout<<"foot "<<frame<<' '<<profile.distanceAt(frame*.008)<<'\n';
}

static void follow(bool trace){
    DirectedWalk walk;
    PetGround ground;
    float yaw=179;
    walk.begin({0,0,-3},true,17,yaw);
    bool arrived=false;
    for(int frame=0;frame<1500&&!arrived;++frame){
        if(frame==65)walk.target={-1.8f,0,1.5f};
        if(frame==140)walk.target={1.6f,0,.75f};
        const auto previous=ground.position;
        const auto facing=yaw;
        arrived=walk.step(ground,1.2,.008,yaw,[&](float heading,double){
            const double scale=frame>=20&&frame<35?0.0:1.0;
            return MovementChoice{heading,scale,scale<1};
        });
        check(std::abs(std::remainder(yaw-facing,360.f))<=1.921f,"Following limits angular speed across the wrap");
        if(frame>=20&&frame<35){
            check(ground.position.x==previous.x&&ground.position.z==previous.z,"A waiting actor stays at the same position");
            near(walk.time,(frame+1)*.008,"Waiting for clearance still advances animation time");
        }
        if(trace)std::cout<<"follow "<<frame<<' '<<ground.position.x<<' '<<ground.position.z<<' '
                          <<yaw<<' '<<walk.travelSpeed<<' '<<walk.active<<' '<<arrived<<'\n';
    }
    check(arrived&&!walk.active&&walk.serial==2&&walk.owner==17,"The last target is reached and releases the walk");
    near(ground.position.x,1.6,"Retargeted destination x");
    near(ground.position.z,.75,"Retargeted destination z");

    walk.begin({0,0,5},false,18,yaw);
    check(!walk.step(ground,1.2,.008,yaw,[](float,double){
        return MovementChoice{std::numeric_limits<float>::quiet_NaN(),0,true};
    })&&walk.blocked&&!walk.active&&walk.serial==4,"An impassable route cancels the walk and marks it blocked");
}

static void jump(bool trace){
    DirectedWalk walk;
    PetGround ground;
    float yaw=25;
    walk.begin({0,0,2},false,1,yaw);
    walk.travelHeading=0;
    walk.travelSpeed=1.5;
    const auto boundary=[](float heading,float lookahead){
        near(heading,0,"Jump looks along the last valid movement");
        near(lookahead,.3,"Jump checks half the duration ahead");
        return lookahead*.5f;
    };
    check(!walk.planJump(ground,2,.4,boundary,[](float,double){return false;})&&!walk.jumping,
          "A blocked landing leaves the walk active");
    check(walk.planJump(ground,2,.4,boundary,[](float heading,double travel){
        near(heading,0,"Landing uses the collision-checked heading");
        near(travel,.45,"Jump reserves travel from the early animation offset");
        return true;
    }),"Accept a jump with space to land");
    check(walk.jumping&&walk.serial==2,"Jump starts a new animation generation");
    check(!walk.step(ground,2,.1,yaw)&&walk.jumping,"A partial jump does not finish walking");
    near(ground.position.z,.15,"Partial jump displacement");
    if(trace)std::cout<<"jump 0 "<<ground.position.z<<' '<<walk.jumpTime<<' '<<yaw<<' '<<walk.serial<<'\n';

    bool checkedLanding=false;
    check(!walk.step(ground,2,.25,yaw,[&](float heading,double travel){
        near(ground.position.z,.45,"The steering callback sees the committed landing position");
        near(yaw,0,"The steering callback sees the jump facing");
        near(travel,.1,"Only time left over after landing is available to walk");
        check(!walk.jumping&&walk.serial==3,"Steering sees the completed jump");
        checkedLanding=true;
        return MovementChoice{heading};
    })&&checkedLanding,"Continue directed walking in the remainder of the landing tick");
    near(ground.position.z,.55,"Walking resumes from the landing position");
    if(trace)std::cout<<"jump 1 "<<ground.position.z<<' '<<walk.jumpTime<<' '<<yaw<<' '<<walk.serial<<'\n';
}

static void steeringException(){
    DirectedWalk walk;
    PetGround ground;
    float yaw=0;
    walk.begin({1,0,2},true,1,yaw);
    bool caught=false;
    try{
        walk.step(ground,2,.008,yaw,[](float,double)->MovementChoice{
            throw std::logic_error("Steering callback");
        });
    }catch(const std::logic_error&){
        caught=true;
    }
    check(caught&&walk.active&&walk.followHeadingReady&&!walk.blocked&&walk.time==0&&ground.position.z==0,
          "A failed steering callback propagates with the pre-movement state");
}

int main(int argc,char** argv){
    try{
        const bool trace=argc>1&&std::string_view(argv[1])=="--trace";
        if(trace)std::cout<<std::setprecision(17);
        stopTurn(trace);
        footStop(trace);
        follow(trace);
        jump(trace);
        steeringException();
        std::cout<<"Stopping curves, retargeting, blocked movement, jumps and callbacks passed.\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
