#include "frontend.hpp"
#include "effect_preferences.hpp"
#include "input_mapping.hpp"
#include "../ClientCore/LoadoutPresentation.h"
#include "../ClientCore/LobbyActions.h"
#include "../ClientCore/LobbyAlliances.h"
#include "../ClientCore/AuthoredText.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Native {
bool Frontend::allianceMenuAvailable()const {
    const auto& r=session_.rules;
    bool otherGoals=r.artifacts || r.teamKills || r.flags || r.prosperity || r.territory!=100;
    return original_ && session_.scenario==Scenario::Conquest && teamPage_>=0 && teamPage_<r.teams &&
        teamActive(teamPage_) &&
        Allegiance::LobbyAlliancesAvailable(networked_?networkHost_:!playing_,
            networked_?networkLobby_ && networkCountdown_<0:!playing_ && countdown_<0,otherGoals,r.teams,
            [&](SideID side){return teamActive(side);});
}
bool Frontend::syncMissionPolicy(const GameState& state) {
    if(state.maxImbalance<0)return false; // UI fixtures without a live mission.
    Net::MissionPolicy policy{};policy.imbalance=state.maxImbalance;policy.alliances=state.alliances;
    policy.flags=(state.allowDefections?1:0)|(state.alliedSight?2:0)|(state.alliedRipcord?4:0);
    if(lastMissionPolicy_ && *lastMissionPolicy_==policy)return false;
    lastMissionPolicy_=policy;
    auto apply=[&](SessionOptions& options){options.alliances=state.alliances;options.rules.maxImbalance=state.maxImbalance;
        options.rules.allowDefections=state.allowDefections;options.rules.alliedSight=state.alliedSight;options.rules.alliedRipcord=state.alliedRipcord;};
    apply(session_);apply(setupSnapshot_);return true;
}
namespace {
std::string percent(float value){char s[40];std::snprintf(s,sizeof(s),"%.0f %%",value*100);return s;}
std::string gainLabel(const char* category,float gain,int direction){
    std::string s=direction>0?"Raise ":"Lower ";s+=category;s+=" Volume ";
    if(direction>0 && gain>=0)return s+"(maxed)";
    if(direction<0 && gain<=-60)return s+"(off)";
    char next[40];std::snprintf(next,sizeof(next),"to %.0f dB",std::clamp(gain+direction,-60.f,0.f));return s+next;
}
}
std::vector<Frontend::PopupRow> Frontend::popupRows(PopupGroup group)const {
    std::vector<PopupRow> rows;
    auto header=[&](std::string label){rows.push_back({std::move(label),0,{MenuOp::PopupItem},{},false});};
    auto sub=[&](const char* label,char key,PopupGroup child){rows.push_back({label,key,{MenuOp::PopupItem},child});};
    auto action=[&](std::string label,char key,MenuAction a,bool enabled=true){rows.push_back({std::move(label),key,a,{},enabled});};
    auto option=[&](std::string label,char key,PopupOption opt){action(std::move(label),key,{MenuOp::PopupOption,int(opt)});};
    auto visual=[&](const char* name,char key,int field){action(std::string(name)+(settings_.visual.*visualFields[field]?" On ":" Off "),key,{MenuOp::VisualSetting,field});};
    auto native=[&](std::string label,char key=0){action(std::move(label),key,{MenuOp::PopupItem},false);};
    const auto& s=settings_;const auto& c=s.controller;
    switch(group){
    case PopupGroup::Alliance:{
        if(!allianceMenuAvailable())break;
        auto allies=Game::effectiveAlliances(session_);
        Allegiance::LobbyAllianceChoices(allies.data(),session_.rules.teams,teamPage_,
            [&](SideID side){return teamActive(side);},
            [&](int id,std::string label){action(std::move(label),0,id==NA?MenuAction{MenuOp::AllianceCancel}:MenuAction{MenuOp::ChangeAlliance,teamPage_,id? id-1:NA});});
        break;}
    case PopupGroup::Combo:break; // immutable host-provided rows, authored control/font
    case PopupGroup::QuickChat:break; // immutable authored rows belong to each level
    case PopupGroup::Commands:return commandRows_;
    case PopupGroup::Player:{
        auto pilot=std::find_if(roster_.begin(),roster_.end(),[&](const auto& p){return p.id==contextPilot_;});
        if(pilot==roster_.end() || pilot->controlled)break;
        auto allowed=Allegiance::LobbyPlayerMenu(commander_ && teamPage_==session_.team && !inactive_,pilot->controlled,
            !pilot->requesting,pilot->requesting,lobbyAvailable(),session_.rules.lockTeamSettings);
        if(allowed.accept && networked_)action("Accept       ",'A',{MenuOp::LobbyAction,int(LobbyAction::Accept)});
        if(allowed.reject && networked_)action(pilot->requesting?"Reject       ":"Boot       ",'R',{MenuOp::LobbyAction,int(pilot->requesting?LobbyAction::Reject:LobbyAction::Boot)});
        if(allowed.makeLeader)action("Make Leader  ",'L',{MenuOp::MakeLeader,pilot->id});
        action(pilot->muted?"UnMute       ":"Mute         ",pilot->muted?'U':'M',{MenuOp::MutePilot,pilot->id,pilot->muted?0:1});break;
    }
    case PopupGroup::LoadoutHulls:
        for(const auto& hull:loadoutPreview_.hulls)action(Allegiance::BuyableName(hull.name.c_str(),hull.price),0,{MenuOp::PreviewHull,hull.id});break;
    case PopupGroup::LoadoutParts:
        if(partMount_<0)sub("Parts For Other Ships",0,PopupGroup::LoadoutOtherParts);
        for(const auto& part:loadoutPrimaryParts_)action(Allegiance::BuyableName(part.name.c_str(),part.price),0,{MenuOp::PreviewPart,part.id});break;
    case PopupGroup::LoadoutOtherParts:
        for(const auto& part:loadoutOtherParts_)action(Allegiance::BuyableName(part.name.c_str(),part.price),0,{MenuOp::PreviewPart,part.id});break;
    case PopupGroup::Options:
        sub("Graphics Device",'D',PopupGroup::Device);sub("Graphics Options",'O',PopupGroup::Graphics);
        sub("Sound Options",'S',PopupGroup::Sound);sub("Game Options",'G',PopupGroup::Game);break;
    case PopupGroup::Main:
        header("FreeAllegiance Native");header("");header("HELP");header("--------------------------");
        action("Manual & Quick Reference",'H',{MenuOp::Help});
        header("");header("OPTIONS");header("--------------------------");
        sub("Graphics Device",'D',PopupGroup::Device);sub("Graphics",'O',PopupGroup::Graphics);
        sub("Game",'G',PopupGroup::Game);sub("Sound",'S',PopupGroup::Sound);
        if(playing_){header("");header("INFORMATION");header("--------------------------");
            action("Game Details",'I',{MenuOp::Navigate,int(Screen::GameDetails)});
            action("Player Pings",'P',{MenuOp::PlayerPings});}
        header("");header("QUIT");header("--------------------------");
        if(playing_ || slideshowActive_)action("Mission",'Q',{MenuOp::Navigate,int(Screen::ConfirmLeave)});
        action("Allegiance",'X',{MenuOp::Quit});break;
    case PopupGroup::Device:
        option("Brightness Up",'U',PopupOption::BrightnessUp);option("Brightness Down",'D',PopupOption::BrightnessDown);header("--------------------------------");
        native("Higher Resolution (native display)",'H');native("Lower Resolution (native display)",'L');header("--------------------------------");
        header("Current device state                            ");header("--------------------------------");
        if(graphicsDeviceService_)for(auto& line:graphicsDeviceService_())header(std::move(line));
        else {header("Vulkan");header(s.fullscreen?"Borderless fullscreen / native resolution":"Borderless window");}
        break;
    case PopupGroup::Graphics:
        sub("Advanced Options",'A',PopupGroup::Advanced);
        visual("Environment",'E',0);visual("Posters",'P',1);option(debrisLabels[s.visual.debris],'D',PopupOption::Debris);
        visual("Stars",'S',2);visual("Strobes",'R',3);visual("Trails",'T',4);
        option(particlePreference(s.particles).label,'L',PopupOption::Particles);
        visual("Lens Flare",'F',5);visual("Bidirectional Lighting",'B',6);
        option(std::array<const char*,6>{"Style: Normal","Style: Software","Style: Custom Hud 1","Style: Custom Hud 2","Style: Custom Hud 3","Style: Custom Hud 3"}[s.hudStyle],'H',PopupOption::HudStyle);native("Use old UI: On",'G');
        option(s.joystickIndicator?"Joystick Indicator: On":"Joystick Indicator: Off",'J',PopupOption::JoystickIndicator);break;
    case PopupGroup::Advanced:
        option("Antialiasing ("+(s.antialiasing?std::to_string(s.antialiasing)+"xFSAA":std::string("No AA"))+")",'A',PopupOption::Antialiasing);native("Vertical Sync (paced to display)",'V');
        option("Max Texture Size ("+std::to_string(256<<s.maxTextureSize)+") ",'X',PopupOption::TextureSize);break;
    case PopupGroup::Game:
        sub("Chat/Mute/Filter",'M',PopupGroup::Chat);
        option(s.preferChase?"Default To Chase View":"Default To Cockpit View",'K',PopupOption::Chase);
        option(c.linear?"Linear Joystick Control Response":"Quadratic Joystick Control Response",'L',PopupOption::Linear);
        option(c.deadZone<=.04f?"Dead Zone: Smallest":c.deadZone<=.1f?"Dead Zone: Small":"Dead Zone: Large",'Z',PopupOption::DeadZone);
        option((mouseInputGet_?mouseInputGet_():s.virtualJoystick)?"Use Mouse As Virtual Joystick: On ":"Use Mouse As Virtual Joystick: Off ",'J',PopupOption::VirtualJoystick);
        option(s.invertY?"Y Axis Flipped ":"Y Axis Not Flipped ",'Y',PopupOption::FlipY);
        sub("Force Feedback",'E',PopupGroup::Feedback);sub("Mouse Options",'Q',PopupGroup::Mouse);
        action("Map Keys and Controls",'C',{MenuOp::Navigate,int(Screen::Controls)});break;
    case PopupGroup::Chat:
        option(s.chat.censor?"Censor Display":"Don't Censor Display",'D',PopupOption::Censor);
        option(s.chat.filterAll?"Filter Chats Sent To All":"Don't Filter Chats Sent To All",'A',PopupOption::FilterAll);
        option(s.chat.filterQuick?"Filter Voice Commands":"Don't Filter Voice Commands",'V',PopupOption::FilterQuick);
        option(s.chat.filterUnknown?"Filter Unknown Chats":"Don't Filter Unknown Chats",'U',PopupOption::FilterUnknown);
        option(s.chat.filterLobby==1?"Filter Chats Sent From Lobby":s.chat.filterLobby==2?"Filter Chats Sent From Lobby Including PMs":"Don't Filter Chats Sent From Lobby",'L',PopupOption::FilterLobby);
        option(s.chatLines==10?"Chat Lines At Maximum":"Increase To "+std::to_string(s.chatLines+1)+" Chat Lines",'I',PopupOption::ChatLinesUp);
        option(s.chatLines==1?"Chat Lines At Minimum":"Reduce To "+std::to_string(s.chatLines-1)+" Chat Lines",'R',PopupOption::ChatLinesDown);
        header("Changing chat text size might require");header("Changing the number of chat lines");
        option(s.chatSize==1?"Chat Size: Small":s.chatSize==2?"Chat Size: Large":"Set Chat: Huge",'Z',PopupOption::ChatSize);break;
    case PopupGroup::Feedback:
        option(c.feedback?"Force Feedback Enabled ":"Force Feedback Disabled ",'E',PopupOption::Feedback);
        option("Raise Force Feedback Gain "+(c.feedbackGain>=1?std::string("(maxed)"):"to "+percent(std::min(1.f,c.feedbackGain+.01f))),'U',PopupOption::FeedbackUp);
        option("Lower Force Feedback Gain "+(c.feedbackGain<=0?std::string("(off)"):"to "+percent(std::max(0.f,c.feedbackGain-.01f))),'D',PopupOption::FeedbackDown);
        option(c.autoCenter?"Auto Center On":"Auto Center Off ",'C',PopupOption::AutoCenter);break;
    case PopupGroup::Mouse:
        header("------------------------------------------------");header("Options use native relative mouse input");header("------------------------------------------------");
        option(std::array<const char*,3>{"Mouse Acceleration Off ","Mouse Acceleration Low (2x) ","Mouse Acceleration High (4x) "}[s.mouseAcceleration],'A',PopupOption::MouseAcceleration);
        option(std::string("Wheel & Keyboard Throttle Delay: ")+std::array<const char*,5>{"Max","High","Med","Min","None"}[s.throttleDelay],'W',PopupOption::ThrottleDelay);
        option("Raise Mouse Sensitivity "+(s.sensitivity>=2?std::string("(highest)"):"to "+percent(std::min(2.f,s.sensitivity+.01f))),'U',PopupOption::SensitivityUp);
        option("Lower Mouse Sensitivity "+(s.sensitivity<=.1f?std::string("(lowest)"):"to "+percent(std::max(.1f,s.sensitivity-.01f))),'D',PopupOption::SensitivityDown);break;
    case PopupGroup::Sound:
        native("Sound Quality (OpenAL)");native("Sound Card Acceleration (OpenAL mixer)");native("DirectSound Restart (not applicable)");
        option(gainLabel("Sound Effect",s.effectsGain,1),'S',PopupOption::EffectsUp);
        option(gainLabel("Sound Effect",s.effectsGain,-1),'A',PopupOption::EffectsDown);
        option(gainLabel("Voice Over",s.voiceGain,1),'V',PopupOption::VoiceUp);
        option(gainLabel("Voice Over",s.voiceGain,-1),'C',PopupOption::VoiceDown);break;
    }
    return rows;
}
std::vector<Frontend::PopupRow> Frontend::quickPopupRows(int menu)const {
    std::vector<PopupRow> rows;if(!quickChat_ || menu<0 || menu>=int(quickChat_->menus.size()))return rows;
    for(auto& entry:quickChat_->menus[menu].items){
        char key=entry.key>='a' && entry.key<='z'?char(entry.key-'a'+'A'):entry.key;
        rows.push_back({entry.label,key,{MenuOp::QuickChatItem,entry.menu>=0?entry.menu:entry.command},entry.menu>=0?std::optional{PopupGroup::QuickChat}:std::nullopt});
    }
    return rows;
}
bool Frontend::lobbyQuickChat(){
    auto host=screen_;
    if(!original_ || !quickChat_ || quickChat_->menus.empty() || hasMessageBox() || teamSettingsOpen_ || missionBriefOpen_ || !popups_.empty() ||
       (host!=Screen::RemoteLobby && host!=Screen::Pilots && host!=Screen::LobbyPilots && host!=Screen::Results))return false;
    popupPosition_={10,10};pointerAction_.reset();popups_={{PopupGroup::QuickChat}};popups_.back().rows=quickPopupRows(0);rebuild();return true;
}
void Frontend::originalPopup(bool append){
    if(!append){view_.original=true;view_.overlay=popupFlight_;view_.artwork=popupBackdrop_.artwork;
        view_.shipPreview=popupBackdrop_.shipPreview;view_.scroll=popupBackdrop_.scroll;view_.popupArtworkBegin=popupBackdrop_.popupArtworkBegin;}
    view_.blockInput();
    markPopupLayer();
    OriginalControl fontStyle;fontStyle.font="smallFont";
    std::string source;
    if(!popups_.empty() && !popups_.front().combo.empty())source=popups_.front().combo.substr(popups_.front().combo.find('/')+1);
    if(!source.empty())for(const auto& control:view_.artwork)if(control.name==source){fontStyle=control;break;}
    auto font=original_->assets().font(fontStyle.font,fontStyle.fontHeight,fontStyle.fontBold);float rowHeight=std::max(11.f,float(font->height))+2;
    auto measure=[&](std::string_view s){return Allegiance::TextExtent(s,[&](unsigned char ch){return font->glyphs[ch].z;});};
    auto rect=[&](glm::vec4 b,glm::vec4 color,const glm::vec4* clip=nullptr){OriginalControl c;c.kind=OriginalControl::Kind::Rectangle;c.bounds=b;c.color=color;if(clip)c.clip=*clip;view_.artwork.push_back(std::move(c));};
    auto text=[&](glm::vec4 b,std::string s,const glm::vec4& clip){OriginalControl c;c.kind=OriginalControl::Kind::Text;c.bounds=b;c.text=std::move(s);c.font=fontStyle.font;c.fontHeight=fontStyle.fontHeight;c.fontBold=fontStyle.fontBold;c.surfaceText();c.clip=clip;view_.artwork.push_back(std::move(c));};
    for(size_t depth=0;depth<popups_.size();++depth){
        auto& popup=popups_[depth];
        // Loadout choices and their formatted labels are immutable while open.
        if(popup.group!=PopupGroup::QuickChat && popup.group!=PopupGroup::Combo && (screen_!=Screen::ShipLoadout || popup.rows.empty()))popup.rows=popupRows(popup.group);
        popup.selected=popup.rows.empty()?-1:std::clamp(popup.selected,0,int(popup.rows.size())-1);
        float labelWidth=0,keyWidth=font->glyphs[' '].z;for(auto& row:popup.rows){labelWidth=std::max(labelWidth,measure(row.label));if(row.key)keyWidth=std::max(keyWidth,float(font->glyphs[uint8_t(row.key)].z));}
        float maximumHeight=std::floor(597/rowHeight)*rowHeight;
        bool pointAnchor=!depth && (screen_==Screen::ShipLoadout || popup.group==PopupGroup::Player || popup.group==PopupGroup::Combo || popup.group==PopupGroup::Alliance);
        if(pointAnchor){
            // An overflowing menu must fit on one side of its anchor. Moving
            // it over the opening click would select a row on that release.
            float room=std::max(popupPosition_.y,600-popupPosition_.y)-3;
            maximumHeight=std::min(maximumHeight,std::max(rowHeight,std::floor(room/rowHeight)*rowHeight));
        }
        float height=std::min(rowHeight*popup.rows.size(),maximumHeight);bool overflow=rowHeight*popup.rows.size()>height;
        float width=popup.rows.empty()?0:std::min(797.f-(overflow?12:0),11+labelWidth+2+keyWidth+9+2);
        glm::vec2 at=popupPosition_;
        if(depth){const auto& parent=popups_[depth-1];at={parent.bounds.x+parent.bounds.z,parent.bounds.y+1+parent.selected*rowHeight-parent.list.offset};
            if(at.x+width+3+(overflow?12:0)>800)at.x=parent.bounds.x-width-3-(overflow?12:0);
        }else if(pointAnchor){
            // BuyPartMenu/HullMenu use a point-sized anchor and cascade upward
            // in native (top-down) coordinates; flip below if there is no room.
            at.y=popupPosition_.y-height-3;
            if(at.y<0)at.y=popupPosition_.y;
        }
        at=glm::clamp(at,glm::vec2(0),glm::vec2(std::max(0.f,800-width-3-(overflow?12:0)),std::max(0.f,600-height-3)));
        at=glm::floor(at);
        popup.bounds={at,width+3+(overflow?12:0),height+3};auto clip=glm::vec4(at+glm::vec2(1),width,height);
        bool firstLayout=popup.list.page==0;
        popup.list.configure(int(popup.rows.size()),rowHeight,clip);popup.list.selection=popup.selected;
        if(firstLayout && popup.selected>=0)popup.list.select(popup.selected);
        // EdgePane's raised border is one pixel at top/left and two at bottom/right.
        rect(popup.bounds,{0,0,0,1});rect({at,popup.bounds.z-1,height+2},{.5f,.5f,.5f,1});
        rect({at,popup.bounds.z-1,1},glm::vec4(1));rect({at,1,height+2},glm::vec4(1));
        for(size_t row=size_t(popup.list.offset/rowHeight);row<popup.rows.size();++row){
            auto& entry=popup.rows[row];glm::vec2 p=at+glm::vec2(1,1+row*rowHeight-popup.list.offset);
            if(p.y>=clip.y+clip.w)break;bool selected=int(row)==popup.selected;
            auto color=selected?glm::vec4(197.f/255,0,41.f/255,1):glm::vec4(33.f/255,20.f/255,30.f/255,1);
            rect({p,width,rowHeight},color,&clip);text({p+glm::vec2(12,1),labelWidth,rowHeight-2},entry.label,clip);
            if(entry.key)text({p+glm::vec2(12+labelWidth+2+keyWidth-font->glyphs[uint8_t(entry.key)].z,1),keyWidth,rowHeight-2},std::string(1,entry.key),clip);
            if(entry.submenu){auto arrow=p+glm::vec2(width-10,(rowHeight-9)*.5f);
                constexpr glm::vec4 bars[]={{1,1,1,7},{2,2,1,5},{3,2,1,5},{4,3,1,3},{5,3,1,3},{6,4,1,1}};
                for(auto bar:bars)rect({arrow+glm::vec2(bar),bar.z,bar.w},{74.f/255,124.f/255,88.f/255,1},&clip);}
            auto hit=glm::vec4(p,width,rowHeight);float bottom=std::min(hit.y+hit.w,clip.y+clip.w);hit.y=std::max(hit.y,clip.y);hit.w=bottom-hit.y;
            auto action=entry.action;if(action.op==MenuOp::PopupItem)action={MenuOp::PopupItem,int(depth),int(row)};
            view_.items.push_back({hit,entry.label,{},action,entry.enabled,selected,"popup/"+std::to_string(depth)+"/"+std::to_string(row),int(depth),int(row)});
            if(popups_.front().group==PopupGroup::Combo)view_.items.back().clickSound=fontStyle.selectSound;
            if(selected && depth+1==popups_.size())view_.focus=int(view_.items.size())-1;
        }
        if(overflow){popup.list.bar={clip.x+width,clip.y,12,height};scrollbar(popup.list,int(depth)+2);}
    }
}
void Frontend::cachePopupLayer(){
    auto [artworkBegin,itemsBegin]=*popupLayer_;popupItemsBegin_=itemsBegin;
    menuPopupScene_=view_;menuPopupScene_.artwork.resize(artworkBegin);menuPopupScene_.items.resize(itemsBegin);
    menuPopupScene_.scroll.reset();menuPopupScene_.pressed=-1;menuPopupScene_.popupArtworkBegin.reset();
    menuPopupView_={};menuPopupView_.original=menuPopupView_.overlay=true;
    menuPopupView_.canvas=view_.canvas;menuPopupView_.classicScale=view_.classicScale;
    menuPopupView_.artwork.assign(view_.artwork.begin()+artworkBegin,view_.artwork.end());
    menuPopupView_.items.assign(view_.items.begin()+itemsBegin,view_.items.end());
    menuPopupView_.focus=std::max(0,view_.focus-int(itemsBegin));menuPopupView_.scroll=view_.scroll;
}
void Frontend::comboPopup(std::string source,std::vector<PopupRow> rows){
    if(rows.empty())return;
    // Original ComboPaneImpl::SetSelection updates only the face. IMenu keeps
    // its own first/hovered row even across cancellation and reopening.
    PopupLevel popup{PopupGroup::Combo};popup.selected=std::clamp(comboSelections_[source],0,int(rows.size())-1);
    popup.rows=std::move(rows);popup.combo=std::move(source);popups_.clear();popups_.push_back(std::move(popup));
    popupPosition_=pointerPosition_;pointerAction_.reset();
}
void Frontend::originalEdit(const OriginalControl& face,std::string text,std::string_view script,bool focused,bool readOnly){
    // Window::SetFocus blurs the previous EditPane. A modal host retains its
    // input draft/focus state for restoration, but only its new field paints
    // the cursor (and the covered field returns to prefix clipping).
    if(focused)for(auto& existing:view_.artwork)if(existing.textMode==OriginalControl::TextMode::Edit)existing.editFocused=false;
    auto control=widgetText(face,std::move(text));control.textMode=OriginalControl::TextMode::Edit;
    control.editFocused=focused;control.editReadOnly=readOnly;control.editLastTyped=editTimes_.try_emplace(std::string(script)+"/"+face.name,uiTime()).first->second;
    view_.artwork.push_back(std::move(control));
}
void Frontend::loadoutPartHover(const PopupRow& row){
    if(screen_!=Screen::ShipLoadout || popups_.empty() || popups_.front().group!=PopupGroup::LoadoutParts)return;
    loadoutMenuInfo_=true;
    if(row.action.op!=MenuOp::PreviewPart || row.action.value<0)hoverPart_=LoadoutSlotPreview{};
    else if(previewPartInfo_)hoverPart_=previewPartInfo_(row.action.value,loadoutPreview_.loadout.hull);
    else {
        auto found=std::find_if(loadoutPreview_.slots.begin(),loadoutPreview_.slots.end(),[&](auto& part){return part.id==row.action.value;});
        hoverPart_=found==loadoutPreview_.slots.end()?LoadoutSlotPreview{}:*found;
    }
    if(hoverPart_)hoverPart_->mount=partMount_;
}
void Frontend::closePopupChildren(int scrollId){
    if(scrollId>=2 && size_t(scrollId-2)<popups_.size()){
        auto count=size_t(scrollId-2)+1;
        if(popups_.size()>count && screen_==Screen::ShipLoadout){hoverPart_=LoadoutSlotPreview{};loadoutMenuInfo_=true;}
        popups_.resize(count);
    }
}
bool Frontend::popupWheel(float amount){
    if(amount==0 || popups_.empty())return false;
    for(int depth=int(popups_.size())-1;depth>=0;--depth){const auto& popup=popups_[depth];auto b=popup.bounds;
        if(pointerPosition_.x>=b.x && pointerPosition_.x<b.x+b.z && pointerPosition_.y>=b.y && pointerPosition_.y<b.y+b.w){
            if(popup.rows.empty() || popup.list.maximum()==0)return true;
            selectPopup(depth,std::clamp(popup.selected+(amount<0?1:-1),0,int(popup.rows.size())-1),false);return true;
        }}
    return true; // Popup capture prevents a wheel event reaching its parent.
}
void Frontend::selectPopup(int depth,int row,bool expand){
    if(depth<0 || depth>=int(popups_.size()) || row<0 || row>=int(popups_[depth].rows.size()))return;
    auto child=popups_[depth].rows[row].submenu;
    bool needsChild=expand && child && popups_.size()==size_t(depth+1);
    bool closesChild=expand && !child && popups_.size()>size_t(depth+1);
    bool needsInfo=screen_==Screen::ShipLoadout && popups_.front().group==PopupGroup::LoadoutParts && popups_[depth].described!=row;
    if(popups_[depth].selected==row && !needsChild && !closesChild && !needsInfo)return;
    if(popups_[depth].selected!=row){pointerAction_.reset();popups_[depth].selected=row;popups_[depth].list.select(row);if(!popups_[depth].combo.empty())comboSelections_[popups_[depth].combo]=row;popups_.resize(depth+1);}
    if(expand && child){if(popups_.size()==size_t(depth+1)){
        int menu=popups_[depth].rows[row].action.value;popups_.push_back({*child});
        if(*child==PopupGroup::QuickChat)popups_.back().rows=quickPopupRows(menu);
    }}
    else if(expand)popups_.resize(depth+1);
    if(needsInfo || popups_[depth].described!=row){loadoutPartHover(popups_[depth].rows[row]);popups_[depth].described=row;}rebuild();
}
FrontResult Frontend::closePopup(){
    pointerAction_.reset();
    if(!popups_.empty() && (popups_.front().group==PopupGroup::Player || popups_.front().group==PopupGroup::Combo || popups_.front().group==PopupGroup::QuickChat || popups_.front().group==PopupGroup::Alliance)){
        if(popups_.front().group==PopupGroup::Combo && teamSettingsOpen_)teamNameFocus_=true;
        popups_.clear();rebuild();return {};
    }
    if(screen_==Screen::ShipLoadout){popups_.clear();hoverPart_.reset();hoverSlot_=-1;loadoutMenuInfo_=false;rebuild();return {};}
    bool flight=popupFlight_;restorePopupParent();rebuild();
    return flight?FrontResult{FrontEffect::Resume}:FrontResult{};
}
void Frontend::restorePopupParent(){
    popups_.clear();pointerAction_.reset();screen_=popupReturn_;view_=popupBackdrop_;view_.pressed=-1;
    if(screen_==Screen::ShipLoadout)loadoutSuspended_=false;
}
FrontResult Frontend::closeSecondary(){
    if(screen_!=Screen::Help && screen_!=Screen::Credits)return {};
    bool resume=secondaryResume_;screen_=secondaryReturn_;view_=secondaryBackdrop_;view_.pressed=-1;pointerAction_.reset();rebuild();
    return resume?FrontResult{FrontEffect::Resume}:FrontResult{};
}
FrontResult Frontend::activatePopup(int depth,int row){
    if(depth<0 || depth>=int(popups_.size()) || row<0 || row>=int(popups_[depth].rows.size()))return {};
    auto entry=popups_[depth].rows[row];
    if(!entry.enabled)return {}; // IMenu::ItemCommand checks before changing selection.
    // Keyboard selection and pointer release both reach MenuSelect exactly once.
    for(const auto& item:view_.items)if(item.popupLevel==depth && item.popupRow==row){feedback_.click(item);break;}
    selectPopup(depth,row,bool(entry.submenu));
    if(entry.submenu || entry.action.op==MenuOp::PopupItem)return {};
    auto a=entry.action;
    if(popups_.front().group==PopupGroup::QuickChat){
        auto recipient=sendChatAction();popups_.clear();pointerAction_.reset();rebuild();
        return {FrontEffect::QuickChat,a.value,recipient.value,-1,recipient.targetId,true};
    }
    if(popups_.front().group==PopupGroup::Commands)return dispatch(a);
    if(popups_.front().group==PopupGroup::Player || popups_.front().group==PopupGroup::Alliance){popups_.clear();pointerAction_.reset();rebuild();return dispatch(a);}
    if(popups_.front().group==PopupGroup::Combo || a.op==MenuOp::PreviewHull || a.op==MenuOp::PreviewPart){popups_.clear();pointerAction_.reset();return dispatch(a);}
    if(a.op==MenuOp::PlayerPings){bool flight=popupFlight_;restorePopupParent();return {FrontEffect::PlayerPings,int(flight)};}
    if(a.op==MenuOp::Navigate || a.op==MenuOp::Help || a.op==MenuOp::Quit){
        bool flight=popupFlight_;restorePopupParent();
        if(a.op==MenuOp::Navigate){
            if(Screen(a.value)==Screen::ConfirmLeave){confirm(authoredLoadouts_ || slideshowActive_?"Quit the Current Training Mission?":"Quit the Current Game?",{FrontEffect::Leave},flight);return {};}
            open(Screen(a.value));if(screen_==Screen::Controls){mappingOverlay_=flight;rebuild();}return {};}
        FrontResult result{a.op==MenuOp::Help?FrontEffect::Help:FrontEffect::Quit};
        result.resumeAfter=flight && screen_!=Screen::Research && screen_!=Screen::Teleport;
        return result;
    }
    int effects=4; // Visual changes never reopen controllers or rebuild key maps.
    if(a.op==MenuOp::VisualSetting)settings_.visual.*visualFields[a.value]=!(settings_.visual.*visualFields[a.value]);
    else if(a.op==MenuOp::PopupOption){
        auto& s=settings_;auto& c=s.controller;effects=1;
        switch(PopupOption(a.value)){
        case PopupOption::Chase:s.preferChase=!s.preferChase;effects=8;break;
        case PopupOption::Linear:c.linear=!c.linear;break;
        case PopupOption::DeadZone:c.deadZone=c.deadZone<.05f?.1f:c.deadZone<.2f?.3f:.04f;break;
        case PopupOption::VirtualJoystick:s.virtualJoystick=!(mouseInputGet_?mouseInputGet_():s.virtualJoystick);if(mouseInputSet_)mouseInputSet_(s.virtualJoystick);effects=128;break;
        case PopupOption::FlipY:s.invertY=!s.invertY;break;
        case PopupOption::JoystickIndicator:s.joystickIndicator=!s.joystickIndicator;effects=4;break;
        case PopupOption::MouseAcceleration:s.mouseAcceleration=(s.mouseAcceleration+1)%3;break;
        case PopupOption::ThrottleDelay:s.throttleDelay=(s.throttleDelay+1)%5;break;
        case PopupOption::SensitivityUp:s.sensitivity=std::clamp(s.sensitivity+.01f,.1f,2.f);break;
        case PopupOption::SensitivityDown:s.sensitivity=std::clamp(s.sensitivity-.01f,.1f,2.f);break;
        case PopupOption::Feedback:c.feedback=!c.feedback;break;
        case PopupOption::FeedbackUp:c.feedbackGain+=.01f;break;
        case PopupOption::FeedbackDown:c.feedbackGain-=.01f;break;
        case PopupOption::AutoCenter:c.autoCenter=!c.autoCenter;break;
        case PopupOption::BrightnessUp:s.gamma*=1.01f;effects=16;break;
        case PopupOption::BrightnessDown:s.gamma/=1.01f;effects=16;break;
        case PopupOption::Antialiasing:{auto current=std::find(graphicsSamples_.begin(),graphicsSamples_.end(),s.antialiasing);s.antialiasing=graphicsSamples_.empty()?0:current==graphicsSamples_.end() || ++current==graphicsSamples_.end()?graphicsSamples_.front():*current;effects=32;break;}
        case PopupOption::TextureSize:s.maxTextureSize=(s.maxTextureSize+1)%4;effects=64;break;
        case PopupOption::EffectsUp:s.effectsGain+=1;effects=2;break;
        case PopupOption::EffectsDown:s.effectsGain-=1;effects=2;break;
        case PopupOption::VoiceUp:s.voiceGain+=1;effects=2;break;
        case PopupOption::VoiceDown:s.voiceGain-=1;effects=2;break;
        case PopupOption::Particles:s.particles=particlePreference(s.particles).next;effects=4;break;
        case PopupOption::Debris:s.visual.debris=(s.visual.debris+1)%4;effects=4;break;
        case PopupOption::Censor:s.chat.censor=!s.chat.censor;effects=4;break;
        case PopupOption::FilterAll:s.chat.filterAll=!s.chat.filterAll;effects=4;break;
        case PopupOption::FilterQuick:s.chat.filterQuick=!s.chat.filterQuick;effects=4;break;
        case PopupOption::FilterUnknown:s.chat.filterUnknown=!s.chat.filterUnknown;effects=4;break;
        case PopupOption::FilterLobby:s.chat.filterLobby=s.chat.filterLobby%3+1;effects=4;break;
        case PopupOption::ChatLinesUp:++s.chatLines;effects=4;break;
        case PopupOption::ChatLinesDown:--s.chatLines;effects=4;break;
        case PopupOption::ChatSize:s.chatSize=s.chatSize%3+1;effects=4;break;
        case PopupOption::HudStyle:s.hudStyle=(s.hudStyle+1)%6;effects=4;break;
        }
    }
    settings_.normalize();draft_=settings_;rebuild();return {FrontEffect::ApplyOptions,effects};
}
FrontResult Frontend::popupKey(SDL_Keycode key){
    if(popups_.empty())return {};
    int depth=int(popups_.size())-1;auto& popup=popups_.back();int selected=popup.selected;
    if(key==SDLK_ESCAPE || key==SDLK_LEFT){if(depth){pointerAction_.reset();popups_.pop_back();auto& parent=popups_.back();if(parent.selected>=0)loadoutPartHover(parent.rows[parent.selected]);rebuild();return {};}return closePopup();}
    if(popup.rows.empty())return {};
    if(key==SDLK_DOWN || key==SDLK_UP){selectPopup(depth,(selected+(key==SDLK_DOWN?1:-1)+int(popup.rows.size()))%int(popup.rows.size()),false);return {};}
    if(popup.list.maximum()>0 && (key==SDLK_PAGEUP || key==SDLK_PAGEDOWN || key==SDLK_HOME || key==SDLK_END)){
        int row=key==SDLK_HOME?0:key==SDLK_END?int(popup.rows.size())-1:std::clamp(selected+(key==SDLK_PAGEUP?-1:1)*popup.list.pageRows(),0,int(popup.rows.size())-1);
        selectPopup(depth,row,false);return {};
    }
    if(key==SDLK_RIGHT){selectPopup(depth,selected,true);return {};}
    if(key==SDLK_RETURN)return activatePopup(depth,selected);
    char accelerator=key>='a' && key<='z'?char(key-'a'+'A'):key>='A' && key<='Z'?char(key):key>='0' && key<='9'?char(key):0;
    if(accelerator)for(size_t row=0;row<popup.rows.size();++row)if(popup.rows[row].key==accelerator){if(popups_.front().group==PopupGroup::Player || popups_.front().group==PopupGroup::QuickChat)popupTextKey_=key;return activatePopup(depth,int(row));}
    return {};
}
FrontResult Frontend::closeMappings(bool accept){
    if(accept){normalizeInputKeyModifiers(draft_);copyInputMappings(settings_,draft_);settings_.normalize();}
    else copyInputMappings(draft_,mappingSnapshot_);
    bool flight=mappingOverlay_;screen_=mappingReturn_;view_=mappingParentView_;popups_=mappingParentPopups_;
    mappingEditing_=false;pointerAction_.reset();view_.pressed=-1;
    if(screen_==Screen::ShipLoadout)loadoutSuspended_=false;
    rebuild();
    return accept?FrontResult{FrontEffect::ApplySettings,flight?1:0}:flight?FrontResult{FrontEffect::Resume}:FrontResult{};
}
}
