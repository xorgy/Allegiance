#include "assets.hpp"
#include "../ClientCore/CoreCatalog.h"
#include "system_fonts.hpp"
#include "primitives.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <bit>
#include <cctype>
#include <cstring>
#include <fstream>
#include <regex>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Native {
namespace {
std::string lower(std::string s) {
    for (char& c:s) c=char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::vector<uint8_t> readFile(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary|std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open asset: "+p.string());
    auto size=f.tellg();
    if (size < 0 || size > 512*1024*1024) throw std::runtime_error("Invalid asset size: "+p.string());
    std::vector<uint8_t> data(static_cast<size_t>(size)); f.seekg(0);
    if (!f.read(reinterpret_cast<char*>(data.data()),size)) throw std::runtime_error("Cannot read asset: "+p.string());
    return data;
}
}
struct Assets::Value {
    float number{};
    std::string text, library;
    std::vector<ValuePtr> list;
    std::shared_ptr<ModelData> model;
    std::shared_ptr<ImageData> image;
    std::shared_ptr<BitmapFont> font;
    std::vector<std::shared_ptr<ImageData>> animation;
    // Only binary frame images need their pre-conversion pixel representation
    // to apply the authored XOR deltas. Ordinary geometry keeps no second copy.
    std::vector<uint8_t> surface;
    uint32_t pitch{},bits{},masks[4]{};bool key{};
    std::vector<ModelFrame> frames;
    glm::mat4 matrix{1};
};
struct Assets::Reader {
    Assets& assets;
    std::string name;
    std::vector<uint8_t> data;
    size_t pos{};
    std::vector<std::string> libraries, exports;
    std::vector<ValuePtr> imports, objects;
    explicit Reader(Assets& a,const std::string& n) : assets(a),name(n),data(readFile(a.find(n+".mdl"))) {}
    [[noreturn]] void fail(const std::string& reason) const {
        throw std::runtime_error(name+".mdl @"+std::to_string(pos)+": "+reason);
    }
    void bytes(void* out,size_t n) {
        if (pos > data.size() || n > data.size()-pos) fail("truncated data");
        if (out) std::memcpy(out,data.data()+pos,n); pos+=n;
    }
    template<class T> T read() { T v; bytes(&v,sizeof(v)); return v; }
    uint32_t count() { auto n=read<uint32_t>(); if(n>4'000'000) fail("invalid count");return n; }
    std::string string() {
        auto start=pos;
        while (pos < data.size() && data[pos]) ++pos;
        if (pos==data.size()) fail("unterminated string");
        std::string s(reinterpret_cast<const char*>(data.data()+start),pos-start);
        pos=start; bytes(nullptr,(s.size()+4)&~size_t(3)); return s;
    }
    ValuePtr pop(std::vector<ValuePtr>& stack) {
        if (stack.empty()) fail("stack underflow");auto v=stack.back();stack.pop_back();return v;
    }
    std::shared_ptr<ModelData> geometry(ValuePtr v) {
        if (!v->model && !v->library.empty()) {
            const auto& ns=assets.load(v->library);
            auto it=ns.find(v->text); if(it==ns.end()) fail("missing imported geometry "+v->text);
            v=it->second;
        }
        if (!v->model) fail("expected geometry");return v->model;
    }
    std::shared_ptr<ImageData> texture(ValuePtr v) {
        if (!v->image && !v->library.empty()) {
            const auto& ns=assets.load(v->library);
            auto it=ns.find(v->text); if(it==ns.end()) fail("missing imported image "+v->text);
            v=it->second;
        }
        if (!v->image) fail("expected image");return v->image;
    }
    ValuePtr apply(std::vector<ValuePtr>& stack,bool binary) {
        auto fn=pop(stack); auto v=std::make_shared<Value>();
        const auto& f=fn->text;++assets.functions_[f+(binary?" binary":" expression")];
        if(f=="ImportFont" && binary) {
            auto font=std::make_shared<BitmapFont>();font->height=count();auto width=count(),size=count();
            if(!font->height || font->height>128 || !width || width>256 || size>16*1024*1024)fail("invalid bitmap font");
            struct Character {int32_t width,height,offset;};std::array<Character,256> characters;
            bytes(characters.data(),sizeof(characters));std::vector<uint8_t> bits(size);bytes(bits.data(),size);
            auto atlas=std::make_shared<ImageData>();atlas->name=name+" bitmap font";atlas->format=ImageFormat::Coverage;atlas->mipmaps=false;
            atlas->width=(width+2)*16;atlas->height=(font->height+2)*16;atlas->pixels.resize(atlas->byteSize());
            for(unsigned c=0;c<256;++c) {
                auto g=characters[c];if(g.width<0 || g.height<0 || g.width>int(width) || g.height>int(font->height) || g.offset<0)fail("invalid font character");
                size_t stride=(g.width+7)/8,offset=g.offset;
                if(offset>bits.size() || stride*size_t(g.height)>bits.size()-offset)fail("truncated font character");
                unsigned x=(c%16)*(width+2)+1,y=(c/16)*(font->height+2)+1;
                font->glyphs[c]={x,y,g.width,g.height};
                for(int j=0;j<g.height;++j)for(int i=0;i<g.width;++i)
                    atlas->pixels[(y+j)*atlas->width+x+i]=(bits[offset+j*stride+i/8]&(1u<<(i%8)))?255:0;
            }
            font->coverage=std::move(atlas);v->font=std::move(font);
        } else if (f=="MeshGeo" && binary) {
            auto mesh=std::make_shared<MeshData>();
            auto nv=count(),ni=count();mesh->vertices.resize(nv);mesh->indices.resize(ni);
            // Legacy Vertex stores UV before the normal (context.h).
            for(auto& vertex:mesh->vertices) {
                float raw[8];bytes(raw,sizeof(raw));
                vertex.position={raw[0],raw[1],raw[2]};
                vertex.uv={raw[3],raw[4]};vertex.normal={raw[5],raw[6],raw[7]};
            }
            for (auto& i:mesh->indices) {i=read<uint16_t>();if(i>=nv) fail("vertex index out of range");}
            v->model=std::make_shared<ModelData>();v->model->meshes.push_back(mesh);
        } else if (f=="ImportImage" && binary) {
            auto im=std::make_shared<ImageData>();im->name=name;
            im->width=count();im->height=count();auto pitch=count(),bits=count();
            uint32_t masks[4];bytes(masks,sizeof(masks));bool key=read<uint32_t>()&255;
            if (im->width==0 || im->height==0 || im->width>16384 || im->height>16384 ||
                (bits!=16 && bits!=24 && bits!=32) || pitch<im->width*(bits/8)) fail("unsupported surface");
            size_t start=pos;bytes(nullptr,size_t(pitch)*im->height);
            if(name.starts_with("anim")){v->surface.assign(data.begin()+start,data.begin()+pos);v->pitch=pitch;v->bits=bits;std::copy_n(masks,4,v->masks);v->key=key;}
            im->pixels.resize(size_t(im->width)*im->height*4);
            for (uint32_t y=0;y<im->height;++y) for(uint32_t x=0;x<im->width;++x) {
                uint32_t pixel=0;std::memcpy(&pixel,data.data()+start+y*pitch+x*(bits/8),bits/8);
                auto* out=im->pixels.data()+(size_t(y)*im->width+x)*4;
                for (int c=0;c<4;++c) {
                    if (!masks[c]) out[c]=c==3 ? 255 : 0;
                    else {auto shift=std::countr_zero(masks[c]);uint32_t mask=masks[c]>>shift;
                        out[c]=uint8_t(((pixel&masks[c])>>shift)*255/mask);}
                }
                if(key && !(pixel&(masks[0]|masks[1]|masks[2]))) out[3]=0;
            }
            v->image=im;
        } else if(f=="FrameImage" && binary) {
            pop(stack);auto base=pop(stack);if(!base->image || base->surface.empty())fail("frame image has no raw background");
            auto n=count();if(n<2 || n>1024)fail("invalid animation offset table");std::vector<uint32_t> offsets(n);bytes(offsets.data(),n*4);
            auto size=offsets.back();if(size>128*1024*1024)fail("invalid animation size");auto start=pos;bytes(nullptr,size);
            auto raw=base->surface;
            // The final offset is a sentinel, not an additional frame.
            for(unsigned frame=0;frame+1<n;++frame) {
                if(offsets[frame]>offsets[frame+1] || offsets[frame+1]>size)fail("invalid animation offset");
                size_t at=start+offsets[frame],end=start+offsets[frame+1],dest=0;
                while(at<end) {
                    if(end-at<2)fail("truncated animation opcode");uint16_t word;std::memcpy(&word,data.data()+at,2);at+=2;
                    size_t length=word&0x3fff,mode=word>>14,unit=mode?size_t(1)<<(mode-1):1,bytesToWrite=length*unit;
                    size_t sourceBytes=mode?unit:bytesToWrite;
                    if(bytesToWrite>raw.size()-dest || sourceBytes>end-at)fail("animation delta outside surface");
                    for(size_t i=0;i<bytesToWrite;++i)raw[dest+i]^=data[at+(mode?i%unit:i)];dest+=bytesToWrite;at+=sourceBytes;
                }
                auto im=std::make_shared<ImageData>(*base->image);im->name=name+" frame "+std::to_string(frame);im->mipmaps=false;
                for(uint32_t y=0;y<im->height;++y)for(uint32_t x=0;x<im->width;++x) {
                    uint32_t pixel=0;std::memcpy(&pixel,raw.data()+y*base->pitch+x*(base->bits/8),base->bits/8);
                    auto* out=im->pixels.data()+(size_t(y)*im->width+x)*4;
                    for(int c=0;c<4;++c){auto mask=base->masks[c];if(!mask)out[c]=c==3?255:0;else {auto shift=std::countr_zero(mask);out[c]=uint8_t(((pixel&mask)>>shift)*255/(mask>>shift));}}
                    if(base->key && !(pixel&(base->masks[0]|base->masks[1]|base->masks[2])))out[3]=0;
                }
                v->animation.push_back(im);
            }
            v->image=v->animation.front();base->surface.clear();
        } else if (f=="FrameData" && binary) {
            auto n=count();v->frames.reserve(n);for(uint32_t i=0;i<n;++i) {ModelFrame frame;frame.name=string();bytes(&frame.position,12);bytes(&frame.forward,12);bytes(&frame.up,12);v->frames.push_back(std::move(frame));}
        } else if (f=="LightsGeo" && binary) {
            auto n=count();v->model=std::make_shared<ModelData>();v->model->lights.resize(n);
            for(auto& light:v->model->lights){bytes(&light,sizeof(light));
                if(!std::isfinite(light.period) || light.period<=0 || light.rampUp<0 || light.hold<0 || light.rampDown<0)fail("invalid strobe envelope");}
        } else if (f=="ModifiableNumber" && !binary) {
            v=pop(stack);
        } else if (f=="TextureGeo" && !binary) {
            auto geo=geometry(pop(stack));auto im=texture(pop(stack));
            v->model=std::make_shared<ModelData>();v->model->lights=geo->lights;
            v->model->defaultImage=im;
            for(auto& m:geo->meshes) {auto mesh=std::make_shared<MeshData>(*m);mesh->image=im;v->model->meshes.push_back(mesh);}
        } else if (f=="GroupGeo" && !binary) {
            auto list=pop(stack);v->model=std::make_shared<ModelData>();
            for(auto& item:list->list) {auto geo=geometry(item);v->model->meshes.insert(v->model->meshes.end(),geo->meshes.begin(),geo->meshes.end());v->model->lights.insert(v->model->lights.end(),geo->lights.begin(),geo->lights.end());
                if(!v->model->defaultImage)v->model->defaultImage=geo->defaultImage;}
        } else if (f=="LODGeo" && !binary) {
            auto base=geometry(pop(stack));auto list=pop(stack);
            std::vector<std::pair<float,std::shared_ptr<ModelData>>> levels;
            for(auto& pair:list->list){if(pair->list.size()!=2)fail("invalid LOD pair");
                float threshold=pair->list[0]->number;if(!std::isfinite(threshold) || threshold<0)fail("invalid LOD radius");
                levels.emplace_back(threshold,geometry(pair->list[1]));}
            std::stable_sort(levels.begin(),levels.end(),[](auto& a,auto& b){return a.first>b.first;});
            v->model=std::make_shared<ModelData>();
            // LODGeo inherits WrapGeo::FindTexture from its base geometry,
            // independently of which projected-radius level will be drawn.
            v->model->defaultImage=base->defaultImage;
            for(size_t i=0;i<=levels.size();++i){
                auto& geo=i==0?base:levels[i-1].second;
                float lower=i<levels.size()?levels[i].first:-std::numeric_limits<float>::infinity();
                float upper=i>0?levels[i-1].first:std::numeric_limits<float>::infinity();
                for(auto& m:geo->meshes){auto mesh=std::make_shared<MeshData>(*m);
                    mesh->lodMin=std::max(lower,mesh->lodMin);mesh->lodMax=std::min(upper,mesh->lodMax);
                    if(mesh->lodMin<mesh->lodMax)v->model->meshes.push_back(std::move(mesh));}
            }
        } else if (f=="Matrix" && binary) {
            float m[16];bytes(m,64);read<uint32_t>();
            for(int r=0;r<4;++r) for(int c=0;c<4;++c) v->matrix[c][r]=m[r*4+c];
        } else if (f=="TransformGeo" && !binary) {
            auto geo=geometry(pop(stack));auto transform=pop(stack)->matrix;
            v->model=std::make_shared<ModelData>();v->model->lights=geo->lights;
            v->model->defaultImage=geo->defaultImage;
            for(auto& light:v->model->lights)light.position=glm::vec3(transform*glm::vec4(light.position,1));
            for(auto& m:geo->meshes) {
                auto mesh=std::make_shared<MeshData>(*m);
                for(auto& vert:mesh->vertices) {vert.position=glm::vec3(transform*glm::vec4(vert.position,1));vert.normal=glm::normalize(glm::transpose(glm::inverse(glm::mat3(transform)))*vert.normal);}
                v->model->meshes.push_back(mesh);
            }
        } else fail("unsupported "+std::string(binary ? "binary " : "")+"function "+f);
        return v;
    }
    ValuePtr object(unsigned depth=0) {
        if(depth>64) fail("excessive nesting");
        std::vector<ValuePtr> stack;
        while(true) {
            auto token=read<uint32_t>();auto v=std::make_shared<Value>();
            switch(token) {
                case 0: if(stack.size()!=1) fail("unbalanced expression");return stack.back();
                case 1: v->number=read<float>();break;
                case 2: v->text=string();break;
                case 3: v->number=1;break;
                case 4: break;
                case 5: {auto n=count();for(uint32_t i=0;i<n;++i) v->list.push_back(object(depth+1));break;}
                case 6: v=apply(stack,false);break;
                case 7: v=apply(stack,true);break;
                case 8: {auto i=count();if(i>=objects.size() || !objects[i]) fail("bad reference");v=objects[i];break;}
                case 9: {auto i=count();if(i>=imports.size()) fail("bad import");v=imports[i];break;}
                case 10: v->list.push_back(pop(stack));v->list.push_back(pop(stack));break;
                default: fail("unknown token "+std::to_string(token));
            }
            stack.push_back(v);
            if(stack.size()>65536) fail("expression too large");
        }
    }
    NameSpace parse() {
        if(read<uint32_t>()!=0xdebadf00 || read<uint32_t>()!=0x10000) fail("unsupported MDL header");
        auto nn=count(),ni=count(),ne=count(),nd=count();
        for(uint32_t i=0;i<nn;++i) libraries.push_back(string());
        for(uint32_t i=0;i<ni;++i) {
            auto lib=count();if(lib>=libraries.size()) fail("bad namespace");
            auto v=std::make_shared<Value>();v->library=libraries[lib];v->text=string();imports.push_back(v);
            if(v->text=="emptyGeo") v->model=std::make_shared<ModelData>();
        }
        for(uint32_t i=0;i<ne;++i) exports.push_back(string());
        objects.resize(size_t(ne)+nd);
        for(size_t i=0;i<objects.size();++i) {auto id=count();if(id>=objects.size()) fail("bad definition");objects[id]=object();}
        NameSpace ns;for(uint32_t i=0;i<ne;++i) ns.emplace(exports[i],objects[i]);return ns;
    }
};
Assets::Assets(std::filesystem::path root) : root_(std::move(root)) {
    if(!std::filesystem::is_directory(root_)) throw std::runtime_error("Artwork directory not found: "+root_.string());
    for(auto& file:std::filesystem::recursive_directory_iterator(root_))
        if(file.is_regular_file()) files_.emplace(lower(file.path().lexically_relative(root_).generic_string()),file.path());
    auto config=files_.find("allegiance.txt");
    std::ifstream names(config!=files_.end()?config->second:root_.parent_path()/"allegiance.txt");
    auto trim=[](std::string_view s){
        const auto first=s.find_first_not_of(" \t\r\n"),last=s.find_last_not_of(" \t\r\n");
        return first==s.npos?std::string_view{}:s.substr(first,last-first+1);
    };
    std::string section,line;
    while(std::getline(names,line)){
        auto s=trim(line);if(s.starts_with("\xef\xbb\xbf"))s.remove_prefix(3);
        if(s.empty() || s.front()==';')continue;
        if(s.front()=='['){section=lower(std::string(s));continue;}
        if(section!="[cores]" && section!="[officialcores]" && section!="[officialservers]")continue;
        auto split=s.find('=');if(split==s.npos)continue;
        auto key=trim(s.substr(0,split)),value=trim(s.substr(split+1));if(key.empty())continue;
        if(value.size()>=2 && ((value.front()=='"' && value.back()=='"') || (value.front()=='\'' && value.back()=='\'')))value=value.substr(1,value.size()-2);
        // CfgGetCoreName uses a c_cbName (25-byte) output buffer.
        if(section=="[cores]")coreNames_.try_emplace(lower(std::string(key)),value.substr(0,24));
        else if(section=="[officialcores]")officialCores_.try_emplace(lower(std::string(key)),lower(std::string(value.substr(0,24)))=="true");
        else officialServers_.try_emplace(lower(std::string(key)),lower(std::string(value.substr(0,24))));
    }
}
bool Assets::officialCore(std::string_view file)const {
    auto found=officialCores_.find(lower(std::string(file)));return found!=officialCores_.end() && found->second;
}
bool Assets::officialServer(std::string_view name,std::string_view address)const {
    auto found=officialServers_.find(lower(std::string(name)));
    return found!=officialServers_.end() && found->second==lower(std::string(address));
}
std::vector<std::string> Assets::coreCatalog()const {
    auto file=files_.find("cores.txt");
    if(file==files_.end())return files_.contains("static_core.igc")?std::vector<std::string>{"static_core"}:std::vector<std::string>{};
    std::ifstream input(file->second);if(!input)throw std::runtime_error("Cannot read cores.txt");
    auto cores=Allegiance::ReadCoreCatalog(input);
    for(const auto& core:cores)if(!files_.contains(lower(core)+".igc"))throw std::runtime_error("Catalogued core is missing: "+core);
    return cores;
}
std::string Assets::coreName(std::string_view file)const {
    auto found=coreNames_.find(lower(std::string(file)));
    return found!=coreNames_.end()?found->second:std::string(file.substr(0,24));
}
Assets::~Assets()=default;
std::span<const std::shared_ptr<ImageData>> Assets::animation(const std::string& name) {
    const auto& ns=load(name);auto i=ns.find("image");if(i==ns.end() || i->second->animation.empty())throw std::runtime_error("No animation in "+name);return i->second->animation;
}
SystemFonts& Assets::systemFonts(){if(!systemFonts_)systemFonts_=std::make_unique<SystemFonts>();return *systemFonts_;}
std::shared_ptr<const BitmapFont> Assets::font(const std::string& symbol,int height,bool bold) {
    if(height)return systemFonts().font(symbol,height,bold);
    const auto& ns=load("font");auto it=ns.find(symbol);
    if(it==ns.end())throw std::runtime_error("Original font not found: "+symbol);
    auto v=it->second;
    for(unsigned i=0;i<16 && !v->font && !v->library.empty();++i) {
        const auto& imported=load(v->library);auto found=imported.find(v->text);
        if(found==imported.end())break;v=found->second;
    }
    if(!v->font)throw std::runtime_error("Not a bitmap font: "+symbol);
    return v->font;
}
std::filesystem::path Assets::find(const std::string& name) const {
    auto it=files_.find(lower(name));if(it==files_.end()) throw std::runtime_error("Asset not found: "+name);return it->second;
}
bool Assets::hasImage(const std::string& name)const {
    // Match image()'s filename-versus-namespace resolution. A namespace name
    // is not itself a filename; image assets generally live in name.mdl.
    if(auto separator=name.find('#');separator!=std::string::npos)return files_.contains(lower(name.substr(0,separator)+".mdl"));
    return files_.contains(lower(name.find('.')==std::string::npos?name+".mdl":name));
}
std::filesystem::path Assets::steamArtwork() {
#ifdef _WIN32
    wchar_t executable[32768];auto length=GetModuleFileNameW(nullptr,executable,32768);
    const auto directory=std::filesystem::path(std::wstring(executable,length)).parent_path();
    for(auto candidate:{directory/"Artwork",directory.parent_path()/"Artwork"})
        if(std::filesystem::is_directory(candidate))return candidate;
    wchar_t steamPath[32768]{};DWORD size=sizeof(steamPath);
    std::filesystem::path steam;
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",RRF_RT_REG_SZ,nullptr,steamPath,&size)==ERROR_SUCCESS)
        steam=steamPath;
    else if(auto* programFiles=std::getenv("ProgramFiles(x86)"))steam=std::filesystem::path(programFiles)/"Steam";
    else steam="C:/Program Files (x86)/Steam";
    auto artwork=[](const std::filesystem::path& library){return library/"steamapps/common/Allegiance/Artwork";};
    if(std::filesystem::is_directory(artwork(steam)))return artwork(steam);
    std::ifstream input(steam/"steamapps/libraryfolders.vdf");
    std::string text((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    const std::regex paths(R"vdf("path"\s*"([^"]+)")vdf");
    for(auto i=std::sregex_iterator(text.begin(),text.end(),paths);i!=std::sregex_iterator();++i) {
        auto path=(*i)[1].str();for(size_t at=0;(at=path.find("\\\\",at))!=std::string::npos;)path.replace(at,2,"/");
        auto candidate=artwork(std::filesystem::u8path(path));if(std::filesystem::is_directory(candidate))return candidate;
    }
    return artwork(steam);
#else
    const char* home=getenv("HOME");if(!home) throw std::runtime_error("Specify --artwork (HOME is unset)");
#ifdef __APPLE__
    return std::filesystem::path(home)/"Library/Application Support/Steam/steamapps/common/Allegiance/Artwork";
#else
    return std::filesystem::path(home)/".local/share/Steam/steamapps/common/Allegiance/Artwork";
#endif
#endif
}
const Assets::NameSpace& Assets::load(const std::string& name) {
    auto key=lower(name);auto it=namespaces_.find(key);if(it!=namespaces_.end())return it->second;
    Reader reader(*this,name);NameSpace ns;
    if(reader.data.size()>4 && reader.data[0]==0 && reader.data[1]==0xdf) ns=reader.parse();
    else {
        std::string text(reader.data.begin(),reader.data.end());std::smatch match;
        std::regex pattern(R"mdl((\w+)\s*=\s*ImportImageFromFile\s*\(\s*"([^"]+)")mdl");
        if(!std::regex_search(text,match,pattern)) throw std::runtime_error("Unsupported text MDL: "+name);
        auto v=std::make_shared<Value>();v->image=decodeImage(find(match[2].str()));ns.emplace(match[1].str(),v);
    }
    return namespaces_.emplace(key,std::move(ns)).first->second;
}
std::shared_ptr<ModelData> Assets::model(const std::string& name) {
    auto key=lower(name);auto it=models_.find(key);if(it!=models_.end())return it->second;
    if(key.size()==7 && key.starts_with("$aleph") && key.back()>='0' && key.back()<='3') {
        auto model=makeWarp(key.back()-'0');models_.emplace(key,model);return model;
    }
    if(key.starts_with("$poster:")) {
        // Sector posters use Modeler::LoadImage(textureName, true).
        auto mesh=std::make_shared<MeshData>();mesh->image=image(name.substr(8),true);
        mesh->vertices={{{-1,1,0},{0,0,1},{0,0}},{{-1,-1,0},{0,0,1},{0,1}},{{1,-1,0},{0,0,1},{1,1}},{{1,1,0},{0,0,1},{1,0}}};
        mesh->indices={0,2,1,0,3,2}; // Two-sided pipeline needs each triangle only once.
        auto model=std::make_shared<ModelData>();model->radius=std::sqrt(2.f);model->meshes.push_back(mesh);
        models_.emplace(key,model);return model;
    }
    auto separator=name.find('#');auto file=name.substr(0,separator);auto symbol=separator==std::string::npos?"object":name.substr(separator+1);
    const auto& ns=load(file);auto v=ns.find(symbol);
    if(v==ns.end() || !v->second->model) throw std::runtime_error("No model export "+symbol+" in "+file);
    // MDL exports/imports can alias the same geometry value. Namespace frame
    // and light metadata belongs to this resolved model, not to that shared
    // value: mutating it can invalidate another cached model's borrowed spans.
    // Meshes/textures remain shared, including the renderer's geometry cache.
    auto model=std::make_shared<ModelData>(*v->second->model);model->radius=0;
    if(auto f=ns.find("frames");f!=ns.end())model->frames=f->second->frames;
    if(auto lights=ns.find("lights");lights!=ns.end() && lights->second->model)model->lights=lights->second->model->lights;
    for(auto& mesh:model->meshes) for(auto& vertex:mesh->vertices) model->radius=std::max(model->radius,glm::length(vertex.position));
    models_.emplace(key,model);return model;
}
std::shared_ptr<ImageData> Assets::image(const std::string& name,bool colorKey) {
    if(colorKey) {
        auto key=lower(name);auto it=keyedImages_.find(key);if(it!=keyedImages_.end())return it->second;
        auto source=image(name);auto result=source;
        // Some radar MDLs leave their embedded key flag unset. The original
        // caller's hint still keys them. Convert once, before filtering/mips,
        // and preserve authored coverage and every nonblack texel.
        for(size_t i=0;i<source->pixels.size();i+=4) {
            const auto* pixel=source->pixels.data()+i;
            if(!(pixel[0]|pixel[1]|pixel[2]) && pixel[3]) {
                if(result==source)result=std::make_shared<ImageData>(*source);
                result->pixels[i+3]=0;
            }
        }
        keyedImages_.emplace(std::move(key),result);return result;
    }
    auto key=lower(name);auto it=images_.find(key);if(it!=images_.end())return it->second;
    std::shared_ptr<ImageData> result;
    if(auto at=name.find('#');at!=std::string::npos){const auto& ns=load(name.substr(0,at));auto found=ns.find(name.substr(at+1));if(found!=ns.end())result=found->second->image;}
    else if(name.find('.')!=std::string::npos) result=decodeImage(find(name));
    else {for(auto& [symbol,v]:load(name)) if(v && v->image) {result=v->image;break;}}
    if(!result) throw std::runtime_error("No image in "+name);
    images_.emplace(key,result);return result;
}
}
