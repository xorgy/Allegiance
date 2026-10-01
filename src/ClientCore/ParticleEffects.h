// Original effect/particlegeo.cpp and thinggeo.cpp math, independent of rendering.
#pragma once
#include <cmath>

namespace Allegiance {
struct ParticleAppearance {float red,green,blue,alpha,size,angle;};
inline ParticleAppearance SmokeAppearance(float size,float elapsed,float duration) {
    float remaining=(duration-elapsed)/duration,c=.25f*remaining;
    return {c,c*.8f,c,c,size*(.75f+2.25f*(1.f-remaining)),0};
}
inline ParticleAppearance FireAppearance(float size,float elapsed,float duration) {
    float ramp=duration*.1f,remaining=(duration-elapsed)/(duration-ramp);
    if(remaining>1)remaining=1;
    bool rising=elapsed<ramp;
    return {1,remaining,rising?remaining:remaining*.25f,.5f*remaining*remaining,
        size*(.5f+.5f*std::pow(rising?elapsed/ramp:remaining,.666f)),0};
}
inline ParticleAppearance BoosterAppearance(float size,float elapsed,float duration) {
    float ramp=duration*.1f,remaining=(duration-elapsed)/(duration-ramp);
    if(remaining>1)remaining=1;
    float c=.25f*remaining;bool rising=elapsed<ramp;
    return {rising?c*4:c,rising?c*3:c*.8f,c,c,size*(1.f+(1.f-remaining)),0};
}
inline ParticleAppearance SparkAppearance(float size,float elapsed,float duration) {
    float remaining=(duration-elapsed)/duration,c=.5f*remaining*remaining;
    return {c*1.5f,c*2,remaining,c,size,remaining*3.14159265358979323846f};
}
template<class V> V ParticlePosition(const V& position,const V& velocity,const V& acceleration,float elapsed) {
    return position+velocity*elapsed+acceleration*(elapsed*elapsed*.5f);
}
template<class V,class Random,class Direction>
void FireParticle(float age,const V& origin,const V& velocity,const V& acceleration,
        const V& direction,float speedMin,float speedMax,float duration,float variance,
        float size,V& start,V& drift,Random random,Direction randomDirection) {
    V thrust=(random(speedMin,speedMax)*size)*direction;
    V theoretical=origin+age*thrust,end=theoretical+duration*thrust;
    start=theoretical+(variance*size)*randomDirection();
    drift=(end-start)/duration+velocity-age*acceleration;
}
template<class V,class Random,class Direction>
void SmokeParticle(float age,const V& origin,const V& velocity,const V& acceleration,
        const V& direction,float speedMin,float speedMax,float variance,float size,
        V& start,V& drift,Random random,Direction randomDirection) {
    V thrust=(random(speedMin,speedMax)*size)*direction;
    start=origin+age*thrust+(variance*size)*randomDirection();
    drift=thrust+velocity-age*acceleration;
}
inline int ThrustParticleCount(float distance,float smokeSize,float power,int limit) {
    if(smokeSize<=0 || power<=0 || limit<=0)return 0;
    float count=2.f*distance/(smokeSize*power);
    return count<1?1:count>=limit?limit:int(count);
}
inline int DamageFrameCount(float fraction) {
    float count=(1.f-fraction)*10.f;
    return count<0?0:count>=10?10:int(count);
}
struct TeleportEmission {float before,scale;int count;};
inline TeleportEmission TeleportParticles(float timeLeft,float radius,int quality) {
    if(timeLeft<0)return {0,0,0};
    float before=timeLeft/3.f;if(before>1)before=1;
    float after=1-before;
    return {before,(after*2+1)*radius,int(std::floor(3.f*quality*after)+.5f)};
}
template<class V> void TeleportParticle(float before,float radius,float jitter,
        const V& origin,const V& velocity,const V& forward,const V& blast,const V& projected,
        V& position,V& drift,V& acceleration) {
    V relative=before*(projected-blast)+blast;
    drift=velocity+relative;acceleration=-velocity;
    position=origin+before*radius*1.5f*forward+relative*((1.f-before)+jitter)+(.5f*jitter*jitter)*acceleration;
}
inline int BoltSubdivision(float imageRadius) {
    return imageRadius>128?6:imageRadius>64?5:imageRadius>32?4:3;
}
template<class V,class Read,class Write,class Random>
void SubdivideBolt(const V& start,const V& end,const V& up,const V& right,float size,
        int levels,Read read,Write write,Random random) {
    int count=(1<<levels)+1;write(0,start);write(count-1,end);
    for(int level=levels;level>0;--level) {
        int big=1<<level,halfSpan=big/2;
        for(int i=halfSpan;i<count;i+=big) {
            float angle=random(0.f,6.28318530717958647692f);
            V middle=.5f*(read(i-halfSpan)+read(i+halfSpan));
            V offset=size*(std::cos(angle)*up+std::sin(angle)*right);
            write(i,middle+offset);
        }
        size/=2;
    }
}
inline float TrailBrightness(float age) {return age<8.f?1.f-age/8.f:0.f;}
struct BuildShellMotion {float innerZ,innerY,outerX,outerY,innerScale;};
inline BuildShellMotion BuildShellAnimation(float time) {
    const float pi=3.14159265358979323846f;
    return {time/pi,.5f*pi*time,time,pi*time,1.f/1.1f};
}
}
