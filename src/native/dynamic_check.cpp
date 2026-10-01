#include "test_fixture.hpp"
#include "test_reference/windows_dynamic_layout.hpp"
#include "test_reference/dynamic_import.hpp"
#include <fstream>
#include <map>
using NativeTest::require;
namespace {
using Key=std::pair<ObjectType,ObjectID>;
std::map<Key,std::string> records(std::span<const char> data,bool originalFile=false) {
    std::map<Key,std::string> result;
    while(!data.empty()) {
        constexpr size_t header=sizeof(ObjectType)+sizeof(int32_t);require(data.size()>=header,"map record header truncated");
        ObjectType type;int32_t size;memcpy(&type,data.data(),2);memcpy(&size,data.data()+2,4);
        require(size>=0 && size_t(size)<=data.size()-header,"map record payload truncated");
        std::string payload(data.data()+header,size);WindowsDynamicLayout::clean(type,payload.data(),size);
        if(originalFile)OriginalDynamicImport::normalize(type,payload.data());
        ObjectID id=NA;
        switch(type) {
        case OT_cluster:memcpy(&id,payload.data()+offsetof(DataClusterIGC,clusterID),2);break;
        case OT_warp:memcpy(&id,payload.data()+offsetof(DataWarpIGC,warpDef)+offsetof(WarpDef,warpID),2);break;
        case OT_asteroid:memcpy(&id,payload.data()+offsetof(DataAsteroidIGC,asteroidDef)+offsetof(AsteroidDef,asteroidID),2);break;
        case OT_station:memcpy(&id,payload.data()+offsetof(DataStationIGC,stationID),2);break;
        default:throw std::runtime_error("unexpected map type");
        }
        require(result.emplace(Key(type,id),std::move(payload)).second,"duplicate dynamic map identity");data=data.subspan(header+size);
    }
    return result;
}
void mapRoundTrip(const std::filesystem::path& art,const char* name) {
    NativeTest::Fixture world(art,false);std::ifstream in(art/(std::string(name)+".igc"),std::ios::binary);int32_t size=0;
    in.read(reinterpret_cast<char*>(&size),4);require(in && size>0 && size<1024*1024,"installed map header");std::vector<char> source(size);in.read(source.data(),size);require(bool(in),"installed map payload");
    require(LoadIGCFile(name,world.mission),"load original dynamic map");
    constexpr int64_t mask=(int64_t(1)<<OT_cluster)|(int64_t(1)<<OT_warp)|(int64_t(1)<<OT_asteroid)|(int64_t(1)<<OT_station);
    std::vector<char> exported(world.mission->Export(mask,nullptr));world.mission->Export(mask,exported.data());
    auto before=records(source,true),after=records(exported);require(before.size()==after.size(),"dynamic map object count changed on export");
    for(auto& [key,payload]:before) {
        auto it=after.find(key);require(it!=after.end(),"dynamic map object identity lost");
        if(it->second!=payload) {
            for(size_t offset=0;offset<payload.size();++offset)if(payload[offset]!=it->second[offset])fprintf(stderr,"Map %s type %d id %d byte %zu: %u -> %u\n",name,key.first,key.second,offset,uint8_t(payload[offset]),uint8_t(it->second[offset]));
            throw std::runtime_error("dynamic map defined payload differs on re-export");
        }
    }
    printf("Map %s: %zu original dynamic records preserve defined payload and identities after original import normalization; padding and C-string tails excluded\n",name,before.size());
}
void selectiveImport(const std::filesystem::path& art) {
    NativeTest::Fixture source(art,false),destination(art,false);
    // Destination already owns its core. Import clusters while skipping every
    // other map record, then import asteroids after those excluded clusters.
    require(LoadIGCFile("training_6",source.mission),"selective import source");
    constexpr int64_t mask=(int64_t(1)<<OT_cluster)|(int64_t(1)<<OT_warp)|(int64_t(1)<<OT_asteroid)|(int64_t(1)<<OT_station);
    std::vector<char> data(source.mission->Export(mask,nullptr));source.mission->Export(mask,data.data());
    destination.mission->Import(destination.mission->GetLastUpdate(),int64_t(1)<<OT_cluster,data.data(),int(data.size()));
    require(destination.mission->GetClusters()->n()==source.mission->GetClusters()->n() && destination.mission->GetAsteroids()->n()==0 && destination.mission->GetStations()->n()==0,"selective cluster import creates excluded types");
    destination.mission->Import(destination.mission->GetLastUpdate(),int64_t(1)<<OT_asteroid,data.data(),int(data.size()));
    require(destination.mission->GetAsteroids()->n()==source.mission->GetAsteroids()->n() && destination.mission->GetStations()->n()==0 && destination.mission->GetWarps()->n()==0,"excluded first record prevents subsequent asteroid import");
    puts("Selective IGC import skips preceding and interleaved excluded records and preserves requested object counts");
}
}
int main(int argc,char** argv) {
    try {
        auto art=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();
        printf("%d original Windows dynamic layouts and %d field offsets compile unchanged natively\n",WindowsDynamicLayout::recordCount,WindowsDynamicLayout::fieldCount);
        for(auto name:{"training_1","training_3","training_5","training_6","training_nan"})mapRoundTrip(art,name);
        selectiveImport(art);return 0;
    }catch(const std::exception& e){fprintf(stderr,"Dynamic records: %s\n",e.what());return 1;}
}
