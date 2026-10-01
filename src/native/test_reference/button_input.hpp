#pragma once
#include "authored_hosts.hpp"
#include "../frontend.hpp"
#include "../../ClientCore/GameTypes.h"

namespace NativeTest {
inline void checkButtonInput(Native::Assets& assets){
    using namespace Native;
    auto find=[](const Frontend& ui,std::string_view name){
        const auto& items=ui.view().items;auto i=std::find_if(items.begin(),items.end(),[&](const auto& item){return item.control==name && !item.blocked;});
        require(i!=items.end(),"button input fixture control is missing");return size_t(i-items.begin());
    };
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);SessionOptions options;options.scenario=Scenario::Conquest;ui.networkSession(options);
    GameState state;state.docked=state.economy=state.commander=state.networked=state.networkHost=true;state.playerSide=0;
    state.networkPilots={{7,0,"Commander",true,true,true,true,false},{8,1,"Opponent",true,false,true,false,false}};
    state.networkHostPilot=7;state.networkReady=true;state.money=1000;state.ship="Scout";state.station="Home";
    ResearchChoice research;research.id=11;research.type=OT_development;research.name="Research";research.model="missing-card";research.price=100;research.available=research.canBuy=research.predecessorReady=true;
    ui.researchService([&]{return std::vector{research};});
    size_t buttons=0;
    // ButtonUIPane: either button captures; right has a separate, unattached
    // event source. Disabled buttons still capture. Double-clicks do not.
    for(auto screen:{Screen::Home,Screen::Training,Screen::Games,Screen::Controls,Screen::Hangar,Screen::RemoteLobby,Screen::Results,Screen::Teleport,Screen::Research,Screen::Credits,Screen::GameDetails}){
        ui.setPlaying(screen==Screen::Hangar || screen==Screen::RemoteLobby || screen==Screen::Teleport || screen==Screen::Research);
        auto local=state;local.networkLobby=screen==Screen::RemoteLobby;local.complete=screen==Screen::Results;
        ui.open(Screen::Home);ui.open(screen,&local);
        std::vector<std::string> names;for(const auto& item:ui.view().items)if(item.captureOnPress && !item.toggleOnRelease && !item.blocked && item.action.op!=MenuOp::ScrollThumb)names.push_back(item.control);
        for(const auto& name:names){
            auto index=find(ui,name);auto before=ui.view().items[index];
            ui.pointerMove(glm::vec2(before.bounds)+glm::vec2(2));ui.clearSounds();
            require(ui.pointerContext(index).effect==FrontEffect::None && ui.contextButtonCaptured() && ui.view().capturedButton() && ui.view().capturedButton()->control==name,"ordinary right button press loses capture or fires left callback");
            ui.displayScale(1);index=find(ui,name);
            auto release=ui.pointerRelease(index,false,SDL_BUTTON_RIGHT);
            if(release.effect!=FrontEffect::None || ui.buttonCaptured() || ui.screen()!=screen || !ui.sounds().empty())throw std::runtime_error("Ordinary right release: "+name+", screen "+std::to_string(int(screen))+", effect "+std::to_string(int(release.effect))+", captured "+std::to_string(ui.buttonCaptured())+", actual screen "+std::to_string(int(ui.screen()))+", sounds "+std::to_string(ui.sounds().size()));
            require(ui.view().items[find(ui,name)].selected==before.selected,"non-toggle right button changes checked state");
            ui.pointerContext(find(ui,name));ui.pointerRelease({},false,SDL_BUTTON_RIGHT);
            require(!ui.buttonCaptured() && ui.screen()==screen && ui.sounds().empty(),"canceled ordinary right release acts or retains capture");
            if(before.action.op!=MenuOp::TrainingSelect){require(ui.pointerContext(find(ui,name),true).effect==FrontEffect::None && !ui.buttonCaptured() && ui.screen()==screen && ui.sounds().empty(),"unhandled right double-click captures or activates ordinary button");}
            ++buttons;
        }
    }
    // The training double-click sink is shared by both buttons and has no
    // normal click sound. It launches the already selected mission.
    for(bool right:{false,true}){
        Frontend training(settings);training.originalInterface(assets);training.open(Screen::Training);training.activate(find(training,"trainMission2ButtonPane"));training.clearSounds();
        auto index=find(training,"trainMission3ButtonPane");
        auto result=right?training.pointerContext(index,true):training.pointerPress(index,true);
        require(result.effect==FrontEffect::Start && training.session().trainingMission==2 && !training.buttonCaptured() && training.sounds().empty(),"training double-click loses selected mission, captures, or inherits normal click sound");
    }
    struct Check {const char* name;bool MissionSettings::*field;};
    constexpr Check checks[]={{"ejectPodsCheckboxPane",&MissionSettings::ejectPods},{"friendlyFireCheckboxPane",&MissionSettings::friendlyFire},
        {"invulnerableStationsCheckboxPane",&MissionSettings::invulnerableStations},{"developmentCheckboxPane",&MissionSettings::developments},
        {"showMapCheckboxPane",&MissionSettings::showMap},{"allowShipyardsCheckboxPane",&MissionSettings::shipyard},
        {"allowTacticalCheckboxPane",&MissionSettings::tactical},{"allowSupremacyCheckboxPane",&MissionSettings::supremacy},
        {"allowExpansionCheckboxPane",&MissionSettings::expansion},{"defectionsCheckboxPane",&MissionSettings::allowDefections},{"joinersCheckboxPane",&MissionSettings::allowJoiners}};
    size_t toggles=0;
    for(const auto& check:checks)for(bool accept:{false,true})for(bool cancelPress:{false,true}){
        Settings preferences;Frontend setup(preferences);setup.originalInterface(assets);setup.networkSession(options);setup.open(Screen::Lobby);
        auto index=find(setup,check.name);bool before=setup.view().items[index].selected;require(setup.view().items[index].toggleOnRelease,"MDL checkbox toggle flag was dropped");
        setup.clearSounds();setup.pointerContext(index);setup.pointerRelease(cancelPress?std::nullopt:std::optional(index),false,SDL_BUTTON_RIGHT);
        require(setup.view().items[find(setup,check.name)].selected==(cancelPress?before:!before) && setup.session().rules.*check.field==before && setup.sounds().empty(),"right checkbox callback leaks into staged rules, loses toggle, or fires sound");
        setup.displayScale(1);require(setup.view().items[find(setup,check.name)].selected==(cancelPress?before:!before),"retained UI refresh loses silent right toggle");
        setup.activate(find(setup,accept?"createButtonPane":"cancelButtonPane"));
        require(setup.session().rules.*check.field==(accept && !cancelPress?!before:before),"setup Accept/Cancel does not read/discard original checked state");++toggles;
    }
    // Only the original related-checkbox callbacks update the preset label.
    for(const char* checkbox:{"ejectPodsCheckboxPane","developmentCheckboxPane"})for(bool right:{false,true}){
        Settings preferences;Frontend setup(preferences);setup.originalInterface(assets);setup.networkSession(options);setup.open(Screen::Lobby);
        auto caption=[&]{for(const auto& c:setup.view().artwork)if(c.widget==OriginalControl::Widget::StringCombo && c.kind==OriginalControl::Kind::Text && c.bounds.x==authoredControl(setup.view().artwork,"gameTypeComboPane").bounds.x)return c.text;throw std::runtime_error("preset caption fixture missing");};
        auto initial=caption();auto index=find(setup,checkbox);
        if(right){setup.pointerContext(index);setup.pointerRelease(index,false,SDL_BUTTON_RIGHT);}else {setup.pointerPress(index);setup.pointerRelease(index);}
        require(caption()==(right || std::string_view(checkbox)=="ejectPodsCheckboxPane"?initial:"Custom"),"preset recognition ignores original callback wiring");
        setup.activate(find(setup,"createButtonPane"));require(setup.screen()==Screen::LobbyPilots,"silent setup toggle cannot be accepted");
    }
    // Artwork can change toggle behavior without changing faces or C++ hosts.
    {
        AuthoredAssets variant(assets.root());variant.change("newgamescreen.mdl",{{"friendlyFireCheckboxPane           = ButtonPane(checkImage, ButtonNormalCheckBox, true)","friendlyFireCheckboxPane           = ButtonPane(checkImage, ButtonNormalCheckBox, false)"}});
        Assets alternate(variant.root);Settings preferences;Frontend setup(preferences);setup.originalInterface(alternate);setup.networkSession(options);setup.open(Screen::Lobby);
        auto index=find(setup,"friendlyFireCheckboxPane");require(!setup.view().items[index].toggleOnRelease,"alternate authored false toggle inferred from bitmap faces");
        setup.pointerPress(index);setup.pointerRelease(index);require(!setup.session().rules.friendlyFire && !setup.view().items[find(setup,"friendlyFireCheckboxPane")].selected,"false-toggle MDL checkbox is toggled by its host");
    }
    {
        Settings preferences;Frontend modal(preferences);modal.originalInterface(assets);modal.startupTrainingSuggestion();auto index=find(modal,"trainingDontAskAgain");modal.clearSounds();
        modal.pointerContext(index);modal.pointerRelease(index,false,SDL_BUTTON_RIGHT);modal.displayScale(1);
        require(modal.view().items[find(modal,"trainingDontAskAgain")].selected && modal.sounds().empty(),"modal right checkbox loses toggle or emits sound");
        modal.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(!preferences.showStartupTrainingSuggestion,"modal cancellation does not read silently toggled original checkbox");
    }
    for(bool pending:{false,true}){
        Settings preferences;Frontend details(preferences);details.originalInterface(assets);details.missionDetailsService([]{return Allegiance::MissionDetails{};});details.open(Screen::GameDetails);
        if(pending)details.connecting();else details.error("Details modal input");
        details.pointerMove({-1,-1});details.clearSounds();
        require(details.pointerContext({}).effect==FrontEffect::None && details.screen()==Screen::GameDetails && details.hasMessageBox() && details.sounds().empty(),"outside right press dismisses Details underneath a captured modal");
        if(!pending){auto index=find(details,"messageOK");details.pointerContext(index);require(details.buttonCaptured(),"Details modal button cannot capture right press");details.pointerRelease(index,false,SDL_BUTTON_RIGHT);require(details.hasMessageBox() && details.screen()==Screen::GameDetails,"Details modal right release fires left callback");}
    }
    {
        Settings preferences;Frontend help(preferences);help.originalInterface(assets);help.showHelp(true);const auto& pane=help.helpView(1);
        auto close=std::find_if(pane.items.begin(),pane.items.end(),[](const auto& item){return item.action.op==MenuOp::SecondaryClose;});require(close!=pane.items.end(),"Help right capture fixture missing");auto index=size_t(close-pane.items.begin());help.clearSounds();
        help.helpPress(index,SDL_BUTTON_RIGHT);require(help.helpPointerCaptured() && help.helpView(1).capturedButton(),"Help right press does not capture");
        help.helpRelease(index,SDL_BUTTON_RIGHT);require(help.helpOn() && !help.helpPointerCaptured() && help.sounds().empty(),"Help right release closes or sounds");
        help.helpPress(index,SDL_BUTTON_RIGHT,true);require(!help.helpPointerCaptured(),"Help right double-click captures");
    }
    printf("Original button input: %zu controls across eleven hosts, %zu setup right-toggle/Accept/Cancel cases, callback-driven presets, alternate MDL toggle, shared training double-click, modal and Help secondary input passed without a Game\n",buttons,toggles);
}
}
