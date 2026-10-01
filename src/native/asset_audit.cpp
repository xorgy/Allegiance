#include "assets.hpp"
#include "game.hpp"
#include "legacy.hpp"
#include "igc.h"
#include "mapmakerigc.h"
struct MapArtwork : CmapMakerIGC {static auto planets(){return std::span(smszPlanetName,c_NumberOfPlanetPosters);}};
#include <set>
#include <cstdio>

// Complete references from catalogued cores and both baseline cores and all four authored training
// maps and every original generated conquest topology/team count. Failed loads
// are reported individually, never silently skipped.
int main(int argc,char** argv) {
    try {
        Native::Assets assets(argc>1?argv[1]:Native::Assets::steamArtwork());UTL::SetArtPath(assets.root().string().c_str());
        std::set<std::string> models,images,cards;size_t failed=0,cardFallbacks=0;
        auto named=[](const char* name){return name && std::isalpha(static_cast<unsigned char>(*name));};
        auto model=[&](const char* name){if(named(name))models.insert(name);};
        auto image=[&](const char* name){if(named(name))images.insert(std::string(name)+"bmp");};
        auto skin=[&](const char* name){if(named(name))images.insert(std::string(name)+"bmp");};
        auto cores=assets.coreCatalog();for(auto core:{"static_core","PCore014"})if(!Allegiance::FindCore(cores,core))cores.emplace_back(core);
        for(const auto& core:cores) {
            TRef<IIgcSite> site=new IIgcSite;auto* mission=CreateMission();mission->Initialize(Time(0u),site);
            if(!LoadIGCStaticCore(core.c_str(),mission,false))throw std::runtime_error("Cannot read static core");
            auto buyables=[&](auto* list){for(auto* l=list->first();l;l=l->next()){model(l->data()->GetModelName());image(l->data()->GetIconName());}};
            buyables(mission->GetHullTypes());buyables(mission->GetPartTypes());buyables(mission->GetStationTypes());
            auto cardImages=[&](auto* list){for(auto* l=list->first();l;l=l->next()){
                auto name=std::string("i")+l->data()->GetModelName()+"bmp";
                if(assets.hasImage(name)){cards.insert(name);images.insert(name);}else ++cardFallbacks;
            }};
            cardImages(mission->GetHullTypes());cardImages(mission->GetPartTypes());cardImages(mission->GetStationTypes());cardImages(mission->GetDroneTypes());cardImages(mission->GetDevelopments());
            for(auto* l=mission->GetHullTypes()->first();l;l=l->next())skin(l->data()->GetTextureName());
            for(auto* l=mission->GetStationTypes()->first();l;l=l->next())skin(l->data()->GetTextureName());
            for(auto* l=mission->GetExpendableTypes()->first();l;l=l->next()){model(l->data()->GetModelName());skin(l->data()->GetModelTexture());}
            for(auto* l=mission->GetProjectileTypes()->first();l;l=l->next()){model(l->data()->GetModelName());skin(l->data()->GetModelTexture());}
            printf("CORE\t%s\t%d hulls\t%d parts\t%d stations\n",core.c_str(),mission->GetHullTypes()->n(),mission->GetPartTypes()->n(),mission->GetStationTypes()->n());
            mission->Terminate();delete mission;
        }
        auto loader=[&](const std::string& name,const std::string& texture) {
            if(name.starts_with("$poster:"))images.insert(name.substr(8));else if(!name.starts_with("$"))models.insert(name);
            if(!texture.empty())skin(texture.c_str());
            return Native::ModelHandle{0,1,{},{}};
        };
        for(int map=0;map<int(Native::mapFiles.size());++map) {
            Native::SessionOptions options;options.scenario=Native::Scenario::FreeFlight;options.map=map;options.enemies=0;
            Native::Game game(assets.root(),loader,0,0,options);
            printf("MAP\t%s\t%d sectors\n",Native::mapFiles[map],game.core()->GetClusters()->n());
        }
        for(int map=0;map<int(Native::conquestMapNames.size());++map)for(int teams=2;teams<=6;++teams) {
            if(!Native::Game::validMap(map,teams))continue;
            Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.map=map;options.rules.teams=teams;
            Native::Game game(assets.root(),loader,0,0,options);
            printf("MAP\t%s / %d teams\t%d sectors\n",Native::conquestMapNames[map],teams,game.core()->GetClusters()->n());
        }
        for(int i=1;i<=8;++i)models.insert("globe"+std::to_string(i));
        for(auto name:MapArtwork::planets())images.insert(name);
        for(auto image:{"f104bmp","f105bmp","f106bmp"})images.insert(image);
        printf("CARDS\t%zu distinct original model images\t%zu fallback references across catalogued cores\n",cards.size(),cardFallbacks);
        size_t meshes=0,triangles=0,lights=0,lodMeshes=0;
        for(auto& name:models)try {
            auto asset=assets.model(name);size_t count=0;for(auto& mesh:asset->meshes){count+=mesh->indices.size()/3;lodMeshes+=std::isfinite(mesh->lodMin) || std::isfinite(mesh->lodMax);}
            lights+=asset->lights.size();
            meshes+=asset->meshes.size();triangles+=count;
            printf("MODEL\t%s\t%zu meshes\t%zu triangles\t%zu frames\t%zu strobes\n",name.c_str(),asset->meshes.size(),count,asset->frames.size(),asset->lights.size());
        }catch(const std::exception& e){fprintf(stderr,"MODEL\t%s\t%s\n",name.c_str(),e.what());++failed;}
        for(auto& name:images)try{auto asset=assets.image(name);printf("IMAGE\t%s\t%u x %u\n",name.c_str(),asset->width,asset->height);}
        catch(const std::exception& e){fprintf(stderr,"IMAGE\t%s\t%s\n",name.c_str(),e.what());++failed;}
        for(auto& [name,count]:assets.functions())printf("FUNCTION\t%s\t%zu calls\n",name.c_str(),count);
        printf("TOTAL\t%zu models\t%zu images\t%zu meshes\t%zu triangles\t%zu strobes\t%zu LOD meshes\t%zu failures\n",models.size(),images.size(),meshes,triangles,lights,lodMeshes,failed);
        return failed?1:0;
    }catch(const std::exception& e){fprintf(stderr,"Asset audit: %s\n",e.what());return 1;}
}
