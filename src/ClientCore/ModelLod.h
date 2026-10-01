#pragma once
namespace Allegiance {
// LODGeo compares projected radius strictly against descending thresholds.
template<class Threshold> inline int SelectModelLod(float radius,int count,Threshold threshold) {
    int index=0;for(;index<count-1;++index)if(radius>threshold(index))break;return index;
}
inline bool ModelLodVisible(float radius,float lower,float upper){return radius>lower && radius<=upper;}
}
