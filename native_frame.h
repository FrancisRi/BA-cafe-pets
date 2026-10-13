#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>
#include "raylib.h"
#include "raymath.h"

struct FrameDeadline {
    static constexpr double period=1.0/60;
    double next=0;
    void reset(double now){next=now;}
    bool due(double now)const{return now>=next;}
    // No queued catch-up frames. When overloaded, render as soon as possible
    // rather than skipping to the following 16.7ms slot (an artificial 30 FPS).
    void advance(double now){next+=period;if(next<=now)next=now+0.0001;}
};
inline Vector3 framePosition(Vector3 position,Vector3 velocity,double remaining,double sampled,double now,bool paused){
    const float ahead=paused?0:float(std::clamp(now-sampled,0.0,std::max(0.0,std::min(0.032,remaining))));
    return Vector3Add(position,Vector3Scale(velocity,ahead));
}
inline POINT draggedFrameOrigin(POINT committed,Vector2 from,Vector2 to){
    return {committed.x+LONG(std::lround(to.x-from.x)),committed.y+LONG(std::lround(to.y-from.y))};
}
// Pixels are RGBA, bottom-up. Draw pixels remain exact; only the hit region
// uses 2x2 cells. Every opaque pixel is included, with at most 1px extra space.
inline void nativePixels(const unsigned char* pixels,unsigned char* dib,int width,int height,std::vector<RECT>& region){
    for(int y=0;y<height;++y){const auto* row=pixels+size_t(height-1-y)*width*4;auto* dst=dib+size_t(y)*width*4;
        for(int x=0;x<width;++x){uint32_t rgba;std::memcpy(&rgba,row+x*4,4);const uint32_t bgra=(rgba&0xff00ff00u)|((rgba&0xff0000u)>>16)|((rgba&0xffu)<<16);std::memcpy(dst+x*4,&bgra,4);}}
    region.clear();size_t previousStart=0,previousCount=0;
    for(int y=0;y<height;y+=2){const size_t rowStart=region.size();const int bottom=std::min(y+2,height);int start=-1;
        for(int x=0;x<width;x+=2){bool hit=false;for(int dy=y;dy<bottom;++dy)for(int dx=x;dx<std::min(x+2,width);++dx)hit|=pixels[(size_t(height-1-dy)*width+dx)*4+3]>=12;
            if(hit&&start<0)start=x;if(!hit&&start>=0){region.push_back({start,y,x,bottom});start=-1;}}
        if(start>=0)region.push_back({start,y,width,bottom});const size_t count=region.size()-rowStart;bool same=count>0&&count==previousCount;
        for(size_t i=0;same&&i<count;++i)same=region[previousStart+i].left==region[rowStart+i].left&&region[previousStart+i].right==region[rowStart+i].right&&region[previousStart+i].bottom==y;
        if(same){for(size_t i=0;i<count;++i)region[previousStart+i].bottom=bottom;region.resize(rowStart);}else{previousStart=rowStart;previousCount=count;}
    }
}
