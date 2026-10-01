#include "loadouts.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace Native {
namespace {
constexpr size_t maxHulls=4096,maxParts=128,maxBytes=16*1024*1024;
void validate(const Loadout& loadout) {
    if(loadout.hull<0 || loadout.parts.size()>maxParts)throw std::runtime_error("Invalid saved loadout");
    for(auto& part:loadout.parts)if(part.id<0 || part.mount < -5 || part.mount>=8)throw std::runtime_error("Invalid saved equipment slot");
}
}
void Loadouts::remember(size_t bank,Loadout loadout) {
    if(bank>=count)throw std::out_of_range("Loadout bank");validate(loadout);
    // WinTrek presets retain type/mount, never stack quantities.
    for(auto& part:loadout.parts)part.amount=32767;
    // SaveLoadout inserts before the first equal mount: reverse stable order.
    std::reverse(loadout.parts.begin(),loadout.parts.end());
    std::stable_sort(loadout.parts.begin(),loadout.parts.end(),[](auto& a,auto& b){return a.mount<b.mount;});
    auto& list=banks[bank];std::erase_if(list,[&](auto& item){return item.hull==loadout.hull;});
    if(list.size()>=maxHulls)list.pop_back();list.insert(list.begin(),std::move(loadout));
}
const Loadout* Loadouts::findExact(size_t bank,int hull)const {
    if(bank>=count)return nullptr;
    for(auto& item:banks[bank])if(item.hull==hull)return &item;
    return nullptr;
}
const Loadout* Loadouts::find(size_t bank,int hull)const {
    if(auto* exact=findExact(bank,hull))return exact;
    if(bank>=count)return nullptr;
    return banks[bank].empty()?nullptr:&banks[bank].front();
}
Loadouts Loadouts::load(const std::filesystem::path& path) {
    Loadouts result;if(!std::filesystem::exists(path))return result;
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    auto length=in.tellg();if(!in || length<0 || uint64_t(length)>maxBytes)throw std::runtime_error("Cannot read saved loadouts");
    in.seekg(0);size_t remaining=size_t(length);
    auto read=[&](size_t bytes) {if(bytes>remaining)throw std::runtime_error("Truncated saved loadouts");uint32_t value=0;
        for(size_t i=0;i<bytes;++i){int c=in.get();if(c<0)throw std::runtime_error("Cannot read saved loadouts");value|=uint32_t(c)<<(i*8);}remaining-=bytes;return value;};
    // Explicit little-endian Windows fields: int32 count, int16 ID, int8 mount.
    if(read(4)!=count)throw std::runtime_error("Unsupported saved loadout bank count");
    for(auto& bank:result.banks) {
        size_t hulls=read(4);if(hulls>maxHulls)throw std::runtime_error("Too many saved hulls");
        for(size_t h=0;h<hulls;++h) {
            Loadout item;item.hull=int16_t(read(2));size_t parts=read(4);
            if(parts>maxParts)throw std::runtime_error("Too many saved parts");
            for(size_t p=0;p<parts;++p){auto id=int16_t(read(2));auto mount=int8_t(read(1));item.parts.push_back({id,mount});}
            validate(item);
            if(std::any_of(bank.begin(),bank.end(),[&](auto& old){return old.hull==item.hull;}))throw std::runtime_error("Duplicate saved hull");
            bank.push_back(std::move(item));
        }
    }
    if(remaining)throw std::runtime_error("Unexpected data after saved loadouts");return result;
}
void Loadouts::save(const std::filesystem::path& path)const {
    for(auto& bank:banks){if(bank.size()>maxHulls)throw std::runtime_error("Too many saved hulls");for(auto& item:bank)validate(item);}
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    auto temporary=path;temporary+=".tmp";
    try {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        auto write=[&](uint32_t value,size_t bytes){for(size_t i=0;i<bytes;++i)out.put(char((value>>(i*8))&255));};
        write(count,4);for(auto& bank:banks){write(bank.size(),4);for(auto& item:bank){write(uint16_t(item.hull),2);write(item.parts.size(),4);for(auto& part:item.parts){write(uint16_t(part.id),2);write(uint8_t(part.mount),1);}}}
        out.close();if(!out)throw std::runtime_error("Cannot write saved loadouts");std::filesystem::rename(temporary,path);
    }catch(...){std::error_code error;std::filesystem::remove(temporary,error);throw;}
}
}
