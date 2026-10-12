#pragma once
#include "raylib.h"
#include <algorithm>
#include <cstdint>
#include <vector>

// Capture the displayed pose, including a partly completed previous blend.
// raylib owns interpolation, bone matrices and skinning; no skeleton remapping.
struct ModelTransition {
    std::vector<Transform> pose;
    int lastClip=-1;
    uint64_t lastGeneration=0;
    float elapsed=1, duration=0.16f;
    bool changed(Model model,int clip,uint64_t generation) {
        const bool transition=lastClip>=0 && (clip!=lastClip || generation!=lastGeneration)
            && model.currentPose && model.skeleton.boneCount>0;
        if(transition) {
            pose.assign(model.currentPose,model.currentPose+model.skeleton.boneCount);
            elapsed=0;
        }
        lastClip=clip;lastGeneration=generation;
        return transition;
    }
    float amount() const {
        const float t=std::clamp(elapsed/duration,0.0f,1.0f);
        return t*t*(3-2*t);
    }
    float advance(float dt,bool started) {if(!started)elapsed+=dt;return amount();}
    void update(Model model,ModelAnimation animation,float frame) {
        if(!pose.empty() && amount()<1) {
            ModelAnimPose snapshot=pose.data();
            ModelAnimation frozen{};
            frozen.boneCount=static_cast<int>(pose.size());frozen.keyframeCount=1;
            frozen.keyframePoses=&snapshot;
            UpdateModelAnimationEx(model,frozen,0,animation,frame,amount());
        } else UpdateModelAnimation(model,animation,frame);
    }
};
