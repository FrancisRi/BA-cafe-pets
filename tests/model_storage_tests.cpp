#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include "native_model_storage.h"
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound
#include "raylib.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
using Json=nlohmann::json;
#include "shared/scene_state.h"
namespace fs=std::filesystem;
static void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
static void writeModel(const fs::path& path,const std::string& payload){
    fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);
    const std::array<uint32_t,3> header{0x46546c67,2,uint32_t(12+payload.size())};
    out.write(reinterpret_cast<const char*>(header.data()),sizeof(header));out.write(payload.data(),payload.size());
}
int main(int argc,char** argv){
    try{
        check(argc==2,"Supply a fresh isolated directory");const auto root=fs::absolute(argv[1]);check(!fs::exists(root),"Test directory must not exist");
        const auto program=root/L"application",profile=root/L"profile/data",custom=root/L"自定义 模型";fs::create_directories(program);
        const auto defaultFolder=program/L"model";
        NativeModelStorage storage(program,profile);check(storage.current==defaultFolder,"Default model location is the model folder beside the real EXE");
        storage.change(custom);check(storage.current==custom,"Change model destination");NativeModelStorage restored(program,profile);check(restored.current==custom,"Unicode paths persist across restart");
        const auto folders=restored.directories();for(const auto& retained:{program,defaultFolder})check(std::any_of(folders.begin(),folders.end(),[&](const auto& folder){return NativeModelStorage::same(folder,retained);}),"Previous default libraries remain available after switching folders");
        const auto blocked=root/L"file-not-folder";{std::ofstream file(blocked);file<<"blocked";}bool failed=false;try{restored.change(blocked);}catch(const std::exception&){failed=true;}check(failed&&restored.current==custom,"Invalid destination preserves previous configuration");
        restored.change(defaultFolder);check(NativeModelStorage(program,profile).current==defaultFolder,"Reset returns to the model subdirectory");
        const auto oldProfile=root/L"old-profile/data";fs::create_directories(oldProfile);{std::ofstream file(oldProfile/L"model-library.json");file<<R"({"directory":"","previousDirectories":[]})";}
        check(NativeModelStorage(program,oldProfile).current==defaultFolder,"An old default setting upgrades to the model subdirectory");
        restored.change(program);check(NativeModelStorage::same(NativeModelStorage(program,profile).current,program),"An explicitly chosen EXE directory remains a supported custom location");restored.change(defaultFolder);
        const auto resource=root/L"cache/models/Shiroko.glb",legacy=program/L"build/bin/data",model=legacy/L"models/Imported.glb";
        fs::create_directories(resource.parent_path());fs::create_directories(model.parent_path());{std::ofstream file(resource);file<<"bundled";}{std::ofstream file(model);file<<"user model";}
        SceneRoster old;old.remember({"legacy-user",NativeModelStorage::utf8(model.wstring()),"Imported","battle",true,0.87f});old.selected="legacy-user";old.save(legacy/L"characters.json");
        SceneRoster current;current.remember({SceneRoster::key(resource),NativeModelStorage::utf8(resource.wstring()),"Shiroko","cafe",true});current.selected=SceneRoster::key(resource);current.save(profile/L"characters.json");
        NativeModelStorage recovery(program,profile);check(recovery.recover(current,resource),"Recover models from the former developer profile");
        check(current.find("legacy-user")&&current.find("legacy-user")->active&&current.selected=="legacy-user","Recover selected and displayed model");
        check(current.find("legacy-user")->mode=="battle"&&std::abs(current.find("legacy-user")->zoom-0.87f)<0.001f,"Recover per-model mode and size");
        SceneRoster unchanged;unchanged.load(legacy/L"characters.json");check(unchanged.selected=="legacy-user"&&fs::is_regular_file(model),"Recovery preserves the old profile and model files");
        check(std::any_of(fs::directory_iterator(profile),fs::directory_iterator{},[](const auto& file){return file.path().filename().wstring().starts_with(L"characters.before-storage-");}),"Recovery backs up existing destination records");
        current.entries.erase(std::remove_if(current.entries.begin(),current.entries.end(),[](const auto& entry){return entry.id=="legacy-user";}),current.entries.end());
        NativeModelStorage again(program,profile);check(!again.recover(current,resource)&&!current.find("legacy-user"),"A later explicit removal is not undone by repeated migration");
        // A single library owns runtime copies, including the launcher's seed.
        const auto unifiedProgram=root/L"unified-app",unifiedData=root/L"unified-profile/data",seed=root/L"unified-profile/app/version/model/Shiroko.glb";
        const auto external=root/L"downloads/角色.glb",alias=root/L"downloads/Alias.glb",oldLocal=unifiedProgram/L"models/Older.glb";
        writeModel(seed,"bundled white");writeModel(external,"user bytes");writeModel(alias,"user bytes");writeModel(oldLocal,"old local");
        NativeModelStorage unified(unifiedProgram,unifiedData,false);SceneRoster all;
        all.remember({"white",NativeModelStorage::utf8(seed.wstring()),"Shiroko","cafe",false,0.87f});
        all.remember({"user",NativeModelStorage::utf8(external.wstring()),"User","battle",true,1.2f});
        all.remember({"alias",NativeModelStorage::utf8(alias.wstring()),"Alias","cafe",false,0.95f});all.selected="user";
        const auto relocations=unified.unify(all,seed,unified.current);check(all.entries.size()==4,"Migrate registered models plus old folder inventory without merging intentional aliases");
        for(const auto& row:all.entries)check(NativeModelStorage::same(fs::path(NativeModelStorage::wide(row.path)).parent_path(),unified.current),"Every model including Shiroko points into the same library");
        check(unified.bundledModel==unified.current/L"Shiroko.glb"&&fs::is_regular_file(unified.bundledModel),"Copy the bundled seed outside the launcher cache");
        check(all.find(all.selected)->name=="User"&&all.find(all.selected)->mode=="battle"&&all.find(all.selected)->active&&std::abs(all.find(all.selected)->zoom-1.2f)<0.001f,"Migration preserves selection, mode, visibility and zoom");
        check(fs::is_regular_file(external)&&fs::is_regular_file(seed)&&fs::is_regular_file(oldLocal),"Migration preserves downloaded files and old copies");
        check(NativeModelLibrary::digest(fs::path(NativeModelStorage::wide(all.find(all.selected)->path)))==NativeModelLibrary::digest(external),"Migration verifies model contents");
        const auto customUnified=root/L"统一 模型目录";unified.unify(all,seed,customUnified);
        for(const auto& row:all.entries)check(NativeModelStorage::same(fs::path(NativeModelStorage::wide(row.path)).parent_path(),customUnified),"Changing the directory migrates all existing rows, including hidden models and Shiroko");
        NativeModelStorage reboot(unifiedProgram,unifiedData,false);check(reboot.unifiedLayout&&reboot.current==customUnified&&reboot.bundledModel==customUnified/L"Shiroko.glb","Unified layout and bundled identity survive restart");
        const auto snapshot=all.json();reboot.unify(all,seed,reboot.current);check(all.json()==snapshot,"Startup is idempotent and does not duplicate models");
        auto removed=std::find_if(all.entries.begin(),all.entries.end(),[](const auto& row){return row.name=="Older";});check(removed!=all.entries.end(),"Old inventory migrated");
        fs::remove(fs::path(NativeModelStorage::wide(removed->path)));all.entries.erase(removed);
        reboot.unify(all,seed,reboot.current);check(std::none_of(all.entries.begin(),all.entries.end(),[](const auto& row){return row.name=="Older";}),"Old source copies cannot resurrect a deleted library model");
        const auto beforeFailure=all.json();const auto broken=root/L"downloads/Broken.glb";{std::ofstream out(broken);out<<"broken";}
        all.remember({"broken",NativeModelStorage::utf8(broken.wstring()),"Broken","cafe",false});const auto staged=all.json();bool rejected=false;
        try{reboot.unify(all,seed,root/L"failed-migration");}catch(const std::exception&){rejected=true;}
        check(rejected&&reboot.current==customUnified&&all.json()==staged&&NativeModelStorage(unifiedProgram,unifiedData,false).current==customUnified,"Failed copies preserve in-memory and persisted directory/roster state");
        all.entries.pop_back();check(all.json()==beforeFailure,"Remove only the test's invalid source");
        // A user file called Shiroko.glb must not be overwritten by the seed.
        const auto collision=root/L"collision";writeModel(collision/L"Shiroko.glb","different user's white");
        reboot.unify(all,seed,collision);check(NativeModelLibrary::digest(collision/L"Shiroko.glb")!=NativeModelLibrary::digest(seed)&&NativeModelLibrary::digest(reboot.bundledModel)==NativeModelLibrary::digest(seed),"A seed filename collision preserves both different models");
        check(std::any_of(fs::directory_iterator(unifiedData),fs::directory_iterator{},[](const auto& file){return file.path().filename().wstring().starts_with(L"characters.before-unified-");}),"Back up roster before changing model paths");
        const auto rollbackData=root/L"rollback-profile",rollbackProgram=root/L"rollback-app";
        fs::create_directories(rollbackData/L"characters.json"); // Block only roster replacement, after configuration was written.
        NativeModelStorage rollback(rollbackProgram,rollbackData,false);SceneRoster pending;
        pending.remember({"user",NativeModelStorage::utf8(external.wstring()),"User","battle",true,1.2f});pending.selected="user";
        const auto pendingBefore=pending.json();bool saveRejected=false;
        try{rollback.unify(pending,seed,root/L"rollback-destination");}catch(const std::exception&){saveRejected=true;}
        check(saveRejected&&pending.json()==pendingBefore&&rollback.current==rollback.defaultDirectory()&&NativeModelStorage(rollbackProgram,rollbackData,false).current==rollback.defaultDirectory(),"Roster persistence failure rolls the saved directory back and leaves in-memory paths unchanged");
        const auto freshProgram=root/L"fresh-app",freshData=root/L"fresh-profile",freshSeed=freshProgram/L"model/Shiroko.glb";
        writeModel(freshSeed,"white");writeModel(freshProgram/L"models/Shiroko.glb","white");
        NativeModelStorage fresh(freshProgram,freshData,false);SceneRoster empty;
        fresh.unify(empty,freshSeed,fresh.current);check(empty.entries.empty(),"An old bundled backup must not make first startup look like an all-hidden user roster");
        std::cout<<"Model storage checks passed: defaults, configuration, recovery, unified runtime copies, aliases, relocation, state preservation, restart, rollback, collisions and deletion safety.\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
