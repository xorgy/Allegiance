#include "haptics.hpp"
#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>
namespace {void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Artwork path required");std::filesystem::path art(argv[1]);
        auto fire=Native::loadForceFile(art/"fire.ffe"),bounce=Native::loadForceFile(art/"bounce.ffe"),explode=Native::loadForceFile(art/"explode.ffe");
        require(fire.size()==1 && bounce.size()==4 && explode.size()==4,"all nine authored RIFF effects retained");
        auto& f=fire[0].effect.periodic;
        require(f.type==SDL_HAPTIC_SINE && f.length==125 && f.period==85 && f.magnitude==11825 && f.phase==0 && f.direction.type==SDL_HAPTIC_CARTESIAN && f.direction.dir[0]==1,"authored gun sine, duration, axis and magnitude");
        require(bounce[0].effect.type==SDL_HAPTIC_SPRING && bounce[0].effect.condition.length==275 && bounce[0].effect.condition.right_coeff[0]==32767 && bounce[0].effect.condition.right_sat[0]==0,"authored first impact spring");
        auto& b=bounce[1].effect.periodic;
        require(b.type==SDL_HAPTIC_TRIANGLE && b.length==735 && b.period==423 && b.phase==25690 && b.offset<0,"authored impact waveform, phase and negative offset");
        require(bounce[2].effect.type==SDL_HAPTIC_INERTIA && bounce[2].effect.condition.delay==810 && bounce[3].effect.condition.right_sat[0]==65535,"delayed inertia and full spring saturation");
        require(explode[0].effect.type==SDL_HAPTIC_TRIANGLE && explode[0].effect.periodic.length==1026 && explode[1].effect.periodic.phase==9465 && explode[1].effect.periodic.delay==68 && explode[2].effect.type==SDL_HAPTIC_SQUARE && explode[2].effect.periodic.delay==1008,"authored explosion bank and start delays");
        for(int i=0;i<3;++i){auto p=Native::defaultForceEffect(i);require(p.effect.constant.length==std::array<unsigned,3>{20,200,1000}[i],"original fallback durations");require(p.directional==(i==1),"only original fallback bounce changes direction");}
        std::ifstream in(art/"fire.ffe",std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});
        int rejected=0;
        for(size_t length=0;length<bytes.size();++length)try{Native::decodeForceFile(std::span(bytes).first(length));}catch(const std::runtime_error&){++rejected;}
        require(rejected==int(bytes.size()),"truncated RIFF chunks must fail safely");
        auto corrupt=bytes;corrupt[32]=0xff;corrupt[33]=0xff;
        bool failed=false;try{Native::decodeForceFile(corrupt);}catch(const std::runtime_error&){failed=true;}require(failed,"oversized RIFF list rejected");
        printf("Nine original DirectInput presets translated: waveforms, conditions, gain, axes, envelopes and delays; %d truncations and invalid chunks rejected.\n",rejected);return 0;
    }catch(const std::exception& e){fprintf(stderr,"haptics: %s\n",e.what());return 1;}
}
