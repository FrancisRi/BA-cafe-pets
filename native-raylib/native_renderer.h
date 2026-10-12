#pragma once
#include "gltf_model.h"
// Win32 is included by main.cpp; this header keeps the model layer platform-free.
namespace nativepet {
class NativeRenderer {
    RenderTexture2D target{},resolved{};Shader unlit{},hull{},contour{};
    HDC memory{};HBITMAP bitmap{};HGDIOBJ previous{};unsigned char* dib{};
    int width=0,height=0;float scale=2;uint64_t generation=UINT64_MAX;double sampledAt=0;
    double statsAt=0,meshMs=0,readMs=0,presentMs=0,maskMs=0,layerMs=0;unsigned frames=0;
    double smallCropAt=0;int smallCropWidth=0,smallCropHeight=0;
    unsigned reusedFrames=0,reusedMasks=0;double changedMeshes=0;
    HFONT hintFont{};std::vector<RECT> inputRegion;std::vector<unsigned char> regionBytes;
    HWND regionWindow{};int regionWidth=0,regionHeight=0;
    std::array<double,20> frameKey{};bool frameValid=false;
    void statistics(double);
    int factorLocation=-1,alphaLocation=-1,cutoffLocation=-1,hullWidthLocation=-1,texelLocation=-1,radiusLocation=-1;
    std::vector<RECT> region;
    void resize(int,int);void drawHint(const Json&);
public:
    // Presentation crop only. Never fed into ground position, camera or drag bounds.
    POINT surfaceOffset{};int surfaceWidth=0,surfaceHeight=0;
    bool didDraw=false;
    std::unique_ptr<GltfModel> model;Json state;
    NativeRenderer();~NativeRenderer();
    void load(const std::filesystem::path&,const std::function<void(int,const std::string&)>&);
    void render(HWND display,HWND input,bool screenshot=false,const std::filesystem::path& output={});
};
}
