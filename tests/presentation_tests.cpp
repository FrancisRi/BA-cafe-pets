#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#include "native_frame.h"
#include "shared/pet_ground.h"
#include <stdexcept>
#include <iostream>
#include <random>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    FrameDeadline frame;frame.reset(10);
    for(int i=0;i<10000;++i){const double scheduled=10+i*FrameDeadline::period;check(std::abs(frame.next-scheduled)<1e-8,"Deadline must not accumulate 8ms tick quantization");check(frame.due(scheduled+1e-9),"Expected frame is due");frame.advance(scheduled+0.003);}
    frame.reset(0);frame.advance(0.080);check(frame.next>0.080&&frame.next<0.081,"Overload renders promptly without queued catch-up or an artificial 30 FPS cap");
    check(framePosition({}, {2,0,1},0.004,10,10.030,false).x<=0.00801,"Prediction cannot overshoot the remaining walking time");
    check(framePosition({1,0,2},{2,0,1},1,10,10.030,true).x==1,"Paused gestures do not extrapolate");
    PetGround ground;Vector3 velocity{1.2f,0,0.4f};double lastTick=0;Vector3 position{};Vector2 previous{};
    for(int i=0;i<600;++i){const double renderAt=i/60.0;lastTick=std::floor(renderAt/0.008)*0.008;position=Vector3Scale(velocity,float(lastTick));const auto sampled=framePosition(position,velocity,1,lastTick,renderAt,false);const auto reference=Vector3Scale(velocity,float(renderAt));
        check(Vector3Distance(sampled,reference)<2e-6,"Frame movement must follow the render timestamp, not the last 125Hz step");
        const auto foot=ground.screen(sampled);const auto dragged=draggedFrameOrigin({320,160},{500,600},foot);check(dragged.x==320+LONG(std::lround(foot.x-500)),"Cached drag depends on committed foot, not a recentered crop");previous=foot;
    }
    std::mt19937 rng(123);for(int width:{1,2,7,32,351,416})for(int height:{1,3,20,609}){
        std::vector<unsigned char> pixels(size_t(width)*height*4),dib(pixels.size());std::vector<RECT> region;
        for(auto& value:pixels)value=unsigned(rng()%256);for(size_t i=3;i<pixels.size();i+=4)pixels[i]=static_cast<unsigned char>(rng()%25);nativePixels(pixels.data(),dib.data(),width,height,region);
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto* src=pixels.data()+(size_t(height-1-y)*width+x)*4;const auto* dst=dib.data()+(size_t(y)*width+x)*4;
            check(dst[0]==src[2]&&dst[1]==src[1]&&dst[2]==src[0]&&dst[3]==src[3],"Display pixels must retain exact BGRA and alpha");
            bool included=false;for(const auto& r:region)included|=x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom;
            bool cell=false;for(int cy=y/2*2;cy<std::min(height,y/2*2+2);++cy)for(int cx=x/2*2;cx<std::min(width,x/2*2+2);++cx)cell|=pixels[(size_t(height-1-cy)*width+cx)*4+3]>=12;
            check(included==cell,"Every opaque pixel is hit; extra hit space is limited to its 2x2 cell");
        }
        std::fill(pixels.begin(),pixels.end(),0);nativePixels(pixels.data(),dib.data(),width,height,region);check(region.empty(),"A transparent frame cannot block mouse input");
    }
    std::cout<<"60Hz deadlines, bounded movement prediction, committed drag anchors, exact BGRA and conservative alpha regions passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
