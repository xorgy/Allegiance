#include "frontend.hpp"
#include "chat_censor.hpp"
#include "../ClientCore/AuthoredText.h"
#include "../ClientCore/LogonPresentation.h"
#include "../ClientCore/CommunityLinks.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Native {
void Frontend::originalLogon(){
    view_.blockInput();
    auto controls=original_->layout("logon","popup",{{"Callsign",0},{"ZoneID",1},{"PassportID",2},{"logonNameType",0}},{{"promptText",Allegiance::LanLogonPrompt}});
    if(controls.empty())return;
    markPopupLayer();
    auto origin=glm::floor((glm::vec2(view_.canvas)-glm::vec2(controls.front().bounds.z,controls.front().bounds.w))*.5f);
    for(auto c:controls){
        if(c.name=="passwordPane" || c.name=="passwordEditPane" || c.name=="savePasswordButtonPane" || c.name=="cdKeyButtonPane" || c.name=="signUpButtonPane")continue;
        c.translate(origin);view_.artwork.push_back(c);
        if(c.name=="logonButtonPane")view_.items.push_back({c.visibleBounds(),"Join this game.",{},{MenuOp::LogonAccept},!logonNameDraft_.empty(),false,c.name});
        else if(c.name=="abortButtonPane")view_.items.push_back({c.visibleBounds(),"Cancel joining this game.",{},{MenuOp::LogonCancel},true,false,c.name});
        else if(c.name=="nameEditPane"){
            view_.items.push_back({c.visibleBounds(),"Enter your call sign.",{},{MenuOp::LogonName},true,false,c.name});
            originalEdit(c,logonNameDraft_,"logon",logonNameFocus_);
        }
    }
}
void Frontend::beginDetails(){
    detailsResume_=playing_ && !networkLobby_ && !complete_ && (!docked_ || trainingCommand_) && screen_!=Screen::Research && screen_!=Screen::Teleport;
    detailsReturn_=screen_;detailsBackdrop_=view_;detailsBackdrop_.blockInput();
    if(detailsResume_){detailsBackdrop_={};detailsBackdrop_.original=detailsBackdrop_.overlay=true;}
    pointerAction_.reset();view_.pressed=-1;screen_=Screen::GameDetails;
}
void Frontend::prepareReport(std::string_view text,const UiStrings& strings){
    detailsControls_=original_->layout("mmlpane","mmlPane");detailsList_={};
    auto page=std::find_if(detailsControls_.begin(),detailsControls_.end(),[](const auto& c){return c.name=="mainPage";});
    if(page==detailsControls_.end())throw std::runtime_error("Original report page missing");
    detailsPage_=compileMmlPage(*original_,text,page->bounds.z,strings);
}
void Frontend::connectionInfo(std::span<const Allegiance::PlayerConnectionInfo> rows,const GameState& state){
    if(!original_)return;
    std::unordered_map<int,const LobbyPilot*> roster;for(const auto& pilot:state.networkPilots)roster.emplace(pilot.id,&pilot);
    std::vector<Allegiance::PlayerConnectionInfo> players;UiStrings names;
    for(const auto& row:rows)if(auto found=roster.find(row.pilot);found!=roster.end()){
        players.push_back(row);names.emplace("pilot"+std::to_string(row.pilot),found->second->name);
    }
    sync(state);if(screen_!=Screen::GameDetails)beginDetails();
    prepareReport(Allegiance::PlayerConnectionReport(players,state.shipId),names);rebuild();
}
void Frontend::prepareMissionDetails() {
    auto info=detailsService_?detailsService_():Allegiance::MissionDetails{};
    bool officialCore=original_->assets().officialCore(info.core);info.core=original_->assets().coreName(info.core);
    auto closest=[&](const char* names,std::initializer_list<const char*> tables,std::span<const double> values){
        auto choices=original_->choices("newgamescreen",names,tables);double best=std::numeric_limits<double>::infinity();std::string label;
        for(const auto& choice:choices){if(choice.values.size()!=values.size())continue;double distance=0;
            for(size_t i=0;i<values.size();++i)distance+=std::abs(values[i]-choice.values[i]);
            if(distance<best){best=distance;label=choice.label;}}
        return label;
    };
    auto scalar=[&](const char* names,const char* table,double value){return closest(names,{table},std::span(&value,1));};
    auto maps=original_->choices("newgamescreen","MapTypeNames",{});
    Allegiance::MissionDetailLabels labels{
        !info.customMap.empty()?"Custom Map "+info.customMap:info.map>=0 && info.map<int(maps.size())?maps[info.map].label:"",
        scalar("ConnectivityNames","ConnectivityValues",info.connectivity),
        closest("ResourcesNames",{"ResourcesNeutralMinable","ResourcesPlayerMinable","ResourcesNeutralSpecial","ResourcesPlayerSpecial"},info.resources),
        scalar("TotalMoneyNames","TotalMoneyValues",info.helium),
        scalar("MaxImbalanceNames","MaxImbalanceValues",info.imbalance)};
    std::string text="<Color|yellow><Font|medBoldVerdana>Game details<Font|smallFont><Color|white><p>";UiStrings strings;size_t next=0;
    size_t paragraphIndex=0;
    for(const auto& paragraph:Allegiance::MissionDetailsParagraphs(info,labels)){
        // Keep dynamic values literal while preserving PagePane word wrapping.
        for(size_t at=0;at<paragraph.size();){
            while(at<paragraph.size() && std::isspace(static_cast<unsigned char>(paragraph[at])))++at;if(at==paragraph.size())break;
            auto end=at;while(end<paragraph.size() && !std::isspace(static_cast<unsigned char>(paragraph[end])))++end;
            auto key="detail"+std::to_string(next++);strings.emplace(key,paragraph.substr(at,end-at));text+="<String|"+key+">";at=end;if(officialCore && paragraphIndex==3 && paragraph.substr(0,end)=="Core:")text+="<Color|green>";
        }
        if(officialCore && paragraphIndex==3)text+="<Color|white>";
        text+="<p>";++paragraphIndex;
    }
    prepareReport(text,strings);
}
void Frontend::originalMissionDetails() {
    auto scale=view_.classicScale;detailsBackdrop_.classicScale=scale;view_=detailsBackdrop_;view_.original=true;view_.canvas={800,600};view_.text.clear();view_.slide.reset();view_.scroll.reset();
    view_.blockInput();
    if(detailsControls_.empty())return;
    auto origin=glm::floor((glm::vec2(view_.canvas)-glm::vec2(detailsControls_.front().bounds.z,detailsControls_.front().bounds.w))*.5f);
    detailsBounds_={origin,detailsControls_.front().bounds.z,detailsControls_.front().bounds.w};
    auto artworkBegin=view_.artwork.size();detailsItemsBegin_=view_.items.size();
    glm::vec4 page{};
    for(auto c:detailsControls_){c.translate(origin);
        if(c.name=="mainPage")page=c.bounds;
        else if(c.name=="closeButton")view_.items.push_back({c.visibleBounds(),"Close this report.",{},{MenuOp::DetailsClose},true,false,c.name});
        else if(c.name=="DiscordButton" || c.name=="WikiButton")view_.items.push_back({c.visibleBounds(),c.help,{},{MenuOp::Web,c.name=="DiscordButton"?0:Allegiance::WikiLink},true,false,c.name});
        view_.artwork.push_back(std::move(c));
    }
    detailsList_.configure(int(std::ceil(detailsPage_.height)),1,page);detailsList_.bar=authoredScrollBarBounds(view_.artwork,"mainPage");
    // ScrollingPanePane's authored line size is eight pixels.
    auto first=std::upper_bound(detailsPage_.lines.begin(),detailsPage_.lines.end(),detailsList_.offset,[](float offset,const auto& line){return offset<line.bottom;});
    for(auto line=first;line!=detailsPage_.lines.end() && line->top<detailsList_.offset+page.w;++line)
        for(size_t i=line->first;i<line->last;++i){auto c=detailsPage_.text[i];c.bounds.x+=page.x;c.bounds.y+=page.y-detailsList_.offset;c.clip=page;view_.artwork.push_back(std::move(c));}
    scrollbar(detailsList_,-7);
    auto close=std::find_if(view_.items.begin(),view_.items.end(),[](const auto& i){return i.action.op==MenuOp::DetailsClose;});
    view_.focus=close==view_.items.end()?0:int(close-view_.items.begin());
    // Retain a separate top layer so the original popup stays above independent
    // console panes, without drawing the report or parent twice each frame.
    detailsView_={};detailsView_.original=detailsView_.overlay=true;
    detailsView_.canvas=view_.canvas;detailsView_.classicScale=view_.classicScale;
    detailsView_.artwork.assign(view_.artwork.begin()+artworkBegin,view_.artwork.end());
    detailsView_.items.assign(view_.items.begin()+detailsItemsBegin_,view_.items.end());
}
void Frontend::originalMessageBox(){
    if(messageBoxResume_){
        view_.items.clear();view_.artwork.clear();view_.shipPreview.reset();view_.scroll.reset();view_.slide.reset();
        menuPopupView_={};menuPopupScene_={};detailsView_={};
        view_.original=true;view_.overlay=playing_ && !docked_ && !networkLobby_;
    }
    // A centered modal is independent of any underlying animated console pane.
    if(view_.slide){view_.slide->artworkEnd=view_.artwork.size();view_.slide->itemsEnd=view_.items.size();}
    view_.blockInput();menuPopupView_.blockInput();detailsView_.blockInput();
    messageBackdrop_=view_;auto artworkBegin=view_.artwork.size(),itemsBegin=view_.items.size();
    messageItemsBegin_=itemsBegin;
    auto image=original_->assets().image("dialogbkgndbmp",true);
    auto at=glm::floor((glm::vec2(view_.canvas)-glm::vec2(image->width,image->height))*.5f);
    OriginalControl backdrop;backdrop.kind=OriginalControl::Kind::Image;backdrop.name="messageBackground";backdrop.image=image;backdrop.bounds={at,image->width,image->height};view_.artwork.push_back(backdrop);
    auto font=original_->assets().font("largeFont");
    float height=font->height*Allegiance::ImageTextLines(messageBox_,403,[&](unsigned char c){return font->glyphs[c].z;});
    OriginalControl text;text.kind=OriginalControl::Kind::Text;text.name="messageShadow";text.font="largeFont";text.text=messageBox_;text.textMode=OriginalControl::TextMode::Image;text.bounds={at+glm::vec2(51,51),403,height};text.clip=backdrop.bounds;text.color=glm::vec4(1.f/16,1.f/16,1.f/16,1);view_.artwork.push_back(text);
    text.name="messageText";text.bounds.x-=1;text.bounds.y-=1;text.color=glm::vec4(1);view_.artwork.push_back(std::move(text));
    if(!messageBoxPending_){
        auto ok=original_->assets().image("btnokbmp",true);OriginalControl button;button.kind=OriginalControl::Kind::Button;button.name="messageOK";button.image=ok;button.faces=15;button.clickSound=positiveButtonClickSound;button.hoverSound=mouseoverSound;button.bounds={at+glm::vec2(messageBoxAccept_?110:140,170),ok->width,ok->height/4};view_.artwork.push_back(button);
        view_.items.push_back({button.bounds,messageBoxAccept_?"Confirm this action.":"Close this message.",{}, {MenuOp::MessageDismiss},true,false,button.name});view_.focus=int(view_.items.size()-1);
        if(messageBoxAccept_){
            auto cancel=original_->assets().image("btnabortbmp",true);button.name="messageCancel";button.image=cancel;button.clickSound=negativeButtonClickSound;button.bounds={at+glm::vec2(290,170),cancel->width,cancel->height/4};view_.artwork.push_back(button);
            view_.items.push_back({button.bounds,"Cancel this action.",{}, {MenuOp::MessageDismiss,0,0},true,false,button.name});
        }
        if(messagePurpose_==MessagePurpose::TrainingSuggestion){
            auto checkbox=original_->assets().image("btndontaskmeagainbmp",true);
            button.name="trainingDontAskAgain";button.image=checkbox;button.faces=255;button.clickSound=mouseclickSound;button.toggleOnRelease=true;
            button.bounds={at+glm::vec2(183,140),checkbox->width,checkbox->height/8};view_.artwork.push_back(button);
            view_.items.push_back({button.bounds,"Don't ask me again.",{},{MenuOp::TrainingDontAskAgain},true,messageDontAskAgain_,button.name});
        }
    }
    // Draw the modal after the independent F6/crew/chat overlays, once in the
    // existing HUD batch. Cache the split on UI rebuild, not on rendered frames.
    messageView_={};messageView_.original=messageView_.overlay=true;
    messageView_.canvas=view_.canvas;messageView_.classicScale=view_.classicScale;
    messageView_.artwork.assign(view_.artwork.begin()+artworkBegin,view_.artwork.end());
    messageView_.items.assign(view_.items.begin()+itemsBegin,view_.items.end());
}
void Frontend::originalTeleport(){
    // Keep the ordinary hangar's controls active underneath this console pane.
    screen_=Screen::Hangar;originalLayout();screen_=Screen::Teleport;
    auto controls=original_->layout("teleportpane","TeleportPane",{}, {},true);
    auto background=original_->assets().image("teleportpanebmp",false);
    const auto* overlay=overlays_->get(0,{},glm::vec2(view_.canvas));if(!overlay || !(overlay->mask&(overlayModes_|ofTeleportPane)))return;
    glm::vec2 origin=glm::floor(overlay->on)-glm::vec2(0,background->height);
    view_.slide=overlaySlide(*overlay,teleportEntered_);view_.slide->artworkBegin=view_.artwork.size();view_.slide->itemsBegin=view_.items.size();
    auto listBounds=authoredControlBounds(controls,"teleportPaneDestinationListPane"),barBounds=authoredScrollBarBounds(controls,"teleportPaneDestinationListPane");
    auto columns=original_->integers("teleportpane","teleportPaneDestinationColumns");if(columns.size()!=4)throw std::runtime_error("Original teleport painter requires four columns");
    auto selected=std::find_if(teleports_.begin(),teleports_.end(),[&](const auto& row){return row.id==teleportSelected_ && row.boarding==teleportShipSelected_;});
    if(selected==teleports_.end()){teleportSelected_=-1;teleportList_.selection=-1;}
    else teleportList_.selection=int(selected-teleports_.begin());
    for(auto& c:controls){
        c.translate(origin);
        if(c.kind==OriginalControl::Kind::Button){
            MenuAction action{MenuOp::TeleportHeader};bool enabled=true,bound=true;
            if(c.name=="teleportTeleportButtonPane"){action={MenuOp::TeleportActivate};enabled=docked_ && !complete_;}
            else if(c.name=="teleportBackButtonPane")action={MenuOp::Resume};
            else if(c.name.starts_with("teleportPaneDestinationListHeader")){
                action.value=std::stoi(c.name.substr(std::string_view("teleportPaneDestinationListHeader").size()));
            }else bound=false;
            if(bound)view_.items.push_back({c.visibleBounds(),c.name=="teleportBackButtonPane"?"Close the teleport pane.":action.op==MenuOp::TeleportHeader?"Destination column.":"Teleport to a station or board a ship.",{},action,enabled,false,c.name});
        }
        view_.artwork.push_back(std::move(c));
    }
    listBounds.x+=origin.x;listBounds.y+=origin.y;listBounds.z=std::max(listBounds.z,float(columns[3]));
    teleportList_.configure(int(teleports_.size()),14,listBounds);
    barBounds.x+=origin.x;barBounds.y+=origin.y;teleportList_.bar=barBounds;
    auto clip=teleportList_.viewport;
    view_.items.push_back({clip,"Choose a teleport destination.",{}, {MenuOp::TeleportSelect,-1,0},docked_ && !complete_,false,"teleportBlank"});
    for(int i=int(teleportList_.offset/14);i<int(teleports_.size());++i){
        const auto& destination=teleports_[i];float y=clip.y+i*14-teleportList_.offset;if(y>=clip.y+clip.w)break;
        bool active=destination.id==teleportSelected_ && destination.boarding==teleportShipSelected_;
        OriginalControl row;row.kind=OriginalControl::Kind::Rectangle;row.name="teleport/"+std::string(destination.boarding?"ship/":"station/")+std::to_string(destination.id);
        row.bounds={clip.x,y,clip.z,14};row.color=active?glm::vec4(1,0,0,1):glm::vec4(0,0,0,1);row.clip=clip;view_.artwork.push_back(row);
        auto hit=row.bounds;float bottom=std::min(hit.y+hit.w,clip.y+clip.w);hit.y=std::max(hit.y,clip.y);hit.w=bottom-hit.y;
        view_.items.push_back({hit,destination.name,{}, {MenuOp::TeleportSelect,destination.id,int(destination.boarding)},docked_ && !complete_,active,row.name});
        auto cell=[&](float x,std::string value){
            OriginalControl text;text.kind=OriginalControl::Kind::Text;text.bounds={clip.x+x,y,0,14};text.text=std::move(value);
            text.font=destination.current?"smallBoldFont":"smallFont";text.color=glm::vec4(destination.keepsShip?1.f:.75f,destination.keepsShip?1.f:.75f,destination.keepsShip?1.f:.75f,1);
            text.clip=clip;text.surfaceText();view_.artwork.push_back(std::move(text));
        };
        cell(0,destination.name);if(destination.boarding)cell(columns[0],destination.ship);cell(columns[1]+2,destination.sector);
        if(destination.boarding && destination.turrets)cell(columns[2]+2,std::to_string(destination.manned)+"/"+std::to_string(destination.turrets));
    }
    scrollbar(teleportList_,-1);
}
bool Frontend::originalSecondaryLayout() {
    if(screen_!=Screen::Online)return false;
    auto scale=view_.classicScale;view_=secondaryBackdrop_;view_.classicScale=scale;
    view_.original=true;view_.items.clear();view_.text.clear();view_.pressed=-1;view_.blockInput();
    auto text=[&](glm::vec4 box,std::string value,const char* font){OriginalControl c;c.kind=OriginalControl::Kind::Text;c.bounds=box;c.text=std::move(value);c.font=font;c.surfaceText();view_.artwork.push_back(std::move(c));};
    auto bind=[&](const OriginalControl& c,std::string label,MenuAction a,bool enabled=true){view_.items.push_back({c.visibleBounds(),std::move(label),{},a,enabled,false,c.name});};
    if(screen_==Screen::Online){
        markPopupLayer();
        // FindServerPopup's actual list: case-insensitive game names, fourteen
        // pixel rows, clipped names and the right-hand occupancy column.
        auto controls=original_->layout("introscreen","ServerDialog");
        auto background=original_->assets().image("findserverdialogbmp",false);
        auto at=glm::floor((glm::vec2(800,600)-glm::vec2(background->width,background->height))*.5f);
        for(auto& c:controls){c.translate(at);
            if(c.name=="serverJoinButtonPane")bind(c,"Join the selected server.",{MenuOp::LanJoin},!playing_ && lanList_.selection>=0 && (games_[lanRows_[lanList_.selection]].info.flags&Net::ServerJoinable));
            else if(c.name=="serverCancelButtonPane")bind(c,"Return to the main menu.",{MenuOp::Navigate,int(Screen::Home)});
            view_.artwork.push_back(std::move(c));}
        auto viewport=authoredControlBounds(view_.artwork,"serverListPane");lanList_.configure(int(lanRows_.size()),14,viewport);lanList_.bar=authoredScrollBarBounds(view_.artwork,"serverListPane");
        view_.items.push_back({viewport,"Choose a server.",{},{MenuOp::LanSelect,-1},!playing_,false,"lanBlank"});
        if(lanRows_.empty()){text({viewport.x+2,viewport.y,viewport.z-40,14},"Searching...","smallFont");view_.artwork.back().clip=viewport;}
        for(int i=int(lanList_.offset/14);i<int(lanRows_.size());++i){
            float y=viewport.y+i*14-lanList_.offset;if(y>=viewport.y+viewport.w)break;
            auto& server=games_[lanRows_[i]];bool selected=server.address==lanSelected_;
            OriginalControl row;row.kind=OriginalControl::Kind::Rectangle;row.name="lan/"+server.address;row.bounds={viewport.x,y,viewport.z,14};row.clip=viewport;row.color=selected?glm::vec4(1,0,0,1):glm::vec4(0);view_.artwork.push_back(row);
            view_.items.push_back({row.visibleBounds(),"Select the server.",{},{MenuOp::LanSelect,i},!playing_,selected,row.name});
            text({viewport.x+2,y,viewport.z-40,14},censorChat(server.info.name,settings_.chat.censor),"smallFont");view_.artwork.back().clip=glm::vec4(viewport.x,viewport.y,viewport.z-40,viewport.w);
            text({viewport.x+viewport.z-38,y,38,14},"("+std::to_string(server.info.players)+"/"+std::to_string(server.info.capacity)+")","smallFont");view_.artwork.back().clip=viewport;
        }
        scrollbar(lanList_,-10);
        return true;
    }
    return false;
}

