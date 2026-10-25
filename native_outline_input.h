// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <optional>
#include <string_view>
#include <cwctype>

inline std::optional<int> parseOutlineWidth(std::wstring_view text){
    const auto trim=[&]{while(!text.empty()&&std::iswspace(text.front()))text.remove_prefix(1);while(!text.empty()&&std::iswspace(text.back()))text.remove_suffix(1);};
    trim();if(!text.empty()&&text.back()==L'%'){text.remove_suffix(1);trim();}
    if(text.empty())return {};
    int value=0;
    for(wchar_t c:text){if(c<L'0'||c>L'9')return {};value=value*10+int(c-L'0');if(value>300)return {};}
    if(value<10)return {};return value;
}
