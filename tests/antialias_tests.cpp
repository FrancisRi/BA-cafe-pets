#include "raylib.h"
#include "rlgl.h"
#include "native_antialias.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <set>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
    try{
        SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(64,64,"Offscreen antialias regression");check(IsWindowReady(),"OpenGL context");
        constexpr int size=64,factor=2;
        auto source=LoadRenderTexture(size*factor,size*factor),result=LoadRenderTexture(size,size);SetTextureFilter(source.texture,TEXTURE_FILTER_BILINEAR);
        NativeAntialias antialias;antialias.resize(size*factor,size*factor,4);check(antialias.samples()>=2,"Offscreen multisample framebuffer is available");
        BeginTextureMode(source);const auto bound=rlGetActiveFramebuffer();antialias.resize(96,80,2);check(antialias.samples()<=2&&rlGetActiveFramebuffer()==bound,"Resizing balanced MSAA preserves the active render target and sample budget");EndTextureMode();
        antialias.resize(100000,100000,4);check(antialias.samples()==0,"Oversized surfaces safely fall back before allocating multisample storage");
        antialias.resize(size*factor,size*factor,0);check(antialias.samples()==0,"Power saving disables the additional framebuffer");antialias.resize(size*factor,size*factor,4);
        const Vector2 a{7.35f,6.1f},b{9.1f,59.7f},c{58.8f,48.35f};
        BeginTextureMode(source);antialias.begin();ClearBackground(BLANK);rlSetBlendFactorsSeparate(0x0302,0x0303,1,0x0303,0x8006,0x8006);BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        DrawTriangle({a.x*factor,a.y*factor},{b.x*factor,b.y*factor},{c.x*factor,c.y*factor},{210,90,30,255});EndBlendMode();antialias.resolve(source.id);EndTextureMode();
        BeginTextureMode(result);ClearBackground(BLANK);rlSetBlendFactors(1,0,0x8006);BeginBlendMode(BLEND_CUSTOM);DrawTexturePro(source.texture,{0,0,float(size*factor),float(-size*factor)},{0,0,float(size),float(size)},{},0,WHITE);EndBlendMode();EndTextureMode();
        auto image=LoadImageFromTexture(result.texture);check(image.data,"Read antialiased pixels");const auto* pixels=static_cast<const Color*>(image.data);
        const auto cross=[](Vector2 u,Vector2 v,double x,double y){return (v.x-u.x)*(y-u.y)-(v.y-u.y)*(x-u.x);};
        double error=0;int edges=0;std::set<int> coverage;
        for(int y=0;y<size;++y)for(int x=0;x<size;++x){
            int inside=0;constexpr int reference=32;
            for(int sy=0;sy<reference;++sy)for(int sx=0;sx<reference;++sx){const double px=x+(sx+.5)/reference,py=y+(sy+.5)/reference;inside+=cross(a,b,px,py)<=0&&cross(b,c,px,py)<=0&&cross(c,a,px,py)<=0;}
            const double expected=double(inside)/(reference*reference);const auto color=pixels[(size-1-y)*size+x];
            if(expected>0&&expected<1){error+=std::abs(color.a/255.0-expected);++edges;coverage.insert(color.a);}
            check(color.r<=color.a&&color.g<=color.a&&color.b<=color.a,"Premultiplied transparent pixels remain valid for the layered window");
            if(color.a==255)check(std::abs(int(color.r)-210)<=1&&std::abs(int(color.g)-90)<=1,"Opaque interiors remain sharp and unchanged");
        }
        UnloadImage(image);antialias.resize(0,0,0);UnloadRenderTexture(source);UnloadRenderTexture(result);CloseWindow();
        std::cout<<"Mean geometric edge error="<<error/edges<<"; coverage levels="<<coverage.size()<<'\n';
        check(error/edges<0.035,"Offscreen outline coverage must match the supersampled reference without visible stair steps");
        check(coverage.size()>=10,"Fine outlines need more than the five coverage levels from a 2x-only resolve");return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
