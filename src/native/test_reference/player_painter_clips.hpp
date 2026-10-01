#pragma once
#include "../player_pane.hpp"
#include "authored_hosts.hpp"

namespace NativeTest {
inline void checkPlayerPainterClips(Native::Assets& assets,glm::ivec2 size={1280,720},
    std::function<void(const Native::MenuView&,const Native::MenuView&,std::string_view)> paint={}){
    using namespace Native;
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    for(int i=0;i<32;++i)require(game.addLocalPilot(std::string(60,'W')+std::to_string(i),0)>=0,"F6 painter clipping fixture lacks pilot rows");
    OriginalUi authored(assets);auto columns=authored.integers("teampane","eTeamPanePlayerColumns");PlayerPane pane(assets);
    size_t checks=0;
    for(bool compact:{false,true}){
        pane.reset();pane.toggle(game,compact);const auto& view=pane.view(size,1);
        auto source=authored.layout("teampane",compact?"teamPane":"expandedTeamPane",{}, {},true);
        // List slots are host descriptors, so the pane retains their geometry
        // separately rather than drawing them. Anchor the authored slot to a
        // retained non-slot control in the same translated root.
        glm::vec2 origin{};bool anchored=false;
        for(const auto& control:source)if(control.kind!=OriginalControl::Kind::Slot && !control.name.empty()){
            auto rendered=std::find_if(view.artwork.begin(),view.artwork.end(),[&](const auto& candidate){return candidate.name==control.name && candidate.kind==control.kind;});
            if(rendered!=view.artwork.end()){origin=glm::vec2(rendered->bounds)-glm::vec2(control.bounds);anchored=true;break;}
        }
        require(anchored,"F6 painter fixture cannot anchor its authored list descriptor");
        auto viewport=authoredControlBounds(source,compact?"teamPanePlayerListPane":"eTeamPanePlayerListPane");viewport.x+=origin.x;viewport.y+=origin.y;viewport.z=std::max(viewport.z,compact?160.f:float(columns[7]-15));
        MenuView actual,reference;actual.original=reference.original=true;actual.canvas=reference.canvas=view.canvas;actual.classicScale=reference.classicScale=view.classicScale;
        for(const auto& control:view.artwork)if(control.kind==OriginalControl::Kind::Text && (control.name.starts_with("player/") || control.name=="playerLocation")){
            // These are explicit SetClipRect calls in teampane.cpp. The
            // expanded name deliberately uses 20 pixels despite 15-pixel rows;
            // compact names keep the whole list surface's clip.
            auto expected=control;expected.clip=viewport;
            if(!compact){
                bool name=control.name.starts_with("player/");float y=control.bounds.y-(name?2:0);
                expected.intersectClip(name?glm::vec4(viewport.x,y,120,20):glm::vec4(viewport.x+columns[2]+2,y,float(columns[3]-columns[2]-2),15));
            }
            require(control.clip==expected.clip,"F6 name/location clip differs from original expanded/compact painter surface");
            // DrawString measures no layout box; preserve raw placement and
            // use only the independently reconstructed painter clip.
            expected.bounds.z=0;expected.singleLine=true;expected.clipCharacters=false;
            actual.artwork.push_back(control);reference.artwork.push_back(std::move(expected));++checks;
        }
        require(!actual.artwork.empty(),"F6 painter clipping fixture contains no name/location records");
        if(paint)paint(actual,reference,compact?"compact-player-clips":"expanded-player-clips");
    }
    printf("Original F6 painter clipping: %zu name/location records, expanded 120x20/sector-height clips, compact unclipped names and partial viewport rows passed; no world rendering\n",checks);
}
}
