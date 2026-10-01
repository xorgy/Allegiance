#pragma once
#include "authored_hosts.hpp"
#include "../console_pane.hpp"
#include "../../ClientCore/AuthoredText.h"

namespace NativeTest {
// Exercises display adapters with real MDL, without constructing Game, a
// model preview or a world scene. The AST-based adapter supplies the reference.
inline void checkCompiledOverlays(Native::Assets& installed,const std::function<void(const Native::MenuView&,const Native::MenuView&,std::string_view)>& pixels={}){
    using namespace Native;
    AuthoredAssets fixture(installed.root());
    fixture.change("dialog.mdl",{{"\nTurretOfImage =","\nOriginalTurretOfImage ="},{"\nInTurretPaneImage =","\nOriginalInTurretPaneImage ="},{"\nBoardTextImage =","\nOriginalBoardTextImage ="},
        {"\nLastOrderImage =","\nOriginalLastOrderImage ="},{"\nCriticalTipImageTranslated =","\nOriginalCriticalTipImageTranslated ="},{"\nVoteTextImageTranslated =","\nOriginalVoteTextImageTranslated ="},
        {"\nTipTextImage =","\nOriginalTipTextImage ="},{"\nTipTextImage1 =","\nOriginalTipTextImage1 ="},{"\nTipTextImage2 =","\nOriginalTipTextImage2 ="}});
    {
        std::ofstream out(fixture.root/"dialog.mdl",std::ios::app);
        out<<R"(
AdapterFont = Win32Font("Verdana",18,true,0);
AdapterCrewPane = PaneImage(BlackPane(ImagePane(ImportImage("inventorylinebkgndbmp",true),[(StringPane(GetName(Me,OnEveryFrame),Color(.2,.4,.6),Point(90,19),JustifyRight,AdapterFont),Point(14,6))])),false,true);
AdapterCrewImage = BlendColorImage(Color(Multiply(StyleHUD,.05),.1,.2,.5),ClipImage(AdapterCrewPane,Rect(10,0,110,18)));
TurretOfImage = SwitchImage(BoardState,emptyImage,[(2,AdapterCrewImage)]);
BoardTextImage = SwitchImage(BoardState,emptyImage,[(1,AdapterCrewImage),(2,AdapterCrewImage)]);
InTurretPaneImage = BlendColorImage(Color(Multiply(StyleHUD,.05),.1,.2,.5),OriginalInTurretPaneImage);
LastOrderImage = BlendColorImage(Color(.05,.1,.2,.5),ClipImage(OriginalLastOrderImage,Rect(0,-50,200,0)));
CriticalTipImageTranslated = BlendColorImage(Color(.05,.1,.2,.5),OriginalCriticalTipImageTranslated);
VoteTextImageTranslated = BlendColorImage(Color(.05,.1,.2,.5),OriginalVoteTextImageTranslated);
TipTextImage = BlendColorImage(Color(.05,.1,.2,.5),OriginalTipTextImage);
TipTextImage1 = BlendColorImage(Color(.05,.1,.2,.5),OriginalTipTextImage1);
TipTextImage2 = BlendColorImage(Color(.05,.1,.2,.5),OriginalTipTextImage2);
)";
        require(bool(out),"alternate overlay MDL write failed");
    }
    Assets assets(fixture.root);OriginalUi source(assets);CrewPane crew(assets);ConsolePane console(assets);
    size_t cases=0;
    auto compare=[&](const MenuView& actual,const MenuView& expected){
        require(actual.artwork.size()==expected.artwork.size(),"compiled display adapter loses painter records");
        for(size_t i=0;i<actual.artwork.size();++i){auto a=actual.artwork[i];const auto& e=expected.artwork[i];
            require(a.kind==e.kind && a.text==e.text && a.font==e.font && a.fontHeight==e.fontHeight && a.fontBold==e.fontBold && a.justify==e.justify && a.singleLine==e.singleLine && a.textMode==e.textMode && a.additive==e.additive && a.image==e.image,"compiled display adapter loses type/font/text/blending metadata");
            require(glm::all(glm::lessThanEqual(glm::abs(a.color-e.color),glm::vec4(.001f))),"compiled display adapter loses authored fill color or HUD style");
            if(a.kind==OriginalControl::Kind::Text && a.textMode==OriginalControl::TextMode::Image){auto font=assets.font(a.font,a.fontHeight,a.fontBold);a.bounds.w=font->height*Allegiance::ImageTextLines(a.text,int(a.bounds.z),[&](unsigned char ch){return font->glyphs[ch].z;});}
            auto av=a.visibleBounds(),ev=e.visibleBounds();
            // Invisible lines retain their painter records and participate in
            // declared group bounds, but have no individual pixel footprint.
            if(((av.z>0 && av.w>0) || (ev.z>0 && ev.w>0)) && glm::any(glm::greaterThan(glm::abs(av-ev),glm::vec4(.001f)))){
                fprintf(stderr,"Display adapter case %zu item %zu kind %d: actual=(%.2f %.2f %.2f %.2f) expected=(%.2f %.2f %.2f %.2f)\n",cases,i,int(a.kind),av.x,av.y,av.z,av.w,ev.x,ev.y,ev.z,ev.w);
                throw std::runtime_error("compiled display adapter loses clipping or placement");
            }
            if(a.kind==OriginalControl::Kind::Image && e.bounds.z>0 && e.bounds.w>0){
                auto size=glm::vec2(e.bounds.z,e.bounds.w),uvSize=glm::vec2(e.uv.z-e.uv.x,e.uv.w-e.uv.y);
                auto first=glm::vec2(e.uv)+(glm::vec2(ev)-glm::vec2(e.bounds))/size*uvSize,last=first+glm::vec2(ev.z,ev.w)/size*uvSize;
                require(glm::all(glm::lessThanEqual(glm::abs(a.uv-glm::vec4(first,last)),glm::vec4(.001f))),"compiled display adapter loses clipped texture coordinates");
            }
        }
        ++cases;
    };
    auto reference=[&](std::string_view root,const HudOverlay& overlay,const UiNumbers& numbers,const UiStrings& strings,MenuView& view){
        for(auto c:source.imageLayout("dialog",std::string(root),numbers,strings)){c.translate(glm::floor(overlay.on));view.artwork.push_back(std::move(c));}
    };
    for(int style=0;style<6;++style){
        UiOverlayLayout positions(source,{"TurretOfImage","InTurretPaneImage","BoardTextImage"},style);
        for(int scale:{1,2})for(int role:{0,1,2})for(bool loadout:{false,true})for(bool shown:{false,true}){
            glm::ivec2 size{1000*scale,700*scale};GameState state;state.passenger=role!=0;state.turret=role==2?0:-1;state.sourcePilot="A long pilot name that clips within its authored pane";
            auto& actual=crew.view(size,scale,state,loadout,shown,style);MenuView expected;expected.original=expected.overlay=true;expected.classicScale=scale;expected.canvas=size/scale;
            HudValues values;values[HudNumber::BoardState]=float(role);UiNumbers numbers{{"StyleHUD",style},{"BoardState",role}};UiStrings strings{{"GetName",state.sourcePilot}};
            auto append=[&](size_t binding,const char* root){if(auto overlay=positions.get(binding,values,glm::vec2(expected.canvas)))reference(root,*overlay,numbers,strings,expected);};
            if(shown && role){if(loadout){if(role==2)append(1,"InTurretPaneImage");append(2,"BoardTextImage");}else if(role==2)append(0,"TurretOfImage");}
            compare(actual,expected);
            auto cached=actual.artwork.data();require(crew.view(size,scale,state,loadout,shown,style).artwork.data()==cached,"unchanged crew display reconstructs controls");
            if(pixels && scale==1 && shown && role==2)pixels(actual,expected,"crew-style-"+std::to_string(style)+(loadout?"-loadout":"-flight"));
        }
    }
    constexpr const char* roots[]={"LastOrderImage","CriticalTipImageTranslated","VoteTextImageTranslated","TipTextImage","TipTextImage1","TipTextImage2"};
    UiOverlayLayout positions(source,{roots[0],roots[1],roots[2],roots[3],roots[4],roots[5]});
    std::array<size_t,6> order{0,1,2,3,4,5};std::sort(order.begin(),order.end(),[&](size_t a,size_t b){return positions.index(a)>positions.index(b);});
    GameState state;state.playerSide=0;state.sideColors[0]=glm::vec4(1);state.commands[0].text="goto Garrison";state.commands[1].text="defend Scout";state.ballot.text="A ballot awaiting a response";
    uint64_t serial=0;auto post=[&](const char* text,bool warning){GameMessage message;message.serial=++serial;message.text=text;message.tip=true;message.warning=warning;message.displayTime=10;state.messages.push_back(std::move(message));};
    post("First ordinary",false);post("Second ordinary",false);post("Third ordinary",false);post("One critical",true);
    for(int scale:{1,2})for(bool loadout:{false,true})for(bool shown:{false,true})for(int lines:{3,9}){
        glm::ivec2 size{1000*scale,700*scale};auto& actual=console.view(size,scale,state,lines,loadout,shown,10);
        MenuView expected;expected.original=expected.overlay=true;expected.classicScale=scale;expected.canvas=size/scale;
        HudValues values;values[HudNumber::ChatLines]=loadout?std::min(lines,6):lines;
        UiNumbers numbers{{"NumChatLines",values[HudNumber::ChatLines]}};UiStrings strings{{"QueuedOrder",state.commands[0].text},{"AcceptedOrder",state.commands[1].text},{"CurrentVote",state.ballot.text},{"CriticalTipText0","One critical"},{"CriticalTipText1",""},{"CriticalTipText2",""},{"CriticalTipText3",""},{"TipText","Third ordinary"},{"TipText1","Second ordinary"},{"TipText2","First ordinary"}};
        if(shown)for(auto binding:order)if(auto overlay=positions.get(binding,values,glm::vec2(expected.canvas)))reference(roots[binding],*overlay,numbers,strings,expected);
        compare(actual,expected);auto cached=actual.artwork.data();require(console.view(size,scale,state,lines,loadout,shown,10.1).artwork.data()==cached,"unchanged console display reconstructs controls");
        if(pixels && scale==1 && shown)pixels(actual,expected,"console-"+std::to_string(lines)+(loadout?"-loadout":"-flight"));
    }
    printf("Compiled display adapters: %zu independent crew/console comparisons, six HUD styles, pilot/observer/gunner, two integer scales, hidden/loadout states, clipped font/fill/UV metadata and unchanged-view caching passed without a game\n",cases);
}
}
