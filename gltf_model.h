#pragma once
#include "raylib.h"
#include "raymath.h"
#include "external/cgltf.h"
#include <behaviortree_cpp/contrib/json.hpp>
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nativepet {
using Json=nlohmann::json;
using Mat=std::array<float,16>; // glTF column-major; independent of raylib's storage convention.
struct Pose {Vector3 t{},s{1,1,1};Quaternion q{0,0,0,1};std::vector<float> weights;};
struct Node {std::string name;int parent=-1;bool matrix=false,inScene=false,visible=true;Mat fixed{},world{};Pose rest,current;Json extras;uint64_t worldVersion=0;};
struct Skin {std::vector<int> joints;std::vector<Mat> inverseBind,palette;std::vector<uint64_t> versions;};
struct Track {int node=-1,path=0,interpolation=0,width=0;std::vector<float> times,values;bool preserveBaseMorph=false;};
struct Clip {std::string name;double duration=0;std::vector<Track> tracks;Json extras;};
struct DrawPart {
    int node=-1,skin=-1;std::string name;Mesh mesh{};Material material{};
    std::vector<Vector3> positions,normals,smoothNormals;
    std::vector<std::vector<Vector3>> morphPositions,morphNormals;
    std::vector<unsigned> collapsedOutlineMorphs;bool outlineCollapsed=false;
    std::vector<std::array<unsigned,8>> joints;
    std::vector<std::array<float,8>> weights;
    std::vector<unsigned char> influenceCounts;
    struct InfluenceGroup {std::array<unsigned,8> joints;std::array<float,8> weights;unsigned count;Mat transform{};};
    std::vector<InfluenceGroup> influenceGroups;std::vector<unsigned> vertexGroups;
    bool sharedInfluences=false;
    std::vector<unsigned> source;
    BoundingBox posedBounds{};
    std::vector<unsigned> usedJoints;std::vector<uint64_t> jointVersions;
    std::vector<float> lastMorphWeights;uint64_t nodeVersion=UINT64_MAX;
    Vector3 geometryOrigin{};float geometryFit=0;bool geometryReady=false,needsUpload=false;
    bool transparent=false,doubleSided=false;float alphaCutoff=-1;
    std::array<float,4> factor{1,1,1,1};
    ~DrawPart();
};
struct Bounds {
    Vector3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
    void add(Vector3 p);void join(const Bounds& b);bool valid()const;
    Json json()const{return {low.x,low.y,low.z,high.x,high.y,high.z};}
};
class GltfModel {
public:
    std::vector<Node> nodes;std::vector<Skin> skins;std::vector<Clip> clips;
    std::vector<std::unique_ptr<DrawPart>> parts;std::vector<Texture2D> textures;
    Json catalog,metadata;float fit=1,eyeHeight=1.7f;Vector3 origin{};
    int currentClip=-1;bool displayed=false;double blendSeconds=0.18,transition=0;
    std::vector<Pose> frozen;
    unsigned updatedParts=0;
    explicit GltfModel(const std::filesystem::path&,const std::function<void(int,const std::string&)>&);
    ~GltfModel();
    GltfModel(const GltfModel&)=delete;GltfModel& operator=(const GltfModel&)=delete;
    void sample(int clip,double seconds,double elapsed=0,bool blend=true);
    void updateGeometry(bool upload=true);
    void uploadGeometry(); // OpenGL only; CPU preparation can run on workers.
    Bounds bounds(bool normalized=true)const;
    Vector3 point(const DrawPart&,size_t vertex,bool normal=false,bool smooth=false,bool cachedSkin=false)const;
    void restore();
private:
    std::vector<int> hierarchyOrder;std::vector<std::vector<int>> children;
    std::vector<float> channelScratch;
    struct PartGuard {int node;std::vector<unsigned char> driven;};
    std::vector<PartGuard> partGuards;
    void preparePartGuards();
    void preparePickupFaceGuard();
    void prepareGraph();
    void evaluate(int clip,double seconds);void hierarchy();void visibility(int clip);
    Json stopProfiles();void analyze(const std::function<void(int,const std::string&)>&);
};
Mat identity();Mat multiply(const Mat&,const Mat&);Vector3 transform(const Mat&,Vector3,bool direction=false);
Json discoverAnimations(const std::vector<Clip>&);
}
