#pragma once
#include "pet_ground.h"
#include <map>

// Authored animated mesh bounds in normalized model units. This only changes
// presentation and placement in the Three.js host, never skeleton transforms.
struct DesktopGeometry {
    Vector3 minimum{-0.9f,-0.12f,-0.9f},maximum{0.9f,2.8f,0.9f};
    Vector3 renderMinimum{-1.5f,-0.3f,-1.5f},renderMaximum{1.5f,3.4f,1.5f};
    float bodyWidth=0.7f;
    int width=480,height=480;
    float above=448,below=64;
    static constexpr float ViewportZoom=1.65f; // Reserve the complete supported wheel range once.
    static constexpr float EdgeClearance=2; // Half the previous 4 px screen-edge clearance.
    float yaw=0;
    std::map<int,std::vector<BoundingBox>> restingBoxes;
    int restingClip=-1;
    Vector2 crop{};
    void prepareViewport(PetGround& ground,float zoom){
        const auto saved=ground.position;const auto low=minimum,high=maximum;
        minimum=renderMinimum;maximum=renderMaximum;
        float left=0,right=0,top=0,bottom=0;
        for(const auto& a:ground.monitors)for(float x:{a.left,a.right})for(float y:{a.top,a.bottom}){
            ground.position=ground.floorAt({x,y});measureBox(ground,ViewportZoom);const auto b=ground.bodyOffsets;
            left=std::min(left,b.left);right=std::max(right,b.right);top=std::min(top,b.top);bottom=std::max(bottom,b.bottom);
        }
        width=int(std::ceil(std::max(480.0f,2*std::max(-left,right)+48)/32)*32);
        above=std::ceil(std::max(440.0f,-top+24)/32)*32;below=std::ceil(std::max(40.0f,bottom+24)/32)*32;
        height=int(above+below);minimum=low;maximum=high;ground.position=saved;measure(ground,zoom);
    }
    PetGround::Area projectBoxes(const PetGround& ground,float zoom,const std::vector<BoundingBox>& boxes,float facing) const {
        PetGround::Area offsets{INFINITY,INFINITY,-INFINITY,-INFINITY};
        const auto foot=ground.screen(ground.position);
        const auto transform=ModelView::matrix(facing,zoom);
        for(const auto& box:boxes)for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z) {
            const auto point=ModelView::apply(transform,{x?box.max.x:box.min.x,y?box.max.y:box.min.y,z?box.max.z:box.min.z});
            const auto p=ground.screen({ground.position.x+point.x,ground.position.y+point.y,ground.position.z+point.z});
            offsets.left=std::min(offsets.left,p.x-foot.x-EdgeClearance);offsets.right=std::max(offsets.right,p.x-foot.x+EdgeClearance);
            offsets.top=std::min(offsets.top,p.y-foot.y-EdgeClearance);offsets.bottom=std::max(offsets.bottom,p.y-foot.y+EdgeClearance);
        }
        return offsets;
    }
    void measureBox(PetGround& ground,float zoom) const {
        ground.bodyOffsets=projectBoxes(ground,zoom,{{minimum,maximum}},0);
    }
    void measure(PetGround& ground,float zoom) const {
        const auto shapes=restingBoxes.find(restingClip);
        if(shapes!=restingBoxes.end()&&!shapes->second.empty())ground.bodyOffsets=projectBoxes(ground,zoom,shapes->second,yaw);
        else measureBox(ground,zoom);
    }
    void place(PetGround& ground,Vector2 foot,float zoom) const {
        // Bounds depend on perspective depth: solve the edge clamp at the
        // requested position, not using margins from the old screen position.
        for(int pass=0;pass<5;++pass){ground.position=ground.floorAt(foot);measure(ground,zoom);foot=ground.clampFoot(foot);}
        ground.position=ground.floorAt(foot);measure(ground,zoom);
    }
    Vector2 idleFoot(PetGround& ground,float zoom) const {
        const auto saved=ground.position;auto target=ground.screen(saved);
        for(int pass=0;pass<5;++pass){ground.position=ground.floorAt(target);measure(ground,zoom);target=ground.idleFoot(target);}
        ground.position=saved;measure(ground,zoom);return target;
    }
    static bool settleStep(PetGround& ground,Vector3 target,double speed,double dt) {
        const float dx=target.x-ground.position.x,dz=target.z-ground.position.z;
        const double remaining=std::hypot(dx,dz);
        if(remaining<=0.002){ground.position=target;return true;}
        const float distance=float(std::min(remaining,speed*dt*std::min(1.0,std::sqrt(2*remaining/(speed*0.35)))));
        // Movement heading is separate from model facing. Docking must never
        // rotate the character or overwrite a user/behavior-facing request.
        ground.position=ground.step(std::atan2(dx,dz)*RAD2DEG,distance);
        if(distance>=remaining){ground.position=target;return true;}
        return false;
    }
    void update(PetGround& ground,float zoom,bool publishBounds=true) {
        if(publishBounds)measure(ground,zoom);
        const auto foot=ground.screen(ground.position);
        crop={std::round(foot.x-width*0.5f),std::round(foot.y-above)};
    }
};
