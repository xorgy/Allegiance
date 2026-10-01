#include "frontend.hpp"
#include "input_mapping.hpp"
#include "mouse.hpp"
#include "classic_scrollbar.hpp"
#include <cmath>

namespace Native {
namespace {
constexpr int commandCount=int(std::size(Allegiance::CommandDescriptions));
bool contains(glm::vec4 b,glm::vec2 p){return p.x>=b.x && p.x<b.x+b.z && p.y>=b.y && p.y<b.y+b.w;}
uint64_t axisIdentity(SDL_JoystickID device,int axis,bool hat){return (uint64_t(device)<<32)|(uint32_t(axis)<<1)|hat;}
}
void Frontend::listBlank(glm::vec4 bounds,MenuOp selection,int group){
    // A ListPane owns its complete authored viewport, including empty space.
    // Register before its rows so their hit regions take precedence.
    view_.items.push_back({bounds,"",{},{MenuOp::ListClear,int(selection),group},true,false,"listBlank/"+std::to_string(int(selection))+"/"+std::to_string(group)});
}
void Frontend::scrollbar(ScrollList& list,int id){
    appendClassicScrollbar(view_,original_->assets(),list,id);
}
void Frontend::originalMappings(){
    view_.original=true;view_.overlay=mappingOverlay_;view_.items.clear();view_.text.clear();view_.artwork=mappingBackdrop_;
    view_.popupArtworkBegin=mappingParentView_.popupArtworkBegin;
    view_.blockInput();
    markPopupLayer();
    std::string message="Press Enter to add a mapping or Delete to remove mapping.";
    if(mappingEditing_)message=mappingList_.selection>=commandCount?"Move desired joystick axis.":"Type desired key sequence or button.  Escape cancels.";
    if(mappingPending_.type!=MappingPending::None){
        if(mappingPending_.type==MappingPending::Axis)message="Axis in use.  Press Enter to lose old mapping.  Escape cancels";
        else {auto conflict=mappingPending_.conflict;const char* name=conflict>=0?commandDescription(TrekKey(conflict)):conflict<=-2 && -conflict-2<int(Binding::Count)?bindingNames[-conflict-2]:"";message=std::string(mappingPending_.type==MappingPending::Key?"Key":"Button")+" in use for \""+name+"\". Press Enter to lose old mapping. Escape cancels.";}
    }
    auto popup=original_->layout("inputmappane","inputMapPane",{},{{"text",message}});
    auto size=glm::vec2(popup.front().bounds.z,popup.front().bounds.w),origin=glm::floor((glm::vec2(view_.canvas)-size)*.5f);
    for(auto& c:popup){
        c.translate(origin);
        if(c.kind==OriginalControl::Kind::Button){MenuOp op=c.name=="okButton"?MenuOp::MappingAccept:c.name=="cancelButton"?MenuOp::MappingCancel:MenuOp::MappingRestore;view_.items.push_back({c.visibleBounds(),c.name,{},{op},true,false,c.name});}
        view_.artwork.push_back(std::move(c));
    }
    // WinTrek passes SmallFont into this painter. The MDL font declaration
    // is unused by the original; colors and the split are authored imports.
    OriginalControl style;style.kind=OriginalControl::Kind::Text;style.font="smallFont";style.surfaceText();
    auto height=float(original_->assets().font(style.font,style.fontHeight,style.fontBold)->height),column=float(original_->number("inputmappane","mappingOffset"));
    auto textColor=original_->color("inputmappane","textColor"),noMappingColor=original_->color("inputmappane","noMappingColor"),selectionColor=original_->color("inputmappane","selectionColor");
    mappingList_.configure(commandCount+6,height,authoredControlBounds(view_.artwork,"listPane"));mappingList_.bar=authoredScrollBarBounds(view_.artwork,"listPane");
    auto clip=mappingList_.viewport;
    listBlank(clip,MenuOp::MappingSelect);
    for(int i=int(mappingList_.offset/height);i<mappingList_.count;++i){
        float y=clip.y+i*height-mappingList_.offset;if(y>=clip.y+clip.w)break;
        glm::vec4 bounds{clip.x,y,clip.z,height};bool selected=i==mappingList_.selection;
        auto command=i<commandCount?Allegiance::CommandDescriptions[i].m_tk:TK_NoKeyMapping;
        OriginalControl row;row.kind=OriginalControl::Kind::Rectangle;row.bounds=bounds;row.clip=clip;row.name="mapping"+std::to_string(i);row.color=selected?selectionColor:glm::vec4(0,0,0,1);view_.artwork.push_back(row);
        auto hit=bounds;float bottom=std::min(hit.y+hit.w,clip.y+clip.w);hit.y=std::max(hit.y,clip.y);hit.w=bottom-hit.y;
        view_.items.push_back({hit,"",{},{MenuOp::MappingSelect,i},true,selected,row.name});
        auto text=style;text.bounds={bounds.x,bounds.y,column,height};text.clip=clip;text.text=i<commandCount?Allegiance::CommandDescriptions[i].m_pcc:Allegiance::AxisDescriptions[i-commandCount];text.color=i<commandCount && command==TK_NoKeyMapping?noMappingColor:textColor;view_.artwork.push_back(text);
        text.bounds.x+=column;text.bounds.z=clip.z-column;text.color=textColor;text.text.clear();
        if(selected && mappingEditing_)text.text=mappingPending_.type!=MappingPending::None?"!!!":"???";
        else if(i<commandCount){if(command!=TK_NoKeyMapping)text.text=inputMappingText(draft_,command);}
        else {auto& axis=draft_.controller.mapAxes[i-commandCount];if(axis.device>=0 && axis.index>=0)text.text="Joy "+std::to_string(axis.device)+(axis.hat?" Hat ":" Axis ")+std::to_string(axis.index);}
        view_.artwork.push_back(std::move(text));
    }
    scrollbar(mappingList_,0);
}
void Frontend::pointerMove(glm::vec2 logical,glm::ivec2 drawable){
    if(hasMessageBox()){pointerPosition_=logical;return;}
    if(original_ && (screen_==Screen::Teleport || screen_==Screen::Research) && view_.slide)logical-=menuSlideOffset(view_,drawable,uiTime());
    auto old=pointerPosition_;
    pointerPosition_=logical;
    if(original_ && screen_==Screen::Lobby && session_.scenario==Scenario::Conquest){
        bool changed=popups_.empty() && contains(setupMapBounds_,old)!=contains(setupMapBounds_,logical);
        if(pointerAction_ && pointerAction_->op==MenuOp::SetupMapDrag && setupMapCanDrag_){
            float scale=setupMapSpan_.x/setupMapBounds_.z;
            auto drag=glm::clamp(setupMapDrag_+scale*glm::vec2(old.x-logical.x,logical.y-old.y),glm::vec2(0),glm::max(glm::vec2(0),setupMapExtent_-setupMapSpan_));
            if(drag!=setupMapDrag_){setupMapDrag_=drag;setupMapDirty_=true;changed=true;}
        }
        if(changed)rebuild();
    }
    if(pointerAction_ && pointerAction_->op==MenuOp::ScrollThumb && screen_==pointerScreen_){auto& list=scrollList(pointerAction_->value);auto before=list.offset;list.drag(logical.y,scrollGrab_);if(list.offset!=before){if(pointerAction_->value==-4 || pointerAction_->value==-6)chatScrolled(pointerAction_->value);rebuild();}}
}
bool Frontend::pointerWheel(float amount){
    if(hasMessageBox())return true;
    if(!original_ || amount==0)return false;
    if(!popups_.empty())return popupWheel(amount);
    if(screen_==Screen::GameDetails)return true; // MMLPopup captures input; its page uses the authored scrollbar.
    if(screen_==Screen::Games){
        if(gameCreateOpen_){
            if(contains(createServerList_.viewport,pointerPosition_) && createServerList_.move(amount<0?1:-1))dispatch({MenuOp::GameCreateServer,createServerList_.selection});
            else if(contains(createCoreList_.viewport,pointerPosition_) && createCoreList_.move(amount<0?1:-1))dispatch({MenuOp::GameCreateCore,createCoreList_.selection});
            return true;
        }
        if(gameFilterOpen_ || findPlayerOpen_)return true;if(!contains(gamesList_.viewport,pointerPosition_) || gamesRows_.empty())return false;if(gamesList_.move(amount<0?1:-1)){gameSelected_=games_[gamesRows_[gamesList_.selection]].address;rebuild();}return true;
    }
    if(screen_==Screen::Online){if(!contains(lanList_.viewport,pointerPosition_) || lanRows_.empty())return false;if(lanList_.move(amount<0?1:-1)){lanSelected_=games_[lanRows_[lanList_.selection]].address;rebuild();}return true;}
    bool lobby=screen_==Screen::Pilots || screen_==Screen::RemoteLobby || screen_==Screen::LobbyPilots;
    int chatId=screen_==Screen::Results?-4:-6;auto& chatList=scrollList(chatId);
    if((lobby || screen_==Screen::Results) && (contains(chatList.viewport,pointerPosition_) || contains(chatList.bar,pointerPosition_))){chatList.position(chatList.offset+(amount<0?1:-1)*chatList.row*3);chatScrolled(chatId);rebuild();return true;}
    if(screen_==Screen::Results){if(!contains(resultsList_.viewport,pointerPosition_) || resultsPlayers_.empty())return false;if(resultsList_.move(amount<0?1:-1)){resultsPlayerSelected_=endgame_.players[resultsPlayers_[resultsList_.selection]].id;rebuild();}return true;}
    if(screen_==Screen::Research){
        if(!contains(researchList_.viewport,pointerPosition_))return false;
        researchList_.position(researchList_.offset+(amount<0?1:-1)*researchList_.page);rebuild();return true;
    }
    if(screen_==Screen::Teleport){
        if(!contains(teleportList_.viewport,pointerPosition_) || teleports_.empty())return false;
        if(teleportList_.move(amount<0?1:-1)){auto& choice=teleports_[teleportList_.selection];teleportSelected_=choice.id;teleportShipSelected_=choice.boarding;rebuild();}return true;
    }
    if(screen_==Screen::Pilots || screen_==Screen::RemoteLobby || screen_==Screen::LobbyPilots){
        if(!contains(rosterList_.viewport,pointerPosition_) || roster_.empty())return false;
        if(rosterList_.move(amount<0?1:-1)){auto& p=roster_[rosterList_.selection];rosterSelected_=p.id;lobbyRecipient_=p.controlled?-1:p.id;rosterFocus_=true;rebuild();}return true;
    }
    if(screen_!=Screen::Controls)return false;
    if(mappingEditing_){if(mappingPending_.type==MappingPending::None && mappingList_.selection<commandCount && amount!=0)addMappingButton(-1,amount<0?8:9);return true;}
    if(!contains(mappingList_.viewport,pointerPosition_))return false;
    mappingList_.move(amount<0?1:-1);rebuild();return true;
}
void Frontend::editMapping(){
    int selected=mappingList_.selection;if(selected<0)return;
    if(selected<commandCount && !bindingFor(Allegiance::CommandDescriptions[selected].m_tk))return;
    mappingEditing_=true;mappingPending_={};mappingAxes_.clear();mappingDevices_=draft_.controller.mapDevices;
    if(selected>=commandCount){
        int count=0;auto* ids=SDL_GetJoysticks(&count);
        for(int i=0;i<count;++i)if(auto* joy=SDL_OpenJoystick(ids[i])){
            for(int axis=0;axis<SDL_GetNumJoystickAxes(joy);++axis)mappingAxes_.push_back({axisIdentity(ids[i],axis,false),float(SDL_GetJoystickAxis(joy,axis))/32768});
            SDL_CloseJoystick(joy);
        }SDL_free(ids);
    }
    rebuild();
}
void Frontend::addMappingKey(SDL_Scancode scan,uint8_t modifiers,bool overwrite){
    int selected=mappingList_.selection;if(selected<0 || selected>=commandCount)return;
    TrekKey command=TrekKey(Allegiance::CommandDescriptions[selected].m_tk);auto binding=bindingFor(command);if(!binding)return;modifiers&=7;
    bool exists=false;int conflict=-1;
    auto match=[&](SDL_Scancode key,uint8_t mod,Binding b,TrekKey cmd){if(key==scan && (mod&7)==modifiers){auto action=mappingCommand(b,cmd);if(action==command)exists=true;else if(conflict==-1)conflict=action>=0?action:-2-int(b);}};
    for(size_t i=0;i<draft_.keys.size();++i)match(draft_.keys[i],draft_.modifiers[i],Binding(i),draft_.keyCommands[i]);
    for(auto& a:draft_.keyAliases)match(a.scan,a.modifiers,a.binding,a.command);
    if(exists){finishMapping();return;}
    if(conflict!=-1 && !overwrite){mappingPending_={MappingPending::Key,0,int(scan),conflict,modifiers};rebuild();return;}
    if(overwrite){for(size_t i=0;i<draft_.keys.size();++i)if(draft_.keys[i]==scan && (draft_.modifiers[i]&7)==modifiers)draft_.keys[i]=SDL_SCANCODE_UNKNOWN;std::erase_if(draft_.keyAliases,[&](auto& a){return a.scan==scan && (a.modifiers&7)==modifiers;});}
    auto i=size_t(*binding);if(draft_.keys[i]==SDL_SCANCODE_UNKNOWN){draft_.keys[i]=scan;draft_.modifiers[i]=modifiers;draft_.keyCommands[i]=command;}
    else draft_.keyAliases.insert(draft_.keyAliases.begin(),{scan,modifiers,*binding,command});
    finishMapping();
}
void Frontend::addMappingButton(int device,int button,bool overwrite){
    int selected=mappingList_.selection;if(selected<0 || selected>=commandCount)return;auto command=Allegiance::CommandDescriptions[selected].m_tk;auto binding=bindingFor(command);if(!binding)return;
    int conflict=-1;bool exists=false;
    auto match=[&](Binding b){auto action=trekKey(b);if(action==command)exists=true;else if(conflict==-1)conflict=action>=0?action:-2-int(b);};
    if(device<0){if(button<0 || button>=10)return;if(draft_.mouseButtons[button]>=0)match(Binding(draft_.mouseButtons[button]));for(auto& a:draft_.mouseAliases)if(a.button==button)match(a.binding);}
    else {for(auto& a:draft_.controller.mapButtons)if(a.device==device && a.button==button)match(a.binding);}
    if(exists){if(device>=0)draft_.controller.mapDevices[device]=mappingDevices_[device];finishMapping();return;}
    if(conflict!=-1 && !overwrite){mappingPending_={MappingPending::Button,device,button,conflict};rebuild();return;}
    if(device<0){draft_.mouseButtons[button]=int(*binding);std::erase_if(draft_.mouseAliases,[&](auto& a){return a.button==button;});}
    else {std::erase_if(draft_.controller.mapButtons,[&](auto& a){return a.device==device && a.button==button;});draft_.controller.mapButtons.insert(draft_.controller.mapButtons.begin(),{device,button,*binding});draft_.controller.mapDevices[device]=mappingDevices_[device];draft_.controller.mapped=true;}
    finishMapping();
}
void Frontend::addMappingAxis(int device,int axis,bool hat,bool overwrite){
    int selected=mappingList_.selection-commandCount;if(selected<0 || selected>=6)return;
    auto& axes=draft_.controller.mapAxes;
    for(int i=0;i<6;++i)if(i!=selected && axes[i].device==device && axes[i].index==axis && axes[i].hat==hat){
        if(!overwrite){mappingPending_={MappingPending::Axis,device,axis,i,0,hat};rebuild();return;}axes[i]={};
    }
    // Preserve the saved inversion convention of this logical axis.
    bool invert=axes[selected].invert;axes[selected]={device,axis,hat,invert,-1};draft_.controller.mapDevices[device]=mappingDevices_[device];draft_.controller.mapped=true;finishMapping();
}
bool Frontend::mappingMouse(int button){
    if(!original_ || screen_!=Screen::Controls || !mappingEditing_)return false;
    if(mappingPending_.type==MappingPending::None && mappingList_.selection<commandCount && button>0 && button<=8)addMappingButton(-1,MouseBindings::index(Uint8(button)));
    return true;
}
bool Frontend::mappingController(const SDL_Event& event){
    if(!original_ || screen_!=Screen::Controls || !mappingEditing_)return false;
    SDL_JoystickID id{};int input{};bool hat=false;float value{};
    if(event.type==SDL_EVENT_JOYSTICK_BUTTON_DOWN){id=event.jbutton.which;input=event.jbutton.button;}
    else if(event.type==SDL_EVENT_JOYSTICK_AXIS_MOTION){id=event.jaxis.which;input=event.jaxis.axis;value=float(event.jaxis.value)/32768;}
    else if(event.type==SDL_EVENT_JOYSTICK_HAT_MOTION){id=event.jhat.which;input=event.jhat.hat;hat=true;value=event.jhat.value;}
    else return false;
    if(mappingPending_.type!=MappingPending::None)return true;
    int count=0;auto* ids=SDL_GetJoysticks(&count);int ordinal=-1;for(int i=0;i<count;++i)if(ids[i]==id)ordinal=i;SDL_free(ids);if(ordinal<0 || ordinal>=16)return true;
    int device=ordinal;if(ordinal<int(controllerDevices_.size())){
        auto& guid=controllerDevices_[ordinal].guid;
        auto known=std::find(mappingDevices_.begin(),mappingDevices_.end(),guid);
        if(known!=mappingDevices_.end())device=int(known-mappingDevices_.begin());
        else if(!mappingDevices_[device].empty()){
            auto empty=std::find(mappingDevices_.begin(),mappingDevices_.end(),std::string{});if(empty==mappingDevices_.end())return true;device=int(empty-mappingDevices_.begin());
        }
        mappingDevices_[device]=guid;
    }
    if(mappingList_.selection>=commandCount){
        if(event.type==SDL_EVENT_JOYSTICK_AXIS_MOTION){auto key=axisIdentity(id,input,false);auto old=std::find_if(mappingAxes_.begin(),mappingAxes_.end(),[&](auto& a){return a.first==key;});if(old!=mappingAxes_.end() && std::abs(value-old->second)>.9f)addMappingAxis(device,input,false);}
        else if(hat && value!=SDL_HAT_CENTERED)addMappingAxis(device,input,true);
    } else if(event.type==SDL_EVENT_JOYSTICK_BUTTON_DOWN)addMappingButton(device,input);
    else if(hat){auto& axis=draft_.controller.mapAxes[5];if(axis.device==device && axis.hat && axis.index==input){int button=value==SDL_HAT_UP?-1:value==SDL_HAT_RIGHT?-2:value==SDL_HAT_DOWN?-3:value==SDL_HAT_LEFT?-4:0;if(button)addMappingButton(device,button);}}
    return true;
}
void Frontend::originalRoster(){
    roster_.clear();hasRequests_=false;int ownSide=session_.team;for(const auto& p:networkPilots_)if(p.controlled)ownSide=p.team;
    auto add=[&](int id,const std::string& name,bool ready,int side,const PilotState* pilot=nullptr,bool requesting=false){
        if(side!=teamPage_ && !requesting)return;
        RosterRow row{id,name,ready?"Ready":"AFK",{}};
        row.controlled=id==-1;row.ready=ready;
        if(pilot){row.status="Playing";row.wingID=pilot->wing;row.ready=true;row.wing=side>=0 && side==ownSide && pilot->wing>=0?std::string(Game::wingName(pilot->wing)):"";row.commander=pilot->commander;row.controlled=pilot->controlled;row.muted=pilot->muted;row.host=networked_ && pilot->id==networkHostPilot_;}
        roster_.push_back(std::move(row));
    };
    if(networked_ || playing_ && !networkPilots_.empty()){
        // The original ConcatinatedList puts members before requests; its
        // stable sorts use that order for ties, independently of report order.
        for(bool applicants:{false,true})for(const auto& p:networkPilots_)if(applicants?(p.team!=teamPage_ && p.requested==teamPage_):p.team==teamPage_){add(p.id,p.name,p.ready,p.team,nullptr,applicants);auto& row=roster_.back();row.host=p.host;row.commander=p.commander;row.controlled=p.controlled;row.muted=p.muted;row.requesting=applicants;hasRequests_|=row.requesting;
            row.wingID=p.wing;if(ownSide>=0 && p.team==ownSide && p.wing>=0)row.wing=std::string(Game::wingName(p.wing));
            row.status=row.requesting?"Requesting":!p.ready?"AFK":(playing_ && !networked_ || (lobbyFlags_&4))?p.team<0?"Not Playing":"Playing":"Ready";}
    } else if(playing_){for(auto& p:pilots_)add(p.id,p.name,p.docked,p.side,&p);}
    else {add(-1,settings_.pilot,ready_,session_.team);if(!roster_.empty() && roster_.back().controlled)roster_.back().host=roster_.back().commander=true;
        int index=0;for(auto& p:session_.localPilots)add(-2-index++,p.name,p.ready,p.team);}
    Allegiance::SortLobbyRoster(roster_,rosterSort_,rosterReverse_,rosterScratch_,rosterLinks_);
    auto columns=original_->integers("teamscreen","playerColumns");
    if(columns.size()!=6 || !std::is_sorted(columns.begin(),columns.end()))throw std::runtime_error("Invalid original lobby player columns");
    rosterList_.configure(int(roster_.size()),14,authoredListBounds(view_.artwork,"playerListPane",columns.back()));rosterList_.bar=authoredScrollBarBounds(view_.artwork,"playerListPane");
    rosterList_.selection=-1;
    for(size_t i=0;i<roster_.size();++i)if(roster_[i].id==rosterSelected_)rosterList_.selection=int(i);
    if(rosterList_.selection<0){rosterSelected_=-100000;lobbyRecipient_=-1;}
    else {auto& selected=roster_[rosterList_.selection];lobbyRecipient_=selected.controlled?-1:selected.id;}
    auto clip=rosterList_.viewport;
    listBlank(clip,MenuOp::RosterSelect);
    for(int index=int(rosterList_.offset/14);index<int(roster_.size());++index){
        float y=clip.y+index*14-rosterList_.offset;if(y>=clip.y+clip.w)break;auto& pilot=roster_[index];glm::vec4 bounds{clip.x,y,clip.z,14};
        auto hit=bounds;float bottom=std::min(y+14,clip.y+clip.w);hit.y=std::max(y,clip.y);hit.w=bottom-hit.y;
        OriginalControl row;row.kind=OriginalControl::Kind::Rectangle;row.bounds=bounds;row.clip=clip;row.name="roster"+std::to_string(pilot.id);row.color=index==rosterList_.selection?glm::vec4(100.f/255,0,0,1):glm::vec4(0,0,0,0);view_.artwork.push_back(row);
        view_.items.push_back({hit,"Select pilot. Right-click for pilot actions.",{},{MenuOp::RosterSelect,index,pilot.id},true,index==rosterList_.selection,row.name});
        auto text=[&](float x,float width,const std::string& value,bool columnClip=true){OriginalControl c;c.kind=OriginalControl::Kind::Text;c.font="smallFont";c.text=value;if(pilot.requesting){c.name="request/"+std::to_string(pilot.id);c.color=glm::vec4(rosterFlash_?.75f:1.f,rosterFlash_?.75f:1.f,rosterFlash_?.75f:1.f,1);}
            c.bounds={clip.x+x,y,width,14};c.clip=clip;if(columnClip)c.intersectClip(c.bounds);c.surfaceText();view_.artwork.push_back(std::move(c));};
        text(0,columns[0],pilot.name);
        // LookupRankName returns an empty name for non-Club games without a
        // rank table. Native pilots have the original unranked value, zero.
        text(columns[2]+2,columns[3]-columns[2]-2," ("+std::to_string(pilot.rank)+") ");
        text(columns[3]+2,columns[4]-columns[3]-2,pilot.status);text(columns[4]+2,columns[5]-columns[4]-2,pilot.wing,false);
        auto check=original_->assets().image("lobbycheckbmp",true);
        auto mark=[&](float x){OriginalControl c;c.kind=OriginalControl::Kind::Image;c.bounds={clip.x+x,y+(14-int(check->height))/2,check->width,check->height};c.image=check;c.clip=clip;view_.artwork.push_back(std::move(c));};
        if(!pilot.requesting){if(pilot.host)mark(columns[0]+2);if(pilot.commander)mark(columns[1]+2);}
    }
    scrollbar(rosterList_,1);
}
FrontResult Frontend::pointerContext(std::optional<size_t> hit,bool doubleClick){
    if(!hasMessageBox() && original_ && screen_==Screen::GameDetails && !insideDetails())return dispatch({MenuOp::DetailsClose});
    if(!popups_.empty())return pointerOther();
    // ScrollBarThumb shares button artwork, but its input primitive only
    // captures the left button; it has no ButtonUIPane right event.
    if(hit && *hit<view_.items.size() && view_.items[*hit].action.op==MenuOp::ScrollThumb)return {};
    if(hit && *hit<view_.items.size() && original_ && !view_.items[*hit].blocked && view_.items[*hit].captureOnPress){
        const auto& item=view_.items[*hit];pointerAction_.reset();view_.pressed=-1;contextCapture_=false;
        if(doubleClick){if(item.enabled && item.action.op==MenuOp::TrainingSelect)return dispatch({MenuOp::TrainingStart,trainingSelection_});return {};}
        pointerAction_=item.action;pointerControl_=item.control;pointerScreen_=screen_;view_.pressed=int(*hit);contextCapture_=true;return {};
    }
    if(hasMessageBox())return {};
    if(original_ && screen_==Screen::Teleport && hit && *hit<view_.items.size() && view_.items[*hit].action.op==MenuOp::TeleportSelect)return activate(*hit);
    if(original_ && screen_==Screen::Research && hit && *hit<view_.items.size() && view_.items[*hit].action.op==MenuOp::ResearchSelect){activate(*hit);return dispatch({MenuOp::ResearchActivate,1});}
    if(!hit || *hit>=view_.items.size() || !view_.items[*hit].enabled || view_.items[*hit].blocked)return {};
    const auto action=view_.items[*hit].action;
    if(original_ && (action.op==MenuOp::LanSelect || action.op==MenuOp::GameSelect || action.op==MenuOp::GameCreateServer || action.op==MenuOp::GameCreateCore || action.op==MenuOp::MappingSelect || action.op==MenuOp::ResultsSelect || action.op==MenuOp::ListClear))return activate(*hit);
    if(action.op==MenuOp::PreviewSlot)return pointerPress(hit);
    if(original_ && action.op==MenuOp::SelectTeam){
        activate(*hit); // ListPane selects the side before the right-click event.
        clearLobbySelection();
        if(!doubleClick && allianceMenuAvailable()){popupPosition_=pointerPosition_;popups_={{PopupGroup::Alliance}};pointerAction_.reset();}
        rebuild();return {};
    }
    if(action.op==MenuOp::RosterSelect && action.delta>=0){
        rosterSelected_=action.delta;rosterList_.select(action.value);contextPilot_=action.delta;
        if(!doubleClick){popupPosition_=pointerPosition_;popups_={{PopupGroup::Player}};pointerAction_.reset();}rebuild();
    }
    return {};
}
FrontResult Frontend::pointerOther(){
    if(hasMessageBox())return {};
    if(original_ && screen_==Screen::GameDetails)return insideDetails()?FrontResult{}:dispatch({MenuOp::DetailsClose});
    if(popups_.empty())return {};
    // PopupContainer::PickImage closes menus on any outside button down.
    // MenuItemPane only executes on left release; other inside buttons do nothing.
    if(std::any_of(popups_.begin(),popups_.end(),[&](const auto& popup){return contains(popup.bounds,pointerPosition_);}))return {};
    return closePopup();
}
}
