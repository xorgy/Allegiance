#include "test_fixture.hpp"
#include "../ClientCore/StarField.h"
#include "test_reference/particle_effects.hpp"
#include "../ClientCore/ParticleEffects.h"
#include "../ClientCore/AlephEffects.h"
#include "../ClientCore/ShieldFlare.h"
#include "../ClientCore/ExplosionEffects.h"
#include "../ClientCore/SurfaceEffects.h"
#include "../ClientCore/Strobe.h"
#include "../ClientCore/ModelLod.h"
#include "particles.hpp"
#include "game.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <array>

namespace {
using NativeTest::require;
void near(float a,float b){require(std::abs(a-b)<1e-5f,"original particle curve mismatch");}
void near(Vector a,Vector b){require((a-b).LengthSquared()<1e-8f,"original particle trajectory mismatch");}
void strobeAndLod() {
    unsigned cases=0;
    for(float period:{1.f,2.f,7.f})for(float initial:{0.f,.5f,3.f})
    for(float up:{0.f,.125f,.25f})for(float hold:{0.f,.25f})for(float down:{0.f,.125f,.25f}) {
        float actualPhase=initial,expectedPhase=initial;
        for(int tick=0;tick<4000;++tick) {
            float time=tick/16.f,value;
            while(true){value=time-expectedPhase;if(value<period)break;expectedPhase+=period;}
            if(value<up)value/=up;else {value-=up;if(value<hold)value=1;else{value-=hold;value=value<down?1-value/down:0;}}
            float actual=Allegiance::StrobeBrightness(time,period,actualPhase,up,hold,down);
            require(std::abs(std::max(value,0.f)-actual)<1.e-6f && actualPhase==expectedPhase,"original strobe envelope/phase differs");++cases;
        }
    }
    float phase=0;require(Allegiance::StrobeBrightness(100,0,phase,0,1,0)==0,"zero-period strobe is finite");
    for(float a:{0.f,16.f,32.f,64.f})for(float b:{0.f,16.f,32.f})if(a>=b) {
        const float thresholds[]={a,b};
        for(float radius:{0.f,.01f,15.f,16.f,16.01f,32.f,32.01f,64.f,64.01f,1000.f}) {
            int expected=0;for(;expected<2;++expected)if(radius>thresholds[expected])break;
            require(Allegiance::SelectModelLod(radius,3,[&](int i){return thresholds[i];})==expected,"original LOD threshold equality changed");
            for(int i=0;i<3;++i){float low=i<2?thresholds[i]:-INFINITY,high=i?thresholds[i-1]:INFINITY;
                require(Allegiance::ModelLodVisible(radius,low,high)==(i==expected),"flattened mesh LOD interval differs from original tree");}
        }
    }
    printf("%u frozen original strobe samples and LOD threshold boundaries passed\n",cases);
}
void curves() {
    OriginalEffects::Data p{2,4,.25f,Vector(1,2,3),Vector(7,8,9),Vector(0,1,0)};
    using Reference=OriginalEffects::Appearance(*)(const OriginalEffects::Data*,float);
    using Shared=Allegiance::ParticleAppearance(*)(float,float,float);
    std::array<Reference,4> references{OriginalEffects::Smoke,OriginalEffects::Fire,OriginalEffects::Booster,OriginalEffects::Spark};
    std::array<Shared,4> shared{Allegiance::SmokeAppearance,Allegiance::FireAppearance,Allegiance::BoosterAppearance,Allegiance::SparkAppearance};
    for(float duration:{.01f,.333f,2.f})for(float size:{.1f,4.f,75.f})for(float fraction:{0.f,.05f,.099f,.1f,.101f,.5f,.95f,1.f}) {
        p.m_fDuration=duration;p.m_fSize=size;
        for(int kind=0;kind<4;++kind) {
            auto a=shared[kind](size,duration*fraction,duration);auto b=references[kind](&p,duration*fraction);
            near(a.red,b.red);near(a.green,b.green);near(a.blue,b.blue);near(a.alpha,b.alpha);near(a.size,b.size);near(a.angle+p.m_fAngle,b.angle);
        }
    }
    for(float age:{0.f,.017f,.1f})for(float size:{.1f,4.f})for(float power:{.2f,1.f}) {
        Vector a,b,c,d;int draws=0;
        auto rng=[&](float low,float high){++draws;return low+(high-low)*.31f;};auto direction=[](){return Vector(.6f,0,.8f);};
        auto emit=[&](bool fire,bool reference){
            if(fire) {
                if(reference)OriginalEffects::Fire(age,p.m_vecPosition,p.m_vecVelocity,p.m_vecAcceleration,Vector(0,0,1),9.f,12.f,.333f,.2f,size*power,c,d,rng,direction);
                else Allegiance::FireParticle(age,p.m_vecPosition,p.m_vecVelocity,p.m_vecAcceleration,Vector(0,0,1),9.f,12.f,.333f,.2f,size*power,a,b,rng,direction);
            } else {
                if(reference)OriginalEffects::Smoke(age,p.m_vecPosition,p.m_vecVelocity,p.m_vecAcceleration,Vector(0,0,1),18.f,24.f,.1f,size*power,c,d,rng,direction);
                else Allegiance::SmokeParticle(age,p.m_vecPosition,p.m_vecVelocity,p.m_vecAcceleration,Vector(0,0,1),18.f,24.f,.1f,size*power,a,b,rng,direction);
            }
        };
        for(bool fire:{false,true}){emit(fire,false);emit(fire,true);near(a,c);near(b,d);}
        require(draws==4,"one ejection-speed draw per original particle");
    }
    require(Allegiance::ThrustParticleCount(100,0,1,5)==0,"zero-size exhaust stays finite");
    require(Allegiance::ThrustParticleCount(0,4,1,5)==1 && Allegiance::ThrustParticleCount(100,4,1,5)==5,"authored density and quality cap");
    require(Allegiance::DamageFrameCount(0)==10 && Allegiance::DamageFrameCount(1)==0,"damage slots remain in bounds");
    require(Allegiance::TeleportParticles(3,10,5).count==0 && Allegiance::TeleportParticles(0,10,5).count==15 && Allegiance::TeleportParticles(-1,10,5).count==0,"teleport ramp and cancellation");
    Vector position,velocity,acceleration;
    Allegiance::TeleportParticle(.5f,10.f,.1f,Vector(1,2,3),Vector(4,5,6),Vector(0,0,1),Vector(2,4,6),Vector(2,4,0),position,velocity,acceleration);
    near(position,Vector(2.18f,4.375f,12.27f));near(velocity,Vector(6,9,9));near(acceleration,Vector(-4,-5,-6));
    require(Allegiance::BoltSubdivision(16)==3 && Allegiance::BoltSubdivision(129)==6,"original bolt screen-space detail bounds");
    std::array<Vector,65> points;int draws=0;
    Allegiance::SubdivideBolt(Vector(0,0,0),Vector(0,0,100),Vector(1,0,0),Vector(0,1,0),12.5f,6,
        [&](int i){return points[i];},[&](int i,Vector p){points[i]=p;},[&](float,float){++draws;return 0.f;});
    near(points[0],Vector(0,0,0));near(points[64],Vector(0,0,100));near(points[32],Vector(12.5f,0,50));
    require(draws==63,"bolt has bounded subdivision and one random draw per new point");
}
void starField() {
    // Frozen Windows CRT seed outputs, independent of the host libc RNG.
    Allegiance::StarRandom random(1);
    constexpr int values[]={41,18467,6334,26500,19169,15724,11478,29358,26962,24464};
    for(int v:values)require(random(0,1)==float(v)/32767.f*(1-FLT_EPSILON),"authored Windows star seed sequence");
    int count=0;
    for(float x:{-.9f,-.3f,0.f,.4f,.99f})for(float y:{-.99f,-.2f,0.f,.6f})for(float z:{-1.f,0.f,1.f})
    for(float focus:{400.f,600.f,1200.f}) {
        float a=x,b=y;if(z>0){a=-a;b=-b;}
        auto p=Allegiance::ProjectStar(x,y,z,focus,960,540);
        require(p.x==float(int(focus*a+960)) && p.y==float(int(540-focus*b)),"original folded star projection");
        for(float dx:{-50.f,-16.f,-1.f,0.f,.4f,3.f,30.f})for(float dy:{-30.f,-1.f,0.f,.5f,7.f,40.f}) {
            float oldX=p.x+dx,oldY=p.y+dy,length=std::abs(p.x-oldX)+std::abs(p.y-oldY),brightness=1;
            if(length>1){if(length>16){oldX=p.x+(oldX-p.x)*(16/length);oldY=p.y+(oldY-p.y)*(16/length);}brightness=std::max(.25f,1-.1f*length);}
            else{oldX=p.x;oldY=p.y;}
            auto m=Allegiance::StarMotion(p,{p.x+dx,p.y+dy});
            near(m.end.x,oldX);near(m.end.y,oldY);near(m.brightness,brightness);require(m.point==(length<=1),"original star point/line threshold");++count;
        }
    }
    printf("Original star projection/motion: %d cases.\n",count);
    // Independent scene-matrix reference: the sky is viewed from its center,
    // shares perspective and handedness with world objects, and ignores eye
    // translation. These checks catch the old orthographic hemisphere fold.
    unsigned projected=0;
    for(float fov:{45.f,65.f,100.f})for(glm::ivec2 size:{glm::ivec2(800,600),glm::ivec2(1920,1080),glm::ivec2(2520,1080)})
    for(float yaw:{-.8f,0.f,1.5f})for(float pitch:{-.4f,0.f,.7f})for(float roll:{-.5f,0.f,.9f}) {
        auto rotation=glm::rotate(glm::mat4(1),yaw,{0,1,0})*glm::rotate(glm::mat4(1),pitch,{1,0,0})*glm::rotate(glm::mat4(1),roll,{0,0,1});
        auto forward=glm::vec3(rotation*glm::vec4(0,0,-1,0)),cameraUp=glm::vec3(rotation*glm::vec4(0,1,0,0));
        auto right=glm::normalize(glm::cross(forward,cameraUp)),up=glm::cross(right,forward);
        auto projection=glm::perspectiveRH_ZO(glm::radians(fov),float(size.x)/size.y,30000.f,.2f);projection[1][1]*=-1;
        auto eye=glm::vec3(137,-283,59);auto vp=projection*glm::lookAtRH(eye,eye+forward,cameraUp);
        float focus=.5f*size.y/std::tan(glm::radians(fov)*.5f);
        for(float z:{-.99f,-.7f,-.3f,.1f,.4f,.85f})for(int angle=0;angle<24;++angle) {
            float a=angle*(2*pi/24),radius=std::sqrt(1-z*z);glm::vec3 direction{std::cos(a)*radius,std::sin(a)*radius,z};
            auto clip=vp*glm::vec4(direction,0);bool expected=clip.w>0 && clip.x>=-clip.w && clip.x<clip.w && clip.y>=-clip.w && clip.y<clip.w;
            Allegiance::StarPoint point;bool actual=Allegiance::ProjectPerspectiveStar(glm::dot(right,direction),glm::dot(up,direction),glm::dot(forward,direction),focus,size.x*.5f,size.y*.5f,point);
            require(actual==expected,"native star frustum differs from world geometry");
            if(actual) {
                auto pixel=(glm::vec2(clip)/clip.w+1.f)*.5f*glm::vec2(size);
                require(std::abs(point.x-float(int(pixel.x)))<=1 && std::abs(point.y-float(int(pixel.y)))<=1,"native star perspective differs from scene matrix");
            }
            ++projected;
        }
    }
    Allegiance::StarPoint point;
    require(!Allegiance::ProjectPerspectiveStar(0,0,-1,600,960,540,point) &&
            !Allegiance::ProjectPerspectiveStar(1,0,0,600,960,540,point),"rear/horizon stars must not fold into view");
    require(Allegiance::ProjectPerspectiveStar(.3f,.2f,1,600,960,540,point) && point.x==1140 && point.y==420,"perspective star analytic projection");
    require(Allegiance::ProjectPerspectiveStar(.6f,.4f,2,600,960,540,point) && point.x==1140 && point.y==420,"sky direction magnitude must not change projection");
    printf("Native perspective stars: %u scene-matrix cases across yaw, pitch, roll, FOV and aspect ratios.\n",projected);
}
void sectorVisuals(const std::filesystem::path& art) {
    Native::Assets assets(art);auto loader=[&](const std::string& name,const std::string&){auto m=assets.model(name);return Native::ModelHandle{0,m->radius,m->frames,m->lights};};
    for(int map=0;map<4;++map) {
        Native::SessionOptions options;options.enemies=0;options.map=map;
        Native::Game game(art,loader,0,0,options);game.camera({});auto& state=game.state();
        auto* sector=game.core()->GetCluster(state.sectorId);require(sector,"sector visual data");
        require(state.stars.size()==size_t(sector->GetNstars())*4,"original per-sector star count");
        Allegiance::StarRandom random(sector->GetStarSeed());
        for(auto& star:state.stars) {
            float pitch=random(-1,1),c=std::sqrt(1-pitch*pitch),yaw=random(0,2*pi);
            near(star.direction.x,std::cos(yaw)*c);near(star.direction.y,std::sin(yaw)*c);near(star.direction.z,pitch);near(star.brightness,random(.25f,1));
        }
        auto light=sector->GetLightColor();near(state.lighting.direction.w,sector->GetBidirectionalAmbientLevel());near(state.lighting.primary.r,light.GetRed());
        int environment=0,posters=0;for(auto& draw:game.scene(true)){environment+=draw.background==1;posters+=draw.background==2;}
        require(environment==1 && posters==1,"original environment and planet posters are retained");
        auto counts=[&](){std::array<int,3> count{};for(auto& draw:game.scene(true))++count[draw.background];return count;};
        auto baseline=counts();Native::VisualPreferences prefs;prefs.environment=false;game.visualPreferences(prefs);
        auto hiddenSky=counts();require(hiddenSky[0]==baseline[0] && hiddenSky[1]==0 && hiddenSky[2]==baseline[2],"environment preference must hide only sector sky geometry");
        prefs.environment=true;prefs.posters=false;game.visualPreferences(prefs);auto hiddenPosters=counts();
        require(hiddenPosters[0]==baseline[0] && hiddenPosters[1]==baseline[1] && hiddenPosters[2]==0,"poster preference must retain sky and world geometry");
        prefs.posters=true;prefs.stars=false;prefs.bidirectional=false;game.visualPreferences(prefs);
        require(game.state().stars.empty() && glm::vec3(game.state().lighting.alternate)==glm::vec3(0),"star/alternate-light preferences must affect actual world snapshots");
        near(game.state().lighting.direction.w,sector->GetAmbientLevel());near(game.state().lighting.primary.r,light.GetRed());
        game.visualPreferences({});require(game.state().stars.size()==size_t(sector->GetNstars())*4 && counts()==baseline,"appearance restoration must reuse original sector resources and geometry");
        near(game.state().lighting.direction.w,sector->GetBidirectionalAmbientLevel());

    }
}
void nativeBatches() {
    {
        Native::Particles trails([](const auto&){return Native::EffectTexture{1,64,64,1};});
        trails.trail(0,{0,0,0},{0,10,0},{1,0,0},{1,0,0},{1,1,1,1});trails.begin(0);
        require(!trails.draws().empty(),"authored trail must render when enabled");auto live=trails.live();
        trails.begin(0,{},{},false);require(trails.draws().empty() && trails.live()==live,"disabled trails must suppress existing ribbons without destroying particle state");
        trails.begin(0);require(!trails.draws().empty(),"reenabling trails restores retained visible ribbon");
    }
    Native::Particles effects([](const auto&){return Native::EffectTexture{1,64,64,1};});
    effects.emit(Native::EffectKind::Smoke,0,{1,2,3},{4,5,6},4,2,{1,1,1,1},0,{0,2,0});
    effects.step(.5f);effects.begin(0);require(effects.draws().size()==1,"native effect batch");
    auto& draw=effects.draws()[0];auto a=Allegiance::SmokeAppearance(4,.5f,2);
    near(draw.centerSize.x,3);near(draw.centerSize.y,4.75f);near(draw.centerSize.z,6);near(draw.centerSize.w,a.size);
    near(draw.color.r,a.red);near(draw.color.a,a.alpha);
    effects.begin(1);require(effects.draws().empty(),"effects remain in their sector");
    effects.step(2);effects.begin(0);require(effects.live()==0,"expired effects retire from bounded pool");
    for(int i=0;i<5000;++i)effects.emit(Native::EffectKind::Fire,0,{},{},1,1);
    require(effects.live()==4096,"particle arena bounded under overload");
    effects.emit(Native::EffectKind::Spark,0,{10000,0,0},{},1,1);effects.begin(0);
    require(std::any_of(effects.draws().begin(),effects.draws().end(),[](auto& d){return d.centerSize.x==10000;}),"new impacts survive saturation");
    effects.quality(0);effects.emit(Native::EffectKind::Smoke,0,{20000,0,0},{},1,1);
    require(effects.live()==1024,"quality reduction retires oldest particles without reallocating");
    effects.step(2);require(effects.live()==0,"saturated arena cleanup");
    // Exercise swap-removal of interior slots, then eviction of the remaining oldest.
    for(int i=0;i<1500;++i){effects.emit(Native::EffectKind::Smoke,0,{float(i),0,0},{},1,i%2?.01f:10.f);effects.step(.001f);}
    effects.step(11);require(effects.live()==0,"interleaved expiry and replacement preserve dense arena");
    effects.begin(0);effects.ribbon({},{1,0,0},{0,1,0},{0,1,0},{.5f,.6f,.7f,1},.5f,.25f);
    near(effects.draws()[0].color.r,.25f);near(effects.draws()[0].color.a,1);
}
void explosions() {
    // Golden authored values from original trekigc.cpp at 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
    constexpr int counts[]={0,4,6,12,16,16,5,24},images[]={0,3,0,4,4,4,3,4};
    constexpr float radius[]={9,25,1,35,165,100,150,350},wave[]={0,50,0,70,250,800,0,1000};
    for(int type=1;type<=7;++type) {
        Native::Particles effects([](const std::string& n){uint32_t id=n=="exp20bmp"?100:n=="exp22bmp"?101:n=="exp23bmp"?102:n=="exp24bmp"?103:n=="exp25bmp"?104:99;return Native::EffectTexture{id,2048,64,32};});
        auto profile=Allegiance::ExplosionAppearance(type,9);require(profile.count==counts[type],"original explosion count");
        near(profile.radius,radius[type]);near(profile.waveRadius,wave[type]);
        effects.explosion(0,{10,20,30},{4,5,6},9,type,{0,0,-1},{1,0,0});
        require(effects.live()==size_t(counts[type]+(wave[type]>0)),"original event creates all delayed bursts");
        effects.step(.125f);effects.begin(0);int sprites=0,waves=0;
        for(auto& d:effects.draws()) {
            if(d.texture&Native::particleOriented) {
                ++waves;near(d.axisWidth.z,-wave[type]*.125f);near(d.uv.x,wave[type]*.125f);
                near(d.color.r,profile.red*(1-.125f*.125f));near(d.color.a,1);near(d.centerSize.x,10.5f);
            } else {
                ++sprites;require((d.texture&0x0fffffffu)==uint32_t(100+images[type]),"authored first explosion image");near(d.centerSize.w,radius[type]);
                require(glm::distance(glm::vec3(d.centerSize),glm::vec3(10.5f,20.625f,30.75f))<=radius[type]*.5f+1e-4f,"explosion offsets stay inside original sphere");
            }
        }
        require(sprites==1 && waves==(wave[type]>0),"quarter-second stagger and oriented wave");
        effects.step(.125f);effects.begin(0);require(effects.draws().size()==size_t(2+(wave[type]>0)),"next burst begins at original delay");
        effects.step(8);require(effects.live()==0,"all staggered effects expire");
    }
    Native::Particles effects([](const auto&){return Native::EffectTexture{1,2048,64,32};});
    effects.explosion(0,{0,0,1},{},100,7);effects.explosion(1,{0,0,1},{},100,7);
    effects.step(.2f);effects.begin(0,{},{0,0,1});
    require(effects.live()==27,"original projected-fill limit drops later bursts only in the rendered sector");
    effects.step(8);require(effects.live()==0,"fill-budget deletion preserves arena links");
}
void shieldAndAleph(const std::filesystem::path& art) {
    Orientation hull;hull.Yaw(.7f);hull.Pitch(.4f);
    for(auto hit:{Vector(1,0,0),Vector(0,1,0),Vector(1,2,-3),Vector::GetZero()}) {
        auto actual=Allegiance::ShieldFlareOrientation(hull,hit,20,nullptr);
        Orientation original;original.Set(hull.TimesInverse(hit));
        near(actual.GetForward(),original.GetForward());near(actual.GetRight(),original.GetRight());near(actual.GetUp(),original.GetUp());
        Vector ellipse(15,20,25),v=hull.TimesInverse(hit);v.x/=15;v.y/=20;v.z/=25;
        original.Set(v);original.Scale(ellipse/20);
        actual=Allegiance::ShieldFlareOrientation(hull,hit,20,&ellipse);
        near(actual.GetForward(),original.GetForward());near(actual.GetRight(),original.GetRight());near(actual.GetUp(),original.GetUp());
    }
    near(Allegiance::AlephStarScale(-1,0),.1f);near(Allegiance::AlephStarScale(8,8),.1f);
    near(Allegiance::AlephStarScale(0,8),10.72f);near(Allegiance::AlephStarScale(-.125f,8),1.575f);
    near(Allegiance::AlephStarScale(-.25f,8),.1f);near(Allegiance::AlephTextureOffset(12.5f),.25f);
    for(auto normal:{Vector(0,0,1),Vector(0,0,-1),Vector(1,0,0),Vector(1,2,3).Normalize(),Vector::GetZero()}) {
        auto axes=Allegiance::HullImpactAxes(normal);
        require(std::isfinite(axes.forward.LengthSquared()) && std::isfinite(axes.right.LengthSquared()),"finite hull-hit tangent frame at both poles");
        near(axes.forward*normal,0);near(axes.right*normal,0);
    }
    near(Allegiance::HullImpactScale(0),.1f);near(Allegiance::HullImpactScale(.1f),1.1f);near(Allegiance::HullImpactScale(.2f),2.f);
    require(Allegiance::MineParticleCount(1,50)==26 && Allegiance::MineParticleCount(.5f,50)==13 && Allegiance::MineParticleCount(0,50)==1,"original mine strength density");
    near(Allegiance::MineBrightness(0),1);near(Allegiance::MineBrightness(200),.5f);
    Native::Assets assets(art);
    int expected[]={297,85,51,27};
    for(int detail=0;detail<4;++detail) {
        auto model=assets.model("$aleph"+std::to_string(detail));auto& mesh=*model->meshes[0];
        require(mesh.vertices.size()==size_t(expected[detail]),"original aleph LOD vertex counts");
        for(auto& v:mesh.vertices) {
            float r=glm::length(glm::vec2(v.position));near(v.position.z,-(1-r)*(1-r));
            near(v.normal.x,2*r*(1-r));near(v.uv.y,r);
        }
        for(auto i:mesh.indices)require(i<mesh.vertices.size(),"aleph indices in range");
    }
    auto loader=[&](const std::string& name,const std::string& skin){
        auto m=assets.model(name);if(!skin.empty())assets.image(skin+"bmp");
        uint32_t id=name.starts_with("lens#")?100+std::stoi(name.substr(9))/30:name.starts_with("$aleph")?200+name.back()-'0':1;
        return Native::ModelHandle{id,m->radius,m->frames,m->lights};
    };
    auto effects=[&](const std::string& name){auto image=assets.image(name);return Native::EffectTexture{name=="f02bmp"?18u:name=="f103bmp"?19u:name=="fx11bmp"?20u:1u,image->width,image->height,int(std::max(1u,image->width/image->height))};};
    Native::SessionOptions options;options.map=3;options.enemies=0;
    {
        Native::Game game(art,loader,0,0,options,effects);game.step({},.1);game.camera({});
        int funnels=0,stars=0;
        for(auto& draw:game.scene(true))if(draw.aleph){++funnels;require(draw.blend==Native::ModelBlend::Additive && draw.model>=200,"authored aleph material and detail");}
        for(auto& draw:game.particles())if((draw.texture&0x1fffffffu)==18)++stars;
        require(funnels>0 && stars>0 && stars%2==0,"native world has paired rotating aleph stars and funnel geometry");
        auto gridLines=[&](){int lines=0;for(auto& draw:game.particles())if(draw.texture==(Native::hudAdditive|Native::particleLine))++lines;return lines;};
        require(gridLines()==0,"pilot grid defaults off");
        game.commandView(true);game.camera({});require(gridLines()>=82,"command grid defaults on independently of pilot preference");
        game.showGrid(true);game.showCommandGrid(false);
        require(gridLines()==0,"pilot grid preference cannot force command grid or height lines on");
        stars=0;for(auto& draw:game.particles())if((draw.texture&0x1fffffffu)==18)++stars;
        require(stars>0,"hiding command overlays retains world effects");
        game.commandView(false);game.camera({});require(gridLines()>=82,"pilot grid remains enabled after leaving command view");
        game.commandView(true);game.camera({});require(gridLines()==0,"returning to command view retains its disabled grid preference");
        game.commandView(false);game.showGrid(false);game.camera({});
        Native::FlightInput input;input.deploy=true;
        for(int i=0;i<80;++i)game.step(input,1./165);
        int expected=0;
        for(auto& c:game.contacts())if(c.type==Native::ContactMine)expected+=Allegiance::MineParticleCount(1,c.radius);
        int actual=0;for(auto& d:game.particles())if(!(d.texture&Native::hudAdditive)){++actual;near(d.centerSize.w,1);}
        require(expected>1 && actual==expected,"deployed IGC mines render full source-alpha fields at authored density");
    }
    options.map=0;options.enemies=3;
    Native::Game game(art,loader,0,0,options,effects);bool flare=false,retired=false,hullHit=false;double hitTime=0;
    Native::CameraInput chase;chase.chase=true;
    for(int i=0;i<14000 && !(retired && hullHit);++i) {
        game.step({},1./165);game.camera(chase);bool present=false;
        for(auto& draw:game.scene(true))if(draw.model>=100 && draw.model<200) {
            present=true;require(draw.blend==Native::ModelBlend::Additive && draw.textureScale>0 && draw.textureScale<=1,"native shield uses lens atlas");
        }
        if(present){flare=true;hitTime=game.state().time;}
        if(flare && !present && game.state().time-hitTime>1)retired=true;
        for(auto& draw:game.particles())if((draw.texture&0x0fffffffu)==19){hullHit=true;require(draw.texture&Native::particleOriented,"hull strike produces original tangent plane");}
    }
    require(flare && retired,"actual IGC combat produces and retires curved shield hits");
    require(hullHit,"actual IGC combat produces surface impacts after shield depletion");
}
struct PulseFixture : NativeTest::Fixture {
    std::vector<float> durations;
    struct Site : ThingSite {PulseFixture& f;explicit Site(PulseFixture& p):f(p){}void AddPulse(float t,const Vector&,float,const Color&)override{f.durations.push_back(t);}};
    explicit PulseFixture(const std::filesystem::path& art):Fixture(art,false){require(LoadIGCFile("training_6",mission),"pulse fixture map");}
    TRef<ThingSite> CreateThingSite(ImodelIGC*)override{return new Site(*this);}
};
void bombClock(const std::filesystem::path& art) {
    PulseFixture f(art);ImissileTypeIGC* type=nullptr;
    for(auto* l=f.mission->GetExpendableTypes()->first();l;l=l->next())if(l->data()->GetObjectType()==OT_missileType){type=static_cast<ImissileTypeIGC*>(l->data());break;}
    require(type && f.mission->GetWarps()->first(),"warp bomb fixture");
    auto* warp=f.mission->GetWarps()->first()->data();warp->AddBomb(f.mission->GetLastUpdate()+8.f,type);
    require(f.durations.size()==2,"both aleph endpoints receive bomb warning");
    for(float duration:f.durations)near(duration,8);
}
void cadence(const std::filesystem::path& art) {
    Native::Assets assets(art);
    auto loader=[&](const std::string& name,const std::string&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};};
    auto effects=[](const std::string& name){return Native::EffectTexture{name=="e_boosterbmp"?17u:1u,64,64,1};};
    std::array<int,3> counts{};int index=0;
    for(int rate:{60,165,240}) {
        Native::SessionOptions options;options.enemies=0;options.scenario=Native::Scenario::FreeFlight;
        Native::Game game(assets.root(),loader,0,0,options,effects);Native::FlightInput input;input.throttle=1;input.boost=true;
        for(int i=0;i<rate*4;++i)game.step(input,1./rate);
        for(auto& p:game.particles())if((p.texture&0x1fffffffu)==17)++counts[index];
        ++index;
    }
    require(counts[0]>0,"actual IGC ship emits boost particles");
    for(int count:counts)require(std::abs(count-counts[0])<=6,"plume density independent of refresh rate");
    printf("Live boost sprites at 60/165/240 Hz: %d/%d/%d.\n",counts[0],counts[1],counts[2]);
}
void particleModes(const std::filesystem::path& art) {
    Native::Assets assets(art);
    auto loader=[&](const std::string& name,const std::string&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};};
    auto effects=[](const std::string& name){return Native::EffectTexture{name=="e_boosterbmp"?17u:name=="e_firebmp"?18u:name=="e_smokebmp"?19u:1u,64,64,1};};
    glm::vec3 position{},velocity{};
    for(int mode:{3,4,0,1,2}) {
        Native::SessionOptions options;options.enemies=0;options.scenario=Native::Scenario::FreeFlight;
        Native::Game game(art,loader,0,0,options,effects);
        game.particleQuality(mode);Native::VisualPreferences visual;visual.debris=0;game.visualPreferences(visual);
        Native::FlightInput input;input.throttle=1;input.boost=true;
        for(int frame=0;frame<240;++frame)game.step(input,1./60);
        int boosters=0,fires=0,smoke=0;
        for(auto& draw:game.particles()){
            auto image=draw.texture&0x0fffffffu;boosters+=image==17;fires+=image==18;smoke+=image==19;
        }
        if(mode==3)require(!boosters && !fires && !smoke,"Particles Off must suppress actual IGC exhaust emission");
        else if(mode==4)require(!boosters && fires>0 && smoke>0,"Performance must replace booster plumes with rocket fire and smoke");
        else require(boosters>0 && !fires && !smoke,"normal smoke quality must retain the authored booster plume");
        auto* ship=game.playerShip();auto v=ship->GetVelocity();glm::vec3 currentVelocity{v.x,v.y,v.z};
        if(mode==3){position=game.state().position;velocity=currentVelocity;}
        else require(glm::distance(position,game.state().position)<.001f && glm::distance(velocity,currentVelocity)<.001f,"graphics smoke modes must leave IGC flight behavior unchanged");
    }
    puts("All five original particle modes: actual IGC exhaust types, Off and unchanged flight passed.");
}
}
int main(int argc,char** argv) {
    try{starField();strobeAndLod();curves();nativeBatches();explosions();if(argc>1){sectorVisuals(argv[1]);cadence(argv[1]);particleModes(argv[1]);shieldAndAleph(argv[1]);bombClock(argv[1]);}puts("Original particle curves, shield hits, alephs, explosions and bounded batches passed.");return 0;}
    catch(const std::exception& e){fprintf(stderr,"effects: %s\n",e.what());return 1;}
}
