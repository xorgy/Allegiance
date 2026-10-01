#pragma once
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace Native {
// Pixel scrolling matches ListPane: the first and last rows can be clipped.
// Selection remains independent of the scrollbar position until navigation.
struct ScrollList {
    int count{},selection{-1};float row{1},page{},offset{};bool wholeRows{};
    glm::vec4 viewport{},bar{};float upHeight{},downHeight{},thumbMinimum{};
    float maximum()const{return std::max(0.f,count*row-page);}
    void position(float value){offset=std::clamp(value,0.f,maximum());if(wholeRows && row>0)offset=std::floor(offset/row)*row;}
    void configure(int items,float line,glm::vec4 bounds){count=items;row=line;viewport=bounds;page=bounds.w;position(offset);selection=std::clamp(selection,-1,count-1);}
    void select(int index,bool reveal=true){selection=count?std::clamp(index,-1,count-1):-1;if(reveal && selection>=0){float y=selection*row;if(y<offset)position(y);else if(y+row>offset+page)position(y+row-page);}}
    // Original ListPane's PreviousItem leaves an empty selection empty;
    // NextItem starts at the first row. Neither steps past an endpoint.
    bool move(int delta){if(!count || (selection<0 && delta<=0))return false;select(std::clamp(selection+delta,0,count-1));return true;}
    int pageRows()const{return std::max(1,int(page/row));}
    glm::vec4 track()const{return {bar.x,bar.y+upHeight,bar.z,std::max(0.f,bar.w-upHeight-downHeight)};}
    glm::vec4 thumb()const{
        auto b=track();float h=count?std::clamp(page>0?std::trunc(b.w*page/(count*row)):b.z,std::min(thumbMinimum,b.w),b.w):b.w;
        return {b.x,b.y+(maximum()>0?std::trunc(offset*(b.w-h)/maximum()):0),b.z,h};
    }
    void drag(float pointerY,float grabOffset){auto t=track(),b=thumb();position(t.w>b.w?std::trunc(std::trunc(pointerY-t.y-grabOffset)*maximum()/(t.w-b.w)):0);}
};
}
