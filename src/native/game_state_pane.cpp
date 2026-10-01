#include "game_state_pane.hpp"
#include "../ClientCore/AuthoredText.h"

namespace Native {
namespace {
bool contains(glm::vec4 b,glm::vec2 p){return p.x>=b.x && p.y>=b.y && p.x<b.x+b.z && p.y<b.y+b.w;}
}
void GameStatePane::reset(){data_={};view_={};active_=closing_=false;pressedButton_=0;dirty_=true;refreshAt_=0;feedback_.reset();}
void GameStatePane::toggle(Game& game){if(active()){close();return;}active_=true;closing_=false;entered_=uiTime();sync(game,true);dirty_=true;}
void GameStatePane::close(){if(active_){active_=false;closing_=true;entered_=uiTime();}pressedButton_=0;dirty_=true;}
void GameStatePane::sync(Game& game,bool force){
    if(!active_)return;auto modes=originalOverlayModes(game.state());if(overlayModes_!=modes){overlayModes_=modes;dirty_=true;}double now=uiTime();if(!force && now<refreshAt_)return;refreshAt_=now+.1;
    auto next=game.gameStateData();if(force || next!=data_){data_=std::move(next);dirty_=true;}
}
void GameStatePane::rebuild(){
    view_={};view_.original=true;view_.overlay=true;view_.classicScale=scale_;view_.canvas={size_.x/scale_,size_.y/scale_};
    if(!active_ && !closing_){dirty_=false;return;}
    auto font=assets_.font(titleFont_.font,titleFont_.fontHeight,titleFont_.fontBold);float rowHeight=font->height;
    auto extent=[&](const std::string& s){float w=0;for(unsigned char c:s)w+=font->glyphs[c].z;return w;};
    auto header=assets_.image("gamestateheaderbmp",true),footer=assets_.image("gamestatefooterbmp",true);
    std::vector<std::array<float,5>> widths(data_.tiles.size());
    float inner=0,height=header->height+1+footer->height;
    for(size_t i=0;i<data_.tiles.size();++i){auto& tile=data_.tiles[i];float gridWidth=0;
        for(int c=0;c<tile.columns;++c){float w=0;for(int r=0;r<tile.rows;++r)w=std::max(w,extent(tile.lines[r].columns[c]));widths[i][c]=w+4;gridWidth+=w+4;}
        auto title=assets_.image(tile.title,true);inner=std::max({inner,float(title->width),extent(tile.description)+6,gridWidth});
        // ColumnPane reparents the grid into the tile's outer column. It is
        // painted once, below the description's (3,1) border, at x=0.
        height+=1+title->height+2+rowHeight+tile.rows*rowHeight+1+1;
    }
    float width=std::max({float(header->width),float(footer->width),inner+2});
    inner=width-2;
    const auto* overlay=overlays_.get(0,{},glm::vec2(view_.canvas));if(!overlay || !(overlay->mask&(overlayModes_|ofGameState))){dirty_=false;return;}
    auto composition=original_.imageLayout("dialog","gameStateImage",{}, {},{{"GameStateImage",{width,height}}});
    auto panelBounds=authoredControlBounds(composition,"GameStateImage");glm::vec2 anchor=glm::floor(overlay->on);
    glm::vec2 at=glm::floor(anchor+glm::vec2(panelBounds));anchor+=at-anchor-glm::vec2(panelBounds);bounds_={at,width,height};
    auto rect=[&](glm::vec4 b,glm::vec4 color){OriginalControl c;c.kind=OriginalControl::Kind::Rectangle;c.bounds=b;c.color=color;view_.artwork.push_back(std::move(c));};
    auto image=[&](std::string name,glm::vec2 p){auto im=assets_.image(name,true);OriginalControl c;c.kind=OriginalControl::Kind::Image;c.name=std::move(name);c.bounds={p,im->width,im->height};c.image=im;view_.artwork.push_back(std::move(c));};
    auto text=[&](std::string name,std::string value,glm::vec2 p,glm::vec4 color){auto c=name=="gameStateClock"?clockFont_:titleFont_;c.name=std::move(name);c.text=std::move(value);auto face=assets_.font(c.font,c.fontHeight,c.fontBold);c.bounds={p,Allegiance::TextExtent(c.text,[&](unsigned char ch){return face->glyphs[ch].z;}),face->height};c.color=color;c.singleLine=true;view_.artwork.push_back(std::move(c));};
    image("gamestateheaderbmp",at);float y=at.y+header->height;rect({at.x,y++,width,1},glm::vec4(1));
    float infoY=y;
    for(size_t i=0;i<data_.tiles.size();++i){auto& tile=data_.tiles[i];auto title=assets_.image(tile.title,true);
        float tileHeight=1+title->height+2+rowHeight+tile.rows*rowHeight+1;
        rect({at.x+1,y,inner,tileHeight},{13.f/255,13.f/255,13.f/255,1});y+=1;
        image(tile.title,{at.x+1,y});y+=title->height;
        text("gameStateDescription/"+std::to_string(i),tile.description,{at.x+4,y+1},glm::vec4(1));y+=rowHeight+2;
        for(int r=0;r<tile.rows;++r){float x=at.x+1;
            for(int c=0;c<tile.columns;++c){text("gameStateCell/"+std::to_string(i)+"/"+std::to_string(r)+"/"+std::to_string(c),tile.lines[r].columns[c],{x,y+r*rowHeight},tile.lines[r].color);x+=widths[i][c];}
        }
        y+=tile.rows*rowHeight+1;rect({at.x+1,y++,inner,1},glm::vec4(1));
    }
    rect({at.x,infoY,1,y-infoY},glm::vec4(1));rect({at.x+inner+1,infoY,1,y-infoY},glm::vec4(1));
    image("gamestatefooterbmp",{at.x,y});text("gameStateClock",data_.clock,{at.x+50,y+3},glm::vec4(1));view_.artwork.back().clip=glm::vec4(at.x,y,width,footer->height);
    for(auto& control:composition)if(control.kind!=OriginalControl::Kind::Slot) {
        control.translate(anchor);
        if(control.kind==OriginalControl::Kind::Button){control.name="gameStateCloseButton";
            if(active_ && !overlay->undetectable)view_.items.push_back({control.visibleBounds(),{}, {},{MenuOp::Resume},true,false,control.name});}
        auto lo=glm::min(glm::vec2(bounds_),glm::vec2(control.bounds)),hi=glm::max(glm::vec2(bounds_)+glm::vec2(bounds_.z,bounds_.w),glm::vec2(control.bounds)+glm::vec2(control.bounds.z,control.bounds.w));bounds_={lo,hi-lo};
        view_.artwork.push_back(std::move(control));
    }
    if(pressedClose_ && pressedButton_)view_.pressed=0;
    view_.bindControlSounds();
    view_.slide=overlaySlide(*overlay,entered_,closing_);view_.slide->artworkEnd=view_.artwork.size();view_.slide->itemsEnd=view_.items.size();dirty_=false;
}
const MenuView& GameStatePane::view(glm::ivec2 size,int scale){
    scale=originalInterfaceScale(size,std::max(1,scale));if(size_!=size || scale_!=scale){size_=size;scale_=scale;dirty_=true;}
    if(closing_ && uiTime()-entered_>=overlays_.duration(0)){closing_=false;dirty_=true;}
    if(dirty_)rebuild();return view_;
}
bool GameStatePane::pointer(glm::ivec2 size,glm::vec2 pixel,int button,bool down){
    if(pressedButton_ && down)return true;
    if(!active_ || view_.items.empty() || (view_.slide && view_.slide->undetectable))return false;auto p=originalMenuLayout(size,scale_,view_.canvas).local(pixel);
    if(view_.slide)p-=menuSlideOffset(view_,size,uiTime());bool close=contains(view_.items.front().bounds,p);
    if(!down && pressedButton_ && (pressedButton_==button || pressedClose_ && (button==SDL_BUTTON_LEFT || button==SDL_BUTTON_RIGHT))){bool activate=pressedClose_ && close && button==SDL_BUTTON_LEFT;pressedButton_=0;dirty_=true;if(activate){feedback_.click(view_.items.front());this->close();}return true;}
    if(!contains(bounds_,p))return false;
    if(down){pressedButton_=button;pressedClose_=(button==SDL_BUTTON_LEFT || button==SDL_BUTTON_RIGHT) && close;view_.pressed=pressedClose_?0:-1;dirty_=true;}return true;
}
void GameStatePane::hover(glm::ivec2 size,glm::vec2 pixel){
    auto hit=active_?menuHit(view_,size,1,pixel):std::optional<size_t>{};
    if(auto captured=view_.capturedButton()){feedback_.capturedHover(*captured,hit && *hit==size_t(view_.pressed));return;}
    feedback_.hover(hit?&view_.items[*hit]:nullptr);
}
bool GameStatePane::key(SDL_Keycode key){if(active_ && key==SDLK_ESCAPE){close();return true;}return false;}
}
