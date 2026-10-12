#include "model_transition.h"
#include "model_faces.h"
#include "raymath.h"
#include <iostream>
#include <stdexcept>

void require(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);}
int main() {
    try {
        Transform bind{{0,0,0},QuaternionIdentity(),{1,1,1}}, current=bind;
        Matrix matrices=MatrixIdentity();BoneInfo bone{};
        Model model{};model.skeleton={1,&bone,&bind};model.currentPose=&current;model.boneMatrices=&matrices;
        Transform a{{2,3,4},QuaternionIdentity(),{1,1,1}};
        Transform b{{10,7,-4},QuaternionFromAxisAngle({0,1,0},PI),{2,2,2}};
        ModelAnimPose pa=&a,pb=&b;
        ModelAnimation first{},second{};first.boneCount=second.boneCount=1;
        first.keyframeCount=second.keyframeCount=1;first.keyframePoses=&pa;second.keyframePoses=&pb;
        ModelTransition blend;blend.changed(model,0,1);blend.update(model,first,0);
        const auto start=current;
        require(blend.changed(model,1,2),"Clip change must capture displayed pose");
        blend.advance(0,true);blend.update(model,second,0);
        require(Vector3Distance(current.translation,start.translation)<1e-5,"Transition must start without a pose jump");
        blend.advance(0.08f,false);blend.update(model,second,0);
        require(Vector3Distance(current.translation,{6,5,0})<1e-5 && std::abs(current.scale.x-1.5f)<1e-5,
            "raylib must interpolate translation and scale at blend midpoint");
        const auto forward=Vector3RotateByQuaternion({0,0,1},current.rotation);
        require(std::abs(forward.z)<1e-5,"Bone rotation must use spherical interpolation");
        const auto interrupted=current;
        blend.changed(model,0,3);blend.advance(0,true);blend.update(model,first,0);
        require(Vector3Distance(current.translation,interrupted.translation)<1e-5,
            "Interrupting a blend must capture the currently displayed intermediate pose");
        blend.advance(0.16f,false);blend.update(model,first,0);
        require(Vector3Distance(current.translation,a.translation)<1e-5,"Blend must reach the exact target pose");
        // Facial morphs use the very same blend amount as the skeleton.
        // A skinned stub avoids uploading a mesh without a graphics context.
        float vertices[3]{};unsigned char indices[4]{};Mesh mesh{};
        mesh.vertices=vertices;mesh.boneIndices=indices;model.meshes=&mesh;model.meshCount=1;
        ModelFaces faces;ModelFaces::Morph mouth;
        mouth.base={0,0,0};mouth.defaults=mouth.weights={0};mouth.deltas={{1,2,3}};
        faces.morphs.push_back(mouth);
        ModelFaces::Track track;track.node=0;track.width=1;track.times={0,1};track.values={1,1};
        faces.clips={{},{track}};faces.apply(model,0,0);faces.captureTransition();
        faces.apply(model,1,0,0);require(vertices[0]==0,"Face blend must start without a mouth jump");
        faces.apply(model,1,0,0.5f);require(vertices[0]==0.5f && vertices[2]==1.5f,"Morph weights must blend with geometry");
        faces.captureTransition();faces.apply(model,0,0,0);
        require(vertices[0]==0.5f,"Interrupted facial blend must preserve displayed weights");
        faces.apply(model,0,0,1);require(vertices[0]==0,"Missing facial tracks must return exactly to defaults");
        std::cout<<"PASS: raylib blend continuity, transforms, interruption and exact endpoints\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
