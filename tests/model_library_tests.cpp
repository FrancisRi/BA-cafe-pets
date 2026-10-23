#include "native_model_library.h"
#include <iostream>
namespace fs=std::filesystem;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void model(const fs::path& file,const std::string& text){
    fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);const std::array<uint32_t,3> header{0x46546c67,2,uint32_t(12+text.size())};out.write(reinterpret_cast<const char*>(header.data()),sizeof(header));out.write(text.data(),text.size());
}
template<class F> static void fails(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
static size_t count(const fs::path& folder){if(!fs::exists(folder))return 0;return size_t(std::distance(fs::directory_iterator(folder),fs::directory_iterator{}));}
int main(int argc,char** argv){
    try{
        check(argc==2,"Pass an isolated scratch directory");const auto scratch=fs::absolute(argv[1]);check(!fs::exists(scratch),"Use a fresh test directory");fs::create_directories(scratch);
        NativeModelLibrary library(scratch/L"data/models");const auto source=scratch/L"downloads/角色.glb";model(source,"first model contents");
        const auto original=NativeModelLibrary::digest(source);const auto plan=library.prepare(source);check(!fs::exists(library.directory),"Preparing an import has no filesystem mutations before the history-cap check");
        const auto imported=library.commit(plan);check(imported.parent_path()==library.directory&&fs::is_regular_file(imported),"Imported model lives in the persistent model library");check(NativeModelLibrary::digest(imported)==original,"Stored GLB is byte-for-byte identical");
        check(fs::is_regular_file(source),"Import never moves or deletes the source file");fs::remove(source);check(NativeModelLibrary::digest(imported)==original,"Model remains available after the original is deleted");
        const auto renamed=scratch/L"another download/renamed.GLB";model(renamed,"first model contents");check(library.store(renamed)==imported&&count(library.directory)==1,"Same content reuses its copy even with a different source filename");
        check(library.store(imported)==imported,"Importing from the model folder does not nest or duplicate models");check(library.prepare(imported).name==L"角色","Reimporting a managed file keeps the friendly model name");
        model(source,"a different model");const auto different=library.store(source);check(different!=imported&&count(library.directory)==2,"Same filename with different content preserves both models");
        const auto preserved=NativeModelLibrary::digest(imported);model(imported,"manually changed local file");const auto third=library.store(renamed);check(third!=imported&&NativeModelLibrary::digest(imported)!=preserved,"Manually modified managed files are never overwritten");
        const auto changing=scratch/L"downloads/changing.glb";model(changing,"before copy");const auto stale=library.prepare(changing);const auto before=count(library.directory);model(changing,"after file changed during import");fails([&]{library.commit(stale);},"Detect a changing source file");check(!fs::exists(stale.file)&&count(library.directory)==before,"Failed copies leave no target or temporary files");
        const auto missing=scratch/L"downloads/missing.glb";fails([&]{library.store(missing);},"Missing source is reported");const auto invalid=scratch/L"downloads/invalid.glb";{std::ofstream out(invalid);out<<"not a GLB";}fails([&]{library.store(invalid);},"Invalid GLB header is rejected before copying");
        const auto blocked=scratch/L"blocked";{std::ofstream out(blocked);out<<"directory cannot be created here";}NativeModelLibrary denied(blocked/L"models");fails([&]{denied.store(source);},"Unwritable library path is reported without damaging source");check(fs::is_regular_file(source),"Source survives a failed import");
        check(library.ensureDirectory()==library.directory,"Folder opener resolves the same persistent library");
        const auto manual=library.directory/L"手动放入.glb";model(manual,"manual file");
        const auto nested=library.directory/L"subfolder/nested.glb";model(nested,"nested file");
        {std::ofstream out(library.directory/L"notes.txt");out<<"not a model";}
        const auto inventory=library.files();check(std::find(inventory.begin(),inventory.end(),manual)!=inventory.end(),"Model inventory discovers files that have no import history");
        check(std::find(inventory.begin(),inventory.end(),nested)==inventory.end(),"Inventory only manages files directly inside the library");
        check(NativeModelLibrary::displayName(third)==L"renamed","Inventory hides managed content suffixes");
        check(library.owns(manual)&&!library.owns(source)&&!library.owns(nested),"Ownership is limited to GLB files inside the model library");
        fails([&]{library.recycle(source);},"External original files cannot be deleted");
        fails([&]{library.recycle(library.directory/L"../../downloads/角色.glb");},"Traversal cannot delete files outside the library");
        check(fs::is_regular_file(source),"Original source survives rejected deletion requests");
        const auto handle=CreateFileW(manual.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(handle!=INVALID_HANDLE_VALUE,"Lock a model against deletion");
        fails([&]{library.recycle(manual);},"Locked files report deletion failure");CloseHandle(handle);check(fs::is_regular_file(manual),"A failed deletion keeps the model file intact");
        check(library.recycle(manual)&&!fs::exists(manual),"Managed model deletion removes the file through the Windows recycle operation");
        check(!library.recycle(manual),"Already missing managed files are harmless");
        check(fs::is_regular_file(source)&&fs::is_regular_file(nested)&&fs::is_regular_file(third),"Deleting one model preserves all other models and original files");
        std::cout<<"Model library checks passed: durable imports, inventory discovery, duplicate handling, bounded ownership, recycle deletion, locked-file failure, source preservation and cleanup.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
