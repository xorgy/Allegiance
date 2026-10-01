#include "frontend.hpp"
#include "chat_censor.hpp"
#include "../ClientCore/GameTypes.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>

namespace Native {
void Frontend::originalMissionBriefing() {
    int side=session_.team;
    if(networked_)for(const auto& pilot:networkPilots_)if(pilot.controlled)side=pilot.team;
    if(side<0 || side>=session_.rules.teams || civilizations_.empty())return;
    auto own=teamCivilization(side);if(!own)return;
    std::string icon=own->icon,team=teamName(side);
    if((networked_ || playing_) && side<int(teamScores_.size()) && !teamScores_[side].civilizationIcon.empty())icon=teamScores_[side].civilizationIcon;
    if(icon.empty())return;
    std::transform(icon.begin(),icon.end(),icon.begin(),[](unsigned char c){return std::tolower(c);});
    missionBriefOpen_=true;
    if(missionBriefStrings_.empty()){
        pointerAction_.reset();popups_.clear();
        auto authored=[&](const char* key){auto result=original_->string("missionbrief",icon+key);return result.empty()?original_->string("missionbrief",std::string("ironleague")+key):result;};
        std::vector<int> opponents;
        for(int other=0;other<session_.rules.teams;++other)if(other!=side){
            auto faction=teamCivilization(other);if(!faction)continue;int id=faction->id;
            if(std::find(opponents.begin(),opponents.end(),id)==opponents.end())opponents.push_back(id);
        }
        std::string enemies;
        for(size_t i=0;i<opponents.size();++i){
            auto choice=std::find_if(civilizations_.begin(),civilizations_.end(),[&](const auto& c){return c.id==opponents[i];});
            if(choice==civilizations_.end())continue;
            if(i)enemies+=i+1==opponents.size()?" and ":", ";enemies+=choice->name;
        }
        // MissionBriefPopup::GetCivMissionBriefing. Use the original strings;
        // formatting is done once per briefing, never in the render loop.
        std::string body;
        if(briefing_ && !briefing_->generateCivilization)body=briefing_->text;
        else{
            const auto& r=session_.rules;
            bool noWin=!r.conquest && r.territory==100 && !r.flags && !r.artifacts && !r.teamKills && !r.minutes && !r.prosperity;
            char buffer[2048];auto format=authored(noWin?"NoWin":"Default");
            auto name=censorChat(team,settings_.chat.censor);
            auto location=briefing_?briefing_->text:sector_.empty()?std::string(conquestMapNames[session_.map]):sector_;
            if(!briefing_ && sector_.empty() && !session_.customMap.empty()){
                auto home=std::find_if(setupMap_.sectors.begin(),setupMap_.sectors.end(),[&](const auto& s){return s.stations[side]>0;});
                if(home!=setupMap_.sectors.end())location=home->name;
            }
            std::snprintf(buffer,sizeof(buffer),format.c_str(),name.c_str(),location.c_str(),enemies.c_str());body=buffer;
            if(r.flags || r.artifacts || r.minutes || (r.conquest && r.conquest!=100) || r.teamKills || r.prosperity){
                body+=authored("SecondaryObjectives");
                auto append=[&](const char* key,int value){auto pattern=authored(key);std::snprintf(buffer,sizeof(buffer),pattern.c_str(),value);body+=buffer;};
                if(r.conquest && r.conquest!=100)append("Conquest",r.conquest);
                if(r.artifacts)append("Artifacts",r.artifacts);
                if(r.flags)append("Flags",r.flags);
                if(r.minutes)append("Countdown",r.minutes);
                if(r.teamKills)append("Deathmatch",r.teamKills);
                if(r.prosperity)append("Prosperity",int(r.prosperity*winMoney_));
            }
        }
        constexpr const char* mapNames[]={"Single Ring","Double Ring","Pinwheel","Diamond Ring","Snowflake","Split Bases","Brawl","Big Ring","HiLo","HiHigher","Star","InsideOut","Grid","EastWest","LargeSplit"};
        auto custom=Allegiance::FindCustomMap(customMaps_,session_.customMap);
        std::string mapType=session_.customMap.empty()?mapNames[conquestMapIds[session_.map]]:custom?custom->name:"Unknown map name";
        int tips=std::max(1,std::atoi(original_->string("missionbrief","numTips").c_str()));
        missionBriefStrings_={{"missionBriefingTitleText",authored("Title")},{"missionBriefingStringText",authored("String")},
            {"missionBriefingBodyText",std::move(body)},{"missionBriefingMapType",std::move(mapType)},
            {"missionBriefingTipText",original_->string("missionbrief","Tip"+std::to_string(session_.seed%unsigned(tips)+1))}};
    }
    view_.blockInput();
    markPopupLayer();
    constexpr const char* teamIcons[]={"iconteamyellowbmp","iconteambluebmp","iconteampurplebmp","iconteamgreenbmp","iconteamredbmp","iconteamwhitebmp"};
    auto teamBackground=original_->assets().image(teamIcons[side],true);
    auto controls=original_->layout("missionbrief","MissionBriefDialog",{{"countdown",networked_?networkCountdown_:std::ceil(std::max(0.,countdown_))}},missionBriefStrings_,false,{{"teamImage",{teamBackground->width,teamBackground->height}}});
    auto origin=glm::floor((glm::vec2(800,600)-glm::vec2(controls.front().bounds.z,controls.front().bounds.w))*.5f);
    for(auto& c:controls){c.translate(origin);view_.artwork.push_back(std::move(c));}
    auto host=authoredControl(view_.artwork,"teamImage");
    for(auto image:{std::string(teamIcons[side]),"icon"+icon+"bmp"}){
        auto im=original_->assets().image(image,true);OriginalControl c;c.kind=OriginalControl::Kind::Image;c.name="missionBriefingTeamImage";c.bounds={glm::vec2(host.bounds),im->width,im->height};c.image=im;c.clip=host.clip;view_.artwork.push_back(std::move(c));
    }
    originalMapPreview(false);
}
void Frontend::originalTeamSettings() {
    view_.blockInput();
    auto controls=original_->layout("teamscreen","TeamSettingsDialog",{{"showSquad",0}});
    if(controls.empty())return;
    markPopupLayer();
    const auto origin=glm::floor((glm::vec2(view_.canvas)-glm::vec2(controls.front().bounds.z,controls.front().bounds.w))*.5f);
    auto text=[&](const OriginalControl& c,std::string value){view_.artwork.push_back(widgetText(c,std::move(value)));};
    for(auto c:controls){
        c.translate(origin);
        view_.artwork.push_back(c);
        auto bind=[&](const char* help,MenuOp op){view_.items.push_back({c.visibleBounds(),help,{},{op},true,false,c.name});};
        if(c.name=="okButtonPane")bind("Accept the team settings.",MenuOp::TeamSettingsAccept);
        else if(c.name=="cancelButtonPane")bind("Cancel the team settings.",MenuOp::TeamSettingsCancel);
        else if(c.name=="teamNameEditPane"){bind("Edit the team name.",MenuOp::TeamName);originalEdit(c,teamNameDraft_,"teamscreen",teamNameFocus_);}
        else if(c.name=="civComboPane"){
            bind("Choose the team's faction.",MenuOp::TeamFaction);
            auto civ=std::find_if(civilizations_.begin(),civilizations_.end(),[&](const auto& choice){return choice.id==teamCivDraft_;});
            text(c,civ==civilizations_.end()?"":civ->name);
        }
    }
    if(!popups_.empty() && popups_.front().group==PopupGroup::Combo)originalPopup(true);
}
namespace {
struct SetupCheck {const char* name;int setting;bool MissionSettings::*value;};
constexpr SetupCheck SetupChecks[]={{"ejectPodsCheckboxPane",20,&MissionSettings::ejectPods},{"friendlyFireCheckboxPane",10,&MissionSettings::friendlyFire},
    {"invulnerableStationsCheckboxPane",19,&MissionSettings::invulnerableStations},{"developmentCheckboxPane",13,&MissionSettings::developments},
    {"showMapCheckboxPane",17,&MissionSettings::showMap},{"allowAlliedRipCheckboxPane",34,&MissionSettings::alliedRipcord},
    {"allowShipyardsCheckboxPane",35,&MissionSettings::shipyard},{"allowTacticalCheckboxPane",36,&MissionSettings::tactical},
    {"allowSupremacyCheckboxPane",37,&MissionSettings::supremacy},{"allowExpansionCheckboxPane",38,&MissionSettings::expansion},
    {"defectionsCheckboxPane",41,&MissionSettings::allowDefections},{"joinersCheckboxPane",42,&MissionSettings::allowJoiners}};
Allegiance::GameTypeValues typeValues(const MissionSettings& r) {
    return {r.developments,r.invulnerableStations,r.ejectPods,short(r.lives),r.prosperity,short(r.conquest),short(r.flags),
        short(r.artifacts),short(r.teamKills),short(r.territory),r.minutes*60.f,r.experimental};
}
}
int Frontend::setupSelection(int field)const {
    const auto& combo=setupCombos_.at(size_t(field));const auto& r=session_.rules;
    if(combo.field==SetupField::GameType){
        if(setupPresetSelection_>=0)return setupPresetSelection_;
        auto values=typeValues(r);for(size_t i=0;i<Allegiance::GameTypes.size();++i)if(Allegiance::MatchesGameType(values,Allegiance::GameTypes[i].values))return int(i);
        return int(Allegiance::GameTypes.size());
    }
    std::array<double,4> values{};size_t count=1;
    switch(combo.field){
    case SetupField::Conquest:values[0]=r.conquest;break;
    case SetupField::Territory:values[0]=r.territory;break;
    case SetupField::Prosperity:values[0]=r.prosperity;break;
    case SetupField::Artifacts:values[0]=r.artifacts;break;
    case SetupField::Flags:values[0]=r.flags;break;
    case SetupField::Kills:values[0]=r.teamKills;break;
    case SetupField::Time:values[0]=r.minutes;break;
    case SetupField::Teams:values[0]=r.teams;break;
    case SetupField::Lives:values[0]=r.lives;break;
    case SetupField::Money:values[0]=r.startingMoney;break;
    case SetupField::Helium:values[0]=r.helium;break;
    case SetupField::Resources:values={double(r.neutralMineable),double(r.homeMineable),double(r.neutralSpecial),double(r.homeSpecial)};count=4;break;
    case SetupField::Map:values[0]=conquestMapIds.at(size_t(session_.map));if(auto custom=Allegiance::FindCustomMap(customMaps_,session_.customMap))values[0]=-1-double(custom-customMaps_.data());break;
    case SetupField::Connectivity:values[0]=r.randomEncounters;break;
    case SetupField::MapSize:values[0]=r.mapSize;break;
    case SetupField::Treasures:values={double(r.homeTreasures),double(r.neutralTreasures),r.homeTreasureRate,r.neutralTreasureRate};count=4;break;
    case SetupField::Asteroids:values={double(r.homeAsteroids),double(r.neutralAsteroids),0,0};count=2;break;
    case SetupField::Miners:values[0]=r.miners;break;
    case SetupField::Drones:values[0]=r.maxDrones;break;case SetupField::Imbalance:values[0]=r.maxImbalance;break;
    case SetupField::GameType:break;
    }
    double nearest=std::numeric_limits<double>::infinity();int selected=0;
    for(size_t i=0;i<combo.choices.size();++i){double distance=0;for(size_t j=0;j<count;++j)distance+=std::abs(values[j]-combo.choices[i].values[j]);
        if(distance<nearest){nearest=distance;selected=int(i);}}
    return selected; // Original FindClosestValue: L1 distance, first item wins ties.
}
void Frontend::readSetupChecks(){
    for(const auto& item:view_.items)if(item.action.op==MenuOp::MissionSetting)
        for(const auto& check:SetupChecks)if(item.action.value==check.setting){session_.rules.*check.value=item.selected;buttonChecks_.erase(item.control);break;}
}
void Frontend::recognizeSetupPreset(){setupPresetSelection_=-1;setupPresetSelection_=setupSelection(0);}
void Frontend::setupApply(int field,int choice,bool recognize) {
    if(!setupEditable_ || field<0 || field>=int(setupCombos_.size()))return;
    const auto& combo=setupCombos_[field];if(choice<0 || choice>=int(combo.choices.size()))return;
    if(combo.field==SetupField::GameType && choice>=int(Allegiance::GameTypes.size()))return;
    readSetupChecks();
    const auto& values=combo.choices[choice].values;auto v=values.empty()?0:values[0];auto& r=session_.rules;
    switch(combo.field){
    case SetupField::GameType:{const auto& preset=Allegiance::GameTypes[choice];const auto& p=preset.values;
        r.developments=p.developments;r.invulnerableStations=p.invulnerableStations;r.ejectPods=p.ejectPods;r.lives=p.lives;
        r.prosperity=p.prosperity;r.conquest=p.conquest;r.flags=p.flags;r.artifacts=p.artifacts;r.teamKills=p.kills;
        r.territory=p.territory;r.minutes=int(p.seconds/60);r.experimental=p.experimental;
        session_.map=int(std::find(conquestMapIds.begin(),conquestMapIds.end(),preset.map)-conquestMapIds.begin());session_.customMap.clear();break;}
    case SetupField::Conquest:r.conquest=int(v);break;
    case SetupField::Territory:r.territory=int(v);break;
    case SetupField::Prosperity:r.prosperity=float(v);break;
    case SetupField::Artifacts:r.artifacts=int(v);break;
    case SetupField::Flags:r.flags=int(v);break;
    case SetupField::Kills:r.teamKills=int(v);break;
    case SetupField::Time:r.minutes=int(v);break;
    case SetupField::Teams:retainTeamCivilizations();r.teams=int(v);session_.team=std::min(session_.team,r.teams-1);for(auto& pilot:session_.localPilots)pilot.team=std::min(pilot.team,r.teams-1);break;
    case SetupField::Lives:r.lives=int(v);break;
    case SetupField::Money:r.startingMoney=float(v);break;
    case SetupField::Helium:r.helium=float(v);break;
    case SetupField::Resources:r.neutralMineable=int(values[0]);r.homeMineable=int(values[1]);r.neutralSpecial=int(values[2]);r.homeSpecial=int(values[3]);break;
    case SetupField::Map:if(v<0){session_.customMap=customMaps_.at(size_t(-1-int(v))).file;session_.map=0;}else{session_.customMap.clear();session_.map=int(std::find(conquestMapIds.begin(),conquestMapIds.end(),int(v))-conquestMapIds.begin());}break;
    case SetupField::Connectivity:r.randomEncounters=int(v);break;
    case SetupField::MapSize:r.mapSize=int(v);break;
    case SetupField::Treasures:r.homeTreasures=int(values[0]);r.neutralTreasures=int(values[1]);r.homeTreasureRate=float(values[2]);r.neutralTreasureRate=float(values[3]);break;
    case SetupField::Asteroids:r.homeAsteroids=int(values[0]);r.neutralAsteroids=int(values[1]);break;
    case SetupField::Miners:r.miners=int(v);break;
    case SetupField::Drones:r.maxDrones=int(v);break;case SetupField::Imbalance:r.maxImbalance=int(v);break;
    }
    if(recognize){
        if(combo.field==SetupField::GameType)setupPresetSelection_=choice;
        else switch(combo.field){
        case SetupField::Conquest:case SetupField::Territory:case SetupField::Prosperity:case SetupField::Artifacts:
        case SetupField::Flags:case SetupField::Kills:case SetupField::Time:case SetupField::Lives:recognizeSetupPreset();break;
        default:break;
        }
    }
    ready_=false;
}
void Frontend::originalGameSettings() {
    if(setupCombos_.empty()){
        std::vector<OriginalChoice> types;for(const auto& preset:Allegiance::GameTypes)types.push_back({preset.name,{}});types.push_back({"Custom",{}});
        setupCombos_.push_back({SetupField::GameType,"gameTypeComboPane",std::move(types)});
        auto combo=[&](SetupField field,const char* control,const char* labels,std::initializer_list<const char*> values){setupCombos_.push_back({field,control,original_->choices("newgamescreen",labels,values)});};
        combo(SetupField::Conquest,"conquestBasesComboPane","ConquestPercentNames",{"ConquestPercentValues"});
        combo(SetupField::Territory,"territoryComboPane","TerritoryNames",{"TerritoryValues"});
        combo(SetupField::Prosperity,"prosperityComboPane","ProsperityTargetNames",{"ProsperityTargetValues"});
        combo(SetupField::Artifacts,"artifactCountComboPane","ArtifactCountNames",{"ArtifactCountValues"});
        combo(SetupField::Flags,"flagCountComboPane","FlagCountNames",{"FlagCountValues"});
        combo(SetupField::Kills,"deathWatchKillsComboPane","DeathWatchKillsNames",{"DeathWatchKillsValues"});
        combo(SetupField::Time,"countdownTimeComboPane","CountdownTimeNames",{"CountdownTimeValues"});
        combo(SetupField::Teams,"teamCountComboPane","TeamCountNames",{"TeamCountValues"});
        combo(SetupField::Lives,"livesComboPane","LivesNames",{"LivesValues"});
        combo(SetupField::Money,"startingMoneyComboPane","StartingMoneyNames",{"StartingMoneyValues"});
        combo(SetupField::Helium,"totalMoneyComboPane","TotalMoneyNames",{"TotalMoneyValues"});
        combo(SetupField::Resources,"resourcesComboPane","ResourcesNames",{"ResourcesNeutralMinable","ResourcesPlayerMinable","ResourcesNeutralSpecial","ResourcesPlayerSpecial"});
        combo(SetupField::Map,"mapTypeComboPane","MapTypeNames",{"MapTypeValues"});
        for(size_t i=0;i<customMaps_.size();++i)setupCombos_.back().choices.push_back({customMaps_[i].name,{-1-double(i)}});
        combo(SetupField::Connectivity,"connectivityComboPane","ConnectivityNames",{"ConnectivityValues"});
        combo(SetupField::MapSize,"mapSizeComboPane","MapSizeNames",{"MapSizeValues"});
        combo(SetupField::Treasures,"treasuresComboPane","TreasuresNames",{"TreasureHomeValues","TreasureNeutralValues","TreasureRateHomeValues","TreasureRateNeutralValues"});
        combo(SetupField::Asteroids,"asteriodsComboPane","AsteriodsNames",{"AsteriodsHomeValues","AsteriodsNeutralValues"});
        combo(SetupField::Miners,"initialMinersCountComboPane","initialMinersNames",{"initialMinersValues"});
        combo(SetupField::Imbalance,"maxImbalanceComboPane","MaxImbalanceNames",{"MaxImbalanceValues"});
        combo(SetupField::Drones,"maxDronesCountComboPane","maxDronesNames",{"maxDronesValues"});
    }
    if(setupPresetSelection_<0)recognizeSetupPreset();
    view_.original=true;view_.items.clear();view_.text.clear();view_.artwork=original_->layout("newgamescreen","screen",{{"CanChooseMaxPlayers",0},{"IsCustomMap",!session_.customMap.empty() && !Allegiance::FindCustomMap(customMaps_,session_.customMap)}});
    auto text=[&](const OriginalControl& face,std::string value){view_.artwork.push_back(widgetText(face,std::move(value)));};
    auto bind=[&](const OriginalControl& c,std::string label,MenuAction action,bool enabled=true,bool selected=false){view_.items.push_back({c.visibleBounds(),c.help.empty()?std::move(label):c.help,{},action,enabled,selected,c.name});};
    bool allied=std::any_of(session_.alliances.begin(),session_.alliances.begin()+session_.rules.teams,[](char alliance){return alliance>=0;});
    if(allied){session_.rules.allowDefections=true;session_.rules.maxImbalance=32767;}
    auto authored=std::move(view_.artwork);view_.artwork.clear();
    for(auto c:authored){
        if(c.name=="cancelButtonPane" || c.name=="backButtonPane"){
            if((c.name=="cancelButtonPane")==setupEditable_){view_.artwork.push_back(c);bind(c,setupEditable_?"Cancel changes.":"Return to the team screen.",{MenuOp::SetupCancel});}continue;
        }
        if(c.name=="createButtonPane"){
            if(setupEditable_){view_.artwork.push_back(c);bind(c,"Accept these game settings.",{MenuOp::SetupAccept});}continue;
        }
        if(c.name=="mapPreviewNextButtonPane" || c.name=="mapPreviewPrevButtonPane"){
            if(setupEditable_){view_.artwork.push_back(c);bind(c,"Preview the next map.",{MenuOp::SetupChoice,13,c.name=="mapPreviewNextButtonPane"?-2:-3});}continue;
        }
        view_.artwork.push_back(c);
        bool handled=false;
        for(size_t i=0;i<setupCombos_.size();++i)if(c.name==setupCombos_[i].control){
            int selected=setupSelection(int(i));text(c,setupCombos_[i].choices[selected].label);
            bind(c,"Select a game setting.",{MenuOp::SetupCombo,int(i)},setupEditable_ && !(allied && setupCombos_[i].field==SetupField::Imbalance));handled=true;break;
        }
        if(handled)continue;
        for(const auto& check:SetupChecks)if(c.name==check.name){bind(c,"Change a game setting.",{MenuOp::MissionSetting,check.setting},setupEditable_ && (check.setting!=34 || allied) && !(allied && check.setting==41),session_.rules.*check.value);handled=true;break;}
        if(handled)continue;
        if(c.name=="gameNameEditPane")originalEdit(c,sessionName(session_),"newgamescreen",false,true);
        else if(c.name=="gamePasswordEditPane")originalEdit(c,"","newgamescreen",false,true); // Native sessions are non-Club, as WinTrek's readonly name/password controls.
        else if(c.name=="maxPlayersStringPane")view_.artwork.back().text="Unlimited"; // Native server has no separate per-team pilot cap.
        else if(c.name=="skillLevelComboPane")text(c,"Any");
        else if(c.name=="squadGameCheckboxPane")bind(c,"Squad games require Club membership.",{MenuOp::PopupItem},false,false);
    }
    originalMapPreview();
    if(!popups_.empty() && popups_.front().group==PopupGroup::Combo)originalPopup(true);
}
void Frontend::originalMapPreview(bool interactive) {
    // Briefing overlays a team screen which has its own preview. Compose the
    // most recently appended authored pane, rather than drawing into its parent.
    auto frame=std::find_if(view_.artwork.rbegin(),view_.artwork.rend(),[](const auto& c){return c.name=="mapPreviewPane";});
    if(frame==view_.artwork.rend())return;
    if(setupMapBounds_!=frame->bounds){setupMapBounds_=frame->bounds;setupMapDirty_=true;setupMapDrag_={};}
    // Map generation and all of its topology decisions are the original preview
    // maker. Only the final bitmap/line composition belongs to the native UI.
    auto key=std::array{session_.map,session_.rules.teams,session_.rules.mapSize,session_.rules.randomEncounters};
    if(key!=setupPreviewKey_ || setupPreviewFile_!=session_.customMap){
        setupPreviewKey_=key;setupPreviewFile_=session_.customMap;setupMap_=Game::mapPreview(session_,original_->assets().root());setupMapDrag_={};setupMapDirty_=true;
    }
    if(setupMapDirty_){
        setupMapDirty_=false;setupPreview_.clear();const auto& preview=setupMap_;setupMapCanDrag_=false;
        if(!preview.sectors.empty()){
            auto bounds=frame->bounds;auto lo=preview.sectors.front().position,hi=lo;
            for(const auto& sector:preview.sectors){lo=glm::min(lo,sector.position);hi=glm::max(hi,sector.position);}
            float buffer=.1f*std::max(.0001f,std::max(hi.x-lo.x,hi.y-lo.y));lo-=buffer;hi+=buffer;
            setupMapExtent_=hi-lo;
            float aspect=(bounds.z-8)/(bounds.w-8);
            if(hi.x-lo.x>2.2f*aspect || hi.y-lo.y>2.2f){setupMapCanDrag_=true;hi=lo+glm::min(hi-lo,glm::vec2(2.2f*aspect,2.2f));}
            float mapAspect=(hi.x-lo.x)/(hi.y-lo.y);
            if(mapAspect<aspect){float growth=((hi.y-lo.y)*aspect-(hi.x-lo.x))*.5f;lo.x-=growth;hi.x+=growth;}
            else {float growth=((hi.x-lo.x)/aspect-(hi.y-lo.y))*.5f;lo.y-=growth;hi.y+=growth;}
            setupMapSpan_=hi-lo;lo+=setupMapDrag_;hi+=setupMapDrag_;
            // AnimatedImagePane maps the original image's upward Y axis into
            // top-down pane coordinates, including its height-minus-one origin.
            auto point=[&](glm::vec2 p){auto local=glm::ivec2(glm::vec2(4)+(p-lo)/(hi-lo)*glm::vec2(bounds.z-8,bounds.w-8));return glm::ivec2(bounds)+glm::ivec2(local.x,int(bounds.w)-1-local.y);};
            auto clip=glm::vec4(glm::vec2(bounds)+glm::vec2(1),bounds.z-2,bounds.w-2);
            auto sectorPoint=[&](int id){return point(std::find_if(preview.sectors.begin(),preview.sectors.end(),[&](const auto& s){return s.id==id;})->position);};
            for(auto link:preview.links){auto p=sectorPoint(link[0]),end=sectorPoint(link[1]);auto d=glm::abs(end-p);int sx=p.x<end.x?1:-1,sy=p.y<end.y?1:-1,error=d.x-d.y;auto start=p,last=p;
                auto run=[&]{OriginalControl c;c.kind=OriginalControl::Kind::Rectangle;c.bounds={std::min(start.x,last.x),start.y,std::abs(last.x-start.x)+1,1};c.color={.5f,.5f,.5f,1};c.clip=clip;setupPreview_.push_back(std::move(c));};
                for(;;){if(p.y!=start.y){run();start=p;}last=p;if(p==end)break;int twice=error*2;if(twice>-d.y){error-=d.y;p.x+=sx;}if(twice<d.x){error+=d.x;p.y+=sy;}}run();
            }
            constexpr glm::vec4 colors[]={{188.f/255,160.f/255,0,1},{0,138.f/255,217.f/255,1},{156.f/255,16.f/255,102.f/255,1},{50.f/255,140.f/255,20.f/255,1},{1,145.f/255,145.f/255,1},{50.f/255,200.f/255,125.f/255,1}};
            auto empty=original_->assets().image("sectoremptybmp",true),owner=original_->assets().image("sectorownerbmp",true),invader=original_->assets().image("sectorinvaderbmp",true);
            for(const auto& sector:preview.sectors){auto at=point(sector.position);auto image=[&](const std::shared_ptr<ImageData>& im,glm::vec4 color){OriginalControl c;c.kind=OriginalControl::Kind::Image;c.image=im;c.bounds={at.x-int(im->width/2),at.y-int(im->height/2),im->width,im->height};c.color=color;c.clip=clip;setupPreview_.push_back(std::move(c));};
                image(empty,glm::vec4(1));int first=-1,second=-1;
                for(int side=0;side<session_.rules.teams;++side)if(sector.stations[side]>0){if(first<0 || sector.stations[side]>sector.stations[first]){second=first;first=side;}else if(second<0 || sector.stations[side]>sector.stations[second])second=side;}
                if(first>=0)image(owner,colors[first]);if(second>=0)image(invader,colors[second]);
            }
        }
    }
    bool details=popups_.empty() && pointerPosition_.x>=setupMapBounds_.x && pointerPosition_.x<setupMapBounds_.x+setupMapBounds_.z && pointerPosition_.y>=setupMapBounds_.y && pointerPosition_.y<setupMapBounds_.y+setupMapBounds_.w;
    if(!setupMap_.error.empty()){
        auto message=setupMap_.error.starts_with('1')?"invalid settings":setupMap_.error;
        OriginalControl c;c.kind=OriginalControl::Kind::Text;c.font="smallFont";c.bounds=setupMapBounds_;c.bounds.y+=std::floor((c.bounds.w-original_->assets().font(c.font)->height)*.5f);c.text=message;c.justify=1;c.clip=setupMapBounds_;c.surfaceText();view_.artwork.push_back(std::move(c));
    }else if(details){
        auto font=original_->assets().font("largeBoldFont");float height=font->height;
        auto label=[&](int count,const char* singular,const char* plural,float y){OriginalControl c;c.kind=OriginalControl::Kind::Text;c.font="largeBoldFont";c.bounds={setupMapBounds_.x,y,setupMapBounds_.z,height};c.text=std::to_string(count)+(count==1?singular:plural);c.justify=1;c.clip=setupMapBounds_;c.surfaceText();view_.artwork.push_back(std::move(c));};
        float y=setupMapBounds_.y+std::floor((setupMapBounds_.w-2*height)*.5f);label(int(setupMap_.sectors.size())," sector"," sectors",y);label(int(setupMap_.links.size())," aleph"," alephs",y+height);
    }else view_.artwork.insert(view_.artwork.end(),setupPreview_.begin(),setupPreview_.end());
    if(interactive)view_.items.push_back({setupMapBounds_,"Preview the selected map.",{},{MenuOp::SetupMapDrag},setupMapCanDrag_,false,"mapPreviewPane"});
}
}
