// SPDX-License-Identifier: GPL-3.0-only
#include "morph_outline.h"
#include "gltf_model.h"
#include <iostream>
#include <stdexcept>
using namespace nativepet;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){
    try{
        // The fourth vertex belongs to a different primitive in the accessor.
        const std::vector<Vector3> base{{-1,0,0},{1,0,0},{0,1,1},{100,100,100}};
        const std::vector<unsigned> referenced{0,1,2};
        const std::vector<Vector3> collapse{{1,0,0},{-1,0,0},{0,-1,-1},{0,0,0}};
        const std::vector<Vector3> expression{{0,0,0},{0,0,0},{0,0,-1},{0,0,0}};
        const auto targets=collapsedOutlineTargets(base,{collapse,expression},referenced);
        check(targets==std::vector<unsigned>{0},"Only a full point collapse disables outlines; ordinary flattening remains visible");
        check(morphOutlineCollapsed(targets,2,{1,0}),"A fully hidden switch must not leave an outline");
        check(!morphOutlineCollapsed(targets,2,{0,0}),"Restoring the base face restores its outline");
        check(!morphOutlineCollapsed(targets,2,{0.5f,0}),"Crossfading faces retain their outlines");
        check(!morphOutlineCollapsed(targets,2,{0.999999f,0}),"Near endpoints are still authored shapes");
        check(!morphOutlineCollapsed(targets,2,{1,0.2f}),"Another active Morph can reopen a collapsed shape");
        check(!morphOutlineCollapsed(targets,2,{}),"Missing weights use the uncollapsed base");
        check(!morphOutlineCollapsed(targets,2,{0,1}),"Ordinary expression endpoints retain outlines");
        check(morphOutlineCollapsed(targets,2,{1,0}),"Repeated switching restores suppression without sticky visibility");
        check(collapsedOutlineTargets(base,{collapse},{0,1,2,3}).empty(),"A partially collapsed primitive remains visible");
        check(collapsedOutlineTargets(base,{collapse},{}).empty(),"Empty primitives are harmless");
        std::cout<<"Morph outline visibility, shared accessors, expressions and crossfades passed\n";
        if(argc>1){
            SetTraceLogLevel(LOG_ERROR);SetConfigFlags(FLAG_WINDOW_HIDDEN);
            InitWindow(64,64,"Morph outline model regression");
            check(IsWindowReady(),"Hidden OpenGL context");
            for(int arg=1;arg<argc;++arg){
                GltfModel model(std::filesystem::path(argv[arg]),[](int,const std::string&){});
                model.sample(model.catalog["cafe"]["pickup"],0,0,false);model.updateGeometry(false);
                unsigned tested=0;
                for(const auto& part:model.parts){
                    if(part->collapsedOutlineMorphs.empty())continue;
                    bool visible=true;for(int n=part->node;n>=0;n=model.nodes[n].parent)visible&=model.nodes[n].visible;
                    if(!visible)continue;
                    auto& pose=model.nodes[part->node].current;const auto saved=pose.weights;
                    pose.weights.assign(part->morphPositions.size(),0);
                    pose.weights[part->collapsedOutlineMorphs.front()]=1;
                    model.updateGeometry(false);check(part->outlineCollapsed,"Real skinned hidden part suppresses its outline");
                    model.updateGeometry(false);check(model.updatedParts==0,"Unchanged geometry and outline state reuse the cache");
                    pose.weights[part->collapsedOutlineMorphs.front()]=0.5f;
                    model.updateGeometry(false);check(!part->outlineCollapsed,"Real model crossfade restores its outline");
                    pose.weights[part->collapsedOutlineMorphs.front()]=0;
                    model.updateGeometry(false);check(!part->outlineCollapsed,"Real model base pose restores its outline");
                    pose.weights=saved;model.updateGeometry(false);++tested;
                }
                check(tested>0,"Supplied model exercises real collapse switches");
                std::cout<<argv[arg]<<": "<<tested<<" part visibility/cache lifecycles passed\n";
            }
            CloseWindow();
        }
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
