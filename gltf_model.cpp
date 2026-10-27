#include "gltf_model.h"
#include "animation_keys.h"
#include "morph_outline.h"
#include "animation_scale_transition.h"
#include "rlgl.h"
#include <algorithm>
#include <bit>
#include <climits>
#include <cstring>
#include <fstream>
#include <map>
#include <numeric>
#include <regex>
#include <set>
#include <stdexcept>

namespace nativepet {
Mat identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
Mat multiply(const Mat& a,const Mat& b){Mat out{};for(int c=0;c<4;c++)for(int r=0;r<4;r++)for(int k=0;k<4;k++)out[c*4+r]+=a[k*4+r]*b[c*4+k];return out;}
Vector3 transform(const Mat& m,Vector3 p,bool direction){const float w=direction?0:1;return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12]*w,m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13]*w,m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]*w};}
static Mat local(const Pose& p){
 const auto q=p.q;const float x=q.x,y=q.y,z=q.z,w=q.w;
 return {(1-2*y*y-2*z*z)*p.s.x,(2*x*y+2*w*z)*p.s.x,(2*x*z-2*w*y)*p.s.x,0,
         (2*x*y-2*w*z)*p.s.y,(1-2*x*x-2*z*z)*p.s.y,(2*y*z+2*w*x)*p.s.y,0,
         (2*x*z+2*w*y)*p.s.z,(2*y*z-2*w*x)*p.s.z,(1-2*x*x-2*y*y)*p.s.z,0,p.t.x,p.t.y,p.t.z,1};
}
void Bounds::add(Vector3 p){if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("Non-finite animated vertex");low=Vector3Min(low,p);high=Vector3Max(high,p);}
void Bounds::join(const Bounds& b){if(b.valid()){add(b.low);add(b.high);}}
bool Bounds::valid()const{return std::isfinite(low.x)&&std::isfinite(high.x)&&high.y>=low.y;}
DrawPart::~DrawPart(){if(mesh.vertices)UnloadMesh(mesh);if(material.maps)MemFree(material.maps);}
GltfModel::~GltfModel(){parts.clear();for(auto t:textures)if(t.id)UnloadTexture(t);}
static Json extras(const cgltf_extras& e){return e.data?Json::parse(e.data):Json::object();}
static std::vector<float> unpack(const cgltf_accessor* a){if(!a)return {};std::vector<float> v(a->count*cgltf_num_components(a->type));if(cgltf_accessor_unpack_floats(a,v.data(),v.size())!=v.size())throw std::runtime_error("Cannot read GLB accessor");for(float f:v)if(!std::isfinite(f))throw std::runtime_error("Non-finite GLB accessor");return v;}
static std::vector<Vector3> vectors(const cgltf_accessor* a,size_t count,Vector3 fallback={}){
 std::vector<Vector3> result(count,fallback);if(!a)return result;
 const auto v=unpack(a);if(a->count!=count||cgltf_num_components(a->type)!=3)throw std::runtime_error("Invalid vertex attribute dimensions");
 for(size_t i=0;i<count;i++)result[i]={v[3*i],v[3*i+1],v[3*i+2]};return result;
}
static std::string compact(std::string s){std::string out;for(unsigned char c:s)if(c>=128||std::isalnum(c))out+=c<128?char(std::tolower(c)):char(c);return out;}
static bool has(const std::string& n,std::initializer_list<const char*> terms){for(const auto* t:terms)if(n.find(t)!=std::string::npos)return true;return false;}
struct Identity {std::string stem,family="Humanoid",group;bool variant=false,excluded=false;};
static Identity identify(const std::string& name){
 Identity id;auto n=compact(name);for(auto [a,b]:{std::pair{"ilde","idle"},{"callsigh","callsign"},{"strat","start"},{"destory","destroy"}}){size_t pos=0;while((pos=n.find(a,pos))!=std::string::npos){n.replace(pos,std::strlen(a),b);pos+=std::strlen(b);}}
 if(std::regex_search(n,std::regex("tank(normal|idle|move|reload|vital|appearance)")))id.family="Tank";
 else if(std::regex_search(n,std::regex("riding(attack|idle|ing|end|reload|retreat|destroy|ex)")))id.family="Riding";
 else if(std::regex_search(n,std::regex("^bst(attack|idle|move|death|formation)")))id.family="B_ST";
 else if(std::regex_search(n,std::regex("^st(attack|idle|move|death|formation)")))id.family="ST";
 auto numbers=[&](const char* expression,int number,int keep){std::regex re(expression);std::smatch m;while(std::regex_search(n,m,re)){auto g=m[number].str();if(g.size()==1)g="0"+g;if(id.group.empty())id.group=g;else id.variant=true;n.replace(m.position(),m.length(),keep?m[keep].str():"");}};
 numbers("^(\\d{1,2})(?=normal|stand|kneel|cafe|move|formation|vital|victory|exs)",1,0);
 numbers("(normal|stand|kneel|attack|shoot|fire|move|exs)(\\d{1,2})(?=idle|attack|ready|reload|public|callsign|move|vital|victory|end|start|delay|ing|loop|jump|$)",2,1);
 numbers("(idle|reload|callsign|moveing|movejump|moveendnormal|moveendstand|moveendkneel|attackstart|attackend|attackdelay|attacking|victorystart|victoryend|vitaldeath|vitalretreat)(\\d{1,2})(?=random|$)",2,1);
 if(id.family=="Tank"){n=std::regex_replace(n,std::regex("tank"),"");if(n.find("normalreload")==std::string::npos)n=std::regex_replace(n,std::regex("reload"),"normalreload");n=std::regex_replace(n,std::regex("vitaldestroy"),"vitaldeath");}
 if(id.family=="Riding")for(auto [a,b]:{std::pair{"ridingattack","normalattack"},{"ridingidle","normalidle"},{"ridingreload","normalreload"},{"ridingendnormal","moveendnormal"},{"ridinging","moveing"},{"ridingretreat","vitalretreat"},{"ridingdestroy","vitaldeath"},{"ridingex[0-9]*","exs"}})n=std::regex_replace(n,std::regex(a),b);
 if(id.family=="ST"||id.family=="B_ST"){n=std::regex_replace(n,std::regex("^b?st"),"");n=std::regex_replace(n,std::regex("^idle"),"normalidle");n=std::regex_replace(n,std::regex("^attack"),"normalattack");n=std::regex_replace(n,std::regex("^death"),"vitaldeath");}
 id.stem=n;id.excluded=has(n,{"cutin","dying","rootmotion","summoned"})||n.ends_with("root")||n.starts_with("fx");return id;
}
Json discoverAnimations(const std::vector<Clip>& clips){
 Json warnings=Json::array(),notes=Json::array();std::vector<Identity> ids;for(const auto& c:clips)ids.push_back(identify(c.name));
 auto poseOf=[](const std::string& n){return has(n,{"normal","default","basic","battle","combat","普通"})?"Normal":has(n,{"stand","standing","站立"})?"Stand":has(n,{"kneel","crouch","squat","蹲","跪"})?"Kneel":"";};
 auto equal=[&](int a,int b){const auto& x=clips[a];const auto& y=clips[b];if(x.duration!=y.duration||x.tracks.size()!=y.tracks.size()||x.tracks.empty())return false;for(size_t i=0;i<x.tracks.size();i++){const auto& p=x.tracks[i];const auto& q=y.tracks[i];if(p.node!=q.node||p.path!=q.path||p.interpolation!=q.interpolation||p.times!=q.times||p.values!=q.values)return false;}return true;};
 struct Candidate{int index,score;};
 auto find=[&](const std::string& role,const std::string& family,const std::string& group,bool quiet){
  std::vector<Candidate> all,list;const auto wanted=compact(role);std::string pose;
  for(const auto* p:{"Normal","Stand","Kneel"})if(role.find(p)!=std::string::npos)pose=p;
  for(size_t i=0;i<clips.size();i++){
   const auto& id=ids[i];const auto& n=id.stem;if(clips[i].duration<=0||id.excluded||id.family!=family)continue;
   const bool cafe=has(n,{"cafe","cafeteria","coffee","咖啡"}),attack=has(n,{"attack","shoot","fire","攻击"}),move=has(n,{"move","walk","run","行走","移动"});const std::string posture=poseOf(n);bool match=false;
   const bool start=has(n,{"start","begin","enter","准备"}),end=has(n,{"end","stop","exit","结束"}),delay=has(n,{"delay","idle","ready","wait","等待"}),reload=has(n,{"reload","reloading","换弹"});
   if(!pose.empty()){
    if(cafe||(!posture.empty()&&posture!=pose)||(posture.empty()&&pose!="Normal"))continue;
    if(role.find("Attack")!=std::string::npos){if(start&&end)continue;match=attack&&(role.ends_with("Start")?start:role.ends_with("End")?end:role.ends_with("Delay")?delay:role.ends_with("Reload")?reload:!start&&!end&&!delay&&!reload);}
    else if(role.ends_with("Reload"))match=reload;
    else if(role.ends_with("Callsign"))match=has(n,{"callsign","command","指挥"});
    else if(role.starts_with("Move"))match=move&&end;
    else match=has(n,{"idle","rest","空闲"})&&!attack&&!move&&!has(n,{"exs","skill","interaction","subidle","idlestart"});
   }else if(role=="Cafe_Idle")match=has(n,{"idle","rest","空闲"})&&!attack&&!move&&posture.empty()&&!has(n,{"exs","skill","interaction","subidle","idlestart"});
   else if(role=="Cafe_Walk"||role=="Move_Ing")match=move&&!has(n,{"end","stop","start","jump","callsign","public","interaction","结束","跳"})&&!attack&&(!cafe||role=="Cafe_Walk");
   else if(role=="Cafe_Reaction")match=has(n,{"reaction","click","touch","反应"});
   else if(role=="Formation_Idle")match=n=="formationidle";
   else if(role=="Formation_Pickup")match=has(n,{"pickup","pick","grab","拖拽"});
   else if(role=="Move_Jump")match=has(n,{"jump","hop","跳"})&&!has(n,{"random"});
   else if(role=="Exs")match=n.ends_with("exs")||n.ends_with("skill")||n.ends_with("技能");
   else if(role=="Vital_Death")match=n.ends_with("death")||n.ends_with("die")||n.ends_with("dead")||n.ends_with("死亡");
   else if(role=="Vital_Retreat")match=has(n,{"retreat","withdraw","退场"});
   else if(role=="Victory_Start")match=has(n,{"victory","win","胜利"})&&start;
   else if(role=="Victory_End")match=has(n,{"victory","win","胜利"})&&end;
   if(match)all.push_back({int(i),(n==wanted?1000:n.ends_with(wanted)?800:300+(!pose.empty()&&posture==pose?100:0)+(role.starts_with("Cafe")&&cafe?100:0))+(n=="formationidle"?20:0)-(n.find("random")!=std::string::npos?40:0)-(id.variant?20:0)});
  }
  const bool numbered=std::any_of(all.begin(),all.end(),[&](auto c){return !ids[c.index].group.empty();});
  for(auto c:all)if(ids[c.index].group==group||(ids[c.index].group.empty()&&(group.empty()||group=="01"||!numbered)))list.push_back(c);
  if(role=="Exs"&&list.empty()&&group.empty()){std::string first="99";for(auto c:all)if(!ids[c.index].group.empty())first=std::min(first,ids[c.index].group);for(auto c:all)if(ids[c.index].group==first)list.push_back(c);}
  std::sort(list.begin(),list.end(),[](auto a,auto b){return a.score!=b.score?a.score>b.score:a.index<b.index;});
  if(list.empty())return -1;
  if(list.size()>1&&list[0].score==list[1].score){bool identical=true;for(auto c:list)if(c.score==list[0].score&&!equal(list[0].index,c.index))identical=false;if(!identical){if(!quiet)warnings.push_back("Ambiguous "+role+": skipped");return -1;}}
  return list[0].index;
 };
 auto posture=[&](const std::string& name,const std::string& family,const std::string& group,bool quiet){Json p={{"name",name},{"family",family},{"group",group}};for(auto [key,suffix]:{std::pair{"idle","Idle"},{"start","Attack_Start"},{"delay","Attack_Delay"},{"attack","Attack_Ing"},{"end","Attack_End"},{"reload","Attack_Reload"},{"callsign","Callsign"}})p[key]=find(name+"_"+suffix,family,group,quiet);p["moveEnd"]=find("Move_End_"+name,family,group,quiet);if(p["reload"]==-1)p["reload"]=find(name+"_Reload",family,group,quiet);return p;};
 std::string family="Humanoid",group;bool selected=false;Json alternatives=Json::array();
 for(const auto* f:{"Humanoid","Tank","Riding","ST","B_ST"}){std::set<std::string> groups{""};for(auto id:ids)if(id.family==f&&!id.excluded&&!id.group.empty()&&has(id.stem,{"normal","stand","kneel","move"}))groups.insert(id.group);
  for(const auto& g:groups){Json names=Json::array();bool explicitPose=false;for(const auto* name:{"Normal","Stand","Kneel"}){auto p=posture(name,f,g,true);if(p["idle"]>=0&&p["attack"]>=0){names.push_back(name);explicitPose|=has(ids[p["idle"].get<int>()].stem,{"normal","stand","kneel"});}}
   bool numberedIdle=false,anyNumberedIdle=false;for(auto id:ids)if(id.family==f&&!id.group.empty()&&has(id.stem,{"idle"})){anyNumberedIdle=true;numberedIdle|=has(id.stem,{"normal","stand","kneel"});}
   if(!names.empty()&&(!g.empty()||(!numberedIdle&&(explicitPose||!anyNumberedIdle)))){alternatives.push_back({{"family",f},{"group",g},{"postures",names}});if(!selected){family=f;group=g;selected=true;}}
  }
 }
 Json battle={{"poses",Json::array()}};for(const auto* name:{"Normal","Stand","Kneel"}){auto p=posture(name,family,group,false);if(p["idle"]>=0&&p["attack"]>=0){if(p["delay"]==-1)p["delay"]=p["idle"];battle["poses"].push_back(p);}}
 for(auto [key,role]:{std::pair{"move","Move_Ing"},{"jump","Move_Jump"},{"pickup","Formation_Pickup"},{"skill","Exs"},{"death","Vital_Death"},{"retreat","Vital_Retreat"},{"victoryStart","Victory_Start"},{"victoryEnd","Victory_End"}})battle[key]=find(role,family,group,false);
 Json cafe;for(auto [key,role]:{std::pair{"idle","Cafe_Idle"},{"walk","Cafe_Walk"},{"reaction","Cafe_Reaction"},{"pickup","Formation_Pickup"}})cafe[key]=find(role,"Humanoid",group,false);
 if(cafe["idle"]==-1){cafe["idle"]=!battle["poses"].empty()?battle["poses"][0]["idle"].get<int>():0;warnings.push_back("Cafe Idle fallback: "+clips[cafe["idle"].get<int>()].name);}
 if(cafe["walk"]==-1)cafe["walk"]=battle["move"];if(cafe["pickup"]==-1)cafe["pickup"]=cafe["idle"];
 Json formation={{"idle",find("Formation_Idle","Humanoid",group,false)},{"pickup",find("Formation_Pickup","Humanoid",group,false)}};
 if(formation["idle"]==-1)formation["idle"]=find("Formation_Idle",family,group,false);
 if(formation["pickup"]==-1)formation["pickup"]=find("Formation_Pickup",family,group,false);
 formation["poses"]=Json::array();
 for(size_t i=0;i<clips.size();++i){const auto& id=ids[i];if(clips[i].duration<=0||id.excluded||(id.stem!="formationidle"&&id.stem!="formationidlerandom"))continue;
  int pickup=find("Formation_Pickup",id.family,id.group,true);if(pickup<0&&id.family!="Humanoid")pickup=find("Formation_Pickup","Humanoid",id.group,true);
  formation["poses"].push_back({{"idle",int(i)},{"pickup",pickup},{"family",id.family},{"group",id.group}});
 }
 if(formation["idle"]==-1&&!formation["poses"].empty()){formation["idle"]=formation["poses"][0]["idle"];formation["pickup"]=formation["poses"][0]["pickup"];}
 int manualSkill=battle["skill"];if(manualSkill<0)manualSkill=find("Exs","Humanoid",group,false);
 notes.push_back("Native coherent group: "+family+":"+(group.empty()?"default":group));
 return {{"cafe",cafe},{"battle",battle},{"formation",formation},{"manual",{{"skill",manualSkill}}},{"warnings",warnings},{"notes",notes},{"selection",{{"family",family},{"group",group},{"alternatives",alternatives}}}};
}

