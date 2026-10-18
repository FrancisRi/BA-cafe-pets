#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>
#if defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#endif
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
// Keep the final DIB and ground coordinates at native size in every quality tier.
inline float nativeRenderScale(int quality,int width,int height){
    const float requested=quality==0?1.0f:quality==1?1.5f:2.0f;
    return std::max(1.0f,std::min(requested,4096.0f/std::max(1,std::max(width,height))));
}
// Pixels are RGBA, bottom-up. Draw pixels remain exact; only the hit region
// uses 2x2 cells. Every opaque pixel is included, with at most 1px extra space.
inline void nativeBgraRow(const unsigned char* row,unsigned char* dst,int width){
    int x=0;
#if defined(_M_X64) || defined(__SSE2__)
    const auto greenAlpha=_mm_set1_epi32(int(0xff00ff00u)),redBlue=_mm_set1_epi32(0x00ff00ff);
    for(;x+4<=width;x+=4){
        const auto rgba=_mm_loadu_si128(reinterpret_cast<const __m128i*>(row+size_t(x)*4));
        const auto rb=_mm_and_si128(rgba,redBlue);
        const auto bgra=_mm_or_si128(_mm_and_si128(rgba,greenAlpha),_mm_or_si128(_mm_slli_epi32(rb,16),_mm_srli_epi32(rb,16)));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+size_t(x)*4),bgra);
    }
#endif
    for(;x<width;++x){uint32_t rgba;std::memcpy(&rgba,row+size_t(x)*4,4);const uint32_t bgra=(rgba&0xff00ff00u)|((rgba&0xff0000u)>>16)|((rgba&0xffu)<<16);std::memcpy(dst+size_t(x)*4,&bgra,4);}
}
inline void nativePixels(const unsigned char* pixels,unsigned char* dib,int width,int height,std::vector<RECT>& region){
    region.clear();size_t previousStart=0,previousCount=0;
    for(int y=0;y<height;y+=2){const size_t rowStart=region.size();const int bottom=std::min(y+2,height);int start=-1;
        // Convert and inspect the same two rows while they are in cache.
        const auto* a=pixels+size_t(height-1-y)*width*4;
        const auto* b=y+1<height?a-size_t(width)*4:a;
        nativeBgraRow(a,dib+size_t(y)*width*4,width);
        if(y+1<height)nativeBgraRow(b,dib+size_t(y+1)*width*4,width);
        for(int x=0;x<width;x+=2){const size_t p=size_t(x)*4+3;
            const bool hit=a[p]>=12||b[p]>=12||(x+1<width&&(a[p+4]>=12||b[p+4]>=12));
            if(hit&&start<0)start=x;if(!hit&&start>=0){region.push_back({start,y,x,bottom});start=-1;}}
        if(start>=0)region.push_back({start,y,width,bottom});const size_t count=region.size()-rowStart;bool same=count>0&&count==previousCount;
        for(size_t i=0;same&&i<count;++i)same=region[previousStart+i].left==region[rowStart+i].left&&region[previousStart+i].right==region[rowStart+i].right&&region[previousStart+i].bottom==y;
        if(same){for(size_t i=0;i<count;++i)region[previousStart+i].bottom=bottom;region.resize(rowStart);}else{previousStart=rowStart;previousCount=count;}
    }
}
