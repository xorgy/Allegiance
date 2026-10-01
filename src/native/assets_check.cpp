#include "assets.hpp"
#include "../ClientCore/ModelLod.h"
#include <cstdio>
#include <stdexcept>
#include <chrono>
#include <fstream>
namespace {
void coreNames(){
    auto root=std::filesystem::temp_directory_path()/("allegiance-core-names-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!std::filesystem::create_directory(root))throw std::runtime_error("Core-name fixture directory");
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{root};
    auto artwork=root/"Artwork";std::filesystem::create_directory(artwork);
    auto require=[](bool value){if(!value)throw std::runtime_error("Original core-name configuration semantics");};
    Native::Assets absent(artwork);require(absent.coreName("UnknownCore")=="UnknownCore");
    {std::ofstream out(root/"allegiance.txt");out<<"\xef\xbb\xbf[Cores]\r\n; comment\r\n PCore014 = \"Pook Core 14\" \r\nwc_04='WeedCore 04'\r\nLong=ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\nempty=\r\nPCORE014=ignored duplicate\r\n[Allegiance]\r\nUnknownCore=Not a core label\r\n[OfficialCores]\r\nPCore014=TrUe\r\nwc_04=false\r\nLong=1\r\nPCORE014=false\r\n[OfficialServers]\r\n Home = \"Example.LOCAL\"\r\nHOME=ignored duplicate\r\nLong=ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n";}
    Native::Assets configured(artwork);
    require(configured.coreName("pCORE014")=="Pook Core 14" && configured.coreName("WC_04")=="WeedCore 04");
    require(configured.officialCore("pCORE014") && !configured.officialCore("wc_04") && !configured.officialCore("Long") && !configured.officialCore("UnknownCore"));
    require(configured.officialServer("hOME","example.local") && !configured.officialServer("Home","another.local") && !configured.officialServer("Unknown","example.local"));
    require(configured.officialServer("long","ABCDEFGHIJKLMNOPQRSTUVWX") && !configured.officialServer("long","ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    require(configured.coreName("long")=="ABCDEFGHIJKLMNOPQRSTUVWX" && configured.coreName("Empty").empty());
    require(configured.coreName("UnknownCore")=="UnknownCore" && configured.coreName("ABCDEFGHIJKLMNOPQRSTUVWXYZ")=="ABCDEFGHIJKLMNOPQRSTUVWX");
    {std::ofstream out(artwork/"Allegiance.txt");out<<"[cOrEs]\nPCore014=Artwork-specific core\n";}
    Native::Assets alternate(artwork);
    require(alternate.coreName("PCore014")=="Artwork-specific core" && alternate.coreName("wc_04")=="wc_04");
    puts("Original core/server labels and flags: optional configuration, case-insensitive section/key/address lookup, BOM/CRLF, whitespace/quotes, first-key precedence, 24-byte limits, empty/fallback values and artwork override passed");
}
}
int main(int argc,char** argv) {
    try {
        coreNames();
        Native::Assets assets(argc>1 ? std::filesystem::path(argv[1]) : Native::Assets::steamArtwork());
        for(auto* name:{"smallFont","promptFont","titleFont","hugeFont"}) {
            auto font=assets.font(name);
            if(!font->height || !font->coverage || font->glyphs['A'].z<=0)
                throw std::runtime_error(std::string("Invalid original font: ")+name);
            printf("%s: %u px height, A advance %.0f\n",name,font->height,font->glyphs['A'].z);
        }
        for(int i=2;i<argc;++i) {
            auto model=assets.model(argv[i]);size_t vertices=0,triangles=0;
            for(auto& mesh:model->meshes) {vertices+=mesh->vertices.size();triangles+=mesh->indices.size()/3;}
            const std::string name=argv[i];
            if(name=="bgrnd03" || name=="bgrnd05" || name=="bgrnd51" || name=="bgrnd52" || name=="bgrnd53" || name=="bgrnd55" || name=="bgrnd56") {
                auto image=assets.image(name+"bmp");
                if(model->defaultImage!=image)throw std::runtime_error(name+": lost base texture inheritance");
                for(float radius:{80.f,64.f,48.f,32.f,16.f,8.f,4.f}) {
                    size_t selected=0;
                    for(auto& mesh:model->meshes)if(Allegiance::ModelLodVisible(radius,mesh->lodMin,mesh->lodMax)) {
                        if((mesh->image?mesh->image:model->defaultImage)!=image)throw std::runtime_error(name+": untextured or wrong-material LOD");
                        ++selected;
                    }
                    if(selected!=1)throw std::runtime_error(name+": missing or overlapping LOD");
                }
            }
            printf("%s: %zu meshes, %zu vertices, %zu triangles, radius %.2f\n",argv[i],model->meshes.size(),vertices,triangles,model->radius);
        }
    } catch(const std::exception& e) {fprintf(stderr,"%s\n",e.what());return 1;}
}
