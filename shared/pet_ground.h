#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <functional>

// A fixed, long-lens perspective camera overlooking a horizontal y=0 floor.
// Window cropping is independent of the camera and never changes model scale.
struct PetGround {
    // 12 degree elevation, ~143 units away. The FOV retains approximate character
    // height and gives ~25% near/far size change in the current 1920x1008 work area.
    Camera3D camera{{0, 29.789228f, 140.147298f}, {0, 0, 0}, {0, 1, 0}, 3.957349f, CAMERA_PERSPECTIVE};
    static constexpr double NearPlane = 5.0, FarPlane = 500.0;
    Vector3 position{};
    int width = 1920, height = 1080;
    float modelZoom = 1;
    struct Area { float left, top, right, bottom; };
    std::vector<Area> monitors; // Work areas relative to the virtual desktop origin.
    // Three.js opts into measured mesh bounds; legacy raylib keeps its margins.
    bool measuredBounds=false;
    Area bodyOffsets{-90,-280,90,18};
    static constexpr int CropSize = 480;

    Vector2 screen(Vector3 point) const { return GetWorldToScreenEx(point, camera, width, height); }
    Vector3 planeAt(Vector2 pixel, float elevation) const {
        const Ray ray = GetScreenToWorldRayEx(pixel, camera, width, height);
        const float t = (elevation - ray.position.y)/ray.direction.y;
        return {ray.position.x + ray.direction.x*t, elevation, ray.position.z + ray.direction.z*t};
    }
    Vector3 floorAt(Vector2 pixel) const { return planeAt(pixel, 0); }
    bool faceCursor(Vector2 pixel, float eyeHeight, float& yaw) const {
        const auto eye = screen({position.x, eyeHeight, position.z});
        const float dx=pixel.x-eye.x,dy=pixel.y-eye.y;
        // Near the eyes, tiny pointer movements can imply a half-turn. Scale
        // this quiet region with the pet so it also works at small/large zoom.
        const float quietRadius=std::clamp(6.0f*modelZoom,3.0f,12.0f);
        if (!std::isfinite(dx)||!std::isfinite(dy)||std::hypot(dx,dy)<quietRadius) return false;
        // Screen-space direction avoids the ~5x depth amplification at 12 degrees.
        // Slightly compress vertical influence to favor three-quarter/side views.
        yaw = std::atan2(dx,dy*0.65f)*RAD2DEG;
        return std::isfinite(yaw);
    }
    static float turnToward(float current, float target, float dt) {
        const float delta = std::remainder(target - current, 360.0f);
        const float limit = 240.0f*std::max(dt, 0.0f);
        return std::remainder(current + std::clamp(delta, -limit, limit), 360.0f);
    }
    Vector2 clampFoot(Vector2 point) const {
        if(measuredBounds) {
            const float left=std::max(0.0f,-bodyOffsets.left),right=std::min(float(width),width-bodyOffsets.right);
            const float top=std::max(0.0f,-bodyOffsets.top),bottom=std::min(float(height),height-bodyOffsets.bottom);
            Vector2 bounded{std::clamp(point.x,left,std::max(left,right)),std::clamp(point.y,top,std::max(top,bottom))};
            if(monitors.empty())return bounded;
            Vector2 closest=bounded;float best=INFINITY;
            for(const auto& a:monitors) {
                const float l=std::max(a.left,left),r=std::min(a.right-1,right),t=std::max(a.top,top),b=std::min(a.bottom-1,bottom);
                if(r<l||b<t)continue;
                const Vector2 c{std::clamp(bounded.x,l,r),std::clamp(bounded.y,t,b)};
                const float d=std::hypot(c.x-point.x,c.y-point.y);if(d<best){best=d;closest=c;}
            }
            return closest;
        }
        const float xMargin = std::min(90.0f*modelZoom, width*0.25f);
        const float top = std::min(height*0.23f*modelZoom + 12, height*0.65f);
        const Vector2 bounded{std::clamp(point.x, xMargin, width - xMargin),
                              std::clamp(point.y, top, std::max(top, height - 18.0f))};
        if (monitors.empty()) return bounded;
        Vector2 closest = bounded;
        float best = INFINITY;
        for (const auto& area : monitors) {
            const float left = std::max(area.left, xMargin), right = std::min(area.right, width-xMargin);
            const float upper = std::max(area.top, top), lower = std::min(area.bottom-1, height-18.0f);
            if (right < left || lower < upper) continue;
            const Vector2 candidate{std::clamp(bounded.x,left,right), std::clamp(bounded.y,upper,lower)};
            const float distance = std::hypot(candidate.x-point.x,candidate.y-point.y);
            if (distance < best) { best=distance; closest=candidate; }
        }
        return closest;
    }
    void place(Vector2 pixel) { position = floorAt(clampFoot(pixel)); }
    Vector2 idleFoot(Vector2 point) const {
        const std::vector<Area> fallback{{0,0,float(width),float(height)}};
        const auto& areas=monitors.empty()?fallback:monitors;
        Vector2 closest=point;float best=INFINITY;
        for(const auto& a:areas) {
            const float l=a.left-bodyOffsets.left,r=a.right-bodyOffsets.right;
            const float t=a.top-bodyOffsets.top,b=a.bottom-bodyOffsets.bottom;
            if(r<l||b<t)continue;
            const Vector2 c{std::clamp(point.x,l,r),std::clamp(point.y,t,b)};
            const float d=std::hypot(c.x-point.x,c.y-point.y);if(d<best){best=d;closest=c;}
        }
        return closest;
    }
    Vector3 step(float yaw, float distance) const {
        const float angle = yaw*DEG2RAD;
        return {position.x + std::sin(angle)*distance, 0, position.z + std::cos(angle)*distance};
    }
    bool canWalk(float yaw, float distance) const {
        const Vector2 point = screen(step(yaw, distance)), clamped = clampFoot(point);
        return std::abs(point.x - clamped.x) < 0.1f && std::abs(point.y - clamped.y) < 0.1f;
    }
    bool move(float yaw, float distance) {
        if (!canWalk(yaw, distance)) return false;
        position = step(yaw, distance);
        return true;
    }
    // A ground line projects to a screen line, but screen distance is not time.
    // Unproject each rectangle crossing to measure its actual ground distance.
    float boundaryDistance(float yaw, float distance, const std::vector<Area>& areas,
                           const std::function<bool(Vector2,const Area&)>& visible={}) const {
        const auto from=screen(position), to=screen(step(yaw,distance));
        const float dx=to.x-from.x, dy=to.y-from.y;
        float nearest=INFINITY;
        for(const auto& r:areas) {
            auto inside=[&](float t) {
                const float x=from.x+dx*t,y=from.y+dy*t;
                return x>=r.left && x<r.right && y>=r.top && y<r.bottom;
            };
            auto candidate=[&](float t) {
                if(t<=0 || t>1 || inside(t-0.0001f)==inside(t+0.0001f))return;
                const Vector2 hit{from.x+dx*t,from.y+dy*t};
                if(visible&&!visible(hit,r))return;
                const auto p=floorAt(hit);
                const float d=std::hypot(p.x-position.x,p.z-position.z);
                if(d>0.002f)nearest=std::min(nearest,d);
            };
            if(std::abs(dx)>0.00001f) {candidate((r.left-from.x)/dx);candidate((r.right-from.x)/dx);}
            if(std::abs(dy)>0.00001f) {candidate((r.top-from.y)/dy);candidate((r.bottom-from.y)/dy);}
        }
        return nearest;
    }
    Vector2 crop() const {
        const auto foot = screen(position);
        return {std::clamp(std::round(foot.x - CropSize/2.0f), 0.0f, static_cast<float>(std::max(0, width - CropSize))),
                std::clamp(std::round(foot.y - (CropSize - 40.0f)), 0.0f, static_cast<float>(std::max(0, height - CropSize)))};
    }
    // Off-axis sub-frustum of the SAME full-desktop camera. Feed to rlFrustum.
    void frustum(Vector2 cropOrigin, double nearPlane, double& left, double& right, double& bottom, double& top) const {
        const double fullTop = nearPlane*std::tan(camera.fovy*DEG2RAD*0.5);
        const double fullRight = fullTop*width/height;
        left = -fullRight + 2*fullRight*cropOrigin.x/width;
        right = left + 2*fullRight*CropSize/width;
        top = fullTop - 2*fullTop*cropOrigin.y/height;
        bottom = top - 2*fullTop*CropSize/height;
    }
};

// Remember the excursion origin across all randomly chosen walking segments.
struct PetJourney {
    std::vector<Vector3> route;
    void begin(Vector3 point) { if (route.empty()) route.push_back(point); }
    bool stepHome(PetGround& ground, float distance, float& yaw) {
        while (!route.empty()) {
            const auto target=route.back();
            const float dx=target.x-ground.position.x, dz=target.z-ground.position.z;
            const float remaining=std::hypot(dx,dz);
            if (remaining < 0.001f) { ground.position=target; route.pop_back(); continue; }
            yaw=std::atan2(dx,dz)*RAD2DEG;
            const float step=std::min(distance,remaining);
            // A direct return may cross a gap between offset monitors. Keep the
            // straight ground trajectory instead of stopping before home.
            ground.position=ground.step(yaw,step);
            if (step >= remaining) { ground.position=target; route.pop_back(); }
            return route.empty();
        }
        return true;
    }
};
