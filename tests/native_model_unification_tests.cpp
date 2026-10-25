// SPDX-License-Identifier: GPL-3.0-only
#define PET_NATIVE_NO_ENTRY
#include "../main.cpp"
#include <iostream>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){try{
    check(argc==2,"Pass a fresh isolated test directory");
    namespace fs=std::filesystem;
    const auto root=fs::absolute(argv[1]);check(!fs::exists(root),"Never use an existing profile");
    const auto source=fs::path(__FILE__).parent_path().parent_path();
    const auto package=root/L"cache/version",program=root/L"application",profile=root/L"profile";
    fs::create_directories(package/L"model");fs::create_directories(package/L"behavior");fs::create_directories(profile);
    fs::copy_file(source/L"assets/Shiroko.glb",package/L"model/Shiroko.glb");
    fs::copy_file(source/L"design/template.bt.xml",package/L"behavior/template.bt.xml");
    fs::copy_file(source/L"release/defaults.ini",package/L"defaults.ini");
    fs::create_directories(root/L"downloads");
    const auto firstFile=root/L"downloads/First.glb",secondFile=root/L"downloads/Second.glb";
    fs::copy_file(source/L"assets/Shiroko.glb",firstFile);fs::copy_file(source/L"assets/Shiroko.glb",secondFile);
    SceneRoster original;
    original.remember({SceneRoster::key(package/L"model/Shiroko.glb"),utf8((package/L"model/Shiroko.glb").wstring()),"Shiroko","cafe",false,1.1f});
    original.remember({SceneRoster::key(firstFile),utf8(firstFile.wstring()),"First","cafe",true,1.1f});
    original.remember({SceneRoster::key(secondFile),utf8(secondFile.wstring()),"Second","battle",false,0.9f});
    original.selected=SceneRoster::key(firstFile);original.save(profile/L"characters.json");
    const auto app=[&]{auto a=std::make_unique<App>();a->directory=package;a->programDirectory=program;a->dataDirectory=profile;a->dataDirectorySpecified=true;
        a->model=package/L"model/Shiroko.glb";a->modelStorageSettings=std::make_shared<NativeModelStorage>(program,profile,false);return a;};
    const auto paths=[&](Scene& scene,const fs::path& folder){
        check(scene.error.empty(),scene.error.c_str());check(scene.roster.entries.size()==3,"Preserve all three model identities");
        for(const auto& row:scene.roster.entries)check(NativeModelStorage::same(fs::path(wide(row.path)).parent_path(),folder),"All runtime paths, including Shiroko, use one library");
        const auto second=std::find_if(scene.roster.entries.begin(),scene.roster.entries.end(),[](const auto& row){return row.name=="Second";});
        check(second!=scene.roster.entries.end()&&second->mode=="battle"&&!second->active&&std::abs(second->zoom-0.9f)<0.001f,"Hidden model settings survive relocation");
        check(scene.leader().modelStorageSettings->bundledModel.parent_path()==folder,"Bundled model identity follows the library");
    };
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);SetTraceLogLevel(LOG_ERROR);
    GraphicsContext graphics;
    {
        Scene scene(app());auto& a=scene.leader();ShowWindow(a.window,SW_HIDE);ShowWindow(a.inputWindow,SW_HIDE);
        paths(scene,program/L"model");check(a.pendingImport,"Exercise migration before the first model load");
        const auto custom=root/L"全部 模型";scene.changeModelsFolder(custom);paths(scene,custom);
        check(a.pendingModel.parent_path()==custom&&a.actorId==scene.roster.selected,"Pending load and actor identity follow the migrated record");
        scene.tick();check(a.loaded,"Load the actual GLB after relocating its pending path");
        auto* renderer=a.renderer.get();scene.changeModelsFolder(root/L"another-library");paths(scene,root/L"another-library");
        check(a.loaded&&a.renderer.get()==renderer&&a.model.parent_path()==root/L"another-library"&&a.actorId==scene.roster.selected,"Relocating a loaded actor preserves its renderer and selection");
        scene.message({{"type","reset_models_folder"}});paths(scene,program/L"model");
        scene.close(scene.roster.selected);check(scene.roster.activeCount()==0,"Hide all models");
        scene.changeModelsFolder(custom);paths(scene,custom);check(scene.roster.activeCount()==0,"Changing folders must not reveal hidden models");
    }
    {
        Scene restarted(app());paths(restarted,root/L"全部 模型");
        check(restarted.roster.activeCount()==0&&!restarted.leader().displayed,"Restart preserves the all-hidden state and does not activate Shiroko");
        check(restarted.roster.find(restarted.roster.selected)!=nullptr,"Restart retains the selected record");
        const auto white=std::find_if(restarted.roster.entries.begin(),restarted.roster.entries.end(),[](const auto& row){return row.name=="Shiroko";});
        check(white!=restarted.roster.entries.end(),"Find the bundled record");const auto whiteId=white->id;
        restarted.message({{"type","model_delete"},{"id",whiteId}});restarted.refreshModels();
        check(!restarted.roster.find(whiteId)&&fs::is_regular_file(restarted.leader().modelStorageSettings->bundledModel),"Removing Shiroko keeps its file but refreshing does not resurrect its record");
    }
    {
        Scene restarted(app());check(restarted.error.empty(),restarted.error.c_str());
        check(restarted.roster.entries.size()==2&&restarted.roster.activeCount()==0,"Restart respects removal of the bundled record");
    }
    check(fs::is_regular_file(firstFile)&&fs::is_regular_file(secondFile)&&fs::is_regular_file(package/L"model/Shiroko.glb"),"Downloaded sources and launcher seed remain intact");
    std::cout<<"Native unified model storage passed: startup, pending/live actors, custom/reset, Shiroko, aliases, hidden settings and restart\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
