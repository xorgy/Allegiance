#pragma once
#include <cmath>

namespace Allegiance {
// WinTrek ClusterSite::AddExplosion and its authored animation sequences.
struct ExplosionProfile {int count;float radius,waveRadius,red,green,blue;};
inline ExplosionProfile ExplosionAppearance(int type,float scale) {
    switch(type) {
    case 1:return {4,25,50,154.f/255,130.f/255,190.f/255};
    case 2:return {6,1,0,1,1,1};
    case 3:return {12,35,70,200.f/255,130.f/255,50.f/255};
    case 4:return {16,165,250,200.f/255,130.f/255,50.f/255};
    case 5:return {16,100,800,220.f/255,130.f/255,50.f/255};
    case 6:return {5,150,0,1,1,1};
    case 7:return {24,350,1000,200.f/255,130.f/255,50.f/255};
    default:return {0,scale,0,1,1,1};
    }
}
// Indices in exp20/22/23/24/25. No visual random draw selects an animation.
inline int ExplosionImage(int type,int index) {
    switch(type) {
    case 1:case 6:return 3;
    case 2:return 0;
    case 3:case 4:return index%2?1:4;
    case 5:case 7:return index%2?2:4;
    default:return index%5;
    }
}
inline int ExplosionImageCount(int type){return type==0?5:(type==3 || type==4 || type==5 || type==7)?2:1;}
inline float ExplosionDelay(int index){return index*.25f;}
inline float ShockwaveBrightness(float age){return 1-age*age;}
template<class V,class Random,class Direction>
V ExplosionPosition(const V& center,float radius,Random random,Direction direction) {
    float draw=random(0.f,1.f);
    return draw==0?center:center+direction()*(radius*.5f*std::pow(draw,1.f/3.f));
}
}