void Frontend::initializeHelp(){
    helpMotion_={};helpView_={};helpView_.original=helpView_.overlay=true;
    helpView_.artwork=original_->layout("helppane","helpPane");helpView_.canvas={1,1};
    for(const auto& c:helpView_.artwork){
        helpView_.canvas=glm::max(helpView_.canvas,glm::ivec2(glm::ceil(glm::vec2(c.bounds)+glm::vec2(c.bounds.z,c.bounds.w))));
        if(c.kind!=OriginalControl::Kind::Button)continue;
        std::optional<MenuAction> action;
        if(c.name=="closeButton")action=MenuAction{MenuOp::SecondaryClose};
        else if(c.name=="DiscordButton")action=MenuAction{MenuOp::Web,0};
        else if(c.name=="WikiButton")action=MenuAction{MenuOp::Web,Allegiance::WikiLink};
        if(action)helpView_.items.push_back({c.visibleBounds(),{}, {},*action,true,false,c.name});
    }
    helpView_.bindControlSounds();MenuSlide slide;slide.closing=true;slide.duration=0;
    slide.screenAnchor=glm::vec2(0,1);helpView_.slide=slide;
}
void Frontend::showHelp(bool forceOn,double time){
    if(!original_)return;
    feedback_.emit(positiveButtonClickSound);helpMotion_.toggle(forceOn,time);
}
const MenuView& Frontend::helpView(int scale,double time){
    helpView_.classicScale=scale;
    if(helpView_.slide)helpView_.slide->off={helpMotion_.x(time),-Allegiance::HelpMotion::y};
    return helpView_;
}
bool Frontend::helpPointerInside(glm::ivec2 size,glm::vec2 pixel)const{
    if(!helpVisible() || pixel.x<0 || pixel.y<0 || pixel.x>=size.x || pixel.y>=size.y)return false;
    auto p=originalMenuLayout(size,helpView_.classicScale,helpView_.canvas).local(pixel)-menuSlideOffset(helpView_,size,uiTime());
    return p.x>=0 && p.y>=0 && p.x<helpView_.canvas.x && p.y<helpView_.canvas.y;
}
void Frontend::helpHover(std::optional<size_t> hit){
    auto* item=hit && *hit<helpView_.items.size()?&helpView_.items[*hit]:nullptr;
    if(helpPointerCaptured())feedback_.capturedHover(helpView_.items[helpView_.pressed],hit && *hit==size_t(helpView_.pressed),int(Screen::Help));
    else feedback_.hover(item,int(Screen::Help));
}
FrontResult Frontend::helpPress(std::optional<size_t> hit,int button,bool doubleClick){
    if(button!=SDL_BUTTON_LEFT && button!=SDL_BUTTON_RIGHT)return {};
    helpView_.pressed=-1;
    if(!hit || *hit>=helpView_.items.size())return {};
    const auto& item=helpView_.items[*hit];
    if(item.captureOnPress){if(!doubleClick)helpView_.pressed=int(*hit);return {};}
    if(!item.enabled || button!=SDL_BUTTON_LEFT)return {};
    feedback_.click(item);
    if(item.action.op==MenuOp::SecondaryClose){helpMotion_.close(uiTime());return {};}
    return {FrontEffect::Web,item.action.value};
}
FrontResult Frontend::helpRelease(std::optional<size_t> hit,int button){
    if(button!=SDL_BUTTON_LEFT && button!=SDL_BUTTON_RIGHT)return {};
    int captured=helpView_.pressed;helpView_.pressed=-1;
    if(button!=SDL_BUTTON_LEFT || captured<0 || !hit || *hit!=size_t(captured) || !helpView_.items[captured].enabled)return {};
    const auto& item=helpView_.items[captured];feedback_.click(item);
    if(item.action.op==MenuOp::SecondaryClose){helpMotion_.close(uiTime());return {};}
    return {FrontEffect::Web,item.action.value};
}

