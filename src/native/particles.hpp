#pragma once
#include "renderer.hpp"
#include "effect_preferences.hpp"
#include <array>
#include <algorithm>
#include <functional>
#include <string>
namespace Native {
struct EffectTexture {uint32_t id{},width{1},height{1};int frames{1};};
using EffectLoader=std::function<EffectTexture(const std::string&)>;
enum class EffectKind { Smoke,Fire,Booster,Spark,Glow,Shield,Shockwave,Trail,Explosion0,Explosion1,Explosion2,Explosion3,Explosion4,Debris,Bolt,AlephStar,HitTangent,HitNormal,Count };
class Particles {
public:
    explicit Particles(EffectLoader loader);
    const EffectTexture& texture(EffectKind kind)const{return textures_[size_t(kind)];}
    EffectTexture load(const std::string& name){return loader_(name);}
    void quality(int value){quality_=std::clamp(value,0,4);emitters(emitterCount_);}
    void emitters(size_t count){emitterCount_=count;int budget=std::min(quality_,2);if(quality_>2)budget=0;liveLimit_=std::max<size_t>(1024<<budget,count*(16u<<budget));}
    int emissionLimit()const{return particlePreference(quality_).limit;}
    bool performance()const{return particlePreference(quality_).performance;}
    void debris(int mode);
    float random(float low,float high){return low+(high-low)*random();}
    glm::vec3 direction();
    void step(float dt);
    void emit(EffectKind kind,int sector,glm::vec3 position,glm::vec3 velocity,float size,float duration,glm::vec4 color={1,1,1,1},float delay=0,glm::vec3 acceleration={});
    void explosion(int sector,glm::vec3 position,glm::vec3 velocity,float radius,int type,glm::vec3 forward={0,1,0},glm::vec3 right={1,0,0});
    void trail(int sector,glm::vec3 from,glm::vec3 to,glm::vec3 rightFrom,glm::vec3 rightTo,glm::vec4 color);
    void ribbon(glm::vec3 from,glm::vec3 to,glm::vec3 rightFrom,glm::vec3 rightTo,glm::vec4 color,float nearBrightness,float farBrightness);
    void line(glm::vec3 from,glm::vec3 to,float width,float u0,float u1,glm::vec4 color,bool plain=false);
    void plane(const EffectTexture& texture,glm::vec3 center,glm::vec3 forward,glm::vec3 right,glm::vec4 color={1,1,1,1});
    void begin(int sector,glm::vec3 eye={},glm::vec3 forward={},bool trails=true);
    void motionDebris(int sector,glm::vec3 position,glm::vec3 forward,glm::vec3 right,glm::vec3 up,float dt,float reciprocalFocus=.63707026f);
    void sprite(const EffectTexture& texture,glm::vec3 position,float size,glm::vec3 axis,float width,float age,float fps=0,glm::vec4 color={1,1,1,1},float angle=0,bool additive=true);
    std::span<const ParticleDraw> draws()const{return draws_;}
    size_t live()const{return particles_.size();}
private:
    struct Particle {EffectKind kind;int sector;glm::vec3 position,velocity;float size,duration,age,angle;glm::vec4 color;glm::vec3 axis{},rightFrom{},rightTo{},acceleration{};int older=-1,newer=-1,explosionIndex=-1;};
    EffectLoader loader_;std::array<EffectTexture,size_t(EffectKind::Count)> textures_;
    std::vector<Particle> particles_;std::vector<ParticleDraw> draws_;int quality_{2};uint32_t random_{347};
    size_t emitterCount_{},liveLimit_{4096};std::vector<uint32_t> explosionOrder_;
    int oldest_=-1,newest_=-1;
    void put(Particle particle);
    void remove(int index);
    glm::vec3 debrisPosition_{};int debrisSector_{-1};float debrisDistance_{},debrisFocus_{.63707026f};
    int debrisMode_{2};float random();
};
}
