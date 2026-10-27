#pragma once
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <cwctype>
struct SceneEntry {std::string id,path,name,mode="cafe";bool active=false;float zoom=1;};
struct SceneRoster {
    static constexpr size_t Maximum=5;
    std::vector<SceneEntry> entries;
    std::string selected;
    static std::string key(const std::filesystem::path& file){
        uint64_t hash=14695981039346656037ull;for(wchar_t c:file.lexically_normal().wstring()){hash^=std::towlower(c);hash*=1099511628211ull;}
        std::ostringstream out;out<<std::hex<<hash;return out.str();
    }
    SceneEntry* find(const std::string& id){for(auto& entry:entries)if(entry.id==id)return &entry;return nullptr;}
    size_t activeCount() const {return std::count_if(entries.begin(),entries.end(),[](const auto& e){return e.active;});}
    bool remember(SceneEntry entry){if(find(entry.id))return true;entries.push_back(std::move(entry));return true;}
    bool enable(const std::string& id){auto* e=find(id);if(!e||(!e->active&&activeCount()>=Maximum))return false;e->active=true;selected=id;return true;}
    bool last(const std::string& id){const auto* e=find(id);return e&&e->active&&activeCount()==1;}
    Json json() const {Json rows=Json::array();for(const auto& e:entries)rows.push_back({{"id",e.id},{"path",e.path},{"name",e.name},{"active",e.active},{"mode",e.mode},{"zoom",e.zoom}});return {{"selected",selected},{"characters",rows}};}
    void load(const std::filesystem::path& file){
        if(!std::filesystem::exists(file))return;std::ifstream input(file);const auto data=Json::parse(input);
        if(!data.at("characters").is_array())throw std::runtime_error("Invalid character history");
        entries.clear();for(const auto& row:data.at("characters")){
            SceneEntry e{row.at("id"),row.at("path"),row.at("name"),row.value("mode","cafe"),row.value("active",false)};
            e.zoom=std::clamp(row.value("zoom",1.0f),0.45f,1.65f);
            if(e.id.empty()||e.path.empty()||find(e.id))continue;if(e.mode!="battle"&&e.mode!="formation")e.mode="cafe";
            if(activeCount()>=Maximum)e.active=false;entries.push_back(e);
        }selected=data.value("selected",std::string{});
    }
    void save(const std::filesystem::path& file) const {
        const auto temporary=file.wstring()+L".tmp";{std::ofstream out(std::filesystem::path(temporary),std::ios::binary);out<<json().dump(2);if(!out)throw std::runtime_error("Cannot save character history");}
        if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace character history");
    }
};
inline double segmentDistance(Vector3 a,Vector3 b,Vector3 p){
    const double x=b.x-a.x,z=b.z-a.z,length=x*x+z*z;
    const double t=length>1e-12?std::clamp(((p.x-a.x)*x+(p.z-a.z)*z)/length,0.0,1.0):0;
    return std::hypot(p.x-a.x-x*t,p.z-a.z-z*t);
}
inline float characterCollisionRadius(float bodyWidth,float height,float zoom){
    // The shoulder-width distance unit is too narrow for an entire character.
    // Include a head/body footprint and clearance, independently of follow distances.
    return std::max(0.12f,std::max(bodyWidth*0.85f,height*0.26f)+0.08f)*zoom;
}

// Wheel scales are quantized to hundredths; ties use the first displayed actor.
inline float modalCharacterZoom(const std::vector<float>& scales){
    if(scales.empty())return 1;
    int winner=std::lround(scales.front()*100),best=0;
    for(float value:scales){const int key=std::lround(value*100);int count=0;
        for(float other:scales)count+=std::lround(other*100)==key;
        if(count>best){best=count;winner=key;}}
    return std::clamp(winner/100.0f,0.45f,1.65f);
}
