#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#include <behaviortree_cpp/contrib/json.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <cwchar>
#include <stdexcept>
#include <iostream>
#include <filesystem>
using Json=nlohmann::json;
#include "native_preferences.h"
#include "shared/directed_walk.h"
#include "shared/cafe_attention.h"
static void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(int argc,char** argv){
 try{
  const Vector2 cursor{};
  DirectedWalk walk;PetGround ground;float facing=0;walk.begin({0,0,10},true,1,0);
  walk.step(ground,2,.01,facing,[](float heading,double){return MovementChoice{heading,.25,true};});
  check(std::abs(ground.position.z-.005)<1e-6&&std::abs(walk.time-.01)<1e-9,"Avoidance reduces displacement without slowing the walking animation");
  walk.step(ground,2,.01,facing,[](float heading,double){return MovementChoice{heading,0,true};});
  check(std::abs(ground.position.z-.005)<1e-6&&std::abs(walk.time-.02)<1e-9,"Waiting for a peer keeps the walking animation at its normal rate");
  pet::Playback attentionState;attentionState.inputActive=true;attentionState.activeSecondsOverride=0;attentionState.lookTarget=true;attentionState.lookYaw=170;
  CafeAttention smoothAttention;facing=0;smoothAttention.update(attentionState,facing,.008,true,false);
  check(facing>240*.008&&facing<170,"Smooth cafe tracking has no fixed angular speed cap and does not snap");
  float rate60=0,rate125=0;CafeAttention track60,track125;
  for(int i=0;i<60;++i)track60.update(attentionState,rate60,1.0/60,true,false);
  for(int i=0;i<125;++i)track125.update(attentionState,rate125,.008,true,false);
  check(std::abs(rate60-rate125)<.001&&std::abs(rate60-170)<.001,"Smooth tracking depends on elapsed time, not update frequency");
  facing=179;attentionState.lookYaw=-179;smoothAttention.update(attentionState,facing,.008,true,false);
  check(facing>179&&facing<180,"Smooth tracking uses the short arc across the angle wrap");
  facing=0;attentionState.lookYaw=170;CafeAttention thresholdAttention;thresholdAttention.update(attentionState,facing,.008,true,true);
  check(thresholdAttention.turning&&std::abs(facing-240*.008)<1e-5,"Threshold tracking retains its turning-walk speed cap");
  attentionState.lookYaw=-90;const float beforeRetarget=facing;const auto turnSerial=thresholdAttention.serial;
  thresholdAttention.update(attentionState,facing,.008,true,true);
  check(thresholdAttention.targetYaw==-90&&facing<beforeRetarget&&thresholdAttention.serial==turnSerial,"An ongoing threshold turn follows the latest cursor without restarting its animation");
  CafeAttention waiting;facing=0;attentionState.lookYaw=29.9;waiting.update(attentionState,facing,.008,true,true);
  check(!waiting.turning&&facing==0,"The thirty-degree threshold still prevents unnecessary idle turns");
  attentionState.lookYaw=90;waiting.update(attentionState,facing,.008,false,false);check(facing==0&&!waiting.turning,"Interactions and movement retain control over facing");
  CafeAttention wrapTurn;facing=179;attentionState.lookYaw=-150;wrapTurn.update(attentionState,facing,.008,true,true);
  check(std::abs(std::remainder(facing-179,360.f)-240*.008)<1e-4,"Threshold turning also takes the short arc across the wrap");
  const float beforeInvalid=facing;attentionState.lookYaw=NAN;wrapTurn.update(attentionState,facing,.008,true,true);
  check(facing==beforeInvalid&&!wrapTurn.turning,"Invalid target samples cannot poison orientation or leave a stale turning animation");
  attentionState.lookYaw=90;wrapTurn.update(attentionState,facing,NAN,true,false);check(facing==beforeInvalid,"An invalid time sample preserves the current orientation");
  attentionState.inputActive=false;attentionState.activeInputSeconds=0;attentionState.activeSecondsOverride=5;attentionState.lookYaw=90;
  CafeAttention timed;facing=0;timed.update(attentionState,facing,.008,true,false);
  check(facing==0,"Movement-enabled profiles retain their input-episode activation policy");
  for(bool threshold:{false,true}){
   CafeAttention continuous;facing=0;continuous.update(attentionState,facing,.008,true,threshold,true,true);
   check(facing>0,"Stationary tracking starts without a continuous-input timer");
   for(int i=0;i<125;++i)continuous.update(attentionState,facing,.008,true,threshold,true,true);
   check(std::abs(facing-90)<.01,"Stationary tracking finishes its turn even while input is idle");
   ++attentionState.inputEpoch;attentionState.lookYaw=-90;
   for(int i=0;i<125;++i)continuous.update(attentionState,facing,.008,true,threshold,true,true);
   check(std::abs(facing+90)<.01,"A new input episode immediately retargets stationary tracking");
   const float paused=facing;attentionState.lookYaw=90;continuous.update(attentionState,facing,.008,false,threshold,true,true);
   check(facing==paused&&!continuous.turning,"Dragging, menus and action overrides still pause stationary tracking");
   continuous.update(attentionState,facing,.008,true,threshold,false,true);check(facing==paused&&!continuous.chosen,"The look toggle still disables continuous tracking");
  }
  const auto eye=ground.screen({ground.position.x,1.7f,ground.position.z});float cursorFacing=42;
  for(float scale:{.45f,1.f,1.65f}){ground.modelZoom=scale;check(!ground.faceCursor({eye.x+2.5f,eye.y},1.7f,cursorFacing)&&cursorFacing==42,"Small pointer motions at the head do not change facing across model scales");}
  check(ground.faceCursor({eye.x+100,eye.y},1.7f,cursorFacing)&&std::abs(cursorFacing-90)<.01,"A clear pointer direction still faces right");
  cursorFacing=42;check(!ground.faceCursor({NAN,eye.y},1.7f,cursorFacing)&&cursorFacing==42,"An invalid pointer sample leaves the last valid direction intact");
  ground.modelZoom=1;
  check(!DirectedWalk::outsideBand({30,0},cursor,10,1,5),"An idle character inside the band must remain still");
  check(!DirectedWalk::outsideBand({10,0},cursor,10,1,5),"The lower endpoint belongs to the band");
  check(!DirectedWalk::outsideBand({50,0},cursor,10,1,5),"The upper endpoint belongs to the band");
  check(DirectedWalk::outsideBand({60,0},cursor,10,1,5),"A distant character must start approaching");
  check(DirectedWalk::continueFollow({45,0},cursor,10,1,5,false),"Approach continues inside the band until its midpoint");
  check(!DirectedWalk::continueFollow({30,0},cursor,10,1,5,false),"Approach stops at the midpoint");
  check(!DirectedWalk::continueFollow({45,0},cursor,10,1,5,false,true),"An obstructed follower may stop anywhere inside the band");
  check(DirectedWalk::continueFollow({60,0},cursor,10,1,5,false,true),"An obstructed follower outside the band must retain its follow task");
  check(DirectedWalk::continueFollow({5,0},cursor,10,1,5,true,true),"A blocked retreat outside the near limit is not a satisfied follow distance");
  check(DirectedWalk::outsideBand({5,0},cursor,10,1,5),"A positive lower limit permits retreat");
  check(DirectedWalk::continueFollow({20,0},cursor,10,1,5,true),"Retreat continues to the midpoint");
  check(!DirectedWalk::continueFollow({30,0},cursor,10,1,5,true),"Retreat stops at the midpoint");
  for(float distance:{0.0f,2.0f,10.0f,25.0f,49.0f})check(!DirectedWalk::outsideBand({distance,0},cursor,10,0,5),"Zero nearest distance never requests retreat");
  check(DirectedWalk::continueFollow({40,0},cursor,10,0,5,false),"Zero-nearest approach still targets the midpoint");
  check(!DirectedWalk::continueFollow({20,0},cursor,10,0,5,false),"A cursor that moves closer stops an ongoing approach");
  check(!DirectedWalk::continueFollow({10,0},cursor,10,0,5,true),"Zero nearest must reject an old retreat route");
  check(!DirectedWalk::approachesCursor({0,0},{10,0},{-5,0}),"A cursor moving behind the destination must not be chased away from");
  check(DirectedWalk::approachesCursor({0,0},{10,0},{5,2}),"A forward destination approaches the cursor");
  const auto destination=DirectedWalk::followTarget({60,0},cursor,10,3);
  check(std::abs(destination.x-30)<1e-5&&std::abs(destination.y)<1e-5,"Destination distance is the midpoint");
  NativePreferences settings;auto value=settings.json();value["followNear"]=0;value["wandering"]=false;value["battleWalking"]=true;
  check(settings.apply(value),"Zero nearest distance and independent toggles must save");
  check(!settings.automaticWalk(false,false)&&!settings.automaticWalk(true,false),"Global wander off disables both modes' idle walks");
  check(settings.automaticWalk(true,true),"Battle short walks remain enabled independently");
  value["wandering"]=true;value["battleWalking"]=false;check(settings.apply(value),"The toggles can be reversed");
  check(settings.automaticWalk(false,false)&&settings.automaticWalk(true,false)&&!settings.automaticWalk(true,true),"Global wander does not enable battle input walks");
  check(argc==2,"Pass an isolated settings fixture path");const auto fixture=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(fixture.parent_path());const auto ini=fixture.wstring();
  WritePrivateProfileStringW(L"Behavior",L"Wandering",nullptr,ini.c_str());WritePrivateProfileStringW(L"Behavior",L"BattleWalking",nullptr,ini.c_str());
  WritePrivateProfileStringW(L"Behavior",L"Walking",L"0",ini.c_str());NativePreferences migrated;migrated.load(ini);check(!migrated.wandering&&!migrated.battleWalking,"An old disabled Walking flag migrates to both toggles");
  WritePrivateProfileStringW(L"Behavior",L"Walking",L"1",ini.c_str());migrated.load(ini);check(migrated.wandering&&migrated.battleWalking,"An old enabled Walking flag migrates to both toggles");
  settings.save(ini);NativePreferences loaded;loaded.load(ini);check(loaded.followNear==0&&loaded.wandering&&!loaded.battleWalking,"Zero nearest and independent toggles survive restart");
  std::cout<<"Cursor orientation, adaptive quiet region, live threshold retargeting, angle wrap, follow bands, independent movement flags and INI migration passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
