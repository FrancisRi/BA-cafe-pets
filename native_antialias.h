#pragma once
#include "rlgl.h"
#include <algorithm>
#include <cstdint>

// Window MSAA flags do not apply to raylib's offscreen render textures.
// Resolve coverage into the existing premultiplied RGBA texture before SSAA.
class NativeAntialias {
    using Generate=void(*)(int,unsigned*);
    using Delete=void(*)(int,const unsigned*);
    using Bind=void(*)(unsigned,unsigned);
    using Storage=void(*)(unsigned,int,unsigned,int,int);
    using Get=void(*)(unsigned,int*);
    using Parameter=void(*)(unsigned,unsigned,int*);
    using Enable=void(*)(unsigned);
    Generate generate{};Delete deleteBuffers{},deleteFrames{};Bind bind{};Storage storage{};Get get{};Parameter parameter{};Enable enable{};
    unsigned framebuffer=0,color=0,depth=0;
    int width=0,height=0,requested=-1,count=0;
    void release(){if(framebuffer)deleteFrames(1,&framebuffer);if(color)deleteBuffers(1,&color);if(depth)deleteBuffers(1,&depth);framebuffer=color=depth=0;count=0;}
public:
    NativeAntialias(){
        generate=reinterpret_cast<Generate>(rlGetProcAddress("glGenRenderbuffers"));deleteBuffers=reinterpret_cast<Delete>(rlGetProcAddress("glDeleteRenderbuffers"));deleteFrames=reinterpret_cast<Delete>(rlGetProcAddress("glDeleteFramebuffers"));bind=reinterpret_cast<Bind>(rlGetProcAddress("glBindRenderbuffer"));storage=reinterpret_cast<Storage>(rlGetProcAddress("glRenderbufferStorageMultisample"));get=reinterpret_cast<Get>(rlGetProcAddress("glGetIntegerv"));enable=reinterpret_cast<Enable>(rlGetProcAddress("glEnable"));
        parameter=reinterpret_cast<Parameter>(rlGetProcAddress("glGetRenderbufferParameteriv"));
    }
    ~NativeAntialias(){release();}
    NativeAntialias(const NativeAntialias&)=delete;NativeAntialias& operator=(const NativeAntialias&)=delete;
    int samples() const {return count;}
    void resize(int w,int h,int wanted){
        if(width==w&&height==h&&requested==wanted)return;release();width=w;height=h;requested=wanted;
        if(w<=0||h<=0||wanted<2||!generate||!deleteBuffers||!deleteFrames||!bind||!storage||!get||!parameter||!enable)return;
        int maximum=0;get(0x8D57,&maximum); // GL_MAX_SAMPLES
        // Bound extra color + depth storage to 64 MiB per pet; large surfaces
        // and unsupported devices retain the existing SSAA path.
        const auto budget=std::uint64_t(64)*1024*1024/(std::uint64_t(w)*h*8);
        const int limit=std::min({wanted,maximum,int(std::min<std::uint64_t>(budget,4))});
        const auto previous=rlGetActiveFramebuffer();int previousBuffer=0;get(0x8CA7,&previousBuffer); // GL_RENDERBUFFER_BINDING
        for(int attempt:{4,2}){
            if(attempt>limit)continue;framebuffer=rlLoadFramebuffer();generate(1,&color);generate(1,&depth);
            bind(0x8D41,color);storage(0x8D41,attempt,0x8058,w,h); // GL_RENDERBUFFER, GL_RGBA8
            int colorSamples=0;parameter(0x8D41,0x8CAB,&colorSamples); // GL_RENDERBUFFER_SAMPLES
            bind(0x8D41,depth);storage(0x8D41,attempt,0x81A6,w,h); // GL_DEPTH_COMPONENT24
            int depthSamples=0;parameter(0x8D41,0x8CAB,&depthSamples);
            // Drivers may round up a requested sample count. Honor the actual
            // allocation when enforcing the quality and memory limits.
            if(colorSamples<2||colorSamples!=depthSamples||colorSamples>limit){release();continue;}
            rlFramebufferAttach(framebuffer,color,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_RENDERBUFFER,0);
            rlFramebufferAttach(framebuffer,depth,RL_ATTACHMENT_DEPTH,RL_ATTACHMENT_RENDERBUFFER,0);
            if(framebuffer&&rlFramebufferComplete(framebuffer)){count=colorSamples;break;}release();
        }
        bind(0x8D41,unsigned(previousBuffer));rlEnableFramebuffer(previous);
    }
    void begin(){if(count){rlDrawRenderBatchActive();enable(0x809D);rlEnableFramebuffer(framebuffer);}} // GL_MULTISAMPLE
    void resolve(unsigned destination){
        if(!count)return;rlDrawRenderBatchActive();rlBindFramebuffer(0x8CA8,framebuffer);rlBindFramebuffer(0x8CA9,destination); // READ/DRAW_FRAMEBUFFER
        rlBlitFramebuffer(0,0,width,height,0,0,width,height,0x4000);rlEnableFramebuffer(destination); // GL_COLOR_BUFFER_BIT
    }
};
