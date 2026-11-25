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
    const auto startupKey=L"Software\\BA-Cafe-Pets-Tests\\Host-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    struct RegistryCleanup {std::wstring key;~RegistryCleanup(){RegDeleteTreeW(HKEY_CURRENT_USER,key.c_str());RegDeleteKeyW(HKEY_CURRENT_USER,L"Software\\BA-Cafe-Pets-Tests");}} registryCleanup{startupKey};
    auto startup=std::make_shared<NativeAutostart>(NativeAutostart::launchExecutable(),profile,startupKey);
    const auto app=[&]{auto a=std::make_unique<App>();a->directory=package;a->programDirectory=program;a->dataDirectory=profile;a->dataDirectorySpecified=true;
        a->model=package/L"model/Shiroko.glb";a->modelStorageSettings=std::make_shared<NativeModelStorage>(program,profile,false);a->autostartSettings=startup;return a;};
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
        scene.message({{"type","set_autostart"},{"enabled",true}});check(startup->enabled()&&scene.snapshot()["startup"]["enabled"]==true,"Host startup command and snapshot work before a model finishes loading");
        scene.message({{"type","set_autostart"},{"enabled",false}});check(!startup->enabled(),"Host can disable startup independently of character preferences");
        scene.message({{"type","set_autostart"},{"enabled","invalid"}});check(!scene.error.empty()&&!startup->enabled(),"Host rejects invalid startup commands without changing Windows state");scene.error.clear();
        const auto custom=root/L"全部 模型";scene.changeModelsFolder(custom);paths(scene,custom);
        check(a.pendingModel.parent_path()==custom&&a.actorId==scene.roster.selected,"Pending load and actor identity follow the migrated record");
        scene.tick();check(a.loaded,"Load the actual GLB after relocating its pending path");
        auto* renderer=a.renderer.get();scene.changeModelsFolder(root/L"another-library");paths(scene,root/L"another-library");
        check(a.loaded&&a.renderer.get()==renderer&&a.model.parent_path()==root/L"another-library"&&a.actorId==scene.roster.selected,"Relocating a loaded actor preserves its renderer and selection");
        const auto file=a.model;
        scene.message({{"type","model_rename"},{"id",a.actorId},{"name","  桌宠白子　"}});
        check(scene.snapshot()["name"]=="桌宠白子"&&a.renderer.get()==renderer&&a.model==file&&fs::is_regular_file(file),
            "Renaming a running model changes its display name without reloading or moving the GLB");
        scene.message({{"type","model_rename"},{"id",a.actorId},{"name","　 "}});
        check(!scene.error.empty()&&scene.roster.find(a.actorId)->name=="桌宠白子","Invalid rename preserves the last saved name");

        const auto& ini=a.settings->ini;
        const auto saved=GetPrivateProfileIntW(L"Messages",L"Opacity",88,ini.c_str());
        scene.message({{"type","bubble_settings"},{"bubbleOpacity",51},{"bubbleBlur",17}});
        check(a.preferences.bubbleOpacity==51&&a.preferences.bubbleBlur==17&&scene.bubbleSaveAt&&
            GetPrivateProfileIntW(L"Messages",L"Opacity",88,ini.c_str())==saved,"Bubble sliders update in memory before saving");
        scene.flushBubblePreferences();
        check(GetPrivateProfileIntW(L"Messages",L"Opacity",88,ini.c_str())==51&&
            GetPrivateProfileIntW(L"Messages",L"Blur",8,ini.c_str())==17,"A deferred save persists both bubble settings");

        scene.message({{"type","reset_models_folder"}});paths(scene,program/L"model");
        const auto firstId=a.actorId;
        const auto whiteId=std::find_if(scene.roster.entries.begin(),scene.roster.entries.end(),
            [](const auto& row){return row.name=="Shiroko";})->id;
        scene.activate(whiteId);
        check(scene.roster.selected==whiteId&&scene.roster.firstActive()->id==firstId,
            "The first activation takes precedence over selection and roster order");
        SceneRoster savedOrder;savedOrder.load(scene.historyFile);
        check(savedOrder.firstActive()->id==firstId&&savedOrder.find(firstId)->name=="桌宠白子",
            "Activation order and renamed display name survive rereading the roster");
        scene.tick();scene.renderScene();
        auto* oldest=scene.find(firstId);check(oldest&&oldest->loaded&&oldest->renderer->modelPixels,"Render both active models");
        scene.message({{"type","preview_bubble"}});
        check(scene.bubble.visible()&&scene.bubble.current->title==L"桌宠白子","Bubble uses the oldest active model's renamed title");
        RECT actor{},model=*oldest->renderer->modelPixels,bubble{};
        GetWindowRect(oldest->window,&actor);OffsetRect(&model,actor.left,actor.top);GetWindowRect(scene.bubble.window,&bubble);
        MONITORINFO work{sizeof(work)};GetMonitorInfoW(MonitorFromPoint({std::midpoint(model.left,model.right),model.top},MONITOR_DEFAULTTONEAREST),&work);
        const auto expected=NativeMessageBubble::position(model,work.rcWork,{bubble.right-bubble.left,bubble.bottom-bubble.top},scene.bubble.theme.px(8));
        check(bubble.left==expected.x&&bubble.top==expected.y,"Bubble is physically anchored to the oldest active model");
        scene.close(firstId);
        check(scene.bubble.visible()&&scene.bubble.current->title==L"Shiroko","Hiding the oldest model hands the visible bubble to the next model");
        scene.activate(firstId);
        check(scene.roster.firstActive()->id==whiteId,"Reopening a model places it after existing active models");
        scene.close(whiteId);
        check(scene.bubble.visible()&&scene.bubble.current->title==L"桌宠白子","The renamed model retains its title after reopening");
        scene.close(firstId);check(scene.roster.activeCount()==0,"Hide all models");
        scene.changeModelsFolder(custom);paths(scene,custom);check(scene.roster.activeCount()==0,"Changing folders must not reveal hidden models");
    }
    {
        Scene restarted(app());paths(restarted,root/L"全部 模型");
        check(restarted.roster.activeCount()==0&&!restarted.leader().displayed,"Restart preserves the all-hidden state and does not activate Shiroko");
        check(std::any_of(restarted.roster.entries.begin(),restarted.roster.entries.end(),[](const auto& row){return row.name=="桌宠白子";}),
            "Renamed model display name survives application restart and model-folder migration");
        check(restarted.roster.find(restarted.roster.selected)!=nullptr,"Restart retains the selected record");
        const auto white=std::find_if(restarted.roster.entries.begin(),restarted.roster.entries.end(),[](const auto& row){return row.name=="Shiroko";});
        check(white!=restarted.roster.entries.end(),"Find the bundled record");const auto whiteId=white->id;
        restarted.message({{"type","model_delete"},{"id",whiteId}});restarted.refreshModels();
        check(!restarted.roster.find(whiteId)&&fs::is_regular_file(package/L"model/Shiroko.glb"),
            "Removing the library copy keeps the packaged source and refreshing does not resurrect the record");
    }
    {
        Scene restarted(app());check(restarted.error.empty(),restarted.error.c_str());
        check(restarted.roster.entries.size()==2&&restarted.roster.activeCount()==0,"Restart respects removal of the bundled record");
    }
    check(fs::is_regular_file(firstFile)&&fs::is_regular_file(secondFile)&&fs::is_regular_file(package/L"model/Shiroko.glb"),"Downloaded sources and launcher seed remain intact");
    std::cout<<"Native unified model storage passed: startup, pending/live actors, custom/reset, Shiroko, aliases, hidden settings and restart\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
