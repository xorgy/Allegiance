#include "legacy.hpp"
#include "igc.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "test_reference/windows_core_layout.hpp"

namespace {
void compactClock() {
    static_assert(sizeof(Time)==4 && sizeof(CompactTime)==2 && sizeof(MissionID)==4);
    unsigned cases=0;
    for(uint32_t base:{0u,1u,1000u,0x7fffffffu,0x80000000u,0xfffffff0u,0xffffffffu})
    for(int offset=-32768;offset<=32767;++offset) {
        Time now(base-uint32_t(offset));CompactTime packed;packed.Set(Time(base),now);
        int16_t bytes;memcpy(&bytes,&packed,2);
        if(bytes!=offset)throw std::runtime_error("Windows compact time offset differs");
        Time restored;packed.Export(Time(base),&restored);
        if(restored!=now)throw std::runtime_error("Windows compact time wrap/delta differs");
        ++cases;
    }
    printf("%u compact timestamps, including negative deltas and 32-bit wrap, passed\n",cases);
}
std::vector<std::string> records(const std::vector<char>& data) {
    std::vector<std::string> records;size_t cursor=0;
    while(cursor<data.size()) {
        constexpr size_t header=sizeof(ObjectType)+sizeof(int32_t);
        if(data.size()-cursor<header)throw std::runtime_error("Truncated IGC record header");
        int32_t length;memcpy(&length,data.data()+cursor+sizeof(ObjectType),sizeof(length));
        if(length<0 || size_t(length)>data.size()-cursor-header)throw std::runtime_error("Truncated IGC record");
        records.emplace_back(data.data()+cursor,header+size_t(length));
        WindowsCoreLayout::normalize(records.back());cursor+=header+size_t(length);
    }
    std::sort(records.begin(),records.end());return records;
}
bool coreRoundTrip(const std::filesystem::path& artwork,const char* name,ImissionIGC* mission) {
    std::ifstream file(artwork/(std::string(name)+".igc"),std::ios::binary);
    int32_t version,size;file.read(reinterpret_cast<char*>(&version),4);file.read(reinterpret_cast<char*>(&size),4);
    if(!file || size<0 || size>64*1024*1024)throw std::runtime_error("Invalid installed static core header");
    std::vector<char> source(size);file.read(source.data(),size);if(!file)throw std::runtime_error("Truncated installed static core");
    std::vector<char> exported(mission->Export(c_maskStaticTypes,nullptr));
    if(mission->Export(c_maskStaticTypes,exported.data())!=int(exported.size()))throw std::runtime_error("Static core export length changed");
    auto original=records(source),native=records(exported);
    if(original!=native) {
        size_t differences=0;std::vector<std::string> missing,added;
        std::set_difference(original.begin(),original.end(),native.begin(),native.end(),std::back_inserter(missing));
        std::set_difference(native.begin(),native.end(),original.begin(),original.end(),std::back_inserter(added));
        for(auto& record:missing){ObjectType type;memcpy(&type,record.data(),sizeof(type));fprintf(stderr,"Windows record differs: type %d, %zu bytes\n",type,record.size());if(++differences>=8)break;}
        fprintf(stderr,"Static core round trip: %zu missing/changed, %zu added/changed records\n",missing.size(),added.size());return false;
    }
    printf("%zu Windows static-core records (%zu bytes): all defined payload bytes preserved; compiler padding excluded\n",native.size(),source.size());return true;
}
}

int main(int argc,char** argv) {
    if (argc < 2) { fprintf(stderr,"usage: %s ARTWORK [CORE]\n",argv[0]); return 2; }
    UTL::SetArtPath(argv[1]);
    TRef<IIgcSite> site = new IIgcSite;
    auto* mission = CreateMission();
    mission->Initialize(Time(0u),site);
    const char* core = argc > 2 ? argv[2] : "static_core";
    auto version = LoadIGCStaticCore(core,mission,false);
    printf("Core %s version %d: %d hulls, %d parts, %d civilizations\n",core,version,
        mission->GetHullTypes()->n(),mission->GetPartTypes()->n(),mission->GetCivilizations()->n());
    for (auto* link=mission->GetHullTypes()->first();link;link=link->next()) {
        auto* hull=link->data();
        printf("  %d %-25s model=%s length=%.1f mass=%.1f\n",hull->GetObjectID(),
            hull->GetName(),hull->GetModelName(),hull->GetLength(),hull->GetMass());
    }
    compactClock();
    size_t lookups=0;
    auto checkLookup=[&](const auto* list,auto getter){for(auto* l=list->first();l;l=l->next()){
        auto* object=l->data();if((mission->*getter)(object->GetObjectID())!=object)
            throw std::runtime_error("Static definition lookup differs from original list identity");++lookups;}};
    checkLookup(mission->GetHullTypes(),&ImissionIGC::GetHullType);checkLookup(mission->GetPartTypes(),&ImissionIGC::GetPartType);
    checkLookup(mission->GetExpendableTypes(),&ImissionIGC::GetExpendableType);checkLookup(mission->GetProjectileTypes(),&ImissionIGC::GetProjectileType);
    checkLookup(mission->GetCivilizations(),&ImissionIGC::GetCivilization);checkLookup(mission->GetStationTypes(),&ImissionIGC::GetStationType);
    checkLookup(mission->GetDroneTypes(),&ImissionIGC::GetDroneType);checkLookup(mission->GetDevelopments(),&ImissionIGC::GetDevelopment);
    checkLookup(mission->GetTreasureSets(),&ImissionIGC::GetTreasureSet);
    printf("%zu static definition lookups preserve original list identities\n",lookups);
    bool ok = version != 0 && mission->GetHullTypes()->n() > 0 && coreRoundTrip(argv[1],core,mission);
    // Installed .igc records are written using MSVC layout. In particular the
    // native C++ ABI must not reuse DataPartTypeIGC tail padding for packs.
    int packs = 0;
    for (auto* link = mission->GetPartTypes()->first(); link; link = link->next()) {
        auto* part = link->data();
        if (part->GetEquipmentType() != ET_Pack) continue;
        auto* data = static_cast<const DataPackTypeIGC*>(part->GetData());
        bool layout = sizeof(DataPartTypeIGC) == 396 && sizeof(DataPackTypeIGC) == 400 &&
            reinterpret_cast<const char*>(&data->packType) - reinterpret_cast<const char*>(data) == 396 &&
            reinterpret_cast<const char*>(&data->amount) - reinterpret_cast<const char*>(data) == 398;
        bool valid = part->Export(nullptr) == sizeof(DataPackTypeIGC) &&
            (data->packType == c_packAmmo || data->packType == c_packFuel) && data->amount > 0;
        if (!layout || !valid) { fprintf(stderr,"Invalid Windows pack record: %s\n",part->GetName()); ok = false; }
        ++packs;
    }
    printf("Validated %d Windows-layout ammo/fuel pack records\n",packs);
    ok = ok && packs > 0;
    mission->Terminate();
    delete mission;
    return ok ? 0 : 1;
}
