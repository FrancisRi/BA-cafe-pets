// Copyright (C) 2026 FrancisRi
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>

// Count completed, stationary clicks. A drag starts a new gesture sequence.
struct NativePetClicks {
    struct Event {bool doubleClick=false,tripleClick=false;};
    unsigned count=0;uint64_t firstAt=0,lastAt=0;int x=0,y=0;
    void reset(){count=0;firstAt=lastAt=0;}
    Event release(uint64_t now,int px,int py,unsigned doubleTime,int width,int height){
        if(!count||now-firstAt>std::max<uint64_t>(1200,doubleTime)||std::abs(px-x)>width/2||std::abs(py-y)>height/2){count=0;firstAt=now;}
        ++count;Event result{count==2&&now-lastAt<=doubleTime,count==3&&now-firstAt<=1200};
        lastAt=now;x=px;y=py;if(count==3)reset();return result;
    }
};
