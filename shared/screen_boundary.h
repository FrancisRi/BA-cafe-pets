#pragma once
#include "pet_ground.h"
// Convert a projected line fraction to ground travel using camera depth.
// Avoid a second ray unprojection, whose float error is visible at slow speeds.
inline float projectedBoundaryDistance(const PetGround& ground,float yaw,float distance,
        const std::vector<PetGround::Area>& areas,const std::function<bool(Vector2,const PetGround::Area&)>& visible={}){
    const auto from=ground.screen(ground.position);const auto end=ground.step(yaw,distance);const auto to=ground.screen(end);
    const float dx=to.x-from.x,dy=to.y-from.y;float nearest=INFINITY;
    const auto camera=ground.camera;
    const Vector3 forward{camera.target.x-camera.position.x,camera.target.y-camera.position.y,camera.target.z-camera.position.z};
    const auto depth=[&](Vector3 p){return double(p.x-camera.position.x)*forward.x+double(p.y-camera.position.y)*forward.y+double(p.z-camera.position.z)*forward.z;};
    const double a=depth(ground.position),b=depth(end);
    for(const auto& r:areas){
        const auto inside=[&](float t){const float x=from.x+dx*t,y=from.y+dy*t;return x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom;};
        const auto candidate=[&](float t){
            if(t<=0||t>1||inside(t-0.0001f)==inside(t+0.0001f))return;
            const Vector2 hit{from.x+dx*t,from.y+dy*t};if(visible&&!visible(hit,r))return;
            const double travel=distance*t*a/((1-t)*b+t*a);if(travel>0.002)nearest=std::min(nearest,float(travel));
        };
        if(std::abs(dx)>0.00001f){candidate((r.left-from.x)/dx);candidate((r.right-from.x)/dx);}
        if(std::abs(dy)>0.00001f){candidate((r.top-from.y)/dy);candidate((r.bottom-from.y)/dy);}
    }return nearest;
}
