// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <vector>

namespace nativepet {
// Morph visibility switches can collapse a part to a point. Inspect only the
// primitive's referenced vertices; POSITION may be shared by other materials.
// Work before skinning, which can separate the collapsed vertices again.
inline std::vector<unsigned> collapsedOutlineTargets(
    const std::vector<Vector3>& positions,
    const std::vector<std::vector<Vector3>>& targets,
    const std::vector<unsigned>& referenced){
    std::vector<unsigned> result;
    if(referenced.empty()||targets.empty())return result;
    Vector3 low=positions[referenced.front()],high=low;
    for(unsigned i:referenced){low=Vector3Min(low,positions[i]);high=Vector3Max(high,positions[i]);}
    const float size2=Vector3LengthSqr(Vector3Subtract(high,low));
    if(size2<1e-12f)return result;
    for(size_t target=0;target<targets.size();++target){
        const auto& delta=targets[target];
        low=high=Vector3Add(positions[referenced.front()],delta[referenced.front()]);
        for(unsigned i:referenced){const auto p=Vector3Add(positions[i],delta[i]);low=Vector3Min(low,p);high=Vector3Max(high,p);}
        if(Vector3LengthSqr(Vector3Subtract(high,low))<=size2*1e-10f)result.push_back(unsigned(target));
    }
    return result;
}

inline bool morphOutlineCollapsed(const std::vector<unsigned>& collapseTargets,
    size_t targetCount,const std::vector<float>& weights){
    for(unsigned target:collapseTargets){
        // Exact endpoints preserve continuous expressions and crossfades.
        // Another active Morph may expand a collapsed part into a new shape.
        if(target>=weights.size()||weights[target]!=1.0f)continue;
        bool alone=true;
        for(size_t i=0;i<std::min(targetCount,weights.size());++i)if(i!=target&&weights[i]!=0.0f){alone=false;break;}
        if(alone)return true;
    }
    return false;
}
}
