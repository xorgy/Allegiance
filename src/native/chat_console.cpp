#include "chat_console.hpp"
#include "../ClientCore/AuthoredText.h"
#include <cctype>
#include <limits>

namespace Native {
namespace {
using Allegiance::ComposeState;using Allegiance::ChatRecipient;
ChatRecipient recipientKind(ChatChannel channel) {
    switch(channel) {
    case ChatChannel::Everyone:return ChatRecipient::Everyone;
    case ChatChannel::Team:return ChatRecipient::Team;
    case ChatChannel::Allies:return ChatRecipient::Allies;
    case ChatChannel::FriendlySector:return ChatRecipient::Sector;
    case ChatChannel::Wing:return ChatRecipient::Wing;
    case ChatChannel::Target:return ChatRecipient::Individual;
    case ChatChannel::Ship:return ChatRecipient::Ship;
    default:return ChatRecipient::Other;
    }
}
ChatChannel channelKind(ChatRecipient kind) {
    switch(kind) {
    case ChatRecipient::Everyone:return ChatChannel::Everyone;
    case ChatRecipient::Team:return ChatChannel::Team;
    case ChatRecipient::Allies:return ChatChannel::Allies;
    case ChatRecipient::Sector:return ChatChannel::FriendlySector;
    case ChatRecipient::Wing:return ChatChannel::Wing;
    case ChatRecipient::Individual:return ChatChannel::Target;
    case ChatRecipient::Ship:return ChatChannel::Ship;
    default:return ChatChannel::Team;
    }
}
bool inside(glm::vec4 b,glm::vec2 p){return p.x>=b.x && p.y>=b.y && p.x<b.x+b.z && p.y<b.y+b.w;}
}
ChatConsole::ChatConsole(Assets& assets,const QuickChatCatalog& quick):assets_(assets),original_(assets),overlays_(original_,{"ChatCompositionImage","CommandPaneImage"}),quick_(quick){
    auto command=original_.layout("dialog","CommandPane");wingBounds_=authoredControlBounds(command,"WingCombo");
    for(auto& c:command)commandHeight_=std::max(commandHeight_,c.bounds.y+c.bounds.w);reset();
}
void ChatConsole::reset() {
    mode_=ComposeState::Idle;subject_=1;verb_=0;audiences_={};audiences_[1].none=true;audiences_[2].none=true;
    typed_.clear();label_.clear();display_.clear();target_.reset();popups_.clear();composer_.clear();subjects_.clear();
    view_={};dirty_=contentDirty_=true;compiledMode_=ComposeState::Idle;compiledVerb_=0;closing_=false;ship_=-1;refreshAt_=0;commandView_=false;soundCount_=0;
}
void ChatConsole::start(Game& game,ComposeState mode,int subject) {
    bool wasIdle=mode_==ComposeState::Idle;
    if(wasIdle){subject_=subject;typed_.clear();target_.reset();closing_=false;entered_=uiTime();}
    mode_=mode;
    auto& a=audiences_[subject_];
    auto subjects=game.chatSubjects(a);
    if(a.channel==ChatChannel::Group){a.group=std::move(subjects);if(a.group.size()==1){a.recipient=a.group.front();a.channel=ChatChannel::Target;a.group.clear();}else if(a.group.empty())a={};}
    if(a.channel==ChatChannel::Target && subjects.empty())a={};
    if(mode==ComposeState::Chat && a.channel==ChatChannel::Target) {
        auto peers=game.chatPeers();auto found=std::find_if(peers.begin(),peers.end(),[&](auto& p){return p.id==a.recipient;});
        if(found!=peers.end() && found->type<playerPilot)a={};
    }
    update(game,mode==ComposeState::Command);
}
void ChatConsole::startChat(Game& game){sound(chatKeySound);start(game,ComposeState::Chat,0);popups_.clear();dirty_=true;}
void ChatConsole::startQuick(Game& game){start(game,ComposeState::Quick,0);popups_={{0,0,{}}};dirty_=true;}
void ChatConsole::quit() {
    if(active()){closing_=true;entered_=uiTime();}
    mode_=ComposeState::Idle;subject_=1;typed_.clear();target_.reset();popups_.clear();dirty_=true;
}
void ChatConsole::verb(Game& game,int command) {
    if(mode_==ComposeState::Shell || command<1 || command>9)return;
    start(game,ComposeState::Command,1);verb_=command;game.commandOrder(command);sound(chatKeySound);update(game,true);
}
void ChatConsole::setAudience(Game& game,ChatAudience value){audiences_[subject_]=std::move(value);update(game,mode_==ComposeState::Command);}
void ChatConsole::recipient(Game& game,int button) {
    if(mode_==ComposeState::Shell)return;
    if(!active())start(game,ComposeState::Chat,0);
    // ConsoleData's recipient bar picks self before changing the chat route.
    game.pickCommandUnit(game.state().shipId);ChatAudience value;
    switch(button) {
    case 0:value.channel=ChatChannel::Target;value.recipient=game.state().shipId;break;
    case 1:value.channel=ChatChannel::FriendlySector;value.recipient=game.state().sectorId;break;
    case 2:value.channel=ChatChannel::Team;break;
    case 3:value.channel=ChatChannel::Everyone;break;
    default:if(button<4 || button>=14)return;value.channel=ChatChannel::Wing;value.recipient=button-4;break;
    }
    setAudience(game,std::move(value));
}
void ChatConsole::wings(Game& game) {
    if(mode_==ComposeState::Shell)return;
    if(!active())start(game,ComposeState::Chat,0);
    popups_={{-1,std::max(0,game.state().wing),{}}};dirty_=true;
}
void ChatConsole::selectUnits(Game& game) {
    if(!active())start(game,ComposeState::Mouse,1);
    ChatAudience a;a.group.assign(game.commandUnits().begin(),game.commandUnits().end());
    if(a.group.empty()){a.none=true;audiences_[subject_]=std::move(a);quit();update(game);return;}
    if(a.group.size()>1){a.channel=ChatChannel::Group;a.recipient=a.group.front();}
    else if(!a.group.empty()){a.channel=ChatChannel::Target;a.recipient=a.group.front();a.group.clear();}
    setAudience(game,std::move(a));
}
bool ChatConsole::selectPeer(Game& game,int id) {
    auto peers=game.chatPeers();if(std::none_of(peers.begin(),peers.end(),[&](auto& p){return p.id==id;}))return false;
    if(!active())start(game,ComposeState::Mouse,1);
    ChatAudience value;value.channel=ChatChannel::Target;value.recipient=id;setAudience(game,std::move(value));return true;
}
void ChatConsole::pickCluster(Game& game,int sector) {
    if(!active()){startChat(game);ChatAudience a;a.channel=ChatChannel::FriendlySector;a.recipient=sector;setAudience(game,std::move(a));return;}
    mode_=ComposeState::Command;verb_=5;sendPosition(game,sector,{},true);quit();update(game);
}
bool ChatConsole::sendTarget(Game& game,int type,int id) {
    return dispatch(game,[&](ChatChannel channel,int recipient){return game.sendChatOrder(channel,recipient,verb_,type,id);});
}
bool ChatConsole::sendPosition(Game& game,int sector,glm::vec3 position,bool cluster) {
    return dispatch(game,[&](ChatChannel channel,int recipient){return game.sendChatWaypoint(channel,recipient,verb_,sector,position,cluster);});
}
bool ChatConsole::pickTarget(Game& game,int type,int id) {
    if(mode_==ComposeState::Shell)return false;
    auto candidates=game.chatTargets(audience(),mode_==ComposeState::Command?verb_:0);
    if(std::none_of(candidates.begin(),candidates.end(),[&](auto& candidate){return candidate.type==type && candidate.id==id;}))return false;
    if(mode_!=ComposeState::Command){if(!active())start(game,ComposeState::Mouse,1);verb_=0;}
    bool sent=sendTarget(game,type,id);quit();update(game);return sent;
}
bool ChatConsole::pickPosition(Game& game,int sector,glm::vec3 position) {
    if(mode_==ComposeState::Shell)return false;
    bool layer=false;auto peers=game.chatPeers();for(int id:game.chatSubjects(audience()))for(auto& peer:peers)if(peer.id==id && peer.type==5)layer=true;
    if(mode_==ComposeState::Command && verb_!=5 && !(verb_==9 && layer))return false;
    if(!active())start(game,ComposeState::Mouse,1);
    if(mode_!=ComposeState::Command)verb_=5;
    bool sent=sendPosition(game,sector,position,false);quit();update(game);return sent;
}
void ChatConsole::cycle(Game& game) {
    ChatAudience sector;sector.channel=ChatChannel::FriendlySector;sector.recipient=game.state().sectorId;
    bool occupied=!game.chatSubjects(sector).empty(),crew=false,allies=false;
    auto peers=game.chatPeers();for(auto& p:peers)crew=crew || (p.id==game.state().shipId && p.crew);
    // The alliance relationship exists even if the allied team has no ships.
    auto relations=Game::effectiveAlliances(game.sessionOptions());int side=game.state().playerSide;
    if(side>=0 && side<int(relations.size()) && relations[side]>=0)for(size_t i=0;i<relations.size();++i)
        if(int(i)!=side && relations[i]==relations[side])allies=true;
    auto kind=Allegiance::NextChatRecipient(audience().none?ChatRecipient::Other:recipientKind(audience().channel),mode_==ComposeState::Chat,allies,crew,occupied);
    ChatAudience value;value.channel=channelKind(kind);
    if(kind==ChatRecipient::Individual)value.recipient=game.state().shipId;
    if(kind==ChatRecipient::Sector)value.recipient=game.state().sectorId;
    setAudience(game,std::move(value));
}
void ChatConsole::tab(Game& game) {
    if(typed_.empty() || typed_[0]==' '){cycle(game);return;}
    int best=Allegiance::badChatMatch;ChatAudience candidate;
    auto match=[&](std::string_view name,ChatChannel channel,int id=-1){int score=Allegiance::ChatPrefixScore(typed_,name);
        if(score<best){best=score;candidate={channel,id};}};
    if(Allegiance::ChatPrefixScore(typed_,"admin")==0){candidate.channel=ChatChannel::Admin;best=0;}
    for(int i=0;i<10 && best>0;++i)match(Game::wingName(i),ChatChannel::Wing,i);
    for(auto& sector:game.state().sectors)if(best>0)match(sector.name,ChatChannel::FriendlySector,sector.id);
    if(best>0)match(game.chatAudienceName({}),ChatChannel::Team);
    if(best>0)match("me",ChatChannel::Target,game.state().shipId);
    if(best>0)match("all",ChatChannel::Everyone);
    auto peers=game.chatPeers();bool crew=false;for(auto& p:peers)if(p.id==game.state().shipId)crew=p.crew;
    if(best>0 && crew)match("ship",ChatChannel::Ship);
    auto relations=Game::effectiveAlliances(game.sessionOptions());int side=game.state().playerSide;
    auto allied=[&](int other){return other==side || (side>=0 && other>=0 && side<int(relations.size()) && other<int(relations.size()) && relations[side]>=0 && relations[side]==relations[other]);};
    bool hasAllies=false;for(int i=0;i<game.sessionOptions().rules.teams;++i)hasAllies=hasAllies || (i!=side && allied(i));
    if(best>0 && hasAllies)match("allies",ChatChannel::Allies);
    for(auto& p:peers)if(best>0 && allied(p.side))match(p.name,ChatChannel::Target,p.id);
    for(auto& p:peers)if(best>0 && !allied(p.side) && p.type>=playerPilot) {
        int score=Allegiance::ChatPrefixScore(typed_,p.name);
        if(candidate.channel==ChatChannel::Target ? score==0 : score<best){best=score;candidate={ChatChannel::Target,p.id};}
    }
    if(best>0)for(int i=1;i<=9;++i)if(Allegiance::ChatPrefixScore(typed_,Allegiance::chatVerbs[i])==0) {
        typed_.clear();target_.reset();verb(game,i);return;
    }
    if(best!=Allegiance::badChatMatch){typed_.clear();target_.reset();setAudience(game,std::move(candidate));}
    else cycle(game);
}
void ChatConsole::send(Game& game) {
    if(typed_.empty()){quit();update(game);return;}
    if(mode_==ComposeState::Command){if(target_) {
        if(target_->type==-2)sendPosition(game,target_->id,{},true);else sendTarget(game,target_->type,target_->id);
    }} else dispatch(game,[&](ChatChannel channel,int recipient){return game.sendChat(channel,typed_,recipient);});
    quit();update(game);
}
bool ChatConsole::key(Game& game,SDL_Keycode key,bool control) {
    if(!keyboard())return false;sound(chatKeySound);
    if(!popups_.empty()) {
        auto& p=popups_.back();int count=p.menu<0?10:int(quick_.menus[p.menu].items.size());
        if(key==SDLK_ESCAPE){if(popups_.size()>1)popups_.pop_back();else popupClose();dirty_=true;return true;}
        if(key==SDLK_LEFT || key==SDLK_BACKSPACE){if(popups_.size()>1){popups_.pop_back();dirty_=true;}return true;}
        if(key==SDLK_UP || key==SDLK_DOWN){p.selected=(p.selected+(key==SDLK_UP?-1:1)+count)%count;dirty_=true;return true;}
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER || key==SDLK_RIGHT){popupSelect(game,int(popups_.size()-1),p.selected);return true;}
        if(key==SDLK_TAB){cycle(game);return true;}
        if(p.menu>=0 && key<128)for(int i=0;i<count;++i)if(std::tolower(uint8_t(quick_.menus[p.menu].items[i].key))==std::tolower(int(key))){popupSelect(game,int(popups_.size()-1),i);return true;}
        return true;
    }
    if(control && mode_!=ComposeState::Command && typed_.empty()) {
        for(int i=1;i<=9;++i)if(key==uint8_t(Allegiance::chatVerbs[i][0])){verb(game,i);break;}return true;
    }
    switch(key) {
    case SDLK_RETURN:case SDLK_KP_ENTER:send(game);break;
    case SDLK_ESCAPE:if(mode_==ComposeState::Command){typed_.clear();target_.reset();mode_=ComposeState::Chat;update(game);}else {quit();update(game);}break;
    case SDLK_BACKSPACE:
        if(mode_==ComposeState::Shell && typed_.size()==1){typed_.clear();mode_=ComposeState::Chat;}
        else if(!typed_.empty())Allegiance::EraseEditCharacter(typed_);
        else if(mode_==ComposeState::Command){mode_=ComposeState::Chat;target_.reset();}
        update(game,mode_==ComposeState::Command);break;
    case SDLK_TAB:tab(game);break;
    case SDLK_GRAVE:quit();startQuick(game);break;
    default:break;
    }return true;
}
void ChatConsole::text(Game& game,std::string_view text) {
    if(!wantsText())return;
    while(!text.empty()){
        auto count=Allegiance::EditInputBytes(text);auto ch=static_cast<unsigned char>(text.front());auto character=text.substr(0,count);text.remove_prefix(count);
        if(ch<32 || ch==127)continue;if(typed_.size()>=255 || count>255-typed_.size())break;
        if(ch=='`' || ch=='~'){quit();startQuick(game);return;}
        if(ch=='!' && mode_==ComposeState::Chat && typed_.empty())mode_=ComposeState::Shell;
        typed_.append(character);
    }
    update(game,mode_==ComposeState::Command);
}
void ChatConsole::update(Game& game,bool match) {
    auto modes=originalOverlayModes(game.state());if(overlayModes_!=modes){overlayModes_=modes;dirty_=true;}
    subjects_=game.chatSubjects(audience());
    if(match){target_.reset();int best=Allegiance::badChatMatch;
        if(!typed_.empty())for(auto& target:game.chatTargets(audience(),verb_)) {
            int score=Allegiance::ChatPrefixScore(typed_,target.name);if(score<best){best=score;target_=std::move(target);}
        }
    }
    auto label=game.chatAudienceName(audience());std::string display=typed_+"|";
    if(mode_==ComposeState::Command && target_ && target_->name.size()>typed_.size()) {
        auto suffix=target_->name.substr(typed_.size());for(char& c:suffix)c=char(std::toupper(uint8_t(c)));display+=suffix;
    }
    auto color=game.state().playerSide>=0?game.state().sideColors[game.state().playerSide]:glm::vec4(1);
    if(label_!=label || display_!=display || color_!=color){label_=std::move(label);display_=std::move(display);color_=color;dirty_=contentDirty_=true;}
    verbs_=game.chatVerbs(audience());if(compiledMode_!=mode_ || compiledVerb_!=verb_)dirty_=true;
}
void ChatConsole::sync(Game& game) {
    if(ship_!=game.state().shipId){reset();ship_=game.state().shipId;}
    if(commandView_!=game.state().commandView){commandView_=game.state().commandView;audiences_[1]={ChatChannel::Target,ship_};
        if(mode_==ComposeState::Mouse)quit();update(game);}
    auto modes=originalOverlayModes(game.state());if(overlayModes_!=modes){overlayModes_=modes;dirty_=true;}
    if(uiTime()<refreshAt_)return;refreshAt_=uiTime()+.5;
    if(active() || commandView_) {
        auto subjects=game.chatSubjects(audience());auto& a=audiences_[subject_];
        if(a.channel==ChatChannel::Group){a.group=subjects;if(subjects.size()==1){a.channel=ChatChannel::Target;a.recipient=subjects[0];a.group.clear();}else if(subjects.empty())a={};}
        else if(a.channel==ChatChannel::Target && subjects.empty())a={};
        update(game,mode_==ComposeState::Command);
    }
}
int ChatConsole::recipientButton()const {
    if(audience().none)return -1;
    switch(audience().channel){case ChatChannel::Target:return audience().recipient==ship_?0:-1;
    case ChatChannel::FriendlySector:return 1;case ChatChannel::Team:return 2;case ChatChannel::Everyone:return 3;default:return -1;}
}
void ChatConsole::popupClose(){bool quick=mode_==ComposeState::Quick;popups_.clear();if(quick)quit();dirty_=true;}
void ChatConsole::popupSelect(Game& game,int depth,int row) {
    if(depth<0 || size_t(depth)>=popups_.size())return;auto p=popups_[depth];
    if(p.menu<0){popups_.clear();recipient(game,row+4);return;}
    if(row<0 || size_t(row)>=quick_.menus[p.menu].items.size())return;
    auto item=quick_.menus[p.menu].items[row];popups_.resize(depth+1);popups_.back().selected=row;
    if(item.menu>=0){popups_.push_back({item.menu,0,{}});dirty_=true;}
    else if(item.command>=0){dispatch(game,[&](ChatChannel channel,int recipient){return game.sendQuickChat(channel,quick_.commands[item.command],recipient);});quit();update(game);}
}
bool ChatConsole::pointer(Game& game,glm::ivec2 size,glm::vec2 pixel,bool down) {
    if(popups_.empty())return false;auto p=originalMenuLayout(size,view_.classicScale,view_.canvas).local(pixel);
    for(size_t i=view_.items.size();i-->0;)if(inside(view_.items[i].bounds,p)) {
        // MenuItemPane executes the released row, including a drag from the
        // opening combo or another row; it requires no matching down event.
        auto& item=view_.items[i];if(!down)popupSelect(game,item.popupLevel,item.popupRow);
        return true;
    }
    if(down && std::none_of(popups_.begin(),popups_.end(),[&](const auto& popup){return inside(popup.bounds,p);}))popupClose();return true;
}
void ChatConsole::hover(glm::ivec2 size,glm::vec2 pixel) {
    if(popups_.empty())return;auto p=originalMenuLayout(size,view_.classicScale,view_.canvas).local(pixel);
    for(auto& item:view_.items)if(inside(item.bounds,p)) {
        auto& popup=popups_[item.popupLevel];
        if(popup.selected!=item.popupRow){popup.selected=item.popupRow;popups_.resize(item.popupLevel+1);dirty_=true;}
        if(popup.menu>=0){auto child=quick_.menus[popup.menu].items[item.popupRow].menu;
            if(child>=0 && popups_.size()==size_t(item.popupLevel+1)){popups_.push_back({child,0,{}});dirty_=true;}}
        return;
    }
}
void ChatConsole::popupLayout() {
    auto font=assets_.font("smallFont");float rowHeight=std::max(11.f,float(font->height))+2;
    auto measure=[&](std::string_view s){return Allegiance::TextExtent(s,[&](unsigned char ch){return font->glyphs[ch].z;});};
    auto rect=[&](glm::vec4 b,glm::vec4 color){OriginalControl c;c.kind=OriginalControl::Kind::Rectangle;c.bounds=b;c.color=color;view_.artwork.push_back(std::move(c));};
    auto text=[&](glm::vec4 b,std::string value){OriginalControl c;c.kind=OriginalControl::Kind::Text;c.font="smallFont";c.bounds=b;c.text=std::move(value);c.surfaceText();view_.artwork.push_back(std::move(c));};
    for(size_t depth=0;depth<popups_.size();++depth) {
        auto& popup=popups_[depth];int count=popup.menu<0?10:int(quick_.menus[popup.menu].items.size());
        auto label=[&](int row)->std::string {return popup.menu<0?std::string(Game::wingName(row)):quick_.menus[popup.menu].items[row].label;};
        auto key=[&](int row)->char {return popup.menu<0?0:quick_.menus[popup.menu].items[row].key;};
        float labelWidth=0,keyWidth=font->glyphs[' '].z;for(int row=0;row<count;++row){labelWidth=std::max(labelWidth,measure(label(row)));if(key(row))keyWidth=std::max(keyWidth,measure(std::string(1,key(row))));}
        float width=11+labelWidth+2+keyWidth+9+2,height=count*rowHeight;
        glm::vec2 at{10,float(view_.canvas.y)-10-height-3};
        if(popup.menu<0)if(auto overlay=overlays_.get(1,{},glm::vec2(view_.canvas))){
            at=glm::floor(overlay->on)+glm::vec2(wingBounds_.x,wingBounds_.y+wingBounds_.w-commandHeight_);
        }
        if(depth){auto& parent=popups_[depth-1];at={parent.bounds.x+parent.bounds.z-1,parent.bounds.y+1+parent.selected*rowHeight};}
        at=glm::clamp(at,glm::vec2(0),glm::max(glm::vec2(view_.canvas)-glm::vec2(width+3,height+3),glm::vec2(0)));
        popup.bounds={at,width+3,height+3};rect(popup.bounds,{0,0,0,1});rect({at,width+2,height+2},{.5f,.5f,.5f,1});rect({at,width+2,1},glm::vec4(1));rect({at,1,height+2},glm::vec4(1));
        for(int row=0;row<count;++row) {
            glm::vec2 p=at+glm::vec2(1,1+row*rowHeight);bool selected=row==popup.selected;
            rect({p,width,rowHeight},selected?glm::vec4(197.f/255,0,41.f/255,1):glm::vec4(33.f/255,20.f/255,30.f/255,1));
            text({p+glm::vec2(12,1),labelWidth,rowHeight-2},label(row));
            if(key(row))text({p+glm::vec2(12+labelWidth+2+keyWidth-measure(std::string(1,key(row))),1),keyWidth,rowHeight-2},std::string(1,key(row)));
            if(popup.menu>=0 && quick_.menus[popup.menu].items[row].menu>=0) {
                auto arrow=p+glm::vec2(width-10,(rowHeight-9)*.5f);constexpr glm::vec4 bars[]={{1,1,1,7},{2,2,1,5},{3,2,1,5},{4,3,1,3},{5,3,1,3},{6,4,1,1}};
                for(auto bar:bars)rect({arrow+glm::vec2(bar),bar.z,bar.w},{74.f/255,124.f/255,88.f/255,1});
            }
            MenuItem hit;hit.bounds={p,width,rowHeight};hit.label=label(row);hit.popupLevel=int(depth);hit.popupRow=row;view_.items.push_back(std::move(hit));
        }
    }
}
void ChatConsole::rebuild() {
    view_={};view_.original=true;view_.overlay=true;view_.classicScale=scale_;view_.canvas=size_/scale_;
    if(active() && (contentDirty_ || compiledMode_!=mode_ || compiledVerb_!=verb_)) {
        composer_=original_.layout("dialog","ComposingPane",{{"ComposeState",int(mode_)},{"CommandVerb",verb_}},{{"ChatRecipient",label_},{"ChatText",display_}},true);
        for(auto& c:composer_)if(c.kind==OriginalControl::Kind::Text)c.color=color_;contentDirty_=false;
    }
    if(active() || closing_) {
        auto overlay=overlays_.get(0,{},glm::vec2(view_.canvas));
        if(overlay && (overlay->mask&overlayModes_)){
            view_.artwork=composer_;float height=0;for(auto& c:composer_)height=std::max(height,c.bounds.y+c.bounds.w);
            glm::vec2 at=glm::floor(overlay->on)-glm::vec2(0,height);
            for(auto& c:view_.artwork){c.bounds.x+=at.x;c.bounds.y+=at.y;if(c.clip){c.clip->x+=at.x;c.clip->y+=at.y;}}
            view_.slide=overlaySlide(*overlay,entered_,closing_);view_.slide->artworkEnd=view_.artwork.size();
        }
    }
    popupLayout();compiledMode_=mode_;compiledVerb_=verb_;dirty_=false;
}
const MenuView& ChatConsole::view(glm::ivec2 size,int scale) {
    scale=originalInterfaceScale(size,scale);
    if(size_!=size || scale_!=scale){size_=size;scale_=scale;dirty_=true;}
    if(closing_ && uiTime()-entered_>=overlays_.duration(0)){closing_=false;composer_.clear();dirty_=true;}
    if(dirty_)rebuild();return view_;
}
}
