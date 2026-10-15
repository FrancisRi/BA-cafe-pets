#include "animation_keys.h"
#include <iostream>
#include <limits>
using namespace nativepet;
static void check(bool condition){if(!condition)throw std::runtime_error("Animation key regression failed");}
int main(){
    std::vector<float> times{0,1,1,1,2},values{0,10,11,12,20};
    check(collapseDuplicateKeys(times,values,1)==2);
    check(times==std::vector<float>({0,1,2})&&values==std::vector<float>({0,12,20}));
    times={0,1,1};values={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18};
    check(collapseDuplicateKeys(times,values,6)==1);
    check(values==std::vector<float>({1,2,3,4,5,6,13,14,15,16,17,18}));
    times={1,1,1};values={1,2,3};check(collapseDuplicateKeys(times,values,1)==2&&values[0]==3);
    times={0,1,std::nextafter(1.0f,2.0f)};values={1,2,3};
    check(collapseDuplicateKeys(times,values,1)==0&&values.size()==3);
    for(auto invalid:std::vector<std::vector<float>>{{0,2,1},{-1,0},{0,std::numeric_limits<float>::quiet_NaN()}}){
        values.resize(invalid.size());bool rejected=false;
        try{collapseDuplicateKeys(invalid,values,1);}catch(const std::runtime_error&){rejected=true;}check(rejected);
    }
    times={0,1};values={0};bool rejected=false;
    try{collapseDuplicateKeys(times,values,1);}catch(const std::runtime_error&){rejected=true;}check(rejected);
    std::cout<<"Animation key regression passed\n";
}
