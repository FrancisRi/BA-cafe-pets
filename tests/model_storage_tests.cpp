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
        std::cout<<"Model storage checks passed: EXE/model default, Unicode configuration, reset, prior libraries, invalid-path rollback, legacy recovery, backup and repeat safety.\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
