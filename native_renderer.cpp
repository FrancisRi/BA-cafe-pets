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
#undef PlaySound
#include "native_renderer.h"
#include "native_health.h"
#include "native_frame.h"
#include "rlgl.h"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <stdexcept>

namespace nativepet {
static double clock(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static Matrix matrix(const Mat& m){return {m[0],m[4],m[8],m[12],m[1],m[5],m[9],m[13],m[2],m[6],m[10],m[14],m[3],m[7],m[11],m[15]};}
static constexpr const char* vertex=R"(#version 330
in vec3 vertexPosition;in vec2 vertexTexCoord;in vec3 vertexNormal;in vec4 vertexColor;
uniform mat4 mvp;out vec2 uv;out vec4 color;
void main(){uv=vertexTexCoord;color=vertexColor;gl_Position=mvp*vec4(vertexPosition,1.0);}
)";
static constexpr const char* fragment=R"(#version 330
in vec2 uv;in vec4 color;uniform sampler2D texture0;uniform vec4 factor;uniform int alphaMode;uniform float cutoff;out vec4 finalColor;
vec3 linearize(vec3 v){return mix(v/12.92,pow((v+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),v));}
vec3 srgb(vec3 v){return mix(v*12.92,1.055*pow(max(v,vec3(0)),vec3(1.0/2.4))-0.055,step(vec3(0.0031308),v));}
void main(){vec4 t=texture(texture0,uv);float a=t.a*factor.a*color.a;if(alphaMode==1&&a<cutoff)discard;
 finalColor=vec4(srgb(linearize(t.rgb)*factor.rgb*color.rgb),alphaMode==2?a:1.0);}
)";
static constexpr const char* hullVertex=R"(#version 330
in vec3 vertexPosition;in vec3 vertexNormal;uniform mat4 matModel;uniform mat4 matView;uniform mat4 matProjection;uniform float outlineWidth;
void main(){vec4 p=matView*matModel*vec4(vertexPosition,1);vec3 n=normalize(mat3(matView*matModel)*vertexNormal);
 p.xyz+=n*outlineWidth*max(-p.z,0.05);gl_Position=matProjection*p;}
)";
static constexpr const char* hullFragment=R"(#version 330
out vec4 finalColor;void main(){finalColor=vec4(0,0,0,1);}
)";
static constexpr const char* contourFragment=R"(#version 330
in vec2 fragTexCoord;in vec4 fragColor;uniform sampler2D texture0;uniform vec2 texel;uniform float radius;out vec4 finalColor;
void main(){vec4 c=texture(texture0,fragTexCoord);float a=c.a;for(int i=0;i<16;i++){float angle=float(i)*6.2831853/16.0;vec2 d=vec2(cos(angle),sin(angle))*texel*radius;a=max(a,texture(texture0,fragTexCoord+d).a);}finalColor=vec4(c.rgb,c.a+(1.0-c.a)*a);}
)";
NativeRenderer::NativeRenderer(){
 unlit=LoadShaderFromMemory(vertex,fragment);hull=LoadShaderFromMemory(hullVertex,hullFragment);contour=LoadShaderFromMemory(nullptr,contourFragment);
 if(unlit.id==rlGetShaderIdDefault()||hull.id==rlGetShaderIdDefault()||contour.id==rlGetShaderIdDefault())throw std::runtime_error("Native shader compilation failed");
 factorLocation=GetShaderLocation(unlit,"factor");alphaLocation=GetShaderLocation(unlit,"alphaMode");cutoffLocation=GetShaderLocation(unlit,"cutoff");hullWidthLocation=GetShaderLocation(hull,"outlineWidth");texelLocation=GetShaderLocation(contour,"texel");radiusLocation=GetShaderLocation(contour,"radius");
 memory=CreateCompatibleDC(nullptr);if(!memory)throw std::runtime_error("Cannot create native surface DC");
 hintFont=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Microsoft YaHei UI");
}
NativeRenderer::~NativeRenderer(){model.reset();if(target.id)UnloadRenderTexture(target);if(resolved.id)UnloadRenderTexture(resolved);if(unlit.id)UnloadShader(unlit);if(hull.id)UnloadShader(hull);if(contour.id)UnloadShader(contour);if(hintFont)DeleteObject(hintFont);if(memory){if(previous)SelectObject(memory,previous);if(bitmap)DeleteObject(bitmap);DeleteDC(memory);}}
void NativeRenderer::resize(int w,int h){
 if(w==width&&h==height)return;if(w<=0||h<=0||w>8192||h>8192)throw std::runtime_error("Native viewport exceeds supported size");
 if(target.id)UnloadRenderTexture(target);if(resolved.id)UnloadRenderTexture(resolved);target={};resolved={};
 width=w;height=h;scale=std::max(1.0f,std::min(2.0f,4096.0f/std::max(w,h)));target=LoadRenderTexture(int(std::ceil(w*scale)),int(std::ceil(h*scale)));resolved=LoadRenderTexture(w,h);if(!target.id||!resolved.id)throw std::runtime_error("Cannot allocate native framebuffer");SetTextureFilter(target.texture,TEXTURE_FILTER_BILINEAR);
 if(previous){SelectObject(memory,previous);previous=nullptr;}if(bitmap)DeleteObject(bitmap);
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;bitmap=CreateDIBSection(memory,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmap)throw std::runtime_error("Cannot allocate transparent window DIB");dib=static_cast<unsigned char*>(bits);previous=SelectObject(memory,bitmap);
}
void NativeRenderer::load(const std::filesystem::path& file,const std::function<void(int,const std::string&)>& progress){
 auto candidate=std::make_unique<GltfModel>(file,progress);model=std::move(candidate);generation=UINT64_MAX;sampledAt=0;frameValid=cpuPrepared=false;
}
void NativeRenderer::drawHint(const Json& s){
 if(!s.value("menuHint",false))return;const int x=s["viewport"][0].get<int>()/2-surfaceOffset.x,y=std::clamp(s["viewport"][1].get<int>()-int(s.value("hintBelow",64))+12-int(surfaceOffset.y),0,std::max(0,height-32));const int left=std::max(0,x-85),right=std::min(width,x+85),bottom=std::min(height,y+32);
 for(int row=y;row<bottom;row++)for(int col=left;col<right;col++){auto* p=dib+(row*width+col)*4;const bool edge=row==y||row==bottom-1||col==left||col==right-1;p[0]=edge?245:255;p[1]=edge?230:251;p[2]=edge?188:245;p[3]=255;}
 auto old=SelectObject(memory,hintFont);SetBkMode(memory,TRANSPARENT);SetTextColor(memory,RGB(8,127,175));RECT rect{left,y,right,bottom};DrawTextW(memory,L"右键打开菜单",-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(memory,old);
 GdiFlush();for(int row=y;row<bottom;row++)for(int col=left;col<right;col++)dib[(row*width+col)*4+3]=255;
}
void NativeRenderer::prepareCpu(double now){
 if(cpuPrepared||!model||state.empty())return;const auto& s=state;const double elapsed=sampledAt?std::clamp(now-sampledAt,0.0,0.1):0;sampledAt=now;const double meshStart=clock();
 const int clip=s["clip"];const auto gen=s["generation"].get<uint64_t>();const bool changed=generation!=gen;if(changed){model->currentClip=-1;generation=gen;}
 double time=s["time"].get<double>();if(!s.value("paused",false)&&!s.value("review",false))time+=std::clamp(now-s["sampleTime"].get<double>()/1000,0.0,0.032)*s.value("playbackRate",1.0);const double duration=model->clips.at(clip).duration;
 time=s.value("loop",true)?std::fmod(time,duration):std::min(time,duration);model->sample(clip,time,s.value("paused",false)?0:elapsed,s.value("smooth",true)&&!s.value("review",false));model->updateGeometry(false);meshMs+=(clock()-meshStart)*1000;changedMeshes+=model->updatedParts;preparedChanged=changed;preparedAt=now;cpuPrepared=true;
}
void NativeRenderer::render(HWND display,HWND input,bool screenshot,const std::filesystem::path& output){
 didDraw=false;if(!model||state.empty())return;if(!cpuPrepared)prepareCpu(clock());const auto& s=state;const double now=preparedAt;const bool changed=preparedChanged;cpuPrepared=false;
 const auto desktop=s["desktop"],crop=s["crop"],cameraPos=s["camera"],position=s["position"];Camera3D camera{{cameraPos[0],cameraPos[1],cameraPos[2]},{0,0,0},{0,1,0},s["fov"],CAMERA_PERSPECTIVE};
 const std::array<double,20> key{cameraPos[0],cameraPos[1],cameraPos[2],s["fov"],s["far"],desktop[0],desktop[1],s["viewport"][0],s["viewport"][1],crop[0],crop[1],position[0],position[1],position[2],s["yaw"],s["zoom"],double(s.value("outlineMode",2)),s.value("outlineWidth",100.0),s.value("menuHint",false)?1.0:0.0,double(s.value("hintBelow",64))};
 // Exact dependency checks: animated geometry, expression weights, projection,
 // appearance and action visibility must all agree before reusing a frame.
 if(frameValid&&!changed&&!model->updatedParts&&key==frameKey&&!screenshot){++reusedFrames;statistics(now);return;}
 NativeHealth::mark("GPU mesh upload");const double uploadStart=clock();model->uploadGeometry();uploadMs+=(clock()-uploadStart)*1000;
 const float angle=s["yaw"].get<float>()*DEG2RAD,z=s["zoom"];Mat actor=identity();actor[0]=std::cos(angle)*z;actor[2]=-std::sin(angle)*z;actor[5]=z;actor[8]=std::sin(angle)*z;actor[10]=std::cos(angle)*z;actor[12]=position[0];actor[13]=position[1];actor[14]=position[2];const Matrix pose=matrix(actor);
 // Keep the full desktop projection, but allocate/read only the current pose.
 // Bounds come from all deformed vertices, never from sparse animation samples
 // or alpha thresholds. The display crop cannot change the ground anchor.
 const int logicalWidth=s["viewport"][0],logicalHeight=s["viewport"][1];
 float left=INFINITY,right=-INFINITY,up=INFINITY,bottom=-INFINITY;
 const auto include=[&](float x0,float y0,float x1,float y1){x0=std::max(0.0f,x0);y0=std::max(0.0f,y0);x1=std::min(float(logicalWidth),x1);y1=std::min(float(logicalHeight),y1);if(x1>=x0&&y1>=y0){left=std::min(left,x0);right=std::max(right,x1);up=std::min(up,y0);bottom=std::max(bottom,y1);}};
 const auto forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
 for(const auto& part:model->parts){bool show=model->nodes[part->node].inScene;for(int i=part->node;i>=0;i=model->nodes[i].parent)show&=model->nodes[i].visible;if(!show)continue;
  const auto& b=part->posedBounds;float x0=INFINITY,y0=INFINITY,x1=-INFINITY,y1=-INFINITY,nearDepth=INFINITY,farDepth=-INFINITY;
  for(int corner=0;corner<8;corner++){const Vector3 point{corner&1?b.max.x:b.min.x,corner&2?b.max.y:b.min.y,corner&4?b.max.z:b.min.z};const auto world=Vector3Transform(point,pose);const float depth=Vector3DotProduct(Vector3Subtract(world,camera.position),forward);nearDepth=std::min(nearDepth,depth);farDepth=std::max(farDepth,depth);const auto pixel=GetWorldToScreenEx(world,camera,desktop[0],desktop[1]);if(!std::isfinite(pixel.x)||!std::isfinite(pixel.y))continue;const float x=pixel.x-crop[0].get<float>(),y=pixel.y-crop[1].get<float>();x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x);y1=std::max(y1,y);}
  if(farDepth<5||nearDepth>s["far"].get<float>())continue;
  if(nearDepth<5)include(0,0,float(logicalWidth),float(logicalHeight));else include(x0,y0,x1,y1);
 }
 if(s.value("menuHint",false)){const float x=logicalWidth/2.0f,y=float(std::clamp(logicalHeight-int(s.value("hintBelow",64))+12,0,std::max(0,logicalHeight-32)));include(x-85,y,x+85,y+32);}
 const int padding=24+int(std::ceil(s.value("outlineWidth",100.0f)/100*4));
 int x0=0,y0=0,x1=1,y1=1;
 if(std::isfinite(left)){x0=std::max(0,int(std::floor(left))-padding);y0=std::max(0,int(std::floor(up))-padding);x1=std::min(logicalWidth,int(std::ceil(right))+padding);y1=std::min(logicalHeight,int(std::ceil(bottom))+padding);}
 const auto desired=[](int need,int limit){return std::min(limit,((need+31)/32)*32+32);};
 const int idealWidth=desired(x1-x0,logicalWidth),idealHeight=desired(y1-y0,logicalHeight);
 bool shrink=false;
 if(!changed&&s.value("resting",false)&&shrunkGeneration!=generation&&(width>idealWidth+32||height>idealHeight+32)){
  if(!smallCropAt){smallCropAt=now;smallCropWidth=idealWidth;smallCropHeight=idealHeight;}
  else {smallCropWidth=std::max(smallCropWidth,idealWidth);smallCropHeight=std::max(smallCropHeight,idealHeight);shrink=now-smallCropAt>=std::clamp(model->clips.at(s["clip"].get<int>()).duration/s.value("playbackRate",1.0),1.5,8.0);}
 }else smallCropAt=0;
 const auto capacity=[&](int need,int current,int limit,int maximum){const int ideal=desired(need,limit);if(current<=0||current<need||current>limit)return ideal;if(shrink&&current>maximum+32)return maximum;return current;};
 const int w=capacity(x1-x0,width,logicalWidth,smallCropWidth),h=capacity(y1-y0,height,logicalHeight,smallCropHeight);if(shrink){smallCropAt=0;shrunkGeneration=generation;}
 // Do not recenter the HWND for breathing/hair movement while its padded
 // bounds still fit. The camera projection continues to use the same anchor.
 if(w!=width||h!=height||x0<surfaceOffset.x||y0<surfaceOffset.y||x1>surfaceOffset.x+w||y1>surfaceOffset.y+h||surfaceOffset.x+w>logicalWidth||surfaceOffset.y+h>logicalHeight)
  surfaceOffset={std::clamp(x0-(w-(x1-x0))/2,0,logicalWidth-w),std::clamp(y0-(h-(y1-y0))/2,0,logicalHeight-h)};
 NativeHealth::mark("GPU / GDI surface resize");surfaceWidth=w;surfaceHeight=h;resize(w,h);
 const double clipNear=5,clipFar=s["far"],top=clipNear*std::tan(camera.fovy*DEG2RAD*0.5),fullHeight=top*2,fullWidth=fullHeight*desktop[0].get<double>()/desktop[1].get<double>();const double l=-fullWidth/2+(crop[0].get<double>()+surfaceOffset.x)/desktop[0].get<double>()*fullWidth,t=top-(crop[1].get<double>()+surfaceOffset.y)/desktop[1].get<double>()*fullHeight;
 const auto projection=MatrixFrustum(l,l+width/desktop[0].get<double>()*fullWidth,t-height/desktop[1].get<double>()*fullHeight,t,clipNear,clipFar);
 NativeHealth::mark("GPU draw");const int mode=s.value("outlineMode",2);const float line=s.value("outlineWidth",100.0f);BeginTextureMode(target);ClearBackground(BLANK);BeginMode3D(camera);rlSetMatrixProjection(projection);
 rlSetBlendFactorsSeparate(0x0302,0x0303,1,0x0303,0x8006,0x8006);BeginBlendMode(BLEND_CUSTOM_SEPARATE);
 const float outline=0.002f*std::tan(camera.fovy*DEG2RAD*0.5f)/std::tan(35*DEG2RAD*0.5f)*480/desktop[1].get<float>()*line/100;
 if(mode==2){SetShaderValue(hull,hullWidthLocation,&outline,SHADER_UNIFORM_FLOAT);rlEnableBackfaceCulling();rlSetCullFace(RL_CULL_FACE_FRONT);
  for(const auto& part:model->parts){bool show=true;for(int i=part->node;i>=0;i=model->nodes[i].parent)show&=model->nodes[i].visible;if(!show||part->transparent)continue;auto material=part->material;material.shader=hull;DrawMesh(part->mesh,material,pose);}rlSetCullFace(RL_CULL_FACE_BACK);
 }
 auto draw=[&](const auto& part){bool show=true;for(int i=part->node;i>=0;i=model->nodes[i].parent)show&=model->nodes[i].visible;if(!show)return;auto material=part->material;material.shader=unlit;const int alpha=part->transparent?2:part->alphaCutoff>=0?1:0;
  SetShaderValue(unlit,factorLocation,part->factor.data(),SHADER_UNIFORM_VEC4);SetShaderValue(unlit,alphaLocation,&alpha,SHADER_UNIFORM_INT);SetShaderValue(unlit,cutoffLocation,&part->alphaCutoff,SHADER_UNIFORM_FLOAT);
  if(part->doubleSided)rlDisableBackfaceCulling();else rlEnableBackfaceCulling();DrawMesh(part->mesh,material,pose);
 };
 for(const auto& part:model->parts)if(!part->transparent)draw(part);rlDisableDepthMask();for(const auto& part:model->parts)if(part->transparent)draw(part);rlEnableDepthMask();rlEnableBackfaceCulling();EndBlendMode();EndMode3D();EndTextureMode();
 BeginTextureMode(resolved);ClearBackground(BLANK);rlSetBlendFactors(1,0,0x8006);BeginBlendMode(BLEND_CUSTOM);
 if(mode==1){const float texel[]{1.0f/target.texture.width,1.0f/target.texture.height},radius=1.5f*line/100*scale;SetShaderValue(contour,texelLocation,texel,SHADER_UNIFORM_VEC2);SetShaderValue(contour,radiusLocation,&radius,SHADER_UNIFORM_FLOAT);BeginShaderMode(contour);}
 DrawTexturePro(target.texture,{0,0,float(target.texture.width),float(-target.texture.height)},{0,0,float(width),float(height)},{},0,WHITE);if(mode==1)EndShaderMode();EndBlendMode();EndTextureMode();
 NativeHealth::mark("GPU readback");const double readStart=clock();Image image=LoadImageFromTexture(resolved.texture);readMs+=(clock()-readStart)*1000;if(!image.data)throw std::runtime_error("Cannot read native frame");const double presentStart=clock();if(screenshot){ImageFlipVertical(&image);const auto path=output.u8string();ExportImage(image,reinterpret_cast<const char*>(path.c_str()));ImageFlipVertical(&image);}
 const auto* pixels=static_cast<const unsigned char*>(image.data);nativePixels(pixels,dib,width,height,region);
 NativeHealth::mark("Win32 input region");UnloadImage(image);const double maskStart=clock();if(input){const bool same=regionWindow==input&&regionWidth==width&&regionHeight==height&&inputRegion.size()==region.size()&&(region.empty()||std::memcmp(inputRegion.data(),region.data(),region.size()*sizeof(RECT))==0);
  if(same)++reusedMasks;else{const size_t size=sizeof(RGNDATAHEADER)+region.size()*sizeof(RECT);regionBytes.resize(size);auto* data=reinterpret_cast<RGNDATA*>(regionBytes.data());data->rdh={sizeof(RGNDATAHEADER),RDH_RECTANGLES,DWORD(region.size()),DWORD(region.size()*sizeof(RECT)),{0,0,width,height}};if(!region.empty())std::memcpy(data->Buffer,region.data(),region.size()*sizeof(RECT));auto r=region.empty()?CreateRectRgn(0,0,0,0):ExtCreateRegion(nullptr,DWORD(size),data);if(r){if(SetWindowRgn(input,r,FALSE)){inputRegion=region;regionWindow=input;regionWidth=width;regionHeight=height;}else DeleteObject(r);}}
 }
 maskMs+=(clock()-maskStart)*1000;drawHint(s);POINT source{},destination{s["surfaceOrigin"][0].get<LONG>()+surfaceOffset.x,s["surfaceOrigin"][1].get<LONG>()+surfaceOffset.y};SIZE size{width,height};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};const double layerStart=clock();NativeHealth::mark("Win32 layered window commit");if(!UpdateLayeredWindow(display,nullptr,&destination,&size,memory,&source,0,&blend,ULW_ALPHA))throw std::runtime_error("Native UpdateLayeredWindow failed");layerMs+=(clock()-layerStart)*1000;
 NativeHealth::mark("Win32 input window move");if(input){RECT rect{};if(!GetWindowRect(input,&rect)||rect.left!=destination.x||rect.top!=destination.y||rect.right-rect.left!=width||rect.bottom-rect.top!=height)SetWindowPos(input,nullptr,destination.x,destination.y,width,height,SWP_NOACTIVATE|SWP_NOZORDER);}
 frameKey=key;frameValid=true;didDraw=true;presentMs+=(clock()-presentStart)*1000;statistics(now);
}
void NativeRenderer::statistics(double now){++frames;if(!statsAt)statsAt=now;if(now-statsAt>=3){TraceLog(LOG_INFO,"PERF native: %.1f FPS; surface %dx%d; SSAA %.2f; mesh %.2f ms (%0.1f/%d updated); readback %.2f ms; present %.2f ms (mask %.2f, layer %.2f); cached frames %u/%u, masks %u; upload %.2f ms",frames/(now-statsAt),width,height,scale,meshMs/frames,changedMeshes/frames,int(model->parts.size()),readMs/frames,presentMs/frames,maskMs/frames,layerMs/frames,reusedFrames,frames,reusedMasks,uploadMs/frames);statsAt=now;frames=reusedFrames=reusedMasks=0;meshMs=readMs=presentMs=maskMs=layerMs=changedMeshes=uploadMs=0;}}
}
