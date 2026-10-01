#pragma once
#include "frontend.hpp"

namespace Native {
// The original CreateTrekScrollPane uses separate arrow images and an eight
// pixel minimum thumb. Compile the same resident controls for every UI host.
inline void appendClassicScrollbar(MenuView& view,Assets& assets,ScrollList& list,int id){
    auto up=assets.image("scrollupbmp",true),down=assets.image("scrolldownbmp",true),thumb=assets.image("scrollthumbbmp",true);
    list.upHeight=up->height;list.downHeight=down->height;list.thumbMinimum=8;
    auto add=[&](glm::vec4 bounds,MenuOp op,int delta,std::shared_ptr<ImageData> image={},std::shared_ptr<ImageData> selected={}){
        if(bounds.z<=0 || bounds.w<=0)return;
        OriginalControl c;c.kind=image?OriginalControl::Kind::Button:OriginalControl::Kind::Rectangle;
        c.bounds=bounds;c.image=std::move(image);c.downImage=std::move(selected);c.color=c.image?glm::vec4(1):glm::vec4(0,0,0,1);
        c.name="scroll"+std::to_string(id)+"/"+std::to_string(int(op))+"/"+std::to_string(delta);
        view.items.push_back({bounds,"",{},{op,id,delta},true,false,c.name});view.items.back().captureOnPress=true;
        view.artwork.push_back(std::move(c));
    };
    add({list.bar.x,list.bar.y,list.bar.z,list.upHeight},MenuOp::ScrollLine,-1,up,assets.image("scrollupselbmp",true));
    add({list.bar.x,list.bar.y+list.bar.w-list.downHeight,list.bar.z,list.downHeight},MenuOp::ScrollLine,1,down,assets.image("scrolldownselbmp",true));
    auto t=list.track(),b=list.thumb();
    add({t.x,t.y,t.z,b.y-t.y},MenuOp::ScrollPage,-1);
    add({t.x,b.y+b.w,t.z,t.y+t.w-b.y-b.w},MenuOp::ScrollPage,1);
    view.items.push_back({b,"",{},{MenuOp::ScrollThumb,id},true,false,"scrollThumb"+std::to_string(id)});
    view.items.back().captureOnPress=true;
    // ThumbPane crops its final tile rather than stretching a button strip.
    for(float y=0;y<b.w;y+=thumb->height){
        OriginalControl c;c.kind=OriginalControl::Kind::Image;c.bounds={b.x,b.y+y,std::min(b.z,float(thumb->width)),std::min(b.w-y,float(thumb->height))};
        c.image=thumb;c.uv={0,0,c.bounds.z/thumb->width,c.bounds.w/thumb->height};view.artwork.push_back(std::move(c));
    }
}
}