GltfModel::GltfModel(const std::filesystem::path& file,const std::function<void(int,const std::string&)>& progress){
 try{
  progress(5,"读取原始 GLB");std::ifstream input(file,std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("Cannot open GLB");const auto bytes=input.tellg();if(bytes<28||bytes>512*1024*1024)throw std::runtime_error("Invalid GLB file size");std::vector<char> memory(static_cast<size_t>(bytes));input.seekg(0);if(!input.read(memory.data(),memory.size()))throw std::runtime_error("Cannot read GLB");
  cgltf_options options{};cgltf_data* raw=nullptr;const auto parsed=cgltf_parse(&options,memory.data(),memory.size(),&raw);if(parsed!=cgltf_result_success)throw std::runtime_error("cgltf parse failed: "+std::to_string(parsed));std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
  if(raw->file_type!=cgltf_file_type_glb)throw std::runtime_error("Please import GLB 2.0");
  for(size_t i=0;i<raw->extensions_required_count;i++){const std::string name=raw->extensions_required[i];if(name!="KHR_materials_unlit"&&name!="KHR_texture_transform"&&name!="KHR_mesh_quantization")throw std::runtime_error("Unsupported required extension: "+name);}
  for(size_t i=0;i<raw->buffers_count;i++)if(raw->buffers[i].uri&&!std::string(raw->buffers[i].uri).starts_with("data:"))throw std::runtime_error("Use embedded GLB buffers");
  if(cgltf_load_buffers(&options,raw,nullptr)!=cgltf_result_success||cgltf_validate(raw)!=cgltf_result_success)throw std::runtime_error("Invalid GLB buffers or accessors");
  progress(18,"建立完整节点层级和独立蒙皮");nodes.resize(raw->nodes_count);
  for(size_t i=0;i<nodes.size();i++){const auto& n=raw->nodes[i];auto& target=nodes[i];target.name=n.name?n.name:n.mesh&&n.mesh->name?n.mesh->name:"node_"+std::to_string(i);target.parent=n.parent?int(n.parent-raw->nodes):-1;target.matrix=n.has_matrix;cgltf_node_transform_local(&n,target.fixed.data());target.extras=n.mesh?extras(n.mesh->extras):Json::object();if(!target.extras.is_object())target.extras=Json::object();if(extras(n.extras).is_object())target.extras.update(extras(n.extras));target.rest.t={n.translation[0],n.translation[1],n.translation[2]};target.rest.q={n.rotation[0],n.rotation[1],n.rotation[2],n.rotation[3]};target.rest.s={n.scale[0],n.scale[1],n.scale[2]};
   const size_t count=n.mesh?n.mesh->weights_count:0;target.rest.weights.assign(count,0);if(n.mesh)for(size_t j=0;j<count;j++)target.rest.weights[j]=n.mesh->weights[j];if(n.weights_count)target.rest.weights.assign(n.weights,n.weights+n.weights_count);
   if(n.mesh&&target.rest.weights.empty()&&n.mesh->primitives_count)target.rest.weights.assign(n.mesh->primitives[0].targets_count,0);target.current=target.rest;
  }
  const auto mark=[&](auto&& self,cgltf_node* n)->void{auto& target=nodes[n-raw->nodes];if(target.inScene)return;target.inScene=true;for(size_t j=0;j<n->children_count;j++)self(self,n->children[j]);};
  auto* scene=raw->scene?raw->scene:raw->scenes_count?&raw->scenes[0]:nullptr;
  if(scene){for(size_t i=0;i<scene->nodes_count;i++)mark(mark,scene->nodes[i]);}else for(size_t i=0;i<raw->nodes_count;i++)if(!raw->nodes[i].parent)mark(mark,&raw->nodes[i]);
  for(size_t i=0;i<raw->skins_count;i++){const auto& source=raw->skins[i];Skin s;const auto matrices=unpack(source.inverse_bind_matrices);if(!matrices.empty()&&matrices.size()!=source.joints_count*16)throw std::runtime_error("Invalid inverse bind matrix count");for(size_t j=0;j<source.joints_count;j++){s.joints.push_back(int(source.joints[j]-raw->nodes));auto m=identity();if(!matrices.empty())std::copy_n(matrices.data()+j*16,16,m.data());s.inverseBind.push_back(m);}s.palette.resize(s.joints.size());skins.push_back(std::move(s));}
  progress(30,"读取原生关键帧和表情通道");
  for(size_t i=0;i<raw->animations_count;i++){const auto& a=raw->animations[i];Clip c;c.name=a.name?a.name:"Clip_"+std::to_string(i);c.extras=extras(a.extras);
   for(size_t j=0;j<a.channels_count;j++){const auto& channel=a.channels[j];if(!channel.target_node||!channel.sampler)continue;Track t;t.node=int(channel.target_node-raw->nodes);t.path=channel.target_path;t.interpolation=channel.sampler->interpolation;t.times=unpack(channel.sampler->input);t.values=unpack(channel.sampler->output);if(t.times.empty())continue;
    const size_t factor=t.interpolation==cgltf_interpolation_type_cubic_spline?3:1;const size_t denominator=t.times.size()*factor;if(t.values.size()%denominator)throw std::runtime_error("Invalid animation sampler dimensions");t.width=int(t.values.size()/denominator);
    if(t.width<1||(t.path!=cgltf_animation_path_type_weights&&t.width!=(t.path==cgltf_animation_path_type_rotation?4:3)))throw std::runtime_error("Unsupported animation path dimensions");
    const auto duplicates=collapseDuplicateKeys(t.times,t.values,size_t(t.width)*factor);
    if(duplicates)TraceLog(LOG_WARNING,"GLB: %s channel %u: collapsed %u duplicate animation keys",c.name.c_str(),unsigned(j),unsigned(duplicates));
    if(nodes[t.node].matrix&&t.path!=cgltf_animation_path_type_weights)throw std::runtime_error("Animated TRS on a matrix node is not valid glTF");c.duration=std::max(c.duration,double(t.times.back()));c.tracks.push_back(std::move(t));
   }if(c.duration<=0)c.duration=1;clips.push_back(std::move(c));
  }
  if(clips.empty())clips.push_back({"Imported_Idle",1,{},Json::object()});catalog=discoverAnimations(clips);
  progress(40,"解码内嵌贴图");textures.resize(raw->textures_count);
  for(size_t i=0;i<raw->textures_count;i++){const auto& t=raw->textures[i];auto* im=t.image;if(!im)throw std::runtime_error("Missing texture image");const unsigned char* content=nullptr;int length=0;unsigned char* decoded=nullptr;
   if(im->buffer_view){content=cgltf_buffer_view_data(im->buffer_view);length=int(im->buffer_view->size);}
   else if(im->uri){const std::string uri=im->uri;const auto comma=uri.find(',');if(!uri.starts_with("data:")||comma==std::string::npos||uri.substr(0,comma).find(";base64")==std::string::npos)throw std::runtime_error("Use embedded PNG/JPEG images");decoded=DecodeDataBase64(uri.c_str()+comma+1,&length);content=decoded;}
   if(!content||length<=0)throw std::runtime_error("Invalid embedded image");const char* extension=im->mime_type&&std::string(im->mime_type)=="image/jpeg"?".jpg":".png";Image image=LoadImageFromMemory(extension,content,length);if(decoded)MemFree(decoded);if(!image.data)throw std::runtime_error("Cannot decode embedded image");textures[i]=LoadTextureFromImage(image);UnloadImage(image);if(!textures[i].id)throw std::runtime_error("Cannot upload GLB texture");GenTextureMipmaps(&textures[i]);SetTextureFilter(textures[i],TEXTURE_FILTER_TRILINEAR);rlTextureParameters(textures[i].id,RL_TEXTURE_WRAP_S,t.sampler?int(t.sampler->wrap_s):10497);rlTextureParameters(textures[i].id,RL_TEXTURE_WRAP_T,t.sampler?int(t.sampler->wrap_t):10497);if(t.sampler){if(t.sampler->mag_filter)rlTextureParameters(textures[i].id,RL_TEXTURE_MAG_FILTER,int(t.sampler->mag_filter));if(t.sampler->min_filter)rlTextureParameters(textures[i].id,RL_TEXTURE_MIN_FILTER,int(t.sampler->min_filter));}
  }
  progress(55,"建立原始网格与 Morph 表情");
  for(size_t ni=0;ni<nodes.size();ni++){const auto& n=raw->nodes[ni];if(!n.mesh||!nodes[ni].inScene)continue;
   for(size_t pi=0;pi<n.mesh->primitives_count;pi++){const auto& p=n.mesh->primitives[pi];if(p.has_draco_mesh_compression)throw std::runtime_error("Draco decoder is not included in this prototype");if(p.type!=cgltf_primitive_type_triangles)throw std::runtime_error("This prototype requires triangle meshes");auto* positions=cgltf_find_accessor(&p,cgltf_attribute_type_position,0);if(!positions)continue;const auto count=positions->count;
    auto part=std::make_unique<DrawPart>();part->node=int(ni);part->skin=n.skin?int(n.skin-raw->skins):-1;part->name=nodes[ni].name;part->positions=vectors(positions,count);part->normals=vectors(cgltf_find_accessor(&p,cgltf_attribute_type_normal,0),count);part->material=LoadMaterialDefault();
    std::vector<unsigned> indices;const auto total=p.indices?p.indices->count:count;if(total%3)throw std::runtime_error("Invalid triangle index count");for(size_t k=0;k<total;k++){const auto index=p.indices?cgltf_accessor_read_index(p.indices,k):k;if(index>=count)throw std::runtime_error("Invalid vertex index");indices.push_back(unsigned(index));}
    // A glTF primitive often references a tiny subset of a shared POSITION
    // accessor. Retain the exact triangles, but do not skin/upload unused vertices.
    std::vector<unsigned> remap(count,UINT_MAX),drawIndices;drawIndices.reserve(indices.size());
    for(unsigned index:indices){if(remap[index]==UINT_MAX){remap[index]=unsigned(part->source.size());part->source.push_back(index);}drawIndices.push_back(remap[index]);}
    const bool expand=part->source.size()>65535;if(expand)part->source=indices;
    std::map<std::array<long,3>,Vector3> sums;auto key=[](Vector3 v){return std::array<long,3>{std::lround(v.x*10000),std::lround(v.y*10000),std::lround(v.z*10000)};};
    std::vector<Vector3> computed(count);for(size_t k=0;k<indices.size();k+=3){const auto a=indices[k],b=indices[k+1],c=indices[k+2];const auto normal=Vector3CrossProduct(Vector3Subtract(part->positions[b],part->positions[a]),Vector3Subtract(part->positions[c],part->positions[a]));for(auto id:{a,b,c}){sums[key(part->positions[id])]=Vector3Add(sums[key(part->positions[id])],normal);computed[id]=Vector3Add(computed[id],normal);}}
    part->smoothNormals.resize(count);for(size_t k=0;k<count;k++){part->smoothNormals[k]=Vector3Normalize(sums[key(part->positions[k])]);if(Vector3LengthSqr(part->normals[k])<1e-12f)part->normals[k]=Vector3Normalize(computed[k]);}
    part->joints.resize(count);part->weights.resize(count);
    if(part->skin>=0)for(int set=0;set<2;set++){const auto* joints=cgltf_find_accessor(&p,cgltf_attribute_type_joints,set);const auto* weights=cgltf_find_accessor(&p,cgltf_attribute_type_weights,set);if(!joints&&!weights)continue;if(!joints||!weights||joints->count!=count||weights->count!=count)throw std::runtime_error("Invalid skin attributes");const auto jv=unpack(joints),wv=unpack(weights);if(jv.size()!=count*4||wv.size()!=count*4)throw std::runtime_error("Expected four weights per skin set");for(size_t k=0;k<count;k++)for(int j=0;j<4;j++){const auto joint=unsigned(jv[k*4+j]);if(wv[k*4+j]>0&&joint>=skins[part->skin].joints.size())throw std::runtime_error("Skin joint index out of range");part->joints[k][set*4+j]=joint;part->weights[k][set*4+j]=wv[k*4+j];}}
    if(part->skin>=0){std::set<unsigned> used;part->influenceCounts.resize(count);for(size_t k=0;k<part->weights.size();k++){auto& weights=part->weights[k];auto& joints=part->joints[k];float sum=std::accumulate(weights.begin(),weights.end(),0.0f);if(sum>0)for(float& w:weights)w/=sum;unsigned char n=0;for(int j=0;j<8;j++)if(weights[j]>0){joints[n]=joints[j];weights[n++]=weights[j];}part->influenceCounts[k]=n;for(int j=n;j<8;j++)weights[j]=0;}for(unsigned k:part->source)for(unsigned j=0;j<part->influenceCounts[k];j++)used.insert(part->joints[k][j]);part->usedJoints.assign(used.begin(),used.end());part->jointVersions.assign(used.size(),UINT64_MAX);}
    if(part->skin>=0){
     // Preserve the previous floating-point path (matrix blend vs vertex blend)
     // so compacting a material does not subtly change antialiased edge coverage.
     std::map<std::array<uint32_t,16>,unsigned> groups;part->vertexGroups.resize(count);
     for(size_t k=0;k<count;k++){std::array<uint32_t,16> key{};for(unsigned j=0;j<part->influenceCounts[k];j++){key[j*2]=part->joints[k][j];key[j*2+1]=std::bit_cast<uint32_t>(part->weights[k][j]);}const auto [group,added]=groups.emplace(key,unsigned(part->influenceGroups.size()));if(added)part->influenceGroups.push_back({part->joints[k],part->weights[k],part->influenceCounts[k]});part->vertexGroups[k]=group->second;}
     part->sharedInfluences=part->influenceGroups.size()*2<count;
     if(!part->sharedInfluences){part->influenceGroups.clear();part->vertexGroups.clear();}
     else{
      std::vector<DrawPart::InfluenceGroup> active;std::vector<unsigned> remapped(part->influenceGroups.size(),UINT_MAX);
      for(unsigned k:part->source){const unsigned group=part->vertexGroups[k];if(remapped[group]==UINT_MAX){remapped[group]=unsigned(active.size());active.push_back(part->influenceGroups[group]);}}
      for(auto& group:part->vertexGroups)group=remapped[group];part->influenceGroups=std::move(active);
     }
    }
    for(size_t k=0;k<p.targets_count;k++){const auto& target=p.targets[k];const cgltf_accessor* pos=nullptr;const cgltf_accessor* normal=nullptr;for(size_t a=0;a<target.attributes_count;a++){if(target.attributes[a].type==cgltf_attribute_type_position)pos=target.attributes[a].data;else if(target.attributes[a].type==cgltf_attribute_type_normal)normal=target.attributes[a].data;}part->morphPositions.push_back(vectors(pos,count));part->morphNormals.push_back(vectors(normal,count));}
    part->collapsedOutlineMorphs=collapsedOutlineTargets(part->positions,part->morphPositions,part->source);
    auto& mesh=part->mesh;mesh.vertexCount=int(part->source.size());mesh.triangleCount=int(indices.size()/3);mesh.vertices=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));mesh.normals=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));mesh.texcoords=static_cast<float*>(MemAlloc(mesh.vertexCount*2*sizeof(float)));mesh.colors=static_cast<unsigned char*>(MemAlloc(mesh.vertexCount*4));if(!mesh.vertices||!mesh.normals||!mesh.texcoords||!mesh.colors)throw std::bad_alloc();
    const cgltf_texture_view* view=nullptr;if(p.material){const auto& m=*p.material;part->transparent=m.alpha_mode==cgltf_alpha_mode_blend;part->doubleSided=m.double_sided;part->alphaCutoff=m.alpha_mode==cgltf_alpha_mode_mask?m.alpha_cutoff:-1;if(m.has_pbr_metallic_roughness){std::copy_n(m.pbr_metallic_roughness.base_color_factor,4,part->factor.data());view=&m.pbr_metallic_roughness.base_color_texture;}else if(m.has_pbr_specular_glossiness){std::copy_n(m.pbr_specular_glossiness.diffuse_factor,4,part->factor.data());view=&m.pbr_specular_glossiness.diffuse_texture;}}
    if(view&&view->texture)part->material.maps[MATERIAL_MAP_DIFFUSE].texture=textures[view->texture-raw->textures];
    const int uvIndex=view&&view->has_transform&&view->transform.has_texcoord?view->transform.texcoord:view?view->texcoord:0;const auto uv=unpack(cgltf_find_accessor(&p,cgltf_attribute_type_texcoord,uvIndex));const auto* colorAccessor=cgltf_find_accessor(&p,cgltf_attribute_type_color,0);const auto colors=unpack(colorAccessor);const int colorWidth=colorAccessor?int(cgltf_num_components(colorAccessor->type)):0;
    for(size_t k=0;k<part->source.size();k++){const auto index=part->source[k];std::memcpy(mesh.vertices+k*3,&part->positions[index],3*sizeof(float));std::memcpy(mesh.normals+k*3,&part->normals[index],3*sizeof(float));float u=uv.empty()?0:uv.at(index*2),v=uv.empty()?0:uv.at(index*2+1);if(view&&view->has_transform){const auto& t=view->transform;const float x=u*t.scale[0],y=v*t.scale[1];u=t.offset[0]+std::cos(t.rotation)*x-std::sin(t.rotation)*y;v=t.offset[1]+std::sin(t.rotation)*x+std::cos(t.rotation)*y;}mesh.texcoords[k*2]=u;mesh.texcoords[k*2+1]=v;for(int c=0;c<4;c++)mesh.colors[k*4+c]=static_cast<unsigned char>(std::clamp(colors.empty()||c>=colorWidth?1.0f:colors.at(index*colorWidth+c),0.0f,1.0f)*255+0.5f);}
    if(!expand){mesh.indices=static_cast<unsigned short*>(MemAlloc(drawIndices.size()*sizeof(unsigned short)));if(!mesh.indices)throw std::bad_alloc();for(size_t k=0;k<drawIndices.size();k++)mesh.indices[k]=static_cast<unsigned short>(drawIndices[k]);}UploadMesh(&mesh,true);if(!mesh.vaoId)throw std::runtime_error("Cannot upload GLB mesh");parts.push_back(std::move(part));
   }
  }
  if(parts.empty())throw std::runtime_error("No visible mesh in GLB scene");size_t full=0,used=0;for(const auto& p:parts){full+=p->positions.size();used+=p->source.size();}TraceLog(LOG_INFO,"MESH referenced vertices: %d / %d; unchanged triangle topology",int(used),int(full));preparePartGuards();preparePickupFaceGuard();analyze(progress);progress(100,"原始 GLB 就绪，无转换步骤");
 }catch(...){parts.clear();for(auto t:textures)if(t.id)UnloadTexture(t);textures.clear();throw;}
}
void GltfModel::restore(){for(auto& n:nodes)n.current=n.rest;hierarchy();}
void GltfModel::prepareGraph(){
 if(hierarchyOrder.size()==nodes.size())return;
 hierarchyOrder.clear();children.assign(nodes.size(),{});std::vector<unsigned char> seen(nodes.size());
 for(size_t i=0;i<nodes.size();i++)if(nodes[i].parent>=0)children.at(nodes[i].parent).push_back(int(i));
 const auto visit=[&](auto&& self,int i)->void{if(seen[i]==2)return;if(seen[i]==1)throw std::runtime_error("Cyclic node hierarchy");seen[i]=1;if(nodes[i].parent>=0)self(self,nodes[i].parent);seen[i]=2;hierarchyOrder.push_back(i);};
 for(size_t i=0;i<nodes.size();i++)visit(visit,int(i));
}
void GltfModel::hierarchy(){
 prepareGraph();for(int i:hierarchyOrder){auto& n=nodes[i];const auto matrix=n.matrix?n.fixed:local(n.current);const auto world=n.parent>=0?multiply(nodes[n.parent].world,matrix):matrix;if(!n.worldVersion||n.world!=world){n.world=world;++n.worldVersion;}}
 for(auto& s:skins){if(s.versions.size()!=s.joints.size())s.versions.assign(s.joints.size(),UINT64_MAX);for(size_t j=0;j<s.joints.size();j++){const auto& n=nodes[s.joints[j]];if(s.versions[j]!=n.worldVersion){s.palette[j]=multiply(n.world,s.inverseBind[j]);s.versions[j]=n.worldVersion;}}}
}
void GltfModel::evaluate(int clip,double seconds){
 prepareGraph();
 for(auto& n:nodes)n.current=n.rest;
 if(clip>=0&&clip<int(clips.size()))for(const auto& t:clips[clip].tracks){
  const double time=std::clamp(seconds,double(t.times.front()),double(t.times.back()));size_t left=0,right=0;float u=0,dt=0;auto at=std::upper_bound(t.times.begin(),t.times.end(),float(time));if(at==t.times.end())left=right=t.times.size()-1;else if(at!=t.times.begin()){right=size_t(at-t.times.begin());left=right-1;dt=t.times[right]-t.times[left];u=float((time-t.times[left])/dt);}
  const bool cubic=t.interpolation==cgltf_interpolation_type_cubic_spline;const auto offset=[&](size_t key,int slot){return (key*(cubic?3:1)+(cubic?slot:0))*size_t(t.width);};channelScratch.resize(t.width);auto& value=channelScratch;
  for(int k=0;k<t.width;k++){const float a=t.values[offset(left,1)+k],b=t.values[offset(right,1)+k];if(left==right||t.interpolation==cgltf_interpolation_type_step)value[k]=a;else if(cubic){const float u2=u*u,u3=u2*u;value[k]=(2*u3-3*u2+1)*a+(u3-2*u2+u)*dt*t.values[offset(left,2)+k]+(-2*u3+3*u2)*b+(u3-u2)*dt*t.values[offset(right,0)+k];}else value[k]=a+(b-a)*u;}
  auto& pose=nodes[t.node].current;
  if(t.path==cgltf_animation_path_type_rotation){Quaternion q{value[0],value[1],value[2],value[3]};if(!cubic&&left!=right&&t.interpolation!=cgltf_interpolation_type_step){const auto a=offset(left,1),b=offset(right,1);q=QuaternionSlerp({t.values[a],t.values[a+1],t.values[a+2],t.values[a+3]},{t.values[b],t.values[b+1],t.values[b+2],t.values[b+3]},u);}pose.q=QuaternionNormalize(q);}
  else if(t.path==cgltf_animation_path_type_translation)pose.t={value[0],value[1],value[2]};
  else if(t.path==cgltf_animation_path_type_scale)pose.s={value[0],value[1],value[2]};
  else if(t.path==cgltf_animation_path_type_weights){if(t.preserveBaseMorph)value=nodes[t.node].rest.weights;pose.weights=value;const auto descend=[&](auto&& self,int node)->void{if(nodes[node].rest.weights.size()==value.size())nodes[node].current.weights=value;for(int child:children[node])self(self,child);};descend(descend,t.node);}
 }
}
static std::string partKey(const std::string& text){std::string result;for(unsigned char c:text){if(std::isspace(c))result+='_';else if(c!='['&&c!=']'&&c!='.'&&c!=':'&&c!='/')result+=c<128?char(std::tolower(c)):char(c);}return result;}
static std::vector<std::string> names(const Json& j){std::vector<std::string> result;if(j.is_array()){for(auto item:j)if(item.is_string())result.push_back(item);}else if(j.is_string()){auto s=j.get<std::string>();size_t begin=0;while(begin<s.size()){auto end=s.find(',',begin);auto part=s.substr(begin,end-begin);part=std::regex_replace(part,std::regex("^\\s+|\\s+$"),"");if(!part.empty())result.push_back(part);if(end==std::string::npos)break;begin=end+1;}}return result;}
void GltfModel::preparePickupFaceGuard(){
 // Some exports animate a switch face into a point, but omit its replacement.
 // Protect only the sole, explicitly tagged face and only a collapse-only
 // pickup channel. Keep eye bones, mouth choices and expressive Morphs intact.
 std::set<int> faces;for(const auto& p:parts)if(compact(p->name).find("face")!=std::string::npos)faces.insert(p->node);
 if(faces.size()!=1)return;
 const int node=*faces.begin();const auto& n=nodes[node];bool switched=false;
 if(n.extras.is_object())for(const auto& [tag,value]:n.extras.items())
  switched|=compact(tag).starts_with("switch")&&!value.is_null()&&value!=false&&value!=0;
 if(!switched||n.rest.weights.size()!=1||n.rest.weights[0]!=0)return;
 for(const auto& p:parts)if(p->node==node){
  if(p->morphPositions.size()!=1||p->positions.empty())return;
  Bounds base,collapsed;for(size_t i=0;i<p->positions.size();i++){base.add(p->positions[i]);collapsed.add(Vector3Add(p->positions[i],p->morphPositions[0][i]));}
  const float size=Vector3Length(Vector3Subtract(base.high,base.low));
  if(size<1e-6f||Vector3Length(Vector3Subtract(collapsed.high,collapsed.low))>size*1e-5f)return;
 }
 for(auto& clip:clips)if(has(compact(clip.name),{"pickup","grab","拖拽"}))for(auto& track:clip.tracks)
  if(track.node==node&&track.path==cgltf_animation_path_type_weights&&track.width==1&&track.interpolation==cgltf_interpolation_type_step){
   // Binary visibility switching, not a continuously authored facial shape.
   if(!std::all_of(track.values.begin(),track.values.end(),[](float w){return w==0||w==1;}))continue;
   track.preserveBaseMorph=true;
   TraceLog(LOG_WARNING,"FACE pickup fallback: %s / %s; sole switch face collapses without a replacement, preserving its base Morph",n.name.c_str(),clip.name.c_str());
  }
}
void GltfModel::preparePartGuards(){
 // An optional mesh can have no node tracks and still follow animated joints
 // or an attachment parent. Inspect its real dependencies, not its filename.
 const auto optional=[&](int node){for(int i=node;i>=0;i=nodes[i].parent){if(partKey(nodes[i].name)=="optional")return true;if(nodes[i].extras.is_object())for(const auto& [tag,value]:nodes[i].extras.items())if(partKey(tag)=="optional"&&!value.is_null()&&value!=false&&value!=0)return true;}return false;};
 int primary=-1;size_t size=0;for(const auto& p:parts)if(p->skin>=0&&!optional(p->node)&&p->positions.size()>size){primary=p->skin;size=p->positions.size();}
 std::set<int> globalRoots;
 if(primary>=0){const std::set<int> joints(skins[primary].joints.begin(),skins[primary].joints.end());for(int joint:joints){bool ancestor=false;for(int i=nodes[joint].parent;i>=0;i=nodes[i].parent)ancestor|=joints.count(i)!=0;if(!ancestor)for(int i=joint;i>=0;i=nodes[i].parent)globalRoots.insert(i);}}
 std::map<int,std::vector<unsigned char>> driven;
 for(const auto& p:parts)if(optional(p->node)){
  auto& flags=driven[p->node];flags.resize(clips.size());std::set<int> transforms,weightParents;
  const auto chain=[&](int node,std::set<int>& result){for(int i=node;i>=0;i=nodes[i].parent)result.insert(i);};chain(p->node,transforms);chain(p->node,weightParents);
  if(p->skin>=0)for(unsigned joint:p->usedJoints)chain(skins[p->skin].joints[joint],transforms);
  for(size_t clip=0;clip<clips.size();clip++)for(const auto& track:clips[clip].tracks){
   const bool morph=track.path==cgltf_animation_path_type_weights;
   // Moving the common character root does not pose an independent shield/rig.
   // A rigid prop attached below that root still sees its hand/head ancestors.
   const bool drivenTransform=!morph&&transforms.count(track.node)&&!(p->skin!=primary&&globalRoots.count(track.node));
   const bool drivenMorph=morph&&!p->morphPositions.empty()&&weightParents.count(track.node)&&track.width==int(p->morphPositions.size());
   if(drivenTransform||drivenMorph){flags[clip]=1;break;}
  }
 }
 partGuards.clear();for(auto& [node,flags]:driven){const auto hidden=std::count(flags.begin(),flags.end(),0);TraceLog(LOG_INFO,"PART animation guard: %s hidden in %d/%d clips without dependent animation channels",nodes[node].name.c_str(),int(hidden),int(flags.size()));partGuards.push_back({node,std::move(flags)});}
}
void GltfModel::visibility(int clip){
 for(auto& n:nodes)n.visible=true;
 auto set=[&](const std::string& tag,bool visible){std::vector<int> matches;for(size_t i=0;i<nodes.size();i++)if(partKey(nodes[i].name)==partKey(tag))matches.push_back(int(i));if(matches.empty())for(size_t i=0;i<nodes.size();i++)if(nodes[i].extras.is_object())for(auto [name,value]:nodes[i].extras.items())if(partKey(name)==partKey(tag)&&!value.is_null()&&value!=false&&value!=0)matches.push_back(int(i));for(auto i:matches)nodes[i].visible=visible;};
 set("optional",false);if(clip>=0&&clip<int(clips.size())&&clips[clip].extras.is_object()){for(auto n:names(clips[clip].extras.value("hide",Json{})))set(n,false);for(auto n:names(clips[clip].extras.value("show",Json{})))set(n,true);}
 for(const auto& guard:partGuards)if(clip<0||clip>=int(guard.driven.size())||!guard.driven[clip])nodes[guard.node].visible=false;
}
void GltfModel::sample(int clip,double seconds,double elapsed,bool blend){
 const bool changed=clip!=currentClip;
 if(changed){frozen.clear();if(displayed&&blend)for(const auto& n:nodes)frozen.push_back(n.current);currentClip=clip;transition=0;blendSeconds=clip>=0?std::min(0.18,clips[clip].duration*0.35):0.18;visibility(clip);}
 transition+=std::max(0.0,elapsed);evaluate(clip,seconds);
 if(changed&&!frozen.empty()&&compensatingScaleTransition(nodes,frozen))frozen.clear();
 if(!frozen.empty()&&blendSeconds>0){float t=float(std::clamp(transition/blendSeconds,0.0,1.0));t=t*t*(3-2*t);for(size_t i=0;i<nodes.size();i++){auto& p=nodes[i].current;const auto& from=frozen[i];p.t=Vector3Lerp(from.t,p.t,t);p.s=Vector3Lerp(from.s,p.s,t);p.q=QuaternionSlerp(from.q,p.q,t);for(size_t j=0;j<p.weights.size();j++)p.weights[j]=(j<from.weights.size()?from.weights[j]:0)*(1-t)+p.weights[j]*t;}if(t>=1)frozen.clear();}
 hierarchy();displayed=true;
}
Vector3 GltfModel::point(const DrawPart& p,size_t index,bool normal,bool smooth,bool cachedSkin)const{
 Vector3 value=normal?(smooth?p.smoothNormals[index]:p.normals[index]):p.positions[index];const auto& weights=nodes[p.node].current.weights;
 if(!smooth)for(size_t k=0;k<std::min(weights.size(),p.morphPositions.size());k++)if(weights[k]!=0)value=Vector3Add(value,Vector3Scale(normal?p.morphNormals[k][index]:p.morphPositions[k][index],weights[k]));
 if(p.skin>=0&&cachedSkin&&p.sharedInfluences)value=transform(p.influenceGroups[p.vertexGroups[index]].transform,value,normal);
 else if(p.skin>=0){Vector3 skinned{};float sum=0;for(int k=0;k<p.influenceCounts[index];k++){const float w=p.weights[index][k];sum+=w;skinned=Vector3Add(skinned,Vector3Scale(transform(skins[p.skin].palette[p.joints[index][k]],value,normal),w));}if(sum>0)value=skinned;else value=transform(nodes[p.node].world,value,normal);}
 else value=transform(nodes[p.node].world,value,normal);
 return normal?Vector3Normalize(value):value;
}
static bool visible(const std::vector<Node>& nodes,int index){for(int i=index;i>=0;i=nodes[i].parent)if(!nodes[i].visible)return false;return nodes[index].inScene;}
void GltfModel::updateGeometry(bool upload){
 updatedParts=0;for(auto& ptr:parts){auto& p=*ptr;if(!visible(nodes,p.node))continue;const auto& n=nodes[p.node];
  bool dirty=!p.geometryReady||p.nodeVersion!=n.worldVersion||p.geometryFit!=fit||p.geometryOrigin.x!=origin.x||p.geometryOrigin.y!=origin.y||p.geometryOrigin.z!=origin.z;
  if(!p.morphPositions.empty())dirty|=p.lastMorphWeights!=n.current.weights;
  if(p.skin>=0)for(size_t j=0;j<p.usedJoints.size();j++)dirty|=p.jointVersions[j]!=skins[p.skin].versions[p.usedJoints[j]];
  if(dirty){++updatedParts;p.posedBounds={{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY}};
   p.outlineCollapsed=morphOutlineCollapsed(p.collapsedOutlineMorphs,p.morphPositions.size(),n.current.weights);
   if(p.sharedInfluences)for(auto& group:p.influenceGroups){group.transform={};if(!group.count)group.transform=n.world;else for(unsigned j=0;j<group.count;j++){const auto& bone=skins[p.skin].palette[group.joints[j]];for(int k=0;k<16;k++)group.transform[k]+=bone[k]*group.weights[j];}}
   for(size_t i=0;i<p.source.size();i++){auto v=Vector3Scale(Vector3Add(point(p,p.source[i],false,false,true),origin),fit);p.posedBounds.min=Vector3Min(p.posedBounds.min,v);p.posedBounds.max=Vector3Max(p.posedBounds.max,v);const auto normal=point(p,p.source[i],true,true,true);std::memcpy(p.mesh.vertices+i*3,&v,3*sizeof(float));std::memcpy(p.mesh.normals+i*3,&normal,3*sizeof(float));}
   p.nodeVersion=n.worldVersion;p.geometryFit=fit;p.geometryOrigin=origin;p.lastMorphWeights=n.current.weights;p.geometryReady=true;p.needsUpload=true;
   if(p.skin>=0)for(size_t j=0;j<p.usedJoints.size();j++)p.jointVersions[j]=skins[p.skin].versions[p.usedJoints[j]];
  }
  if(upload&&p.needsUpload){UpdateMeshBuffer(p.mesh,0,p.mesh.vertices,p.mesh.vertexCount*3*sizeof(float),0);UpdateMeshBuffer(p.mesh,2,p.mesh.normals,p.mesh.vertexCount*3*sizeof(float),0);p.needsUpload=false;}
 }
}
void GltfModel::uploadGeometry(){
 for(auto& part:parts)if(part->needsUpload){auto& mesh=part->mesh;UpdateMeshBuffer(mesh,0,mesh.vertices,mesh.vertexCount*3*sizeof(float),0);UpdateMeshBuffer(mesh,2,mesh.normals,mesh.vertexCount*3*sizeof(float),0);part->needsUpload=false;}
}
Bounds GltfModel::bounds(bool normalized)const{
 Bounds body;std::vector<Bounds> props;for(const auto& p:parts){if(!visible(nodes,p->node))continue;Bounds part;for(size_t i=0;i<p->positions.size();i++){auto v=point(*p,i);if(normalized)v=Vector3Scale(Vector3Add(v,origin),fit);part.add(v);}if(has(compact(p->name),{"weapon","shield","bullet","prop","effect"}))props.push_back(part);else body.join(part);}
 const auto core=body;const float reach=core.valid()?std::max(3.0f,Vector3Length(Vector3Subtract(core.high,core.low))*2):INFINITY;
 for(auto& p:props){const Vector3 d{std::max({0.0f,core.low.x-p.high.x,p.low.x-core.high.x}),std::max({0.0f,core.low.y-p.high.y,p.low.y-core.high.y}),std::max({0.0f,core.low.z-p.high.z,p.low.z-core.high.z})};if(!core.valid()||Vector3Length(d)<=reach)body.join(p);}return body;
}
Json GltfModel::stopProfiles(){
 std::set<int> bones;for(const auto& skin:skins)bones.insert(skin.joints.begin(),skin.joints.end());
 std::array<int,2> feet{-1,-1};for(int side=0;side<2;side++){int best=0;bool tie=false;for(size_t i=0;i<nodes.size();i++){if(!bones.count(int(i)))continue;auto n=nodes[i].name;std::transform(n.begin(),n.end(),n.begin(),[](unsigned char c){return char(std::tolower(c));});const bool left=std::regex_search(n,std::regex("left|(^|[_. :])l([_. :]|$)")),right=std::regex_search(n,std::regex("right|(^|[_. :])r([_. :]|$)"));const int score=std::regex_search(n,std::regex("(toe0|toebase|toes)$"))?2:n.ends_with("foot")?1:0;if(!(side==0?left:right)||!score)continue;if(score>best){best=score;feet[side]=int(i);tie=false;}else if(score==best)tie=true;}if(tie)feet[side]=-1;}
 Json profiles=Json::array();if(feet[0]<0||feet[1]<0)return profiles;std::set<int> stops;for(const auto& p:catalog["battle"]["poses"])if(p["moveEnd"]>=0)stops.insert(p["moveEnd"].get<int>());
 for(int clip:stops){const double duration=clips[clip].duration;const int count=std::min(3600,int(std::ceil(duration*60)));if(count<2)continue;const double dt=duration/count;std::vector<std::array<Vector3,2>> frames;float floor=INFINITY;
  for(int i=0;i<=count;i++){evaluate(clip,duration*i/count);hierarchy();std::array<Vector3,2> points;for(int f=0;f<2;f++){points[f]=Vector3Scale(transform(nodes[feet[f]].world,{}),fit);floor=std::min(floor,points[f].y);}frames.push_back(points);}
  int plant=-1;const int window=std::max(2,int(std::ceil(0.15/dt)));for(int i=1;i+window<int(frames.size())&&plant<0;i++)for(int f=0;f<2;f++){const auto at=frames[i][f],last=frames.back()[f];if(at.y>floor+0.045||std::hypot(at.x-last.x,at.z-last.z)>0.055)continue;double travel=0;bool raised=false;for(int j=i;j<i+window;j++){auto a=frames[j][f],b=frames[j+1][f];travel+=std::hypot(b.x-a.x,b.z-a.z);raised|=b.y>floor+0.045;}if(!raised&&travel<=0.018){plant=i;break;}}
  if(plant<0||plant*dt<0.12)continue;const double stop=plant*dt;double support=0;for(int i=0;i<plant;i++){const auto& a=frames[i];const auto& b=frames[i+1];const int f=a[0].y+b[0].y<a[1].y+b[1].y?0:1;if((a[f].y+b[f].y)*0.5<=floor+0.07)support+=std::max(0.0f,a[f].z-b[f].z);}
  const double budget=std::min(2.376*stop*0.5,std::max(2.376*0.06,support));auto curve=[&](double power){std::vector<double> speeds(65);for(int i=0;i<65;i++){const double t=i/64.0;speeds[i]=2.376*std::pow(std::max(0.0,1-t*t*(3-2*t)),power);}return speeds;};auto integral=[&](const auto& v){double result=0;for(size_t i=1;i<v.size();i++)result+=(v[i]+v[i-1])*stop/64*0.5;return result;};double low=1,high=64;for(int i=0;i<24;i++){const double mid=(low+high)*0.5;if(integral(curve(mid))>budget)low=mid;else high=mid;}profiles.push_back({{"clip",clip},{"stopTime",stop},{"speeds",curve((low+high)*0.5)}});
 }return profiles;
}
void GltfModel::analyze(const std::function<void(int,const std::string&)>& progress){
 progress(65,"计算统一大小和身体尺度");const int idle=catalog["cafe"]["idle"];visibility(idle);evaluate(idle,0);hierarchy();const auto initial=bounds(false);const float height=initial.high.y-initial.low.y;if(!initial.valid()||height<1e-6)throw std::runtime_error("Invalid model height");fit=2.3f/height;origin={-(initial.low.x+initial.high.x)/2,-initial.low.y,-(initial.low.z+initial.high.z)/2};
 float eyeSum=0;int eyeCount=0;for(const auto& n:nodes)if(n.name=="Bip001 eye_L"||n.name=="Bip001 eye_R"){eyeSum+=(transform(n.world,{}).y+origin.y)*fit;eyeCount++;}eyeHeight=eyeCount?eyeSum/eyeCount:1.7f;
 std::vector<float> xs;for(const auto& part:parts){const auto& p=*part;if(p.skin<0||!visible(nodes,p.node)||has(compact(p.name),{"weapon","bullet","halo","backpack","prop","glasses"}))continue;
  for(size_t i=0;i<p.positions.size();i++){float influence=0;for(int k=0;k<8;k++)if(p.weights[i][k]>0&&has(compact(nodes[skins[p.skin].joints[p.joints[i][k]]].name),{"spine","pelvis","hips","chest","head"}))influence+=p.weights[i][k];if(influence<0.5)continue;const auto v=Vector3Scale(Vector3Add(point(p,i),origin),fit);if(v.y>=2.3*0.35&&v.y<=2.3*0.9)xs.push_back(v.x);}}
 float bodyWidth=2.3f*0.28f;std::string source="height-fallback";if(xs.size()>=20){std::sort(xs.begin(),xs.end());const auto w=xs[size_t((xs.size()-1)*0.98)]-xs[size_t((xs.size()-1)*0.02)];if(w>0.1){bodyWidth=w;source="body-skin";}}
 if(source=="height-fallback"){std::vector<Vector3> shoulders;for(const auto& n:nodes)if(compact(n.name).find("upperarm")!=std::string::npos)shoulders.push_back(transform(n.world,{}));if(shoulders.size()==2&&Vector3Distance(shoulders[0],shoulders[1])*fit>0.1){bodyWidth=Vector3Distance(shoulders[0],shoulders[1])*fit;source="shoulder-bones";}}
 const auto profiles=stopProfiles();progress(75,"采样必要动作的显示边界");Bounds resting;std::set<int> restingClips{idle};for(const auto& p:catalog["battle"]["poses"])restingClips.insert(p["idle"].get<int>());
 auto measure=[&](int index,double time){visibility(index);evaluate(index,time);hierarchy();return bounds();};
 const auto originalRestingClips=restingClips;const int formationIdle=catalog["formation"]["idle"];if(formationIdle>=0)restingClips.insert(formationIdle);
 for(const auto& pose:catalog["formation"]["poses"])restingClips.insert(pose["idle"].get<int>());
 Json placement=Json::array();
 for(int index:restingClips)if(index>=0){
  std::vector<Bounds> boxes(parts.size());
  for(double fraction:{0.0,0.25,0.5,0.75,1.0}){
   const auto sampled=measure(index,clips[index].duration*fraction);if(originalRestingClips.count(index))resting.join(sampled);
   // Placement uses only vertices this visible primitive actually draws.
   // Keep normalization and the rendering envelope unchanged.
   for(size_t k=0;k<parts.size();++k){const auto& p=*parts[k];if(!visible(nodes,p.node))continue;
    for(unsigned vertex:p.source)boxes[k].add(Vector3Scale(Vector3Add(point(p,vertex),origin),fit));
   }
  }
  Json shapes=Json::array();for(const auto& box:boxes)if(box.valid())shapes.push_back(box.json());
  placement.push_back({{"clip",index},{"boxes",shapes}});
 }
 Bounds envelope=resting;std::set<int> used;const auto collect=[&](auto&& self,const Json& j)->void{if(j.is_number_integer()){const int i=j;if(i>=0&&i<int(clips.size()))used.insert(i);}else if(j.is_object()||j.is_array())for(const auto& item:j)self(self,item);};collect(collect,catalog["cafe"]);collect(collect,catalog["battle"]);collect(collect,catalog["formation"]);collect(collect,catalog["manual"]);
 size_t done=0;for(int index:used){const int count=std::clamp(int(std::ceil(clips[index].duration*4)),2,16);for(int i=0;i<=count;i++)envelope.join(measure(index,clips[index].duration*i/count));progress(75+int(++done*20/std::max(size_t(1),used.size())),"计算动作范围 "+std::to_string(done)+" / "+std::to_string(used.size()));}
 const auto padding=Vector3Add(Vector3Scale(Vector3Subtract(envelope.high,envelope.low),0.12f),{0.12f,0.12f,0.12f});envelope.low=Vector3Subtract(envelope.low,padding);envelope.high=Vector3Add(envelope.high,padding);
 Json list=Json::array();for(size_t i=0;i<clips.size();i++)list.push_back({{"name",clips[i].name},{"duration",clips[i].duration},{"index",i}});
 metadata={{"type","loaded"},{"catalog",catalog},{"clips",list},{"skins",skins.size()},{"outlineMeshes",parts.size()},{"eyeHeight",eyeHeight},{"stopProfiles",profiles},{"spatial",{{"idle",resting.json()},{"envelope",envelope.json()},{"placement",placement},{"bodyWidth",bodyWidth},{"bodyWidthSource",source}}}};
 currentClip=-1;displayed=false;frozen.clear();visibility(idle);restore();sample(idle,0,0,false);updateGeometry();currentClip=-1;displayed=false;
}
}
