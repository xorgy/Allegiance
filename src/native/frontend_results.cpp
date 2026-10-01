#include "frontend.hpp"
#include "chat_censor.hpp"
#include "text_wrap.hpp"
#include "../Igc/igc.h"
#include "../ClientCore/MissionLifecycle.h"
#include <numeric>
namespace Native {
FrontResult Frontend::sendChatAction()const {
    if(screen_==Screen::Results || (original_ && (screen_==Screen::RemoteLobby || screen_==Screen::Pilots || screen_==Screen::LobbyPilots))){
        if(chat_.starts_with("!"))return {FrontEffect::SendChat,int(ChatChannel::Everyone)};
        auto colon=chat_.find(':');if(colon!=std::string::npos){auto name=std::string_view(chat_).substr(0,colon);
            if(!Allegiance::EndgameNameCompare(name,"admin"))return {FrontEffect::SendChat,int(ChatChannel::Admin)};
            for(auto& p:networkPilots_)if(!Allegiance::EndgameNameCompare(name,p.name))return {FrontEffect::SendChat,int(ChatChannel::Target),0,-1,p.id};
            for(auto& p:pilots_)if(!Allegiance::EndgameNameCompare(name,p.name))return {FrontEffect::SendChat,int(ChatChannel::Target),0,-1,p.id};
        }if(screen_==Screen::Results)return {FrontEffect::SendChat,int(ChatChannel::Everyone)};
    }
    return {FrontEffect::SendChat,lobbyRecipient_>=0?int(ChatChannel::Target):chatChannel_,0,-1,lobbyRecipient_>=0?lobbyRecipient_:chatChannel_==int(ChatChannel::FriendlySector)?chatSector_:chatChannel_==int(ChatChannel::Target) && targetType_==ContactShip?targetId_:-1};
}
std::string Frontend::outgoingChatText()const {
    auto action=sendChatAction();if(chat_.find(':')!=std::string::npos && (action.value==int(ChatChannel::Target) || action.value==int(ChatChannel::Admin))){
        auto name=std::string_view(chat_).substr(0,chat_.find(':'));
        if(!Allegiance::EndgameNameCompare(name,"admin"))return chat_.substr(chat_.find(':')+1);
        for(const auto& p:networkPilots_)if(p.id==action.targetId && !Allegiance::EndgameNameCompare(name,p.name))return chat_.substr(chat_.find(':')+1);
        for(const auto& p:pilots_)if(p.id==action.targetId && !Allegiance::EndgameNameCompare(name,p.name))return chat_.substr(chat_.find(':')+1);
    }return chat_;
}
void Frontend::originalResults(){
    view_.original=true;view_.overlay=false;view_.items.clear();view_.text.clear();
    view_.artwork=original_->layout("gameoverscreen","screen",{{"SquadGame",0}},{{"headerName",endgame_.pods?"headerendplayersejectsbmp":"headerendplayersbmp"}});
    auto bind=[&](const OriginalControl& c,std::string label,MenuAction action,bool enabled=true,bool selected=false){view_.items.push_back({c.visibleBounds(),std::move(label),{},action,enabled,selected,c.name});};
    auto text=[&](std::string name,glm::vec4 bounds,std::string value,const char* font,glm::vec4 color=glm::vec4(1),std::optional<glm::vec4> clip={}){
        OriginalControl c;c.kind=OriginalControl::Kind::Text;c.name=std::move(name);c.bounds=bounds;c.text=std::move(value);c.font=font;c.color=color;c.surfaceText();c.clip=clip;view_.artwork.push_back(std::move(c));};
    constexpr const char* playerHelp[]={"Shows where the player is ranked by the current sort order.","Shows the name of the player.","Shows the player's rank at the end of the game.","Shows the number of stations the player destroyed during the game.","Shows the number of stations the player captured during the game.","Shows the number of points the player earned during the game.","Shows the number of kills where the player inflicted the more damage than all but one of the other attackers.","Shows the number of kills where the player inflicted the more damage than any of the other attackers.","Shows the number of times the player's ship was destroyed.","Shows the amount of time that the player played, in hours and minutes (red if the win/loss didn't count)."};
    constexpr const char* teamHelp[]={"Shows the ranking of the team by the current sort order.","Shows the name of the team.","Shows the number of stations the team destroyed during the game.","Shows the number of stations the team captured during the game.","Shows the number of flags the team captured during the game.","Shows the number of artifacts team recovered during the game.","Shows the number of kills the team scored during the game.","Shows the number of times team member's ships were destroyed."};
    auto alliances=Game::effectiveAlliances(session_);bool won=postgameSide_==endgame_.winner || (postgameSide_>=0 && endgame_.winner>=0 && alliances[postgameSide_]>=0 && alliances[postgameSide_]==alliances[endgame_.winner]);
    for(auto& c:view_.artwork){
        if(c.name=="textTitle")c.text=endgame_.winner<0 || postgameSide_<0?"":won?"You are victorious!":"You have been defeated.";
        else if(c.name=="textReason")c.text=censorChat(postgameReason_,settings_.chat.censor);
        else if(c.name=="textCountdown"){int left=std::max(0,networkCountdown_);bool timer=missionStage_==STAGE_STARTING || (missionStage_==STAGE_NOTSTARTED && (lobbyFlags_&Allegiance::LobbyAutoRestart));c.text=timer && networkCountdown_>=0?(left>=60?std::to_string(left/60)+(left%60<10?":0":":")+std::to_string(left%60):std::to_string(left)):"";}
        else if(c.name=="textGameCounted")c.text.clear();
        else if(c.name=="backButtonPane")bind(c,"Return to the game lobby",{MenuOp::ResultsBack});
        else if(c.name=="saveButtonPane")bind(c,"Save scores to a XML file",{MenuOp::SaveStats},endgame_.frozen);
        else if(c.name=="sendButtonPane")bind(c,"Send your message.",{MenuOp::SendChat},!chat_.empty());
        else if(c.name=="chatEditPane")bind(c,"Enter a chat message.",{MenuOp::ComposeChat});
        else if(c.name.starts_with("statsListHeader")){
            int col=c.name.back()-'0';bind(c,playerHelp[col],{MenuOp::ResultsSort,col,0},true,col==resultsPlayerSort_);
            // Button3State: normal, checked, checked2. Preserve both original
            // directions, including rank-column clicks toggling current sort.
            if(col==resultsPlayerSort_)c.checkedState=resultsPlayerReverse_?1:2;
        }else if(c.name.starts_with("statsSideListHeader")){
            int col=c.name.back()-'0';bind(c,teamHelp[col],{MenuOp::ResultsSort,col,1},true,col==resultsTeamSort_);
            if(col==resultsTeamSort_)c.checkedState=resultsTeamReverse_?1:2;
        }
    }
    if(resultsDirty_){
        resultsPlayers_.resize(endgame_.players.size());std::iota(resultsPlayers_.begin(),resultsPlayers_.end(),0);
        resultsTeams_.resize(endgame_.teams.size());std::iota(resultsTeams_.begin(),resultsTeams_.end(),0);
        std::stable_sort(resultsPlayers_.begin(),resultsPlayers_.end(),[&](size_t a,size_t b){return Allegiance::EndgamePlayerLess(endgame_.players[a],endgame_.players[b],resultsPlayerSort_,resultsPlayerReverse_,endgame_.pods);});
        std::stable_sort(resultsTeams_.begin(),resultsTeams_.end(),[&](size_t a,size_t b){return Allegiance::EndgameTeamLess(endgame_.teams[a],endgame_.teams[b],resultsTeamSort_,resultsTeamReverse_);});resultsDirty_=false;
    }
    auto playerColumns=original_->integers("gameoverscreen","statsColumns"),teamColumns=original_->integers("gameoverscreen","statsSideColumns");
    if(playerColumns.size()!=10 || teamColumns.size()!=8 || !std::is_sorted(playerColumns.begin(),playerColumns.end()) || !std::is_sorted(teamColumns.begin(),teamColumns.end()))throw std::runtime_error("Invalid original results columns");
    resultsList_.configure(int(resultsPlayers_.size()),14,authoredListBounds(view_.artwork,"statsListPane",playerColumns.back()));resultsList_.bar=authoredScrollBarBounds(view_.artwork,"statsListPane");
    auto playerViewport=resultsList_.viewport;
    listBlank(playerViewport,MenuOp::ResultsSelect);
    auto color=[&](int side){auto t=std::find_if(endgame_.teams.begin(),endgame_.teams.end(),[&](auto& t){return t.id==side;});return t==endgame_.teams.end()?glm::vec4(1):glm::vec4(t->color[0],t->color[1],t->color[2],t->color[3]);};
    auto rectangle=[&](std::string name,glm::vec4 bounds,std::optional<glm::vec4> clip){OriginalControl c;c.kind=OriginalControl::Kind::Rectangle;c.name=std::move(name);c.bounds=bounds;c.clip=clip;c.color={1,0,0,1};view_.artwork.push_back(std::move(c));};
    for(int i=int(resultsList_.offset/14);i<int(resultsPlayers_.size());++i){
        auto& p=endgame_.players[resultsPlayers_[i]];float y=playerViewport.y+i*14-resultsList_.offset;if(y>=playerViewport.y+playerViewport.w)break;auto clip=playerViewport;
        if(p.id==resultsPlayerSelected_)rectangle("selectedResultPlayer",{clip.x,y,clip.z,14},clip);
        OriginalControl hit;hit.name="resultPlayer/"+std::to_string(p.id);hit.bounds={clip.x,std::max(y,clip.y),clip.z,std::min(y+14,clip.y+clip.w)-std::max(y,clip.y)};bind(hit,"",{MenuOp::ResultsSelect,p.id,0},true,p.id==resultsPlayerSelected_);
        auto cell=[&](int x,int width,std::string value,const char* font="smallFont",glm::vec4 tint=glm::vec4(1),bool columnClip=false){
            auto bounds=glm::vec4(clip.x+x,y,width,14);auto surface=glm::vec4(clip.x,std::max(y,clip.y),clip.z,std::min(y+14,clip.y+clip.w)-std::max(y,clip.y));
            if(columnClip){surface.x=bounds.x;surface.z=width;}text(hit.name+"/"+std::to_string(x),bounds,std::move(value),font,tint,surface);};
        cell(2,playerColumns[0]-2,std::to_string(i+1),"smallFont",glm::vec4(i?glm::vec3(.75f):glm::vec3(1),1));
        cell(playerColumns[0]+4,playerColumns[1]-playerColumns[0]-4,p.name,p.flags&1?"smallBoldFont":"smallFont",color(p.side),true);
        // Native ranks have no authenticated persistent source yet.
        cell(playerColumns[1]+4,playerColumns[2]-playerColumns[1]-4,"","smallFont",glm::vec4(1),true);
        cell(playerColumns[2]+7,playerColumns[3]-playerColumns[2]-7,std::to_string(p.baseKills));cell(playerColumns[3]+5,playerColumns[4]-playerColumns[3]-5,std::to_string(p.baseCaptures));cell(playerColumns[4]+5,playerColumns[5]-playerColumns[4]-5,std::to_string(int(p.score)));
        cell(playerColumns[5]+5,playerColumns[6]-playerColumns[5]-5,std::to_string(p.assists));cell(playerColumns[6]+5,playerColumns[7]-playerColumns[6]-5,std::to_string(p.kills));cell(playerColumns[7]+9,playerColumns[8]-playerColumns[7]-9,std::to_string(endgame_.pods?p.ejections:p.deaths));
        cell(playerColumns[8]+5,playerColumns[9]-playerColumns[8]-5,Allegiance::EndgamePlayTime(p.played),"smallFont",p.flags&2?glm::vec4(1):glm::vec4(1,0,0,1));
    }
    scrollbar(resultsList_,-3);
    auto teamViewport=authoredListBounds(view_.artwork,"statsSideListPane",teamColumns.back());
    listBlank(teamViewport,MenuOp::ResultsSelect,1);
    for(size_t i=0;i<resultsTeams_.size();++i){auto& p=endgame_.teams[resultsTeams_[i]];float y=teamViewport.y+i*14;auto clip=teamViewport;if(y>=clip.y+clip.w)break;
        if(p.id==resultsTeamSelected_)rectangle("selectedResultTeam",{clip.x,y,clip.z,14},clip);
        OriginalControl hit;hit.name="resultTeam/"+std::to_string(p.id);hit.bounds={clip.x,y,clip.z,std::min(14.f,clip.y+clip.w-y)};bind(hit,"",{MenuOp::ResultsSelect,p.id,1},true,p.id==resultsTeamSelected_);
        auto cell=[&](int x,int right,std::string value,glm::vec4 tint=glm::vec4(1),bool columnClip=false){
            glm::vec4 surface{clip.x,y,clip.z,std::min(14.f,clip.y+clip.w-y)};if(columnClip){surface.x=clip.x+x;surface.z=right-x;}
            text(hit.name+"/"+std::to_string(x),{clip.x+x,y,right-x,14},std::move(value),"smallFont",tint,surface);};
        cell(2,teamColumns[0],std::to_string(i+1),glm::vec4(i?glm::vec3(.75f):glm::vec3(1),1));cell(teamColumns[0]+2,teamColumns[1],censorChat(p.name,settings_.chat.censor),color(p.id),true);
        cell(teamColumns[1]+2,teamColumns[2],std::to_string(p.baseKills));cell(teamColumns[2]+2,teamColumns[3],std::to_string(p.baseCaptures));cell(teamColumns[3]+2,teamColumns[4],std::to_string(p.flags));cell(teamColumns[4]+2,teamColumns[5],std::to_string(p.artifacts));cell(teamColumns[5]+2,teamColumns[6],std::to_string(p.kills));cell(teamColumns[6]+2,teamColumns[7],std::to_string(p.deaths));
    }
    auto edit=std::find_if(view_.artwork.begin(),view_.artwork.end(),[](const auto& c){return c.name=="chatEditPane";});
    if(edit!=view_.artwork.end()){auto face=*edit;originalEdit(face,chat_,"gameoverscreen",editingChat_);}
    originalChatHistory(-4);
}
void Frontend::originalChatHistory(int id,int channel){
    auto host=authoredControl(view_.artwork,"chatText");glm::vec4 clip{glm::vec2(host.bounds),host.faceSize};
    if(host.widget!=OriginalControl::Widget::Chat)throw std::runtime_error("Authored chat host has the wrong type");
    auto& pane=id==-6?lobbyChat_:resultsChat_;
    auto fontName=Allegiance::ChatFontName(host.chatFont),boldName=Allegiance::ChatFontName(host.chatFont,true);
    auto font=original_->assets().font(fontName),bold=original_->assets().font(boldName);float height=bold->height;
    bool changedChannel=pane.channel!=channel;if(changedChannel){pane.channel=channel;pane.until=0;}
    if(pane.dirty || changedChannel || pane.censor!=settings_.chat.censor || pane.font!=host.chatFont || pane.width!=clip.z){
        uint64_t anchor=0;unsigned anchorLine=0;
        if(pane.until && !pane.lines.empty()){auto& line=pane.lines[std::min(pane.lines.size()-1,size_t(pane.list.offset/height))];anchor=line.serial;anchorLine=line.line;}
        pane.lines.clear();std::vector<WrappedTextLine> wrapped;
        auto add=[&](std::string value,glm::vec4 color,bool leader=false,uint64_t serial=0){const auto& face=leader?bold:font;
            wrapOriginalChat(value,int(clip.z)-6,[&](unsigned char ch){return face->glyphs[ch].z;},wrapped);
            unsigned index=0;for(auto line:wrapped)pane.lines.push_back({(line.continued?"  ":"")+value.substr(line.offset,line.length),color,leader,serial,index++});};
        // TeamScreen's channel tabs always include individual/nofilter/admin
        // messages. System notifications are separate from the player chat list.
        for(const auto& m:lobbyMessages_)if(m.chat && (channel<0 || m.chatTarget<0 || m.chatTarget==channel ||
            m.chatTarget==CHAT_INDIVIDUAL || m.chatTarget==CHAT_INDIVIDUAL_NOFILTER || m.chatTarget==CHAT_ADMIN))
            add(censorChat(m.text,settings_.chat.censor),m.color,m.leader,m.serial);
        if(id==-4 && !statsFeedback_.empty())add(statsFeedback_,glm::vec4(1));
        pane.dirty=false;pane.censor=settings_.chat.censor;pane.font=host.chatFont;pane.width=clip.z;
        if(anchor){auto first=std::find_if(pane.lines.begin(),pane.lines.end(),[&](auto& line){return line.serial>anchor || (line.serial==anchor && line.line>=anchorLine);});pane.list.offset=float(first-pane.lines.begin())*height;}
    }
    auto& list=pane.list;list.wholeRows=true;list.configure(int(pane.lines.size()),height,clip);
    // ListPaneOld scrolls an integral number of visible lines even when the
    // authored viewport has a few spare pixels. Its scrollbar leaves 8 below.
    list.page=std::floor(clip.w/height)*height;list.position(list.offset);list.bar=host.showScrollBar?glm::vec4(clip.x+clip.z,clip.y,host.bounds.z-host.faceSize.x,std::max(0.f,clip.w-8)):glm::vec4(0);
    if(!pane.until)list.position(list.maximum());
    for(int row=int(list.offset/height);row<int(pane.lines.size());++row){float y=clip.y+3+row*height-list.offset;if(y>=clip.y+clip.w)break;auto& line=pane.lines[row];
        OriginalControl c;c.kind=OriginalControl::Kind::Text;c.name=std::string(id==-6?"lobbyChat/":"resultChat/")+std::to_string(row);
        c.bounds={clip.x+3,y,clip.z-6,height};c.text=line.text;c.font=line.leader?boldName:fontName;c.color=line.color;c.clip=clip;c.intersectClip(host.clip.value_or(host.bounds));c.surfaceText();view_.artwork.push_back(std::move(c));
    }
    if(host.showScrollBar)scrollbar(list,id);
}
}
