#include "player_pane.hpp"
#include "../ClientCore/LobbyActions.h"
#include "../ClientCore/AuthoredText.h"
#include "classic_scrollbar.hpp"
#include <cctype>
#include <cstdlib>

namespace Native {
namespace {
bool contains(glm::vec4 b,glm::vec2 p){return p.x>=b.x && p.y>=b.y && p.x<b.x+b.z && p.y<b.y+b.w;}
}
PlayerPane::PlayerPane(Assets& assets):assets_(assets),original_(assets),overlays_(original_,{"TeamPaneImage"}) {
    authored_[0]=original_.layout("teampane","teamPane",{}, {},true);authored_[1]=original_.layout("teampane","expandedTeamPane",{}, {},true);
    for(int expanded=0;expanded<2;++expanded){auto& layout=layouts_[expanded];const std::string prefix=expanded?"eTeamPane":"teamPane";
        layout.players=authoredControlBounds(authored_[expanded],prefix+"PlayerListPane");layout.bar=authoredScrollBarBounds(authored_[expanded],prefix+"PlayerListPane");
        layout.columns=original_.integers("teampane",prefix+"PlayerColumns");if(layout.columns.size()<(expanded?8:6))throw std::runtime_error("Original team painter column count differs");
        if(expanded){layout.teams=authoredControlBounds(authored_[expanded],prefix+"TeamListPane");layout.selected=authoredControlBounds(authored_[expanded],prefix+"SelTeamListPane");layout.icons=authoredControlBounds(authored_[expanded],prefix+"IconListPane");}
        for(const auto& control:authored_[expanded]){if(control.name==prefix+"TeamComboPane")layout.combo=control;else if(control.name==prefix+"WingComboPane")layout.wing=control;}
    }
    reset();
}
void PlayerPane::reset(){mode_=Allegiance::PlayerPaneMode::Hidden;selected_=team_=-1;sort_=-1;list_={};teamOffset_=0;teamViewport_={};data_={};rows_.clear();popup_.clear();comboSelections_={};pressed_.reset();view_={};closing_=drag_=focus_=false;dirty_=true;refreshAt_=0;feedback_.reset();}
void PlayerPane::close(){if(active()){mode_=Allegiance::PlayerPaneMode::Hidden;closing_=true;entered_=uiTime();}popup_.clear();pressed_.reset();drag_=false;dirty_=true;}
void PlayerPane::toggle(Game& game,bool compact){
    auto next=Allegiance::TogglePlayerPane(mode_,compact);if(next==Allegiance::PlayerPaneMode::Hidden){close();return;}
    bool opening=!active();mode_=next;closing_=false;popup_.clear();pressed_.reset();drag_=false;
    if(opening)entered_=uiTime();
    sync(game,true);dirty_=true;
}
const PlayerPaneRow* PlayerPane::selection()const {for(auto* row:rows_)if(row->id==selected_)return row;return nullptr;}
void PlayerPane::sort(){
    rows_.clear();for(const auto& row:data_.rows)if(row.side==team_ || (team_==data_.side && row.requesting))rows_.push_back(&row);
    std::stable_sort(rows_.begin(),rows_.end(),[&](auto* a,auto* b){return Allegiance::PlayerPaneBefore(*a,*b,sort_);});
    list_.selection=-1;for(size_t i=0;i<rows_.size();++i)if(rows_[i]->id==selected_)list_.selection=int(i);
    if(list_.selection<0){bool lost=selected_>=0;selected_=-1;if(popupKind_==PopupKind::Context)popup_.clear();if(lost){pressed_.reset();drag_=false;}}
}
void PlayerPane::sync(Game& game,bool force){
    if(!active())return;auto modes=originalOverlayModes(game.state());if(overlayModes_!=modes){overlayModes_=modes;dirty_=true;}double now=uiTime();if(!force && now<refreshAt_)return;refreshAt_=now+.1;
    auto next=game.playerPaneData();if(next==data_)return;
    bool oldCommander=data_.commander;data_=std::move(next);
    if(team_<-2 || !std::any_of(data_.teams.begin(),data_.teams.end(),[&](auto& t){return t.id==team_;}))team_=data_.side;
    sort();if(!popup_.empty() && popupKind_==PopupKind::Context)context();
    if(oldCommander!=data_.commander)pressed_.reset();dirty_=true;
}
glm::vec2 PlayerPane::local(glm::ivec2 size,glm::vec2 pixel)const {
    auto point=originalMenuLayout(size,scale_,view_.canvas).local(pixel);
    if(view_.slide)point-=menuSlideOffset(view_,size,uiTime());return point;
}
void PlayerPane::text(std::string name,glm::vec2 at,std::string value,glm::vec4 clip,const char* font,glm::vec4 color){
    OriginalControl c;c.kind=OriginalControl::Kind::Text;c.name=std::move(name);c.bounds={at,clip.z,15};c.clip=clip;c.font=font;c.color=color;c.text=std::move(value);c.surfaceText();view_.artwork.push_back(std::move(c));
}
void PlayerPane::image(std::string name,glm::vec2 at,bool keyed,std::optional<glm::vec4> clip){
    auto im=assets_.image(name,keyed);OriginalControl c;c.kind=OriginalControl::Kind::Image;c.name=std::move(name);c.bounds={at,im->width,im->height};c.image=im;c.clip=clip;view_.artwork.push_back(std::move(c));
}
void PlayerPane::rectangle(glm::vec4 b,glm::vec4 color,std::optional<glm::vec4> clip){OriginalControl c;c.kind=OriginalControl::Kind::Rectangle;c.bounds=b;c.color=color;c.clip=clip;view_.artwork.push_back(std::move(c));}
void PlayerPane::hit(glm::vec4 b,MenuAction a,bool enabled,bool selected,std::string control){view_.items.push_back({b,{}, {},a,enabled,selected,std::move(control)});}
void PlayerPane::scrollbar(){
    appendClassicScrollbar(view_,assets_,list_,0);
}
void PlayerPane::context(){
    std::optional<MenuAction> previous;
    if(popupKind_==PopupKind::Context && popupSelected_>=0 && size_t(popupSelected_)<popup_.size())previous=popup_[popupSelected_].action;
    popup_.clear();auto* row=selection();if(!row)return;popupKind_=PopupKind::Context;
    if(row->side==data_.side && !row->human && data_.commander && !data_.closed) {
        bool carrier=row->type==9;if(!carrier || (row->current && !row->docked))
            popup_.push_back({carrier?"Dock/Flee ":row->docked && row->stayDocked?"Launch  ":"Dock    ",'D',{MenuOp::DroneStandby,row->id,row->docked && row->stayDocked?0:1}});
    }
    if(row->human && !row->controlled)popup_.push_back({"Chat       ",'C',{MenuOp::Unit,row->id}});
    if(row->requesting && data_.commander && !data_.closed){if(data_.acceptJoiners)popup_.push_back({"Accept       ",'A',{MenuOp::LobbyAction,int(LobbyAction::Accept)}});popup_.push_back({"Reject       ",'R',{MenuOp::LobbyAction,int(LobbyAction::Reject)}});}
    popupSelected_=0;
    if(previous)for(size_t i=0;i<popup_.size();++i)if(popup_[i].action.op==previous->op && popup_[i].action.value==previous->value){popupSelected_=int(i);break;}
}
void PlayerPane::popupLayout(){
    if(popup_.empty())return;const auto& layout=layouts_[expandedLayout_];auto style=popupKind_==PopupKind::Wings?layout.wing:popupKind_==PopupKind::Teams?layout.combo:OriginalControl{};
    if(style.font.empty())style.font="smallFont";auto font=assets_.font(style.font,style.fontHeight,style.fontBold);float rowHeight=std::max(11.f,float(font->height))+2,labelWidth=0,keyWidth=font->glyphs[' '].z;
    auto measure=[&](std::string_view text){return Allegiance::TextExtent(text,[&](unsigned char ch){return font->glyphs[ch].z;});};
    for(const auto& row:popup_){labelWidth=std::max(labelWidth,measure(row.text));if(row.key)keyWidth=std::max(keyWidth,font->glyphs[uint8_t(row.key)].z);}
    float width=11+labelWidth+2+keyWidth+9+2,height=float(popup_.size())*rowHeight;glm::vec2 at=popupAt_;at.y-=height+3;
    at=glm::floor(glm::clamp(at,glm::vec2(0),glm::max(glm::vec2(view_.canvas)-glm::vec2(width+3,height+3),glm::vec2(0))));popupBounds_={at,width+3,height+3};
    rectangle(popupBounds_,{.5f,.5f,.5f,1});rectangle({at.x+1,at.y+height+1,width+2,1},{0,0,0,1});rectangle({at.x+width+1,at.y+1,1,height+2},{0,0,0,1});
    rectangle({at,width+2,1},glm::vec4(1));rectangle({at,1,height+2},glm::vec4(1));
    auto clip=glm::vec4(at+glm::vec2(1),width,height);
    auto label=[&](std::string name,glm::vec2 point,std::string value,float extent){auto control=style;control.kind=OriginalControl::Kind::Text;control.name=std::move(name);control.image.reset();control.text=std::move(value);control.bounds={point,extent,rowHeight-2};control.clip=clip;control.surfaceText();view_.artwork.push_back(std::move(control));};
    for(size_t i=0;i<popup_.size();++i){glm::vec4 b{at.x+1,at.y+1+i*rowHeight,width,rowHeight};bool selected=int(i)==popupSelected_;
        glm::vec4 color=selected?glm::vec4(197.f/255,0,41.f/255,1):glm::vec4(33.f/255,20.f/255,30.f/255,1);
        if(popupKind_==PopupKind::Teams){auto side=Allegiance::PlayerSideColor(popup_[i].action.value);color={side[0],side[1],side[2],1};if(selected)color*=glm::vec4(1.5f,1.5f,1.5f,1);}
        rectangle(b,color,clip);label("playerPopup/"+std::to_string(i),{b.x+12,b.y+1},popup_[i].text,labelWidth);
        if(popup_[i].key)label("playerPopupKey",{b.x+12+labelWidth+2+keyWidth-font->glyphs[uint8_t(popup_[i].key)].z,b.y+1},std::string(1,popup_[i].key),keyWidth);
        hit(b,{MenuOp::PopupItem,int(i)},true,selected);
        view_.items.back().control="playerPopup/"+std::to_string(i);
        view_.items.back().clickSound=style.selectSound;
    }
}
void PlayerPane::rebuild(){
    view_={};view_.original=true;view_.overlay=true;view_.classicScale=scale_;view_.canvas={size_.x/scale_,size_.y/scale_};
    if(!active() && !closing_){dirty_=false;return;}
    bool expanded=active()?mode_==Allegiance::PlayerPaneMode::Expanded:expandedLayout_;expandedLayout_=expanded;auto& layout=layouts_[expanded];
    auto& authored=authored_[expanded];float height=0,width=0;for(auto& c:authored){height=std::max(height,c.bounds.y+c.bounds.w);width=std::max(width,c.bounds.x+c.bounds.z);}
    const auto* overlay=overlays_.get(0,{},glm::vec2(view_.canvas));
    auto paneFlag=expanded?ofExpandedTeam:ofTeam;if(!overlay || !(overlay->mask&(overlayModes_|paneFlag))){dirty_=false;return;}
    glm::vec2 at=glm::floor(overlay->on)-glm::vec2(0,height);
    paneBounds_={at,width,height};
    auto* selected=selection();bool other=selected && !selected->controlled,own=selected && selected->side==data_.side;
    bool requesting=selected && selected->requesting;
    int ownLeader=-1;for(auto& row:data_.rows)if(row.side==data_.side && row.commander)ownLeader=row.id;
    for(auto c:authored){
        auto name=c.name;
        if(name.ends_with("PlayerScrollBar") || name.ends_with("PlayerListPane") || name.ends_with("TeamListPane") || name.ends_with("SelTeamListPane") || name.ends_with("IconListPane"))continue;
        if(name.ends_with("AcceptButtonPane") && !(requesting && data_.commander && team_==data_.side))continue;
        if(name.ends_with("TakeMeToButtonPane") && requesting)continue;
        if(name.ends_with("BootButtonPane") && !(data_.commander && team_==data_.side))continue;
        if(name.ends_with("StopDonateButtonPane") && !(data_.commander && team_==data_.side && other && own))continue;
        c.translate(at);
        view_.artwork.push_back(c);
        if(name.ends_with("WingComboPane") && c.widget!=OriginalControl::Widget::ImageCombo){int wing=-1;for(const auto& row:data_.rows)if(row.id==data_.self)wing=row.wing;view_.artwork.push_back(widgetText(c,std::string(Game::wingName(wing))));}
        if(c.kind!=OriginalControl::Kind::Button || c.bounds.z<=0)continue;
        if(name.ends_with("CloseButtonPane"))hit(c.visibleBounds(),{MenuOp::Resume},true,false,name);
        else if(name.ends_with("ExpandButtonPane") || name.ends_with("CollapseButtonPane"))hit(c.visibleBounds(),{MenuOp::Navigate},true,false,name);
        else if(name.ends_with("TakeMeToButtonPane"))hit(c.visibleBounds(),{MenuOp::TakeMeTo,selected_},other,false,name);
        else if(name.ends_with("AcceptButtonPane"))hit(c.visibleBounds(),{MenuOp::LobbyAction,int(LobbyAction::Accept)},requesting && data_.acceptJoiners && !data_.closed,false,name);
        else if(name.ends_with("BootButtonPane"))hit(c.visibleBounds(),{MenuOp::ConfirmBoot,selected_},!data_.closed && (requesting || (selected && selected->human && Allegiance::CanBootTeammate(data_.commander,selected->controlled,own,data_.teamSettingsLocked))),false,name);
        else if(name.ends_with("AutoDonateButtonPane"))hit(c.visibleBounds(),{MenuOp::AutoDonate,selected_,data_.money},other && own && selected->human && (data_.commander || (data_.donating<0 && selected->id==ownLeader)) && !data_.training && !data_.closed,false,name);
        else if(name.ends_with("StopDonateButtonPane"))hit(c.visibleBounds(),{MenuOp::AutoDonate,selected_,-1},other && selected->human && !data_.training && !data_.closed,false,name);
        else if(name.ends_with("DonateButtonPane"))hit(c.visibleBounds(),{MenuOp::Donate,selected_,0},other && (own || selected->allied) && selected->human && data_.money>0 && !data_.training && !data_.closed,false,name);
        else if(name.ends_with("WingComboPane"))hit(c.visibleBounds(),{MenuOp::Wing,0,0},!data_.closed,false,name);
        else if(name.ends_with("ComboButtonPane"))hit(c.visibleBounds(),{MenuOp::SelectTeam,0,0},true,false,name);
        else if(auto pos=name.find("PlayerListHeader");pos!=std::string::npos){int column=std::atoi(name.c_str()+pos+16);hit(c.visibleBounds(),{MenuOp::Order,column},true,column==sort_,name);}
    }
    auto chosen=std::find_if(data_.teams.begin(),data_.teams.end(),[&](auto& t){return t.id==team_;});
    if(!expanded){auto face=layout.combo;face.translate(at);auto label=widgetText(face,chosen==data_.teams.end()?"":chosen->id<0?chosen->name:chosen->name+"  ("+std::to_string(chosen->pilots)+")");label.name="playerTeam";
        if(face.widget==OriginalControl::Widget::ColorCombo){auto color=Allegiance::PlayerSideColor(chosen==data_.teams.end()?-1:chosen->id);rectangle(label.bounds,{color[0],color[1],color[2],1});}
        view_.artwork.push_back(std::move(label));hit(face.visibleBounds(),{MenuOp::SelectTeam,0,0},true,false,"playerTeamCombo");view_.items.back().hoverSound=face.hoverSound;}
    else {
        auto teamRow=[&](const PlayerPaneTeam& team,glm::vec2 p,bool single){
            glm::vec4 clip{p,170,single?20:17};if(!single){float bottom=std::min(p.y+17,teamViewport_.y+teamViewport_.w);clip.y=std::max(p.y,teamViewport_.y);clip.w=bottom-clip.y;}
            constexpr const char* tabs[]={"btnteamyellowbmp","btnteambluebmp","btnteampurplebmp","btnteamgreenbmp","btnteamredbmp","btnteamwhitebmp"};
            image(team.id>=0 && team.id<6?tabs[team.id]:"btnteamlobbybmp",p,true,clip);std::string name=team.name;
            if(team.alliance>=0){constexpr glm::vec3 colors[]={{0,1,0},{1,.5f,0},{1,0,0}};rectangle({p,9,20},{colors[team.alliance%3]*.75f,1},clip);name=std::to_string(team.alliance+1)+" "+name;}
            text("playerTeam",p+glm::vec2(1,2),name,{p.x+1,clip.y,104,clip.w});if(team.id>=0)text("playerTeamCount",p+glm::vec2(115,2),std::to_string(team.pilots)+"|0",clip);
            if(!single && team.id==team_)image("arrowbmp",p+glm::vec2(157,0),true,clip);if(!single)hit(clip,{MenuOp::SelectTeam,team.id,1});};
        if(chosen!=data_.teams.end()){teamRow(*chosen,at+glm::vec2(layout.selected),true);if(!chosen->icon.empty())image(chosen->icon,at+glm::vec2(layout.icons));if(!chosen->civilizationIcon.empty())image(chosen->civilizationIcon,at+glm::vec2(layout.icons)+glm::vec2(2));}
        // Original six-faction list uses seven 17px rows, including NOAT.
        teamViewport_={at+glm::vec2(layout.teams),std::max(170.f,layout.teams.z),layout.teams.w};teamOffset_=std::clamp(teamOffset_,0.f,std::max(0.f,float(data_.teams.size())*17-teamViewport_.w));
        for(size_t i=0;i<data_.teams.size();++i){float y=teamViewport_.y+i*17-teamOffset_;if(y>=teamViewport_.y+teamViewport_.w)break;if(y+17>teamViewport_.y)teamRow(data_.teams[i],{teamViewport_.x,y},false);}
    }
    // ListPane grows from the MDL minimum to the item painter's width.
    float painterWidth=expanded?float(layout.columns[7]-15):160.f;
    list_.configure(int(rows_.size()),expanded?15:28,{at+glm::vec2(layout.players),std::max(layout.players.z,painterWidth),layout.players.w});
    list_.bar={at+glm::vec2(layout.bar),layout.bar.z,layout.bar.w};auto clip=list_.viewport;
    for(int i=int(list_.offset/list_.row);i<int(rows_.size());++i){float y=clip.y+i*list_.row-list_.offset;if(y>=clip.y+clip.w)break;auto& row=*rows_[i];glm::vec2 p{clip.x,y};
        glm::vec4 b{p,clip.z,list_.row};if(row.id==selected_)rectangle({p,list_.viewport.z,list_.row},{1,0,0,1},clip);
        auto h=b;h.y=std::max(h.y,clip.y);h.w=std::min(y+list_.row,clip.y+clip.w)-h.y;hit(h,{MenuOp::RosterSelect,row.id});
        if(row.human)image(std::string(Allegiance::PlayerPaneDonationIcon(row.commander,row.id,data_.donating,row.autoDonate,data_.self)),p,true,clip);
        auto nameColor=row.docked?glm::vec4(.617f,0,.156f,1):glm::vec4(1);auto font=row.docked?"smallBoldFont":"smallFont";
        auto fieldClip=[&](float x,float width,float height){
            auto lo=glm::max(glm::vec2(clip),p+glm::vec2(x,0)),hi=glm::min(glm::vec2(clip)+glm::vec2(clip.z,clip.w),p+glm::vec2(x+width,height));
            return glm::vec4(lo,glm::max(hi-lo,glm::vec2(0)));
        };
        text("player/"+std::to_string(row.id),p+glm::vec2(17,2),row.name,expanded?fieldClip(0,120,20):clip,font,nameColor);
        if(expanded){
            if(row.human)text("playerRank",p+glm::vec2(layout.columns[0]+2,1)," 0",clip,font,nameColor);
            text("playerKills",p+glm::vec2(layout.columns[1]+2,1),std::to_string(row.kills),clip,font,nameColor);text("playerDeaths",p+glm::vec2(layout.columns[1]+27,1),std::to_string(row.deaths),clip,font,nameColor);
            text("playerLocation",p+glm::vec2(layout.columns[2]+2,0),row.location,fieldClip(layout.columns[2]+2,float(layout.columns[3]-layout.columns[2]-2),15),font,nameColor);
            if(!row.icon.empty())image(row.icon,p+glm::vec2(layout.columns[3]+10,-1),true,clip);
            if(row.side==data_.side && row.wing>=0)text("playerWing",p+glm::vec2(layout.columns[4]+2,1),std::string(Game::wingName(row.wing)),clip,font,nameColor);
            if(row.human && row.side==data_.side)text("playerCredits",p+glm::vec2(layout.columns[5]+2,1),std::to_string(row.money),clip,font,nameColor);
            if(row.command>0 && row.command<10)image(std::string(Allegiance::PlayerPaneOrderIcons[row.command]),p+glm::vec2(layout.columns[6]+2,-1),true,clip);
        }else {float brightness=row.current || row.side==data_.side?1.f:.75f;auto color=glm::vec4(brightness,brightness,brightness,1);text("playerKills",p+glm::vec2(120,2),std::to_string(row.kills),clip,"smallFont",color);text("playerDeaths",p+glm::vec2(143,2),std::to_string(row.deaths),clip,"smallFont",color);
            if(row.human && ((row.side==data_.side && row.money>=0) || (row.side!=data_.side && row.allied)))text("playerCredits",p+glm::vec2(19,14),Allegiance::PlayerPaneCompactAccount(data_.commander,row.commander,row.docked,row.dockedSeconds,row.money),clip,"smallFont",color);}
    }
    scrollbar();view_.slide=overlaySlide(*overlay,entered_,closing_);view_.slide->artworkEnd=view_.artwork.size();view_.slide->itemsEnd=view_.items.size();
    if(!active())view_.items.clear();popupLayout();
    view_.bindControlSounds();
    if(pressed_)for(size_t i=0;i<view_.items.size();++i)if(view_.items[i].action.op==pressed_->op && view_.items[i].action.value==pressed_->value && (view_.items[i].action.delta==pressed_->delta || pressedButton_==SDL_BUTTON_RIGHT && pressed_->op==MenuOp::Donate)){view_.pressed=int(i);break;}
    dirty_=false;
}
const MenuView& PlayerPane::view(glm::ivec2 size,int scale){
    scale=originalInterfaceScale(size,std::max(1,scale));if(size_!=size || scale_!=scale){size_=size;scale_=scale;dirty_=true;}
    if(closing_ && uiTime()-entered_>=overlays_.duration(0)){closing_=false;dirty_=true;}
    if(dirty_)rebuild();return view_;
}
FrontResult PlayerPane::activate(MenuAction a,bool controlEvent){
    if(a.op==MenuOp::Donate){
        auto item=std::find_if(view_.items.begin(),view_.items.end(),[&](const auto& i){return i.action.op==a.op && i.action.value==a.value;});
        if(item==view_.items.end() || !item->enabled || item->blocked || data_.money<=0)return {};
        // TeamPane's right-event handler explicitly supplies this sound; the
        // left repeat events use the ordinary ButtonPane sound sink.
        if(a.delta)feedback_.emit(positiveButtonClickSound);else if(controlEvent)feedback_.click(*item);
    }else if(controlEvent)for(const auto& item:view_.items)if(item.action.op==a.op && item.action.value==a.value && item.action.delta==a.delta){if(!item.enabled || item.blocked)return {};feedback_.click(item);break;}
    switch(a.op){
    case MenuOp::Resume:close();break;
    case MenuOp::Navigate:mode_=mode_==Allegiance::PlayerPaneMode::Compact?Allegiance::PlayerPaneMode::Expanded:Allegiance::PlayerPaneMode::Compact;sort();break;
    case MenuOp::RosterSelect:selected_=selected_==a.value?-1:a.value;focus_=true;sort();break;
    case MenuOp::Order:sort_=a.value;sort();break;
    case MenuOp::SelectTeam:
        if(a.delta==1){team_=a.value;selected_=-1;list_.offset=0;popup_.clear();sort();}
        else {popupKind_=PopupKind::Teams;popup_.clear();for(auto& team:data_.teams)popup_.push_back({team.id<0?team.name:team.name+"  ("+std::to_string(team.pilots)+")",0,{MenuOp::SelectTeam,team.id,1}});selectPopup(comboSelections_[mode_==Allegiance::PlayerPaneMode::Expanded][0]);popupAt_=pointer_;}
        break;
    case MenuOp::Wing:if(a.delta==0){popupKind_=PopupKind::Wings;popup_.clear();for(int i=0;i<10;++i)popup_.push_back({std::string(Game::wingName(i)),0,{MenuOp::Wing,i,1}});selectPopup(comboSelections_[mode_==Allegiance::PlayerPaneMode::Expanded][1]);popupAt_=pointer_;}
        else {popup_.clear();dirty_=true;return {FrontEffect::Wing,data_.self,a.value};}break;
    case MenuOp::Unit:popup_.clear();dirty_=true;return {FrontEffect::SelectUnit,a.value};
    case MenuOp::TakeMeTo:close();return {FrontEffect::TakeMeTo,a.value};
    case MenuOp::Donate:dirty_=true;return {FrontEffect::Donate,a.value,Allegiance::PlayerPaneDonation(data_.money,a.delta!=0)};
    case MenuOp::AutoDonate:dirty_=true;return {FrontEffect::AutoDonate,a.value,a.delta};
    case MenuOp::DroneStandby:popup_.clear();dirty_=true;return {FrontEffect::DroneStandby,a.value,a.delta};
    case MenuOp::ConfirmBoot:popup_.clear();dirty_=true;return {FrontEffect::ConfirmBoot,a.value,data_.side};
    case MenuOp::LobbyAction:popup_.clear();dirty_=true;return {FrontEffect::LobbyAction,data_.side,a.value,-1,selected_};
    case MenuOp::PopupItem:{if(a.value>=0 && size_t(a.value)<popup_.size()){selectPopup(a.value);auto action=popup_[a.value].action;popup_.clear();return activate(action);}return {};}
    default:break;
    }dirty_=true;return {};
}
std::optional<FrontResult> PlayerPane::pointer(glm::ivec2 size,glm::vec2 pixel,int button,bool down,bool twice){
    if(!active() || (popup_.empty() && view_.slide && view_.slide->undetectable))return {};pointer_=originalMenuLayout(size,scale_,view_.canvas).local(pixel);auto p=local(size,pixel);
    if(pressed_ && down){
        if(drag_ || (button!=SDL_BUTTON_LEFT && button!=SDL_BUTTON_RIGHT))return FrontResult{};
        pressedButton_=button;
        if(button==SDL_BUTTON_LEFT){repeatTimer_=true;repeat_=pressed_->op==MenuOp::ScrollPage?.25:.5;}
        if(pressed_->op==MenuOp::Donate){pressed_->delta=button==SDL_BUTTON_RIGHT?1:0;dirty_=true;return activate(*pressed_,button==SDL_BUTTON_LEFT);}
        if(button==SDL_BUTTON_LEFT && (pressed_->op==MenuOp::ScrollLine || pressed_->op==MenuOp::ScrollPage)){
            list_.position(list_.offset+pressed_->delta*(pressed_->op==MenuOp::ScrollLine?list_.row:list_.page));dirty_=true;
        }
        return FrontResult{};
    }
    if(!popup_.empty())p=pointer_;
    auto found=menuHit(view_,size,1,pixel);if(found && !view_.items[*found].enabled){
        if(down && !twice && view_.items[*found].captureOnPress && (button==SDL_BUTTON_LEFT || button==SDL_BUTTON_RIGHT)){pressed_=view_.items[*found].action;if(button==SDL_BUTTON_RIGHT && pressed_->op==MenuOp::Donate)pressed_->delta=1;pressedButton_=button;repeatTimer_=false;view_.pressed=int(*found);dirty_=true;}
        else if(!down && pressed_ && (button==pressedButton_ || !drag_ && (button==SDL_BUTTON_LEFT || button==SDL_BUTTON_RIGHT))){pressed_.reset();drag_=false;dirty_=true;}
        return FrontResult{};
    }if(!popup_.empty() && (!found || view_.items[*found].action.op!=MenuOp::PopupItem)){if(down && !contains(popupBounds_,p)){popup_.clear();pressed_.reset();drag_=false;dirty_=true;}return FrontResult{};}
    if(!down && button==SDL_BUTTON_LEFT && !popup_.empty() && found){pressed_.reset();drag_=false;return activate(view_.items[*found].action,true);}
    if(!down && pressed_ && (button==pressedButton_ || !drag_ && (button==SDL_BUTTON_LEFT || button==SDL_BUTTON_RIGHT))){auto action=*pressed_;pressed_.reset();drag_=false;dirty_=true;
        if(action.op==MenuOp::Donate || action.op==MenuOp::ScrollLine || action.op==MenuOp::ScrollPage || action.op==MenuOp::ScrollThumb)return FrontResult{};
        if(button==SDL_BUTTON_LEFT && found && view_.items[*found].action.op==action.op && view_.items[*found].action.value==action.value)return activate(action,true);return FrontResult{};}
    if(!found){
        if(down && (button==SDL_BUTTON_LEFT || button==SDL_BUTTON_RIGHT) && contains(list_.viewport,p)) {
            selected_=list_.selection=-1;focus_=true;pressed_.reset();drag_=false;popup_.clear();dirty_=true;
            return FrontResult{};
        }
        if(down && (button==SDL_BUTTON_LEFT || button==SDL_BUTTON_RIGHT) && mode_==Allegiance::PlayerPaneMode::Expanded && contains(teamViewport_,p))
            return activate({MenuOp::SelectTeam,-2,1}); // Original OnSelTeam(NULL): SIDE_TEAMLOBBY.
        return contains(paneBounds_,p)?std::optional<FrontResult>(FrontResult{}):std::nullopt;
    }
    auto action=view_.items[*found].action;
    if(!down)return FrontResult{};
    if(action.op==MenuOp::RosterSelect){if(button!=SDL_BUTTON_LEFT && button!=SDL_BUTTON_RIGHT)return FrontResult{};selected_=action.value;list_.selection=-1;for(size_t i=0;i<rows_.size();++i)if(rows_[i]->id==selected_)list_.selection=int(i);focus_=true;dirty_=true;
        if(button==SDL_BUTTON_RIGHT && !twice){popupAt_=pointer_;context();}return FrontResult{};}
    if(action.op==MenuOp::SelectTeam && action.delta==1) {
        if(button==SDL_BUTTON_LEFT || button==SDL_BUTTON_RIGHT)return activate(action);
        return FrontResult{};
    }
    if(action.op==MenuOp::Donate && button==SDL_BUTTON_RIGHT){if(!twice){action.delta=1;pressed_=action;pressedButton_=button;repeatTimer_=false;view_.pressed=int(*found);dirty_=true;return activate(action);}return FrontResult{};}
    if(button!=SDL_BUTTON_LEFT && button!=SDL_BUTTON_RIGHT)return FrontResult{};
    if((action.op==MenuOp::Wing || action.op==MenuOp::SelectTeam) && action.delta==0)return button==SDL_BUTTON_LEFT?activate(action,true):FrontResult{};
    auto control=std::find_if(view_.artwork.begin(),view_.artwork.end(),[&](const auto& c){return c.name==view_.items[*found].control;});
    if(control!=view_.artwork.end() && control->kind==OriginalControl::Kind::Button) {
        if(control->pressAction)return button==SDL_BUTTON_LEFT?activate(action,true):FrontResult{};
        if(twice){pressed_.reset();drag_=false;dirty_=true;return FrontResult{};}
    }
    if(button==SDL_BUTTON_RIGHT && action.op==MenuOp::ScrollThumb)return FrontResult{};
    pressed_=action;pressedButton_=button;repeatTimer_=button==SDL_BUTTON_LEFT;repeat_=action.op==MenuOp::ScrollPage?.25:.5;view_.pressed=int(*found);
    if(action.op==MenuOp::Donate)return activate(action,true);
    if(action.op==MenuOp::ScrollThumb){drag_=true;grab_=p.y-list_.thumb().y;}
    else if(button==SDL_BUTTON_LEFT && (action.op==MenuOp::ScrollLine || action.op==MenuOp::ScrollPage)){list_.position(list_.offset+action.delta*(action.op==MenuOp::ScrollLine?list_.row:list_.page));dirty_=true;}
    return FrontResult{};
}
void PlayerPane::hover(glm::ivec2 size,glm::vec2 pixel){
    auto hit=active()?menuHit(view_,size,1,pixel):std::optional<size_t>{};
    pointer_=originalMenuLayout(size,scale_,view_.canvas).local(pixel);
    if(auto captured=view_.capturedButton()){
        feedback_.capturedHover(*captured,hit && *hit==size_t(view_.pressed),int(mode_));
        if(drag_){auto before=list_.offset;list_.drag(local(size,pixel).y,grab_);dirty_|=list_.offset!=before;}
        return;
    }
    feedback_.hover(hit?&view_.items[*hit]:nullptr,int(mode_));
    if(popup_.empty() && view_.slide && view_.slide->undetectable)return;
    if(drag_){auto before=list_.offset;list_.drag(local(size,pixel).y,grab_);dirty_|=list_.offset!=before;return;}
    if(!popup_.empty()){auto hit=menuHit(view_,size,1,pixel);if(hit && view_.items[*hit].action.op==MenuOp::PopupItem && popupSelected_!=view_.items[*hit].action.value)selectPopup(view_.items[*hit].action.value);}
}
void PlayerPane::selectPopup(int row){
    popupSelected_=popup_.empty()?0:std::clamp(row,0,int(popup_.size())-1);
    if(popupKind_==PopupKind::Teams || popupKind_==PopupKind::Wings)
        comboSelections_[mode_==Allegiance::PlayerPaneMode::Expanded][popupKind_==PopupKind::Wings]=popupSelected_;
    dirty_=true;
}
bool PlayerPane::wheel(float amount){
    if(!active())return false;if(!popup_.empty())return true; // IMenu has no wheel navigation.
    if(view_.slide && view_.slide->undetectable)return false;
    auto p=pointer_;if(view_.slide)p-=menuSlideOffset(view_,size_,uiTime());
    if(mode_==Allegiance::PlayerPaneMode::Expanded && contains(teamViewport_,p)){teamOffset_=std::clamp(teamOffset_-amount*17,0.f,std::max(0.f,float(data_.teams.size())*17-teamViewport_.w));dirty_=true;return true;}
    if(!contains(list_.viewport,p) && !contains(list_.bar,p))return false;
    if(!rows_.empty() && list_.move(amount<0?1:-1)){selected_=rows_[list_.selection]->id;dirty_=true;}return true;
}
std::optional<FrontResult> PlayerPane::key(SDL_Keycode key){
    if(!active())return {};
    if(!popup_.empty()){
        if(key==SDLK_ESCAPE || key==SDLK_LEFT){popup_.clear();dirty_=true;return FrontResult{};}
        if(key==SDLK_UP || key==SDLK_DOWN){selectPopup((popupSelected_+(key==SDLK_UP?-1:1)+int(popup_.size()))%int(popup_.size()));return FrontResult{};}
        if(key==SDLK_RETURN)return activate({MenuOp::PopupItem,popupSelected_},true);
        if(key<128)for(size_t i=0;i<popup_.size();++i)if(popup_[i].key && std::tolower(popup_[i].key)==std::tolower(int(key)))return activate({MenuOp::PopupItem,int(i)},true);
        return FrontResult{};
    }
    if(key==SDLK_ESCAPE){close();return FrontResult{};}
    if(focus_ && !rows_.empty() && (key==SDLK_UP || key==SDLK_DOWN || key==SDLK_PAGEUP || key==SDLK_PAGEDOWN)){
        int delta=key==SDLK_UP?-1:key==SDLK_DOWN?1:key==SDLK_PAGEUP?-list_.pageRows():list_.pageRows();
        if(list_.move(delta)){selected_=rows_[list_.selection]->id;dirty_=true;}return FrontResult{};
    }
    return {};
}
std::optional<FrontResult> PlayerPane::tick(double seconds){
    if(!pressed_ || drag_ || !repeatTimer_)return {};auto a=*pressed_;
    if(a.op!=MenuOp::Donate && a.op!=MenuOp::ScrollLine && a.op!=MenuOp::ScrollPage)return {};
    repeat_-=std::max(0.,seconds);if(repeat_>0)return {};repeat_=a.op==MenuOp::ScrollPage?.25:.1;
    if(pressedButton_!=SDL_BUTTON_LEFT && a.op!=MenuOp::Donate)return {};
    auto p=pointer_;if(view_.slide)p-=menuSlideOffset(view_,size_,uiTime());
    bool inside=false;for(auto& item:view_.items)if(item.action.op==a.op && item.action.value==a.value && (item.action.delta==a.delta || a.op==MenuOp::Donate) && item.enabled && !item.blocked && contains(item.bounds,p)){inside=true;break;}
    if(!inside)return {};
    if(a.op==MenuOp::Donate)return activate(a,pressedButton_==SDL_BUTTON_LEFT);
    list_.position(list_.offset+a.delta*(a.op==MenuOp::ScrollLine?list_.row:list_.page));dirty_=true;return FrontResult{};
}
}
