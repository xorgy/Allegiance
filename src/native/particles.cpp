#include "particles.hpp"
#include "../ClientCore/ParticleEffects.h"
#include "../ClientCore/ExplosionEffects.h"
#include <algorithm>
#include <cmath>
namespace Native {
Particles::Particles(EffectLoader loader):loader_(std::move(loader)) {
    constexpr const char* names[]={"e_smokebmp","e_firebmp","e_boosterbmp","e_telebmp","f101bmp","fx09bmp","fx18bmp","trailbmp","exp20bmp","exp22bmp","exp23bmp","exp24bmp","exp25bmp","debris1bmp","lightningbmp","f02bmp","f103bmp","fx11bmp"};
    for(size_t i=0;i<textures_.size();++i)textures_[i]=loader_(names[i]);
    particles_.reserve(4096);draws_.reserve(8192);
}
float Particles::random(){random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return float(random_>>8)*(1.f/16777216.f);}
glm::vec3 Particles::direction() {
    float z=random(-1,1),angle=random(0,6.28318530718f),r=std::sqrt(std::max(0.f,1-z*z));
    return {r*std::cos(angle),r*std::sin(angle),z};
}
void Particles::step(float dt) {
    for(size_t i=0;i<particles_.size();) {
        auto& p=particles_[i];p.age+=dt;
        if(p.age>=p.duration)remove(int(i));else ++i;
    }
}
void Particles::remove(int index) {
    auto& p=particles_[index];
    if(p.older<0)oldest_=p.newer;else particles_[p.older].newer=p.newer;
    if(p.newer<0)newest_=p.older;else particles_[p.newer].older=p.older;
    int last=int(particles_.size())-1;
    if(index!=last) {
        p=particles_.back();
        if(p.older<0)oldest_=index;else particles_[p.older].newer=index;
        if(p.newer<0)newest_=index;else particles_[p.newer].older=index;
    }
    particles_.pop_back();
}
void Particles::put(Particle p) {
    // Dense iteration plus intrusive insertion order: no allocation, search,
    // or movement of the rest of the arena when an expired/old particle retires.
    while(particles_.size()>=liveLimit_)remove(oldest_);
    int index=int(particles_.size());p.older=newest_;p.newer=-1;
    if(newest_<0)oldest_=index;else particles_[newest_].newer=index;
    newest_=index;particles_.push_back(p);
}
void Particles::emit(EffectKind kind,int sector,glm::vec3 position,glm::vec3 velocity,float size,float duration,glm::vec4 color,float delay,glm::vec3 acceleration) {
    if(size<=0 || duration<=0)return;
    put({kind,sector,position,velocity,size,duration,-delay,random()*6.2831853f,color,{},{},{},acceleration});
}
void Particles::explosion(int sector,glm::vec3 position,glm::vec3 velocity,float radius,int type,glm::vec3 forward,glm::vec3 right) {
    auto profile=Allegiance::ExplosionAppearance(type,radius);
    if(profile.waveRadius>0)
        put({EffectKind::Shockwave,sector,position,velocity,profile.waveRadius,1,0,0,
            {profile.red,profile.green,profile.blue,1},forward,right});
    for(int i=0;i<profile.count;++i) {
        auto kind=EffectKind(int(EffectKind::Explosion0)+Allegiance::ExplosionImage(type,i));
        auto origin=Allegiance::ExplosionPosition(position,profile.radius,[&](float a,float b){return random(a,b);},[&](){return direction();});
        Particle p{kind,sector,origin,velocity,profile.radius,texture(kind).frames/20.f,-Allegiance::ExplosionDelay(i),random()*6.2831853f,{1,1,1,1}};
        p.explosionIndex=i;put(p);
    }
}
void Particles::trail(int sector,glm::vec3 from,glm::vec3 to,glm::vec3 rightFrom,glm::vec3 rightTo,glm::vec4 color) {
    auto axis=(to-from)*.5f;float length=glm::length(axis);
    if(length<.01f || length>500)return;
    put({EffectKind::Trail,sector,(from+to)*.5f,{},1,8,0,0,color,axis,rightFrom,rightTo});
}
void Particles::ribbon(glm::vec3 from,glm::vec3 to,glm::vec3 rightFrom,glm::vec3 rightTo,glm::vec4 color,float nearBrightness,float farBrightness) {
    if(nearBrightness<=0)return;
    draws_.push_back({{(from+to)*.5f,rightTo.x},{(to-from)*.5f,rightTo.y},{glm::vec3(color)*nearBrightness,color.a},
        {rightFrom,farBrightness/nearBrightness},texture(EffectKind::Trail).id|hudAdditive|particleRibbon,rightTo.z});
}
void Particles::line(glm::vec3 from,glm::vec3 to,float width,float u0,float u1,glm::vec4 color,bool plain) {
    draws_.push_back({{from,width},{to,0},color,{u0,0,u1,0},(plain?0:texture(EffectKind::Bolt).id)|hudAdditive|particleLine,0});
}
void Particles::plane(const EffectTexture& t,glm::vec3 center,glm::vec3 forward,glm::vec3 right,glm::vec4 color) {
    draws_.push_back({{center,0},{forward,0},color,{right,0},t.id|hudAdditive|particleOriented,0});
}
void Particles::sprite(const EffectTexture& t,glm::vec3 position,float size,glm::vec3 axis,float width,float age,float fps,glm::vec4 color,float angle,bool additive) {
    if(size<=0 || color.a<=0)return;
    int frame=int(std::max(age,0.f)*(fps>0?fps:t.frames*.5f))%t.frames;
    // Half-texel insets prevent interpolation into adjacent animation frames.
    float frameWidth=float(t.width)/t.frames;
    float left=(float(frame)*frameWidth+.5f)/t.width,right=(float(frame+1)*frameWidth-.5f)/t.width;
    draws_.push_back({{position,size},{axis,width},color,{left,.5f/t.height,right,1-.5f/t.height},t.id|(additive?hudAdditive:0),angle});
}
void Particles::begin(int sector,glm::vec3 eye,glm::vec3 forward,bool trails) {
    draws_.clear();std::array<int,25> counts{};float fill=0;
    auto footprint=[&](glm::vec3 position,float radius) {
        float depth=glm::dot(position-eye,forward);return depth>0?4*radius*radius/(depth*depth):0;
    };
    for(auto& p:particles_)if(p.sector==sector) {
        if(p.explosionIndex>=0){++counts[p.explosionIndex+1];continue;}
        if(p.age<0)continue;
        float age=p.age,remaining=1-age/p.duration,size=p.size,angle=p.angle;auto color=p.color;
        auto appearance=[&](Allegiance::ParticleAppearance a){size=a.size;color*=glm::vec4(a.red,a.green,a.blue,a.alpha);angle+=a.angle;};
        switch(p.kind) {
        case EffectKind::Smoke:appearance(Allegiance::SmokeAppearance(size,age,p.duration));break;
        case EffectKind::Booster:appearance(Allegiance::BoosterAppearance(size,age,p.duration));break;
        case EffectKind::Fire:appearance(Allegiance::FireAppearance(size,age,p.duration));break;
        case EffectKind::Spark:appearance(Allegiance::SparkAppearance(size,age,p.duration));break;
        case EffectKind::Shockwave:
            plane(texture(p.kind),p.position+age*p.velocity,p.axis*(age*size),p.rightFrom*(age*size),{glm::vec3(color)*Allegiance::ShockwaveBrightness(age),color.a});
            fill+=footprint(p.position+age*p.velocity,age*size);
            continue;
        case EffectKind::Shield:case EffectKind::Glow:color.a*=remaining;break;
        case EffectKind::Debris:
            size=.1f*debrisFocus_;
            if(glm::distance(p.position,debrisPosition_)<20)continue;
            color.a*=std::max(0.f,1-.25f*age*age);break;
        case EffectKind::Trail:
            if(!trails)continue;
            ribbon(p.position-p.axis,p.position+p.axis,p.rightFrom,p.rightTo,color,
                Allegiance::TrailBrightness(age),Allegiance::TrailBrightness(age+.5f));continue;
        default:break;
        }
        sprite(texture(p.kind),Allegiance::ParticlePosition(p.position,p.velocity,p.acceleration,age),size,{},1,age,p.kind>=EffectKind::Explosion0 && p.kind<=EffectKind::Explosion4?20:0,color,angle);
    }
    // Original ExplosionGeo processes each burst ordinal across all events,
    // dropping later bursts once its projected-fill budget exceeds two. Counting
    // sort into fixed scratch storage preserves that policy in two linear passes.
    for(int i=1;i<25;++i)counts[i]+=counts[i-1];
    if(!counts.back())return;
    explosionOrder_.resize(counts.back());auto& ordered=explosionOrder_;auto offsets=counts;
    for(int i=0;i<int(particles_.size());++i)if(particles_[i].sector==sector && particles_[i].explosionIndex>=0)
        ordered[offsets[particles_[i].explosionIndex]++]=uint32_t(i);
    for(int ordinal=0;ordinal<24;++ordinal) {
        for(int j=counts[ordinal];j<counts[ordinal+1];++j) {
            auto& p=particles_[ordered[j]];if(p.age<0)continue;
            auto position=p.position+p.age*p.velocity;
            sprite(texture(p.kind),position,p.size,{},1,p.age,20,p.color,p.angle);fill+=footprint(position,p.size);
        }
        if(fill>2) {
            for(int i=0;i<int(particles_.size());) {
                if(particles_[i].sector==sector && particles_[i].explosionIndex>ordinal)remove(i);else ++i;
            }
            break;
        }
    }
}
void Particles::motionDebris(int sector,glm::vec3 position,glm::vec3 forward,glm::vec3 right,glm::vec3 up,float dt,float reciprocalFocus) {
    debrisFocus_=reciprocalFocus;
    if(!debrisMode_){debrisPosition_=position;debrisSector_=sector;return;}
    float distance=glm::distance(position,debrisPosition_);
    if(debrisSector_!=sector || distance>100) {
        for(size_t i=0;i<particles_.size();)if(particles_[i].kind==EffectKind::Debris)remove(int(i));else ++i;
        debrisDistance_=0;distance=0;
    }
    debrisSector_=sector;
    // Preserve DebrisGeo's effective density, including its inverse-speed
    // calculation, independently of the engine-smoke quality selection.
    float inverseSpeed=distance>0?dt/distance:0;
    float spacing=debrisSpacing[debrisMode_]-std::max(0.f,.3f-.004f*inverseSpeed);
    debrisDistance_+=distance;
    int count=std::min(128,int(debrisDistance_/spacing));debrisDistance_-=count*spacing;
    for(int i=0;i<count;++i) {
        float angle=random()*6.2831853f,depth=100*std::sqrt(random()),radius=debrisFocus_*depth*std::sqrt(random());
        auto origin=glm::mix(debrisPosition_,position,(float(i)+.5f)/count)+forward*depth+radius*(right*std::cos(angle)+up*std::sin(angle));
        emit(EffectKind::Debris,sector,origin,{},.06371f,2,{.5f+.25f*random(),.75f+.25f*random(),.75f+.25f*random(),1});
    }
    debrisPosition_=position;
}
void Particles::debris(int mode) {
    mode=std::clamp(mode,0,3);if(mode==debrisMode_)return;
    debrisMode_=mode;debrisDistance_=0;
    if(!mode)for(size_t i=0;i<particles_.size();)if(particles_[i].kind==EffectKind::Debris)remove(int(i));else ++i;
}
}