void Frontend::originalCommandMenu() {
    // Compatibility commands whose authored panels are still being restored
    // use the existing classic popup primitive. Never silently enter the
    // experimental sidebar/card frontend from the default game.
    commandRows_.clear();commandRows_.push_back({view_.subtitle,0,{MenuOp::PopupItem},{},false});
    struct Entry {glm::vec2 position;PopupRow row;};std::vector<Entry> entries;
    for(const auto& item:view_.items)if(item.bounds.x>=256){
        std::string label=item.label;if(!item.value.empty())label+="  "+item.value;
        if(label=="<" || label==">"){
            for(const auto& t:view_.text)if(t.position.x>=256 && std::abs(t.position.y-(item.bounds.y+8))<.5f){
                if(t.position.x<565)label=t.text+(item.action.delta<0?": Previous":": Next");
                else label+="  "+t.text;
            }
        }
        entries.push_back({glm::vec2(item.bounds),{std::move(label),0,item.action,{},item.enabled}});
    }
    std::stable_sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.position.y==b.position.y?a.position.x<b.position.x:a.position.y<b.position.y;});
    for(auto& entry:entries)commandRows_.push_back(std::move(entry.row));
    commandRows_.push_back({"Close",0,playing_?MenuAction{MenuOp::Resume}:MenuAction{MenuOp::Navigate,int(classicReturn_)}});
    view_.items.clear();view_.text.clear();view_.original=true;
    popupFlight_=playing_ && !docked_ && !networkLobby_ && !complete_;popupReturn_=classicReturn_;
    view_.overlay=popupFlight_;view_.artwork=popupFlight_?std::vector<OriginalControl>{}:original_->layout(playing_ && docked_?"hangar":"introscreen");
    popupBackdrop_=view_;
    if(popups_.empty() || popups_.front().group!=PopupGroup::Commands){popups_={{PopupGroup::Commands}};popupPosition_={200,40};}
    originalPopup();
}
}
