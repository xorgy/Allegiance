#pragma once
#include "authored_hosts.hpp"

namespace NativeTest {
inline void checkClassicUiBatch(Native::Assets& assets) {
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);SessionOptions options;options.scenario=Scenario::Conquest;ui.networkSession(options);ui.setPlaying(true);
    GameState lobby;lobby.networked=lobby.networkLobby=lobby.networkHost=lobby.commander=lobby.docked=true;lobby.playerSide=0;lobby.shipId=40;lobby.networkHostPilot=40;
    for(int id=40;id<70;++id)lobby.networkPilots.push_back({id,0,"Pilot "+std::to_string(id),true,id==40,id==40,id==40,false,noTeamRequest,id%10});
    auto item=[&](MenuOp op,int delta=-1){auto& list=ui.view().items;auto i=std::find_if(list.begin(),list.end(),[&](const auto& c){return c.action.op==op && (delta<0 || c.action.delta==delta);});require(i!=list.end(),"Grouped classic UI action missing");return size_t(i-list.begin());};
    auto enter=[&]{ui.open(Screen::Home);ui.open(Screen::RemoteLobby,&lobby);ui.activate(item(MenuOp::RosterSort));ui.activate(item(MenuOp::RosterSelect,41));ui.activate(item(MenuOp::ComposeChat));while(!ui.chatText().empty())ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);ui.text("Preserved private draft");};
    auto parent=[&]{require(ui.screen()==Screen::RemoteLobby && !ui.hasMessageBox(),"Closing a transient UI changes its lobby parent");
        require(ui.view().artwork.front().image==assets.image("teamscreenbmp",false),"Transient UI substitutes a hangar/intro backdrop");
        require(ui.chatText()=="Preserved private draft" && ui.wantsText(),"Transient UI loses the private chat draft/focus");
        require(std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.hoverBlocked;}),"Closing a transient screen leaves its parent hover blocked");
        require(std::any_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.action.op==MenuOp::RosterSelect && c.action.delta==41 && c.selected;}),"Transient UI resets stable roster selection");
        require(ui.outgoingChatText()=="Preserved private draft" && ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).targetId==41,"Transient UI changes the private draft's recipient");};
    for(auto popup:{Screen::Settings,Screen::Pause,Screen::Controls,Screen::Credits,Screen::GameDetails}){
        enter();ui.open(popup);require(ui.keyboardPopup() || popup==Screen::GameDetails,"Transient UI fails keyboard capture");
        if(popup!=Screen::Help)require(std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.helpSite=="teamscreen::hoverSite" && !c.hoverBlocked;}),"Transient screen retains active lobby hover sources");
        require(!ui.wantsText(),"Transient UI permits typing into its parent's retained field");ui.text("leaked");
        require(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::None,"Docked popup cancellation incorrectly resumes flight");parent();
    }
    enter();auto focus=ui.view().focus;ui.open(Screen::ConfirmLeave);
    require(ui.screen()==Screen::RemoteLobby && ui.hasMessageBox() && ui.keyboardPopup(),"Quit is a replacement screen rather than an original modal");
    require(ui.sceneView().artwork.front().image==assets.image("teamscreenbmp",false),"Quit modal loses live lobby artwork");
    require(std::all_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& c){return c.action.op==MenuOp::MessageDismiss || c.blocked;}),"Quit modal leaks a parent action");
    ui.pageChat(true);ui.pointerContext({});ui.key(SDLK_F1,SDL_SCANCODE_F1);ui.text("leaked");require(ui.hasMessageBox(),"Quit modal fails input capture");
    ++lobby.economyRevision;ui.sync(lobby);ui.pointerPress(item(MenuOp::MessageDismiss));ui.pointerRelease({});require(ui.hasMessageBox(),"Quit accepts a release outside its capture");
    require(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::None,"Lobby Quit cancellation resumes flight");parent();require(ui.view().focus==focus,"Modal cancellation loses the parent's keyboard selection");
    ui.open(Screen::ConfirmLeave);require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::Leave && !ui.hasMessageBox(),"Quit confirmation fails to leave the mission");
    // The original global menu closes before opening Quit, Help or mappings.
    enter();ui.open(Screen::Pause);ui.key(SDLK_Q,SDL_SCANCODE_Q);require(ui.hasMessageBox() && ui.screen()==Screen::RemoteLobby,"Menu Quit is not hosted by the actual lobby");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);parent();
    enter();ui.open(Screen::Pause);ui.key(SDLK_H,SDL_SCANCODE_H);ui.open(Screen::Help);ui.toggleHelp();parent();
    for(auto popup:{Screen::Pause,Screen::GameDetails}){
        enter();ui.open(popup);auto base=ui.sceneView().artwork.size();ui.error("Nested modal failure.");
        require(ui.sceneView().artwork.size()==base,"Nested modal duplicates its separately composed popup layer");
        require(std::all_of(ui.menuPopupView().items.begin(),ui.menuPopupView().items.end(),[](const auto& c){return c.blocked;}),"Nested modal leaves popup hover/actions active");
        require(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::None && ui.screen()==popup,"Nested message dismissal closes its popup parent");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);parent();
    }
    GameState flight;flight.docked=false;ui.open(Screen::Hangar,&flight);ui.open(Screen::Pause,&flight);ui.key(SDLK_Q,SDL_SCANCODE_Q);
    require(ui.hasMessageBox() && ui.view().overlay && ui.messageView().items.size()==2 && ui.sceneView().artwork.empty(),"Flight Quit adds a static hangar or retains the global menu");
    require(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::Resume,"Flight Quit cancellation fails to resume flight");
    ui.open(Screen::Pause,&flight);ui.error("Flight failure.",true);require(ui.menuPopupView().artwork.empty() && ui.sceneView().artwork.empty(),"Flight modal reintroduces a separately rendered Escape menu");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    auto training=flight;training.trainingMission=2;ui.open(Screen::Hangar,&training);ui.open(Screen::ConfirmLeave);
    require(authoredControl(ui.view().artwork,"messageText").text=="Quit the Current Training Mission?","Training uses the wrong original Quit text");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    ui.open(Screen::Home);require(ui.activate(item(MenuOp::Quit)).effect==FrontEffect::Quit,"Application exit invents a confirmation absent from WinTrek");
    ui.error("ONE\nTWO\nTHREE\nFOUR\nFIVE\nSIX\nSEVEN\nEIGHT\nNINE\nTEN");auto message=authoredControl(ui.view().artwork,"messageText");
    require(message.textMode==OriginalControl::TextMode::Image && message.bounds.w==assets.font("largeFont")->height*10 && message.clip==std::optional(authoredControlBounds(ui.view().artwork,"messageBackground")),"Message box uses an invented height rather than original StringImage measurement/clipping");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    Frontend browser(settings);browser.originalInterface(assets);
    for(auto op:{MenuOp::GameFilter,MenuOp::GameCreate}){
        browser.open(Screen::Games);auto action=std::find_if(browser.view().items.begin(),browser.view().items.end(),[&](const auto& c){return c.action.op==op;});
        require(action!=browser.view().items.end(),"Grouped dialog hover check lacks its original Games action");browser.activate(size_t(action-browser.view().items.begin()));
        auto captured=[&]{bool parent=false,child=false;for(const auto& c:browser.view().artwork)if(!c.helpSite.empty()){
            if(c.helpSite=="gamescreen::hoverSite"){parent=true;require(c.hoverBlocked,"Games dialog keeps its parent hover active");}
            else {child=true;require(!c.hoverBlocked,"Games dialog's active hover is blocked with its parent");}}
            require(parent && child,"Games dialog hover capture test misses one layer");};
        captured();browser.error("Nested dialog message.");require(std::all_of(browser.view().artwork.begin(),browser.view().artwork.end(),[](const auto& c){return c.helpSite.empty() || c.hoverBlocked;}),"Nested message leaves the active dialog hover unblocked");
        browser.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);captured();browser.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    }

    AuthoredAssets fixture(assets.root());
    fixture.change("hangar.mdl",{{"hoverInfoPane","renamedHoverHost"},{"StringPane(\"Launch ship\", HoverTextColor, HoverTextSize, JustifyCenter, HoverTextFont)","StringPane(\"Authored launch feedback\", Color(.2,.4,.6), Point(211,17), JustifyRight, smallFont)"}});
    fixture.change("introscreen.mdl",{{"hoverInfoPane","renamedIntroFeedback"},{"Play a training mission.","Authored training feedback."}});
    fixture.change("loadoutpane.mdl",{{"geoPosition = Point(170, 180)","geoPosition = Point(181, 191)"},{"geoSize     = Point(310, 185)","geoSize     = Point(281, 165)"}});
    fixture.change("loadout.mdl",{{"//loadout8overlay  = loadoutLightFighterOverlay;","loadout8overlay = TranslateImage(ImportImage(\"ldefaultbmp\",true),Point(3,7));"}});
    Assets alternate(fixture.root);Frontend authored(settings);authored.originalInterface(alternate);authored.setPlaying(true);authored.open(Screen::Hangar,&flight);
    auto text=std::find_if(authored.view().artwork.begin(),authored.view().artwork.end(),[](const auto& c){return c.text=="Authored launch feedback";});
    require(text!=authored.view().artwork.end() && text->name=="renamedHoverHost" && text->font=="smallFont" && text->color==glm::vec4(.2,.4,.6,1) && text->bounds==glm::vec4(100,561,211,17) && text->justify==2,"Hover host ignores the authored branch/name/font/style/size");
    require(text->hoverConditions.size()==1 && text->hoverConditions[0].site=="CurrentHover" && text->hoverConditions[0].value==1,"Hangar hover lacks a resident branch predicate");
    require(authoredControl(authored.view().artwork,"launchButtonPane").helpID==1,"Hangar host fails to publish the original hover event");
    authored.setPlaying(false);authored.open(Screen::Home);
    require(std::any_of(authored.view().artwork.begin(),authored.view().artwork.end(),[](const auto& c){return c.text=="Authored training feedback." && c.name=="renamedIntroFeedback" && !c.hoverConditions.empty();}),"Intro feedback still substitutes a copied caption");
    auto loader=[&](const std::string& name,const std::string&){auto m=assets.model(name);return ModelHandle{1,m->radius,m->frames,m->lights};};
    SessionOptions local;local.startInHangar=true;local.enemies=0;Game game(assets.root(),loader,0,0,local);
    authored.loadoutServices([&](const Loadout& l,bool defaults,int target){return game.previewLoadout(l,defaults,target);},[&](PartKind k,int m,int h){return game.loadoutPartChoices(k,m,h);});
    authored.setPlaying(true);authored.open(Screen::ShipLoadout,&game.state());
    require(authoredControl(authored.view().artwork,"loadoutGeoPane").bounds==glm::vec4(181,191,281,165),"Loadout preview retains copied MDL placement or dimensions");
    auto staged=authored.stagedLoadout().loadout;authored.open(Screen::Pause,&game.state());authored.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    require(authored.screen()==Screen::ShipLoadout && authored.stagedLoadout().loadout==staged,"Options return refits or replaces the staged loadout");
    OriginalUi definitions(alternate);auto overlay=definitions.loadoutOverlay(8);auto original=definitions.imageLayout("loadout","loadout8overlay");
    require(!overlay.empty() && original.size()==overlay.size(),"Loadout overlay fixture differs structurally");
    for(size_t i=0;i<overlay.size();++i)require(glm::vec2(overlay[i].bounds)-glm::vec2(original[i].bounds)==glm::vec2(181,356) && overlay[i].clip==std::optional(glm::vec4(181,191,281,165)),"Loadout overlay fails to follow the authored preview image origin/clip");
    puts("Grouped classic UI: six transient parent/focus/draft paths, lobby/flight/training Quit, modal capture/refresh, immediate app exit, renamed authored hover branches and loadout preview geometry passed");
}
}
