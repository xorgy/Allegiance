#pragma once
#include "CustomMaps.h"
#include <array>

namespace Allegiance {
inline constexpr unsigned CoreCatalogCapacity=32; // Original server/lobby core mask.
inline bool CoreFileEqual(std::string_view a,std::string_view b){
    auto lower=[](unsigned char c){return c>='A' && c<='Z'?c+('a'-'A'):c;};
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[&](auto x,auto y){return lower(x)==lower(y);});
}
inline const std::string* FindCore(std::span<const std::string> cores,std::string_view file){
    for(const auto& core:cores)if(CoreFileEqual(core,file))return &core;return nullptr;
}
// FedSrv cores.txt: count, then one c_cbFileName basename per line. Map files
// and historical cores installed alongside them are not implicitly advertised.
inline std::vector<std::string> ReadCoreCatalog(std::istream& input){
    auto trim=[](std::string_view s){auto first=s.find_first_not_of(" \t\r\n");return first==s.npos?std::string_view{}:s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);};
    std::string line;if(!std::getline(input,line))throw std::runtime_error("Missing cores.txt count");
    auto countText=trim(line);if(countText.empty())throw std::runtime_error("Missing cores.txt count");unsigned count{};auto [end,error]=std::from_chars(countText.data(),countText.data()+countText.size(),count);
    if(error!=std::errc{} || end!=countText.data()+countText.size() || count>CoreCatalogCapacity)throw std::runtime_error("Invalid cores.txt count");
    std::vector<std::string> cores;cores.reserve(count);
    for(unsigned i=0;i<count;++i){
        if(!std::getline(input,line))throw std::runtime_error("Incomplete cores.txt catalog");auto file=trim(line);
        if(file.empty() || !ValidMapFile(file) || FindCore(cores,file))throw std::runtime_error("Invalid cores.txt basename");
        cores.emplace_back(file);
    }
    while(std::getline(input,line))if(!trim(line).empty())throw std::runtime_error("Excess cores.txt rows");
    return cores;
}
}
