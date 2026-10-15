#include "gltf_model.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <algorithm>
using namespace nativepet;

// Uses the application's real loader and GPU upload path, without showing a
// window or touching user settings. No model files are bundled in this test.
int wmain(int argc,wchar_t** argv){
    if(argc!=3){std::cerr<<"import_scan MODEL_OR_DIRECTORY REPORT.json\n";return 2;}
    const auto source=std::filesystem::path(argv[1]);
    const auto report=std::filesystem::path(argv[2]);
    std::vector<std::filesystem::path> files;
    if(std::filesystem::is_directory(source)){
        for(const auto& e:std::filesystem::recursive_directory_iterator(source))
            if(e.is_regular_file()&&e.path().extension()==".glb")files.push_back(e.path());
    }else files.push_back(source);
    std::sort(files.begin(),files.end());
    if(files.empty()){std::cerr<<"No GLB files found\n";return 2;}
    {std::ofstream output(report);if(!output){std::cerr<<"Cannot write report\n";return 2;}}
    SetTraceLogLevel(LOG_ERROR);SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64,64,"GLB import check");
    if(!IsWindowReady()){std::cerr<<"Cannot create OpenGL context\n";return 2;}
    Json results=Json::array();int failures=0;
    for(const auto& file:files){
        const auto started=std::chrono::steady_clock::now();
        auto u8=file.u8string();std::string name(u8.begin(),u8.end());
        Json row={{"file",name}};int progress=0;
        std::cout<<"CHECK "<<name<<std::endl;
        try{
            GltfModel model(file,[&](int p,const std::string&){progress=p;});
            size_t poses=0;
            for(size_t i=0;i<model.clips.size();++i)for(double fraction:{0.0,0.5,0.999}){
                model.sample(int(i),model.clips[i].duration*fraction,0,false);
                model.updateGeometry(true);
                if(!model.bounds().valid())throw std::runtime_error("Invalid pose bounds: "+model.clips[i].name);
                ++poses;
            }
            row["ok"]=true;row["clips"]=model.clips.size();row["sampledPoses"]=poses;
            row["warnings"]=model.catalog["warnings"];
        }catch(const std::exception& error){row["ok"]=false;row["error"]=error.what();++failures;}
        row["progress"]=progress;
        row["seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
        std::cout<<(row["ok"].get<bool>()?"PASS ":"FAIL ")<<row.dump()<<std::endl;
        results.push_back(std::move(row));
        std::ofstream(report)<<Json{{"total",files.size()},{"checked",results.size()},{"failures",failures},{"results",results}}.dump(2);
        PollInputEvents();
    }
    CloseWindow();return failures?1:0;
}
