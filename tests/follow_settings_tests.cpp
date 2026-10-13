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
static void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(int argc,char** argv){
 try{
  const Vector2 cursor{};
  check(!DirectedWalk::outsideBand({30,0},cursor,10,1,5),"An idle character inside the band must remain still");
  check(!DirectedWalk::outsideBand({10,0},cursor,10,1,5),"The lower endpoint belongs to the band");
  check(!DirectedWalk::outsideBand({50,0},cursor,10,1,5),"The upper endpoint belongs to the band");
  check(DirectedWalk::outsideBand({60,0},cursor,10,1,5),"A distant character must start approaching");
  check(DirectedWalk::continueFollow({45,0},cursor,10,1,5,false),"Approach continues inside the band until its midpoint");
  check(!DirectedWalk::continueFollow({30,0},cursor,10,1,5,false),"Approach stops at the midpoint");
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
  std::cout<<"Follow band, midpoint, zero-nearest, independent movement flags and INI migration passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
