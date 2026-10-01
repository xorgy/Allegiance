#include "haptics.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace Native {
namespace {
void check(bool valid){if(!valid)throw std::runtime_error("Invalid DirectInput force-effect preset");}
uint32_t word(std::span<const unsigned char> d,size_t p){check(p<=d.size() && d.size()-p>=4);return uint32_t(d[p])|(uint32_t(d[p+1])<<8)|(uint32_t(d[p+2])<<16)|(uint32_t(d[p+3])<<24);}
bool tag(std::span<const unsigned char> d,size_t p,const char* t){return p<=d.size() && d.size()-p>=4 && !memcmp(d.data()+p,t,4);}
uint32_t ms(uint32_t us){return us==UINT32_MAX?SDL_HAPTIC_INFINITY:uint32_t((uint64_t(us)+999)/1000);}
Sint16 signedLevel(int32_t value,uint32_t gain=10000){return Sint16(int64_t(std::clamp(value,-10000,10000))*32767*std::min(gain,10000u)/100000000);}
Uint16 unsignedLevel(uint32_t value,uint32_t gain=10000){return Uint16(uint64_t(std::min(value,10000u))*65535*std::min(gain,10000u)/100000000);}
}
std::vector<HapticPreset> decodeForceFile(std::span<const unsigned char> d) {
    // DirectInput's 32-bit RIFF FORC file serializes a 64-byte name, a
    // DIEFFECT, the effect GUID, then sized axes/direction/envelope/parameter
    // chunks. Serialized pointer values are offsets, never native pointers.
    check(d.size()>=28 && d.size()<=65536 && tag(d,0,"RIFF") && tag(d,8,"FORC"));
    std::vector<HapticPreset> result;
    for(size_t p=28;p<d.size();) {
        check(tag(d,p,"LIST"));uint32_t size=word(d,p+4);size_t body=p+8;
        check(size>=144 && size<=d.size()-body && tag(d,body,"efct"));auto b=d.subspan(body,size);
        size_t h=68;check(word(b,h)==56);uint32_t flags=word(b,h+4),axes=word(b,h+28),gain=word(b,h+16);
        check(axes>=1 && axes<=3 && (flags&0x70)==0x10); // authored Cartesian presets
        uint32_t guid=word(b,h+56);check(guid>=0x13541c20 && guid<=0x13541c2a);
        constexpr unsigned char tail[]={0x33,0x8e,0xd0,0x11,0x9a,0xd0,0,0xa0,0xc9,0xa0,0x6e,0x35};
        check(!memcmp(b.data()+h+60,tail,sizeof(tail)));
        std::array<std::span<const unsigned char>,4> chunks;size_t c=144;
        for(auto& chunk:chunks){check(tag(b,c,"data"));uint32_t n=word(b,c+4);c+=8;check(n<=b.size()-c);chunk=b.subspan(c,n);c+=n+(n&1);}
        check(c==b.size() && chunks[0].size()==4*axes && chunks[1].size()==4*axes && (chunks[2].empty() || chunks[2].size()==20) && chunks[3].size()==word(b,h+44));
        HapticPreset preset;auto& e=preset.effect;
        constexpr SDL_HapticEffectType types[]={SDL_HAPTIC_CONSTANT,SDL_HAPTIC_RAMP,SDL_HAPTIC_SQUARE,SDL_HAPTIC_SINE,SDL_HAPTIC_TRIANGLE,SDL_HAPTIC_SAWTOOTHUP,SDL_HAPTIC_SAWTOOTHDOWN,SDL_HAPTIC_SPRING,SDL_HAPTIC_DAMPER,SDL_HAPTIC_INERTIA,SDL_HAPTIC_FRICTION};
        e.type=types[guid-0x13541c20];SDL_HapticDirection direction{};direction.type=SDL_HAPTIC_CARTESIAN;
        std::array<unsigned,3> mapping{};
        for(unsigned i=0;i<axes;++i){auto axis=word(chunks[0],4*i);check(axis<=8 && axis%4==0);mapping[i]=axis/4;direction.dir[axis/4]=int32_t(word(chunks[1],4*i));}
        // A one-axis DInput effect ignores its zero direction value.
        if(axes==1 && !direction.dir[mapping[0]])direction.dir[mapping[0]]=1;
        uint32_t length=ms(word(b,h+8));Uint16 delay=Uint16(std::min(ms(word(b,h+52)),65535u));
        auto envelope=[&](auto& target){if(!chunks[2].empty()){check(word(chunks[2],0)==20);target.attack_length=Uint16(std::min(ms(word(chunks[2],8)),65535u));target.fade_length=Uint16(std::min(ms(word(chunks[2],16)),65535u));target.attack_level=unsignedLevel(word(chunks[2],4),gain)/2;target.fade_level=unsignedLevel(word(chunks[2],12),gain)/2;}};
        auto params=chunks[3];
        if(e.type==SDL_HAPTIC_CONSTANT){check(params.size()==4);e.constant.direction=direction;e.constant.length=length;e.constant.delay=delay;e.constant.level=signedLevel(int32_t(word(params,0)),gain);envelope(e.constant);}
        else if(e.type==SDL_HAPTIC_RAMP){check(params.size()==8);e.ramp.direction=direction;e.ramp.length=length;e.ramp.delay=delay;e.ramp.start=signedLevel(int32_t(word(params,0)),gain);e.ramp.end=signedLevel(int32_t(word(params,4)),gain);envelope(e.ramp);}
        else if(guid<=0x13541c26){check(params.size()==16);e.periodic.direction=direction;e.periodic.length=length;e.periodic.delay=delay;e.periodic.magnitude=signedLevel(int32_t(word(params,0)),gain);e.periodic.offset=signedLevel(int32_t(word(params,4)),gain);e.periodic.phase=Uint16(word(params,8)%36000);e.periodic.period=Uint16(std::clamp(ms(word(params,12)),1u,65535u));envelope(e.periodic);}
        else {check(params.size()==24*axes);e.condition.direction=direction;e.condition.length=length;e.condition.delay=delay;
            for(unsigned i=0;i<axes;++i){unsigned axis=mapping[i];size_t q=24*i;e.condition.center[axis]=signedLevel(int32_t(word(params,q)));e.condition.right_coeff[axis]=signedLevel(int32_t(word(params,q+4)),gain);e.condition.left_coeff[axis]=signedLevel(int32_t(word(params,q+8)),gain);e.condition.right_sat[axis]=unsignedLevel(word(params,q+12),gain);e.condition.left_sat[axis]=unsignedLevel(word(params,q+16),gain);e.condition.deadband[axis]=unsignedLevel(word(params,q+20));}}
        result.push_back(preset);p=body+size+(size&1);check(result.size()<=32);
    }
    check(!result.empty());return result;
}
std::vector<HapticPreset> loadForceFile(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Missing force-effect preset: "+path.string());
    auto size=in.tellg();check(size>=0 && size<=65536);std::vector<unsigned char> bytes(size);in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),size);check(bool(in));return decodeForceFile(bytes);
}
HapticPreset defaultForceEffect(int id) {
    HapticPreset p;
    if(id==2){auto& e=p.effect.periodic;e.type=SDL_HAPTIC_SINE;e.direction.type=SDL_HAPTIC_CARTESIAN;e.direction.dir[0]=1;e.length=1000;e.period=100;e.magnitude=24575;e.fade_length=1000;}
    else {auto& e=p.effect.constant;e.type=SDL_HAPTIC_CONSTANT;e.length=id==0?20:200;e.level=24575;e.direction.type=id==0?SDL_HAPTIC_CARTESIAN:SDL_HAPTIC_POLAR;e.direction.dir[id==0?1:0]=id==0?1:0;p.directional=id==1;}
    return p;
}
}
