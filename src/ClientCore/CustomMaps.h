#pragma once
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <istream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Allegiance {
inline constexpr size_t MapFileCapacity=13; // Original c_cbFileName, including NUL.
struct CustomMapInfo {int teams{};std::string name,file;};
inline bool ValidMapFile(std::string_view file){
    return file.size()<MapFileCapacity && std::all_of(file.begin(),file.end(),[](unsigned char c){return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-';});
}
inline const CustomMapInfo* FindCustomMap(std::span<const CustomMapInfo> maps,std::string_view file){
    auto lower=[](unsigned char c){return c>='A' && c<='Z'?c+('a'-'A'):c;};
    for(const auto& map:maps)if(map.file.size()==file.size() && std::equal(map.file.begin(),map.file.end(),file.begin(),[&](auto a,auto b){return lower(a)==lower(b);}))return &map;
    return nullptr;
}
// FedSrv's maps.txt format: count, then teams,friendly name,IGC basename.
inline std::vector<CustomMapInfo> ReadCustomMaps(std::istream& input){
    auto trim=[](std::string_view s){auto first=s.find_first_not_of(" \t\r\n");return first==s.npos?std::string_view{}:s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);};
    auto number=[&](std::string_view s){s=trim(s);if(s.empty())throw std::runtime_error("Invalid maps.txt number");int value{};auto [end,error]=std::from_chars(s.data(),s.data()+s.size(),value);if(error!=std::errc{} || end!=s.data()+s.size())throw std::runtime_error("Invalid maps.txt number");return value;};
    std::string line;if(!std::getline(input,line))throw std::runtime_error("Missing maps.txt count");int count=number(line);
    if(count<0 || count>32767)throw std::runtime_error("Invalid maps.txt count");
    std::vector<CustomMapInfo> maps;maps.reserve(count);
    for(int i=0;i<count;++i){
        if(!std::getline(input,line))throw std::runtime_error("Incomplete maps.txt catalog");
        std::string_view row=line;auto first=row.find(','),last=row.find(',',first==row.npos?row.size():first+1);
        if(first==row.npos || last==row.npos)throw std::runtime_error("Invalid maps.txt row");
        CustomMapInfo map{number(row.substr(0,first)),std::string(trim(row.substr(first+1,last-first-1))),std::string(trim(row.substr(last+1)))};
        if(map.teams<2 || map.teams>6 || map.name.empty() || map.name.size()>24 || map.file.empty() || !ValidMapFile(map.file) || FindCustomMap(maps,map.file) ||
           std::any_of(map.name.begin(),map.name.end(),[](unsigned char c){return c<32 || c==127;}))throw std::runtime_error("Invalid maps.txt map metadata");
        maps.push_back(std::move(map));
    }
    while(std::getline(input,line))if(!trim(line).empty())throw std::runtime_error("Excess maps.txt rows");
    return maps;
}
}
