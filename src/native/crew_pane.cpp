#include "crew_pane.hpp"
namespace Native {
const MenuView& CrewPane::view(glm::ivec2 size,int scale,const GameState& game,bool loadout,bool shown,int style) {
    scale=originalInterfaceScale(size,std::max(1,scale));auto board=Allegiance::CrewState(game.passenger,game.turret);
    glm::vec4 color=game.playerSide>=0 && game.playerSide<int(game.sideColors.size())?game.sideColors[game.playerSide]:glm::vec4(1);
    if(size_!=size || scale_!=scale || board_!=board || pilot_!=game.sourcePilot || color_!=color || loadout_!=loadout || shown_!=shown || style_!=style){
        style_=style;
        size_=size;scale_=scale;board_=board;pilot_=game.sourcePilot;color_=color;loadout_=loadout;shown_=shown;rebuild();
    }return view_;
}
void CrewPane::rebuild(){
    view_={};view_.original=true;view_.overlay=true;view_.classicScale=scale_;view_.canvas={size_.x/scale_,size_.y/scale_};
    if(!shown_ || board_==Allegiance::CrewBoardState::Pilot)return;
    auto& authored=authored_[std::clamp(style_,0,5)];
    if(!authored.overlays){
        authored.overlays=std::make_unique<UiOverlayLayout>(ui_,std::initializer_list<std::string_view>{"TurretOfImage","InTurretPaneImage","BoardTextImage"},style_);
        authored.turret=std::make_unique<HudProgram>(ui_,"TurretOfImage",style_);
        authored.board=std::make_unique<HudProgram>(ui_,"BoardTextImage",style_);
        authored.inTurret=std::make_unique<HudProgram>(ui_,"InTurretPaneImage",style_);
        for(auto* program:{authored.turret.get(),authored.board.get(),authored.inTurret.get()})for(auto& font:program->fonts())ui_.assets().font(font.symbol,font.height,font.bold);
    }
    HudValues values;values[HudNumber::BoardState]=board_==Allegiance::CrewBoardState::Turret?2:1;
    values[HudString::Player]=pilot_;values[HudColor::Side]=color_;values[HudColor::Player]=color_*.85f;values[HudColor::Player].a=.75f;
    auto add=[&](HudProgram& program,size_t binding){
        auto overlay=authored.overlays->get(binding,values,glm::vec2(view_.canvas));if(!overlay)return;
        auto at=glm::floor(overlay->on);
        for(auto& draw:program.evaluate(values)){
            auto name=binding==1?"InTurretPaneImage":binding==2?"BoardTextImage":draw.textBinding==HudString::Player?"PilotNameImage":"TurretHeader";
            if(auto control=authoredHudControl(program,draw,name,at))view_.artwork.push_back(std::move(*control));
        }
    };
    if(loadout_){
        if(board_==Allegiance::CrewBoardState::Turret)add(*authored.inTurret,1);
        add(*authored.board,2);
    }else if(board_==Allegiance::CrewBoardState::Turret)add(*authored.turret,0);
}
}
