#pragma once
struct NativePreferences {
    bool follow=false,cafeTracking=true;
    std::string cafeLookMode="smooth";
    int outlineMode=2;
    double followNear=3,followFar=5,moveInterval=8,moveBodies=3,clickProbability=10,skillProbability=5,keyboardIdle=10,inputGrace=3,outlineWidth=100,walkTimeJitter=10,walkDistanceJitter=15;
    static constexpr const wchar_t* keys[]={L"FollowNear",L"FollowFar",L"MoveInterval",L"MoveBodies",L"ClickProbability",L"SkillProbability",L"KeyboardIdle",L"InputGrace",L"OutlineWidth",L"WalkTimeJitter",L"WalkDistanceJitter"};
    static constexpr const char* names[]={"followNear","followFar","moveInterval","moveBodies","clickProbability","skillProbability","keyboardIdle","inputGrace","outlineWidth","walkTimeJitter","walkDistanceJitter"};
    static constexpr double minimum[]={0.5,0.5,1,0.5,0,0,1,0.1,10,0,0},maximum[]={20,20,600,20,100,100,3600,10,300,50,50};
    std::vector<double*> values(){return {&followNear,&followFar,&moveInterval,&moveBodies,&clickProbability,&skillProbability,&keyboardIdle,&inputGrace,&outlineWidth,&walkTimeJitter,&walkDistanceJitter};}
    Json json() const {return {{"cafeTracking",cafeTracking},{"cafeLookMode",cafeLookMode},{"follow",follow},{"outlineMode",outlineMode},{"followNear",followNear},{"followFar",followFar},{"moveInterval",moveInterval},{"moveBodies",moveBodies},{"clickProbability",clickProbability},{"skillProbability",skillProbability},{"keyboardIdle",keyboardIdle},{"inputGrace",inputGrace},{"outlineWidth",outlineWidth},{"walkTimeJitter",walkTimeJitter},{"walkDistanceJitter",walkDistanceJitter}};}
    bool apply(const Json& j){
        auto v=values();for(size_t i=0;i<v.size();++i){if(i==7)continue;if(!j.contains(names[i])||!j[names[i]].is_number())return false;const double n=j[names[i]];if(!std::isfinite(n)||n<minimum[i]||n>maximum[i])return false;}
        const auto lookMode=j.value("cafeLookMode",cafeLookMode);if(lookMode!="smooth"&&lookMode!="threshold")return false;
        if(j.contains("cafeTracking")&&!j["cafeTracking"].is_boolean())return false;
        if(!j.contains("follow")||!j["follow"].is_boolean())return false;
        if(!j.contains("outlineMode")||!j["outlineMode"].is_number_integer()||j["outlineMode"].get<int>()<0||j["outlineMode"].get<int>()>2)return false;
        if(j["followNear"].get<double>()>j["followFar"].get<double>())return false;
        for(size_t i=0;i<v.size();++i)if(i!=7)*v[i]=j[names[i]];inputGrace=3;cafeTracking=j.value("cafeTracking",cafeTracking);cafeLookMode=lookMode;follow=j["follow"];outlineMode=j["outlineMode"];return true;
    }
    void load(const std::wstring& ini){
        wchar_t lookMode[32]{};GetPrivateProfileStringW(L"Three",L"CafeLookMode",L"smooth",lookMode,32,ini.c_str());cafeLookMode=std::wcscmp(lookMode,L"threshold")==0?"threshold":"smooth";
        cafeTracking=GetPrivateProfileIntW(L"Three",L"CafeTracking",GetPrivateProfileIntW(L"Behavior",L"Tracking",1,ini.c_str()),ini.c_str())!=0;
        follow=GetPrivateProfileIntW(L"Three",L"Follow",0,ini.c_str())!=0;
        outlineMode=GetPrivateProfileIntW(L"Three",L"OutlineMode",GetPrivateProfileIntW(L"Behavior",L"Outline",1,ini.c_str())?2:0,ini.c_str());if(outlineMode<0||outlineMode>2)outlineMode=2;
        // Migrate the previous target setting without changing other options.
        wchar_t legacy[64]{};GetPrivateProfileStringW(L"Three",L"FollowBodies",L"4",legacy,64,ini.c_str());
        const double middle=std::wcstod(legacy,nullptr);if(std::isfinite(middle)&&middle>=3&&middle<=5){followNear=std::max(0.5,middle-1);followFar=middle+1;}
        auto v=values();for(size_t i=0;i<v.size();++i){wchar_t text[64]{};GetPrivateProfileStringW(L"Three",keys[i],L"",text,64,ini.c_str());wchar_t* end=nullptr;const double n=std::wcstod(text,&end);if(end!=text&&!*end&&std::isfinite(n)&&n>=minimum[i]&&n<=maximum[i])*v[i]=n;}
        inputGrace=3;
        if(followNear>followFar){followNear=3;followFar=5;}
    }
    void save(const std::wstring& ini){
        WritePrivateProfileStringW(L"Three",L"CafeTracking",cafeTracking?L"1":L"0",ini.c_str());
        WritePrivateProfileStringW(L"Three",L"AttentionProbability",nullptr,ini.c_str());
        inputGrace=3;WritePrivateProfileStringW(L"Three",L"CafeLookMode",cafeLookMode=="threshold"?L"threshold":L"smooth",ini.c_str());
        WritePrivateProfileStringW(L"Three",L"Follow",follow?L"1":L"0",ini.c_str());auto v=values();
        WritePrivateProfileStringW(L"Three",L"OutlineMode",std::to_wstring(outlineMode).c_str(),ini.c_str());
        for(size_t i=0;i<v.size();++i)WritePrivateProfileStringW(L"Three",keys[i],std::to_wstring(*v[i]).c_str(),ini.c_str());
    }
};
