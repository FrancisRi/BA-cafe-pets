#include "shared/crowd_steering.h"
#include "shared/desktop_path.h"
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static double distance(Vector2 a,Vector2 b){return std::hypot(a.x-b.x,a.y-b.y);}
static double oldSegmentDistance(Vector3 a,Vector3 b,Vector3 p){const double x=b.x-a.x,z=b.z-a.z,length=x*x+z*z;
    const double t=length>1e-12?std::clamp(((p.x-a.x)*x+(p.z-a.z)*z)/length,0.0,1.0):0;
    return std::hypot(p.x-a.x-x*t,p.z-a.z-z*t);}
static constexpr double dt=0.008,speed=2;
static auto freeSpace=[](float,double){return true;};
static void recoverDesktopEnvelope(){
    PetGround ground;ground.measuredBounds=true;ground.position=ground.floorAt({100,600});
    const auto foot=ground.screen(ground.position);ground.bodyOffsets={-foot.x-1.5f,-100,100,20};
    check(DesktopPath::stepAllowed(ground,90,.0005),"A partly outside envelope can take a small inward recovery step");
    check(!DesktopPath::stepAllowed(ground,-90,.0005),"Recovery cannot move farther outside the desktop");
    ground.bodyOffsets={-50,-100,50,20};check(!DesktopPath::stepAllowed(ground,-90,100),"An inside pet still cannot leave the screen");
    DesktopGeometry geometry;ground.monitors={{0,0,float(ground.width),float(ground.height)}};ground.position=ground.floorAt({2,800});geometry.measure(ground,1);
    check(DesktopPath::restingMonitor(ground,geometry,1,ground.position)<0,"Fixture starts with a clipped resting envelope");
    check(!DesktopPath::crossing(ground,geometry,1,ground.floorAt({1000,800})),"Same-screen envelope recovery must retain cursor replanning");
    ground.width=3840;ground.monitors={{0,0,1920,1080},{1920,0,3840,1080}};ground.position=ground.floorAt({1000,800});
    check(DesktopPath::crossing(ground,geometry,1,ground.floorAt({3000,800})),"A real transfer between monitors remains a crossing");
}
static void draggedNearPeer(){
    for(float initial:{-.92f,-.2f}){
        std::array<CrowdBody,2> bodies{CrowdBody{1,{0,initial},{},.45f},CrowdBody{2,{0,0},{},.45f}};CrowdSteering steering;
        const Vector2 goal{0,3};bool separated=false;
        for(int frame=0;frame<2000;++frame){
            const auto q=CrowdSteering::subtract(goal,bodies[0].position);const double left=std::sqrt(CrowdSteering::norm(q));if(left<.03)break;
            const auto move=steering.step(bodies[0],bodies,std::atan2(q.x,q.y)*RAD2DEG,std::min(left,speed*dt),dt,freeSpace);
            check(std::isfinite(move.heading),"A pet dragged into a peer's clearance must find an escape toward its follow goal");
            const auto u=CrowdSteering::direction(move.heading);const auto before=distance(bodies[0].position,bodies[1].position);
            bodies[0].position.x+=u.x*speed*dt*move.scale;bodies[0].position.y+=u.y*speed*dt*move.scale;
            const auto after=distance(bodies[0].position,bodies[1].position);
            if(before<.9)check(after>=before-1e-5,"Recovering from a drag must not deepen overlap");
            if(separated)check(after>=.9-1e-3,"After clearing overlap the pet must not penetrate the peer again");separated|=after>=.9;
        }
        check(distance(bodies[0].position,goal)<.04,"Follow resumes beyond a stationary peer after a central drag");
        check(distance(bodies[1].position,{0,0})==0,"Recovery must not push the idle peer");
    }
}
static void fiveWayCrossing(){
    std::array<CrowdBody,5> bodies{};std::array<Vector2,5> goals{};std::array<CrowdSteering,5> steering{};
    std::array<bool,5> stopped{};double nearest=1e9;int arrived=0;
    for(size_t i=0;i<5;++i){const auto u=CrowdSteering::direction(float(i*72));bodies[i]={i+1,{u.x*3,u.y*3},{float(-u.x*speed),float(-u.y*speed)},.45f};
        bodies[i].movable=true;goals[i]={-bodies[i].position.x,-bodies[i].position.y};}
    for(int frame=0;frame<2000;++frame){const auto snapshot=bodies;
        for(size_t i=0;i<5;++i){if(stopped[i])continue;const auto q=CrowdSteering::subtract(goals[i],bodies[i].position);const double left=std::sqrt(CrowdSteering::norm(q));
            if(left<.03){stopped[i]=true;bodies[i].velocity={};++arrived;continue;}
            const double travel=std::min(left,speed*dt);const auto move=steering[i].step(bodies[i],snapshot,std::atan2(q.x,q.y)*RAD2DEG,travel,dt,freeSpace);
            if(!std::isfinite(move.heading)){std::cout<<"abandoned id="<<i+1<<" t="<<frame*dt<<" left="<<left<<" offset="<<steering[i].offset<<" wait="<<steering[i].blockedTime<<" yield="<<steering[i].yieldTime<<'\n';stopped[i]=true;bodies[i].velocity={};continue;}
            const auto u=CrowdSteering::direction(move.heading);bodies[i].velocity={float(u.x*travel/dt*move.scale),float(u.y*travel/dt*move.scale)};
        }
        for(auto& body:bodies){body.position.x+=body.velocity.x*dt;body.position.y+=body.velocity.y*dt;}
        for(size_t i=0;i<5;++i)for(size_t j=i+1;j<5;++j){nearest=std::min(nearest,distance(bodies[i].position,bodies[j].position));
            check(nearest>=.9-1e-3,"Five-way crossing must not penetrate another actor");}
    }
    std::cout<<"five-way crossing nearest="<<nearest<<" arrived="<<arrived<<"/5\n";
    for(size_t i=0;i<5;++i)if(distance(bodies[i].position,goals[i])>=.03)std::cout<<"unfinished id="<<i+1<<" at="<<bodies[i].position.x<<','<<bodies[i].position.y<<" left="<<distance(bodies[i].position,goals[i])<<" heading="<<steering[i].heading<<" offset="<<steering[i].offset<<" obstacle="<<steering[i].obstacle<<" clear="<<steering[i].clearTime<<" scale="<<steering[i].scale<<'\n';
    check(arrived==5,"A five-way open-space crossing must finish instead of mutually blocking");
}
static void encounter(std::array<CrowdBody,2> bodies,std::array<Vector2,2> goals,bool stationary=false,bool landing=false){
    std::array<CrowdSteering,2> steering;std::array<float,2> previousHeading{};std::array<double,2> previousScale{1,1};std::array<bool,2> sampled{};
    double nearest=1e9,maxTurn=0,maxBrake=0;const auto original=bodies[1].position;
    for(int frame=0;frame<1250;++frame){const auto snapshot=bodies;std::array<MovementChoice,2> moves{};
        for(size_t i=0;i<2;++i){if(stationary&&i==1)continue;const auto q=CrowdSteering::subtract(goals[i],bodies[i].position);const double left=std::sqrt(CrowdSteering::norm(q));
            if(left<0.03){bodies[i].velocity={};continue;}
            const float heading=std::atan2(q.x,q.y)*RAD2DEG;const double travel=std::min(left,speed*dt);
            moves[i]=steering[i].step(bodies[i],snapshot,heading,travel,dt,freeSpace);check(std::isfinite(moves[i].heading),"A passable encounter must not abandon its goal");
            if(sampled[i]){maxTurn=std::max(maxTurn,double(std::abs(std::remainder(moves[i].heading-previousHeading[i],360.0f))));maxBrake=std::max(maxBrake,previousScale[i]-moves[i].scale);}
            previousHeading[i]=moves[i].heading;previousScale[i]=moves[i].scale;sampled[i]=true;
            const auto u=CrowdSteering::direction(moves[i].heading);bodies[i].velocity={float(u.x*travel/dt*moves[i].scale),float(u.y*travel/dt*moves[i].scale)};
        }
        for(size_t i=0;i<2;++i)if(!(stationary&&i==1)){bodies[i].position.x+=bodies[i].velocity.x*dt;bodies[i].position.y+=bodies[i].velocity.y*dt;}
        nearest=std::min(nearest,distance(bodies[0].position,landing?bodies[1].landing:bodies[1].position));
        check(nearest>=bodies[0].radius+bodies[1].radius-1e-3,"Characters must not intersect during passing");
    }
    std::cout<<"encounter nearest="<<nearest<<" maxTurn="<<maxTurn<<" maxBrake="<<maxBrake<<" remaining="<<distance(bodies[0].position,goals[0])<<','<<distance(bodies[1].position,goals[1])<<'\n';
    check(distance(bodies[0].position,goals[0])<0.04,"First character reaches its goal around a peer");
    if(stationary)check(distance(bodies[1].position,original)==0,"An idle peer cannot be pushed");
    else check(distance(bodies[1].position,goals[1])<0.04,"Second character reaches its goal");
    check(maxTurn<=240*dt+0.6,"Steering must not snap or switch lanes abruptly");
    check(maxBrake<=0.055,"Ordinary passing must brake gradually");
}
int main(){try{
    recoverDesktopEnvelope();
    draggedNearPeer();
    encounter({CrowdBody{1,{0,-3},{0,2},0.45f},CrowdBody{2,{0,3},{0,-2},0.45f}},{Vector2{0,3},Vector2{0,-3}});
    encounter({CrowdBody{1,{-3,0},{2,0},0.45f},CrowdBody{2,{0,-3},{0,2},0.45f}},{Vector2{3,0},Vector2{0,3}});
    encounter({CrowdBody{1,{0,-3},{0,2},0.45f},CrowdBody{2,{0,0},{},0.45f}},{Vector2{0,3},Vector2{}},true);
    encounter({CrowdBody{1,{0,-3},{0,2},0.45f},CrowdBody{2,{3,0},{},0.45f,true,{0,0}}},{Vector2{0,3},Vector2{3,0}},true,true);
    fiveWayCrossing();
    CrowdSteering trapped;std::array<CrowdBody,2> bodies{CrowdBody{1,{0,-3},{0,2},0.45f},CrowdBody{2,{0,0},{},0.45f}};bool abandoned=false;double previous=1,maxBrake=0;
    for(int frame=0;frame<800;++frame){auto allowed=[&](float angle,double travel){auto u=CrowdSteering::direction(angle);return std::abs(bodies[0].position.x+u.x*travel)<0.18;};
        auto move=trapped.step(bodies[0],bodies,0,speed*dt,dt,allowed);if(!std::isfinite(move.heading)){abandoned=true;check(trapped.scale<0.08,"A trapped character first slows before abandoning");break;}
        maxBrake=std::max(maxBrake,previous-move.scale);previous=move.scale;auto u=CrowdSteering::direction(move.heading);bodies[0].position.x+=u.x*speed*dt*move.scale;bodies[0].position.y+=u.y*speed*dt*move.scale;
        check(distance(bodies[0].position,bodies[1].position)>=0.9,"No penetration in a blocked corridor");}
    check(abandoned&&maxBrake<=0.055,"An unreachable corridor uses bounded graceful yielding, not endless retries");
    // A lane selected earlier must still obey a boundary as we approach it.
    CrowdSteering edge;std::array<CrowdBody,2> wall{CrowdBody{1,{0,0},{0,2},.45f},CrowdBody{2,{0,1.2f},{},.45f}};bool stoppedAtEdge=false;
    for(int frame=0;frame<600;++frame){auto allowed=[&](float angle,double travel){return wall[0].position.x+CrowdSteering::direction(angle).x*travel<=.6;};
        const auto move=edge.step(wall[0],wall,0,speed*dt,dt,allowed);if(!std::isfinite(move.heading)){stoppedAtEdge=true;break;}
        check(allowed(move.heading,speed*dt*move.scale),"A held lane cannot cross the screen boundary");
        const auto u=CrowdSteering::direction(move.heading);wall[0].position.x+=u.x*speed*dt*move.scale;wall[0].position.y+=u.y*speed*dt*move.scale;
    }
    check(stoppedAtEdge&&wall[0].position.x<=.6,"A blocked passing lane yields and stays within the desktop");
    CrowdSteering overlap;std::array<CrowdBody,2> stacked{CrowdBody{1,{0,0.1f},{},0.45f},CrowdBody{2,{0,0},{},0.45f}};
    check(overlap.step(stacked[0],stacked,0,speed*dt,dt,freeSpace).scale>0,"Dragging overlap permits escape");
    // A noisy one-frame straight-line opening must not reverse the passing lane.
    CrowdSteering held;std::array<CrowdBody,2> noisy{CrowdBody{1,{0,0},{0,2},.45f},CrowdBody{2,{0,1.2f},{},.45f}};
    held.step(noisy[0],noisy,0,speed*dt,dt,freeSpace);check(held.laneLocked,"An encounter acquires a passing lane");
    const int heldSide=held.side;const float heldOffset=std::abs(held.offset);
    for(int frame=0;frame<100;++frame){noisy[1].position.x=frame%2?1.02f:0.85f;
        held.step(noisy[0],noisy,0,speed*dt,dt,freeSpace);
        check(held.laneLocked&&held.side==heldSide&&std::abs(held.offset)>=heldOffset,"Noisy clearance cannot straighten or switch the held lane");}
    noisy[1].position={0,-1.2f};for(int frame=0;frame<100;++frame)held.step(noisy[0],noisy,0,speed*dt,dt,freeSpace);
    check(!held.active,"A completed encounter releases its lane and returns to the original direction");
    // Batch separation is bounded, symmetric and does not move idle or jumping peers.
    stacked[0].movable=stacked[1].movable=true;
    auto first=CrowdSteering::separation(stacked[0],stacked,dt),second=CrowdSteering::separation(stacked[1],stacked,dt);
    check(first.y>0&&second.y<0&&distance(first,Vector2{-second.x,-second.y})<1e-6,"Moving peers share separation symmetrically");
    check(std::sqrt(CrowdSteering::norm(first))<=stacked[0].radius*0.5*dt+1e-6,"Separation cannot snap a position");
    stacked[1].movable=false;check(CrowdSteering::norm(CrowdSteering::separation(stacked[1],stacked,dt))==0,"Idle peer stays fixed");
    stacked[0].jumping=true;check(CrowdSteering::norm(CrowdSteering::separation(stacked[0],stacked,dt))==0,"Separation preserves jump motion");
    stacked[0].jumping=false;stacked[0].position=stacked[1].position;
    check(CrowdSteering::norm(CrowdSteering::separation(stacked[0],stacked,dt))>0,"Coincident centers have a deterministic escape direction");
    // Fixed five-body worst-case benchmark. No GPU, model or user data involved.
    std::array<CrowdBody,5> crowded{CrowdBody{1,{0,0},{0,2},.45f},CrowdBody{2,{0,1.1f},{0,-2},.45f},CrowdBody{3,{1.1f,0},{-2,0},.45f},CrowdBody{4,{-1.1f,0},{2,0},.45f},CrowdBody{5,{0,-1.1f},{0,2},.45f}};
    std::array<CrowdSteering,5> states;double checksum=0;constexpr int ticks=100000;auto start=std::chrono::steady_clock::now();
    for(int tick=0;tick<ticks;++tick)for(size_t i=0;i<5;++i){const auto move=states[i].step(crowded[i],crowded,0,speed*dt,dt,freeSpace);checksum+=move.scale;if(!std::isfinite(move.heading))states[i].reset();}
    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/ticks;
    std::cout<<"Five-body local steering: "<<us<<" us / scene tick; checksum="<<checksum<<'\n';
    const auto measure=[&](bool updated){double sink=0;std::array<CrowdSteering,5> cache;auto began=std::chrono::steady_clock::now();
        for(int tick=0;tick<ticks;++tick)for(size_t i=0;i<5;++i){PetGround ground;ground.position={crowded[i].position.x,0,crowded[i].position.y};
            if(updated){auto choice=cache[i].step(crowded[i],crowded,0,speed*dt,dt,[&](float angle,double travel){return DesktopPath::stepAllowed(ground,angle,travel);});sink+=choice.scale;if(!std::isfinite(choice.heading))cache[i].reset();}
            else {bool clear=DesktopPath::stepAllowed(ground,0,speed*dt);const auto end=ground.step(0,float(speed*0.3));
                for(size_t j=0;j<5&&clear;++j)if(i!=j){const Vector3 other{crowded[j].position.x,0,crowded[j].position.y};clear=oldSegmentDistance(ground.position,end,other)>=crowded[i].radius+crowded[j].radius+0.04;}
                sink+=clear;}}
        std::cout<<(updated?"new":"old")<<" steering + desktop bounds: "<<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-began).count()/ticks<<" us / five actors; checksum="<<sink<<'\n';};
    measure(false);measure(true);
    for(auto& body:crowded)body.movable=true;start=std::chrono::steady_clock::now();
    for(int tick=0;tick<ticks;++tick)for(const auto& body:crowded){auto push=CrowdSteering::separation(body,crowded,dt);checksum+=push.x+push.y;}
    std::cout<<"Five-body overlap fallback: "<<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/ticks<<" us / scene tick; checksum="<<checksum<<'\n';
    std::cout<<"Passing, crossing, stationary peers, landing reservations, graceful yielding, clearance hysteresis and separation passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
