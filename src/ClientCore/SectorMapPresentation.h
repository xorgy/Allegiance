// WinTrek SectorMapPane presentation rules, without Image/Pane dependencies.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <utility>

namespace Allegiance {
struct SectorMapPoint {float x{},y{};};
struct SectorMapBounds {
    SectorMapPoint minimum{},maximum{},fullMinimum{},fullMaximum{},drag{};
    bool canDrag{};
    static SectorMapBounds Calculate(std::span<const SectorMapPoint> points,float width,float height,SectorMapPoint drag={}) {
        SectorMapBounds b;if(points.empty())return b;
        b.fullMinimum=b.fullMaximum=points.front();
        for(auto p:points){b.fullMinimum.x=std::min(b.fullMinimum.x,p.x);b.fullMinimum.y=std::min(b.fullMinimum.y,p.y);b.fullMaximum.x=std::max(b.fullMaximum.x,p.x);b.fullMaximum.y=std::max(b.fullMaximum.y,p.y);}
        float buffer=.1f*std::max(.0001f,std::max(b.fullMaximum.x-b.fullMinimum.x,b.fullMaximum.y-b.fullMinimum.y));
        b.fullMinimum.x-=buffer;b.fullMinimum.y-=buffer;b.fullMaximum.x+=buffer;b.fullMaximum.y+=buffer;
        b.minimum=b.fullMinimum;b.maximum=b.fullMaximum;
        float desired=(width-8)/(height-8),maxHeight=2.2f,maxWidth=maxHeight*desired;
        b.canDrag=b.maximum.x-b.minimum.x>maxWidth || b.maximum.y-b.minimum.y>maxHeight;
        if(b.canDrag){b.maximum.x=b.minimum.x+std::min(b.maximum.x-b.minimum.x,maxWidth);b.maximum.y=b.minimum.y+std::min(b.maximum.y-b.minimum.y,maxHeight);}
        float aspect=(b.maximum.x-b.minimum.x)/(b.maximum.y-b.minimum.y);
        if(aspect<desired){float growth=(desired/aspect-1)*(b.maximum.x-b.minimum.x);b.maximum.x+=growth*.5f;b.minimum.x-=growth*.5f;}
        else if(aspect>desired){float growth=(aspect/desired-1)*(b.maximum.y-b.minimum.y);b.maximum.y+=growth*.5f;b.minimum.y-=growth*.5f;}
        b.drag={std::clamp(drag.x,0.f,std::max(0.f,(b.fullMaximum.x-b.fullMinimum.x)-(b.maximum.x-b.minimum.x))),std::clamp(drag.y,0.f,std::max(0.f,(b.fullMaximum.y-b.fullMinimum.y)-(b.maximum.y-b.minimum.y)))};
        b.minimum.x+=b.drag.x;b.minimum.y+=b.drag.y;b.maximum.x+=b.drag.x;b.maximum.y+=b.drag.y;return b;
    }
    SectorMapPoint Pixel(SectorMapPoint p,float width,float height)const {
        if(maximum.x<=minimum.x || maximum.y<=minimum.y)return {width*.5f,height*.5f};
        // WinPoint truncates the original bottom-origin coordinates. Convert
        // to the native top-origin coordinates after that integer conversion.
        return {float(int((p.x-minimum.x)/(maximum.x-minimum.x)*(width-8)+4)),height-1-float(int((p.y-minimum.y)/(maximum.y-minimum.y)*(height-8)+4))};
    }
};
inline std::pair<int,int> SectorOwners(std::span<const int> stations) {
    int owner=-1,secondary=-1,first=0,second=0;
    for(size_t side=0;side<stations.size();++side)if(stations[side]>first){secondary=owner;second=first;owner=int(side);first=stations[side];}
        else if(stations[side]>second){secondary=int(side);second=stations[side];}
    return {owner,secondary};
}
inline const char* SectorWarningImage(int warning) {
    return warning>=13?"sectorwarningbmp":warning>=10?"sectorcapitalbmp":warning>=8?"sectorbomberbmp":warning>=5?"sectorminerbmp":warning>=4?"sectorcombatbmp":nullptr;
}
}
