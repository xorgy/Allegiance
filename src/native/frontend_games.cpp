#include "frontend.hpp"
#include "chat_censor.hpp"
#include "../ClientCore/GameTypes.h"
#include "../ClientCore/GameListPresentation.h"
#include <cctype>
#include <cstdio>
#include <numeric>

namespace Native {
namespace {
std::string lower(std::string text){for(auto& c:text)c=char(std::tolower(static_cast<unsigned char>(c)));return text;}
// Native listings include a listener port; the original config identifies the
// authority by its remote host address, independent of the hosted mission.
std::string_view remoteHost(std::string_view address){
    if(address.starts_with("[")){auto end=address.find(']');if(end!=address.npos)return address.substr(1,end-1);}
    if(auto colon=address.find(':');colon!=address.npos && address.find(':',colon+1)==address.npos)return address.substr(0,colon);
    return address;
}
constexpr const char* filters[]{"developmentsComboPane","livesComboPane","scoresCountComboPane","zoneComboPane",
    "conquestComboPane","deathmatchComboPane","prosperityComboPane","countdownComboPane","artifactComboPane","flagsComboPane","territorialComboPane"};
constexpr const char* choices[]{"No","Yes","DontCare"};
std::array<bool,11> properties(const Net::ServerInfo& info){auto& r=info.session.rules;
    return {r.developments,r.lives!=32767,false,false,r.conquest!=0,r.teamKills!=0,r.prosperity!=0,r.minutes!=0,r.artifacts!=0,r.flags!=0,r.territory!=100};}
size_t typeIndex(const Net::ServerInfo& info){auto& r=info.session.rules;
    Allegiance::GameTypeValues v{r.developments,r.invulnerableStations,r.ejectPods,short(r.lives),r.prosperity,short(r.conquest),short(r.flags),short(r.artifacts),short(r.teamKills),short(r.territory),float(r.minutes*60),r.experimental};
    for(size_t i=0;i<Allegiance::GameTypes.size();++i)if(Allegiance::MatchesGameType(v,Allegiance::GameTypes[i].values))return i;
    return Allegiance::GameTypes.size();
}
void backspace(std::string& text){if(text.empty())return;size_t n=text.size()-1;while(n && (uint8_t(text[n])&0xc0)==0x80)--n;text.resize(n);}
template<class Row> std::optional<Net::Instance> selectedInstance(const std::vector<Row>& rows,const std::string& address){
    auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.address==address;});
    if(row!=rows.end() && Net::validInstance(row->info.instance))return row->info.instance;
    return {};
}
template<class Row> void restoreAddress(const std::vector<Row>& rows,std::string& address,const std::optional<Net::Instance>& instance){
    if(!instance)return;
    auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.info.instance==*instance;});
    address=row==rows.end()?std::string{}:row->address;
}
}
void Frontend::sortGames(){
    gameCoreNames_.clear();gameCoreNames_.reserve(games_.size());
    for(const auto& game:games_)gameCoreNames_.push_back(original_?original_->assets().coreName(game.info.core):std::string(game.info.core));
    gamesRows_.clear();
    for(size_t i=0;i<games_.size();++i){const auto& game=games_[i];auto values=properties(game.info);bool keep=true;
        for(size_t f=0;f<values.size();++f)if(gameFilters_[f]!=2 && bool(gameFilters_[f])!=values[f])keep=false;
        if(keep && (gameNameFilter_.empty() || lower(game.info.name).find(lower(gameNameFilter_))!=std::string::npos))gamesRows_.push_back(i);
    }
    // Stable passes preserve the original header's stack of sorting criteria.
    // Most recently clicked header is primary; previous choices break ties.
    for(auto [column,reverse]:gameSorts_)std::stable_sort(gamesRows_.begin(),gamesRows_.end(),[&](size_t a,size_t b){
        const auto& x=games_[reverse?b:a].info;const auto& y=games_[reverse?a:b].info;
        switch(column){
        case 0:if(bool(x.flags&Net::ServerJoinable)!=bool(y.flags&Net::ServerJoinable))return bool(x.flags&Net::ServerJoinable)>bool(y.flags&Net::ServerJoinable);return bool(x.flags&Net::ServerRunning)<bool(y.flags&Net::ServerRunning);
        case 1:return lower(x.name)<lower(y.name);
        case 2:if(bool(x.flags&Net::ServerRunning)!=bool(y.flags&Net::ServerRunning))return bool(x.flags&Net::ServerRunning)>bool(y.flags&Net::ServerRunning);return x.seconds>y.seconds;
        case 4:return lower(x.server)<lower(y.server);
        case 5:return lower(gameCoreNames_[reverse?b:a])<lower(gameCoreNames_[reverse?a:b]);
        case 6:return x.players<y.players;
        case 7:return typeIndex(x)<typeIndex(y);
        default:return false; // Unranked native sessions all have skill Any.
        }
    });
    gamesList_.selection=-1;
    for(size_t i=0;i<gamesRows_.size();++i)if(games_[gamesRows_[i]].address==gameSelected_)gamesList_.selection=int(i);
    if(gamesList_.selection<0)gameSelected_.clear(); // Original ListChanged retires a removed/filtered selection.
    gamesList_.count=int(gamesRows_.size());gamesList_.position(gamesList_.offset);
}
void Frontend::gameListings(std::vector<Net::ServerListing> listings){
    auto selected=selectedInstance(games_,gameSelected_),lan=selectedInstance(games_,lanSelected_);
    games_=Net::coalesceGames(std::move(listings));
    restoreAddress(games_,gameSelected_,selected);restoreAddress(games_,lanSelected_,lan);sortGames();
    lanRows_.resize(games_.size());std::iota(lanRows_.begin(),lanRows_.end(),0);
    std::stable_sort(lanRows_.begin(),lanRows_.end(),[&](size_t a,size_t b){return lower(games_[a].info.name)<lower(games_[b].info.name);});
    lanList_.selection=-1;
    for(size_t i=0;i<lanRows_.size();++i)if(games_[lanRows_[i]].address==lanSelected_)lanList_.selection=int(i);
    if(lanList_.selection<0)lanSelected_.clear();
    lanList_.count=int(lanRows_.size());lanList_.position(lanList_.offset);
    if(screen_==Screen::Games || screen_==Screen::Online)rebuild();
}
void Frontend::updateCoreCivilizations(){
    auto core=sessionCoreFile(session_);if(!original_ || Allegiance::CoreFileEqual(core,civilizationCore_))return;
    float money{};auto choices=Game::civilizations(original_->assets().root(),&money,core);
    civilizations_=std::move(choices);winMoney_=money;civilizationCore_=core;comboSelections_.erase("teamscreen/civComboPane");
}
void Frontend::createCoreCatalog(){
    if(localCores_.empty()){
        localCores_={"static_core"};if(original_)for(const auto& core:original_->assets().coreCatalog())if(!Allegiance::FindCore(localCores_,core))localCores_.push_back(core);
    }
    createCores_=localCores_;
    for(const auto& authority:authorities_)for(const auto& core:authority.cores)if(!Allegiance::FindCore(createCores_,core))createCores_.push_back(core);
    if(!Allegiance::FindCore(createCores_,createCoreFile_))createCoreSelected_=false;
}
bool Frontend::canCreateGame()const{
    if(!createServerSelected_ || !createCoreSelected_)return false;
    if(createAuthority_.empty())return Allegiance::FindCore(localCores_,createCore())!=nullptr;
    auto found=std::find_if(authorities_.begin(),authorities_.end(),[&](const auto& a){return a.address==createAuthority_;});
    return found!=authorities_.end() && found->info.count<found->info.capacity && found->supports(createCore());
}
void Frontend::gameAuthorities(std::vector<Net::AuthorityListing> authorities){
    auto selected=selectedInstance(authorities_,createAuthority_);bool remote=!createAuthority_.empty();
    authorities_=Net::coalesceAuthorities(std::move(authorities));restoreAddress(authorities_,createAuthority_,selected);
    std::stable_sort(authorities_.begin(),authorities_.end(),[](const auto& a,const auto& b){auto x=lower(a.info.server),y=lower(b.info.server);return x==y?a.address<b.address:x<y;});
    if(remote && (createAuthority_.empty() || std::none_of(authorities_.begin(),authorities_.end(),[&](const auto& a){return a.address==createAuthority_;}))){createAuthority_.clear();createServerSelected_=false;}
    if(screen_==Screen::Games && gameCreateOpen_){createCoreCatalog();rebuild();}
}
void Frontend::foundPlayer(std::optional<Net::ServerListing> game){
    if(screen_!=Screen::Games || !findPlayerPending_)return;
    findPlayerPending_=false;
    if(game){
        gameFilters_.fill(2);gameNameFilter_.clear();
        auto listings=games_;auto existing=std::find_if(listings.begin(),listings.end(),[&](const auto& g){return (Net::validInstance(game->info.instance) && g.info.instance==game->info.instance) || g.address==game->address;});
        if(existing==listings.end())listings.push_back(*game);else *existing=*game;
        gameListings(std::move(listings));gameSelected_=game->address;
        sortGames();gamesList_.select(gamesList_.selection);
    }
    error(game?"Player found!":"The player was not found.");
}
void Frontend::gameFilterCombo(int field){
    if(!gameFilterOpen_ || field<0 || field>=int(std::size(filters)))return;
    std::vector<PopupRow> rows;for(int choice:{1,0,2})rows.push_back({choices[choice],0,{MenuOp::GameFilterChoice,field,choice}});
    comboPopup(std::string("gamescreen/")+filters[field],std::move(rows));
}
void Frontend::originalGames(){
    view_.original=true;view_.overlay=false;view_.items.clear();view_.text.clear();
    uint32_t players=0;for(const auto& game:games_)players+=game.info.players;
    view_.artwork=original_->layout("gamescreen","screen",{{"playerCount",players}});
    auto text=[&](glm::vec4 bounds,std::string value,const char* font="smallFont",glm::vec4 color=glm::vec4(1),int justify=0){OriginalControl c;c.kind=OriginalControl::Kind::Text;c.bounds=bounds;c.text=std::move(value);c.font=font;c.color=color;c.justify=justify;c.surfaceText();view_.artwork.push_back(std::move(c));};
    auto bind=[&](const OriginalControl& c,std::string help,MenuAction action,bool enabled=true,bool selected=false){view_.items.push_back({c.visibleBounds(),std::move(help),{},action,enabled,selected,c.name});};
    auto selected=std::find_if(games_.begin(),games_.end(),[&](const auto& g){return g.address==gameSelected_;});
    bool canJoin=selected!=games_.end() && gamesList_.selection>=0 && (selected->info.flags&Net::ServerJoinable);
    for(auto& c:view_.artwork)if(c.kind==OriginalControl::Kind::Button){
        if(c.name=="backButtonPane")bind(c,"Return to the main menu.",{MenuOp::Navigate,int(Screen::Home)});
        else if(c.name=="joinButtonPane")bind(c,"Join the game selected in the list.",{MenuOp::GameJoin},canJoin);
        else if(c.name=="newGameButtonPane")bind(c,"Create a new game.",{MenuOp::GameCreate});
        else if(c.name=="filterButtonPane")bind(c,"Filter the list of games.",{MenuOp::GameFilter});
        else if(c.name=="findPlayerButtonPane")bind(c,"Find the game containing a player.",{MenuOp::FindPlayer});
        else if(c.name.starts_with("gameListHeader")){int column=c.name.back()-'0';if(column>=0 && column<8){c.checkedState=gameSorts_.back().first==column?(gameSorts_.back().second?1:2):0;bind(c,"Sort the list of games.",{MenuOp::GameSort,column});}}
    }
    auto columns=original_->integers("gamescreen","gameColumns");
    if(columns.size()!=8 || !std::is_sorted(columns.begin(),columns.end()))throw std::runtime_error("Invalid original games-list columns");
    auto viewport=authoredListBounds(view_.artwork,"gameListPane",columns.back());
    gamesList_.configure(int(gamesRows_.size()),28,viewport);gamesList_.bar=authoredScrollBarBounds(view_.artwork,"gameListPane");
    listBlank(viewport,MenuOp::GameSelect);
    float left=viewport.x,top=viewport.y,bottom=top+viewport.w;
    for(size_t row=0;row<gamesRows_.size();++row){float y=top+row*28-gamesList_.offset;if(y+28<=top || y>=bottom)continue;const auto& game=games_[gamesRows_[row]];auto& info=game.info;
        auto clipped=[&](OriginalControl c){c.clip=viewport;view_.artwork.push_back(std::move(c));};
        if(game.address==gameSelected_){OriginalControl c;c.kind=OriginalControl::Kind::Rectangle;c.bounds={left+56,y+5,viewport.z-58,19};c.color={1,0,0,1};clipped(c);}
        OriginalControl hit;hit.name="game/"+game.address;hit.bounds={left,std::max(y,top),viewport.z,std::min(y+28,bottom)-std::max(y,top)};bind(hit,"Join the selected game.",{MenuOp::GameSelect,int(row)},true,game.address==gameSelected_);
        auto cell=[&](float x,float right,std::string value,float dy=6,glm::vec4 color=glm::vec4(1),int justify=0,float clipLeft=-1){
            text({left+x,y+dy,right-x,16},std::move(value),"smallFont",color,justify);
            auto l=clipLeft>=0?left+clipLeft:left,r=clipLeft>=0?left+right:left+viewport.z;
            view_.artwork.back().clip=glm::vec4(l,std::max(y,top),r-l,std::min(y+28,bottom)-std::max(y,top));
        };
        auto icon=std::string(info.flags&Net::ServerRunning?"iconrunning":"iconwaiting")+(info.flags&Net::ServerJoinable?"greenbmp":"redbmp");auto image=original_->assets().image(icon,true);
        OriginalControl c;c.kind=OriginalControl::Kind::Image;c.image=image;c.bounds={left+(columns[0]+54)/2-int(image->width)/2,y+(28-int(image->height))/2,image->width,image->height};clipped(c);
        cell(columns[0]+4,columns[1],info.name[0]?censorChat(info.name,settings_.chat.censor):"<unnamed>",6,glm::vec4(1),0,columns[0]+4);
        if(info.flags&(Net::ServerRunning|Net::ServerStarting)){char duration[32];Allegiance::FormatGameListTime(duration,sizeof(duration),info.seconds,info.flags&Net::ServerStarting);cell(columns[1],columns[2]-5,duration,6,glm::vec4(1),2);}
        cell(columns[2]+4,columns[3],"Any",6,glm::vec4(1),0,columns[2]+4);
        cell(columns[3]+4,columns[4],info.server,6,glm::vec4(1),0,columns[3]);
        cell(columns[4]+4,columns[5],gameCoreNames_[gamesRows_[row]],6,glm::vec4(1),0,columns[4]+4);
        cell(columns[5],columns[6]-3,std::to_string(info.players)+"/"+std::to_string(info.capacity),3,glm::vec4(1),2);
        cell(columns[5],columns[6]-3,"Noat: "+std::to_string(info.noTeam),12,{.5,.5,.5,1},2);
        auto drawIcon=[&](const char* name,int x,int center){auto im=original_->assets().image(name,true);OriginalControl c;c.kind=OriginalControl::Kind::Image;c.image=im;c.bounds={left+x,y+center-int(im->height)/2,im->width,im->height};clipped(c);};
        auto type=typeIndex(info);if(type<Allegiance::GameTypes.size())cell(columns[6]+4,columns[7],Allegiance::GameTypes[type].name);
        else {constexpr const char* icons[]{"iconconquestbmp","iconterritorialbmp","iconprosperitybmp","iconartifactsbmp","iconflagsbmp","icondeathmatchbmp","iconcountdownbmp"};auto& r=info.session.rules;bool goals[]{r.conquest!=0,r.territory!=100,r.prosperity!=0,r.artifacts!=0,r.flags!=0,r.teamKills!=0,r.minutes!=0};
            for(int i=0;i<7;++i)if(goals[i])drawIcon(icons[i],columns[6]+3+i*16,14);}
        if(original_->assets().officialServer(info.server,remoteHost(game.address)) && original_->assets().officialCore(info.core))drawIcon("iconscorescountbmp",columns[6]+107,13);
        if(info.session.rules.developments)drawIcon("icondevelopmentsbmp",columns[6]+107,13);
        if(info.session.rules.lives!=32767)drawIcon("iconlivesbmp",columns[6]+107,13);
    }
    scrollbar(gamesList_,-5);
    if(!gameFilterOpen_ && !gameCreateOpen_ && !findPlayerOpen_)return;
    view_.blockInput();
    markPopupLayer();
    const char* pane=findPlayerOpen_?"FindDialog":gameCreateOpen_?"createDialog":"FilterDialog";
    auto popup=original_->layout("gamescreen",pane);glm::vec2 size{550,325};if(!popup.empty())size={popup.front().bounds.z,popup.front().bounds.w};auto at=glm::floor((glm::vec2(800,600)-size)*.5f);
    for(auto& c:popup){c.translate(at);view_.artwork.push_back(c);
        if(c.name=="filterOkButtonPane")bind(c,"Apply these filters.",{MenuOp::GameFilterAccept});
        else if(c.name=="findOkButtonPane")bind(c,"Locate the player name entered in the text box.",{MenuOp::FindPlayerAccept});
        else if(c.name=="findCancelButtonPane")bind(c,"Cancel the search and return to the Games List.",{MenuOp::FindPlayerCancel});
        else if(c.name=="findPlayerEditPane"){bind(c,"Enter the player's name.",{MenuOp::FindPlayerName});originalEdit(c,findPlayerName_,"gamescreen",true);}
        else if(c.name=="filterCancelButtonPane")bind(c,"Cancel filter changes.",{MenuOp::GameFilterCancel});
        else if(c.name=="gameNameEditPane" || c.name=="createEditPane"){bind(c,"Enter the game name.",{gameCreateOpen_?MenuOp::GameCreateName:MenuOp::GameFilterName});originalEdit(c,gameNameDraft_,"gamescreen",gameFilterEditing_);}
        else if(c.name=="createOkButtonPane"){bind(c,"Create a game on the selected server.",{MenuOp::GameCreateAccept},canCreateGame());if(!canCreateGame())view_.artwork.back().clip=glm::vec4(0);}
        else if(c.name=="createCancelButtonPane")bind(c,"Cancel and return to the Games List.",{MenuOp::GameCreateCancel});
        else if(c.name.starts_with("serverListHeader") || c.name.starts_with("coreListHeader"))bind(c,"",{MenuOp::PopupItem});
        else for(int f=0;f<11;++f)if(c.name==filters[f]){bind(c,"Choose Yes, No, or DontCare.",{MenuOp::GameFilterCombo,f});view_.artwork.push_back(widgetText(c,choices[gameFilterDraft_[f]]));}
    }
    if(gameCreateOpen_){
        auto servers=original_->integers("gamescreen","serverColumns"),cores=original_->integers("gamescreen","coreColumns");
        if(servers.size()!=5 || cores.size()!=2 || !std::is_sorted(servers.begin(),servers.end()) || !std::is_sorted(cores.begin(),cores.end()))throw std::runtime_error("Invalid original create-game columns");
        auto bounds=authoredListBounds(view_.artwork,"serverListPane",servers.back());
        createServerList_.configure(int(authorities_.size()+1),14,bounds);createServerList_.bar=authoredScrollBarBounds(view_.artwork,"serverListPane");createServerList_.selection=-1;
        listBlank(bounds,MenuOp::GameCreateServer);
        auto paint=[&](glm::vec4 viewport,float y,std::span<const int> columns,std::span<const std::string> values,bool selected,bool supported,bool full,int ping,MenuAction action,std::string name,bool official=false){
            auto clip=glm::vec4(viewport.x,std::max(y,viewport.y),viewport.z,std::min(y+14,viewport.y+viewport.w)-std::max(y,viewport.y));if(clip.w<=0)return;
            OriginalControl c;c.kind=OriginalControl::Kind::Rectangle;c.bounds={viewport.x,y,viewport.z,14};c.clip=viewport;
            if(selected){c.color={0,0,action.op==MenuOp::GameCreateServer?.7f:1.f,1};view_.artwork.push_back(c);}
            OriginalControl hit;hit.bounds=clip;hit.name=std::move(name);bind(hit,"",action,true,selected);
            if(official){auto image=original_->assets().image("iconscorescountbmp",true);OriginalControl icon;icon.kind=OriginalControl::Kind::Image;icon.image=image;icon.bounds={viewport.x,y+7-int(image->height)/2,image->width,image->height};icon.clip=viewport;view_.artwork.push_back(std::move(icon));}
            for(size_t i=0;i<values.size();++i){text({viewport.x+columns[i]+1,y+1,columns[i+1]-columns[i]-1,14},values[i]);auto& cell=view_.artwork.back();cell.clip=glm::vec4(viewport.x+columns[i],y,columns[i+1]-columns[i],14);cell.intersectClip(viewport);
                if(!supported && i<2)cell.color={.5f,.5f,.5f,1};
                if(i==2 && full)cell.color={1,0,0,1};
                if(i==3)cell.color=ping<0?glm::vec4(.5,.5,.5,1):ping<100?glm::vec4(0,1,0,1):ping>250?glm::vec4(1,0,0,1):glm::vec4(1,1,0,1);
            }
            if(action.op==MenuOp::GameCreateServer){c.bounds={viewport.x,y+13,viewport.z,1};c.color={.5f,.5f,.5f,1};view_.artwork.push_back(c);}
        };
        for(int row=0;row<createServerList_.count;++row){
            const Net::AuthorityListing* authority=row?&authorities_[size_t(row-1)]:nullptr;
            bool selected=createServerSelected_ && (authority?authority->address==createAuthority_:createAuthority_.empty());
            if(selected)createServerList_.selection=row;
            bool supported=!createCoreSelected_ || !authority || authority->supports(createCore());
            std::array<std::string,4> values=authority?std::array<std::string,4>{authority->info.server,authority->info.location,std::to_string(authority->info.count)+"/"+std::to_string(authority->info.capacity),authority->ping<0?"?":std::to_string(authority->ping)}:std::array<std::string,4>{"Local game","This computer","0/1","?"};
            paint(bounds,bounds.y+row*14-createServerList_.offset,servers,values,selected,supported,authority && authority->info.count>=authority->info.capacity,authority?authority->ping:-1,{MenuOp::GameCreateServer,row},"create/server/"+std::to_string(row),authority && original_->assets().officialServer(authority->info.server,remoteHost(authority->address)));
        }
        scrollbar(createServerList_,-8);
        auto coreBounds=authoredListBounds(view_.artwork,"coreListPane",cores.back());createCoreList_.configure(int(createCores_.size()),14,coreBounds);createCoreList_.bar=authoredScrollBarBounds(view_.artwork,"coreListPane");createCoreList_.selection=-1;
        listBlank(coreBounds,MenuOp::GameCreateCore);
        auto selected=std::find_if(authorities_.begin(),authorities_.end(),[&](const auto& a){return a.address==createAuthority_;});
        for(size_t row=0;row<createCores_.size();++row){const auto& core=createCores_[row];bool selectedCore=createCoreSelected_ && Allegiance::CoreFileEqual(core,createCore());if(selectedCore)createCoreList_.selection=int(row);
            bool supported=!createServerSelected_ || (createAuthority_.empty()?Allegiance::FindCore(localCores_,core)!=nullptr:selected!=authorities_.end() && selected->supports(core));
            std::array<std::string,1> coreValues{original_->assets().coreName(core)};
            paint(coreBounds,coreBounds.y+row*14-createCoreList_.offset,cores,coreValues,selectedCore,supported,false,-1,{MenuOp::GameCreateCore,int(row)},"create/core/"+std::to_string(row),original_->assets().officialCore(core));
        }
        scrollbar(createCoreList_,-9);
    }
    if(!popups_.empty() && popups_.front().group==PopupGroup::Combo)originalPopup(true);
}
bool Frontend::gamesKey(SDL_Keycode key,FrontResult& result){
    if(screen_!=Screen::Games)return false;
    if(findPlayerOpen_){
        if(key==SDLK_ESCAPE){findPlayerOpen_=false;rebuild();}
        else if(key==SDLK_RETURN)result=dispatch({MenuOp::FindPlayerAccept});
        else if(key==SDLK_BACKSPACE){backspace(findPlayerName_);rebuild();}
        return true;
    }
    if(gameFilterOpen_ || gameCreateOpen_){
        if(key==SDLK_ESCAPE){gameFilterOpen_=gameCreateOpen_=gameFilterEditing_=false;rebuild();}
        // CreateGameDialog forwards Return to its EditPane, but never subscribes
        // to the edit's string event. FilterDialog explicitly accepts Return.
        else if(key==SDLK_RETURN && (!original_ || !gameCreateOpen_))result=dispatch({gameCreateOpen_?MenuOp::GameCreateAccept:MenuOp::GameFilterAccept});
        else if(gameFilterEditing_ && key==SDLK_BACKSPACE){backspace(gameNameDraft_);rebuild();}
        return true;
    }
    if(original_ && (key==SDLK_RETURN || key==SDLK_HOME || key==SDLK_END))return true; // No original screen/button/list handler for these keys.
    if(key==SDLK_RETURN){result=dispatch({MenuOp::GameJoin});return true;}
    if(key==SDLK_UP || key==SDLK_DOWN || key==SDLK_PAGEUP || key==SDLK_PAGEDOWN || key==SDLK_HOME || key==SDLK_END){
        if(gamesRows_.empty())return true;int row=gamesList_.selection;
        if(key==SDLK_HOME)row=0;else if(key==SDLK_END)row=int(gamesRows_.size())-1;else row+=(key==SDLK_UP || key==SDLK_PAGEUP?-1:1)*(key==SDLK_PAGEUP || key==SDLK_PAGEDOWN?gamesList_.pageRows():1);
        if(row<0 && gamesList_.selection<0)return true;
        gamesList_.select(std::max(0,row));gameSelected_=games_[gamesRows_[gamesList_.selection]].address;rebuild();return true;
    }
    return false;
}
}
