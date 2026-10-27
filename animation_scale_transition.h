// Copyright (C) 2026 FrancisRi
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>

// Some clips express the same size with opposite, very large ancestor/child
// scales (for example .01 * 100 versus .0001 * 10000). Independent TRS lerps
// create a huge intermediate mesh. Keep such unit-conversion switches atomic.
template<class Nodes,class Poses>
bool compensatingScaleTransition(const Nodes& nodes,const Poses& from){
    if(from.size()!=nodes.size())return false;
    const auto uniform=[](const auto& s){return std::isfinite(s.x)&&s.x>1e-12&&std::abs(s.y-s.x)<=s.x*1e-4&&std::abs(s.z-s.x)<=s.x*1e-4;};
    const auto ratio=[&](size_t index){const auto& a=from[index].s;const auto& b=nodes[index].current.s;return uniform(a)&&uniform(b)?double(b.x)/a.x:0.0;};
    for(size_t i=0;i<nodes.size();++i){const double child=ratio(i);if(child<=0||(child>=.25&&child<=4))continue;
        for(int parent=nodes[i].parent;parent>=0&&size_t(parent)<nodes.size();parent=nodes[parent].parent){
            const double ancestor=ratio(size_t(parent));
            if(ancestor>0&&((child>4&&ancestor<.25)||(child<.25&&ancestor>4))&&std::abs(child*ancestor-1)<.001)return true;
        }
    }
    return false;
}
