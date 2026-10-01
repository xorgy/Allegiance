#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include "../../ClientCore/LogonPresentation.h"
namespace NativeTest {
inline void checkLanDialog(Native::Assets& assets){
    using namespace Native;Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.open(Screen::Online);
    auto item=[&](MenuOp op,int value=0){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.action.op==op && c.action.value==value;});require(i!=ui.view().items.end(),"LAN dialog action missing");return size_t(i-ui.view().items.begin());};
    require(!ui.view().items[item(MenuOp::LanJoin)].enabled && !ui.wantsText(),"empty original LAN list is joinable or invents an edit control");
    std::vector<Net::ServerListing> listings;
    for(int i=0;i<32;++i){Net::ServerInfo info{};info.flags=Net::ServerWaiting|Net::ServerJoinable;info.players=i;Net::text(info.name,"Game "+std::string(i<10?"0":"")+std::to_string(i));listings.push_back({"127.0.0.1:"+std::to_string(28000+i),info});}
    ui.gameListings(listings);auto list=authoredControlBounds(ui.view().artwork,"serverListPane");auto row=ui.view().items[item(MenuOp::LanSelect)];
    require(row.bounds==glm::vec4(list.x,list.y,list.z,14),"LAN row ignores original MDL viewport or fourteen-pixel painter");
    require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.text.starts_with("(0/") && c.singleLine && c.font=="smallFont";}),"LAN occupancy painter wraps instead of drawing one original bitmap-font line");
    require(!ui.view().items[item(MenuOp::LanJoin)].enabled,"LAN discovery refresh selects a server without player input");
    ui.pointerPress(item(MenuOp::LanSelect));ui.pointerRelease(item(MenuOp::LanSelect));ui.pointerPress(item(MenuOp::LanSelect,-1));
    require(!ui.view().items[item(MenuOp::LanJoin)].enabled && !ui.pointerCaptured(),"LAN blank-list click retains a selected join target");ui.pointerRelease({});
    ui.pointerPress(item(MenuOp::LanSelect,1));require(ui.view().items[item(MenuOp::LanSelect,1)].selected && !ui.pointerCaptured(),"LAN list fails to select on down without capture");
    require(ui.pointerRelease(item(MenuOp::LanSelect,1)).effect==FrontEffect::None,"LAN list joins on an ordinary release");
    require(ui.pointerPress(item(MenuOp::LanSelect,1),true).effect==FrontEffect::None && ui.serverAddress()=="127.0.0.1:28001" && ui.screen()==Screen::Home && ui.keyboardPopup(),"LAN double-click skips the original callsign dialog or selects another endpoint");require(ui.pointerRelease({}).effect==FrontEffect::None,"LAN double-click release connects twice");
    require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::Connect,"prefilled original callsign does not join on Enter");ui.open(Screen::Online);
    std::reverse(listings.begin(),listings.end());ui.gameListings(listings);
    require(ui.view().items[item(MenuOp::LanSelect,1)].selected && ui.activate(item(MenuOp::LanJoin)).effect==FrontEffect::None && ui.serverAddress()=="127.0.0.1:28001" && ui.keyboardPopup(),"LAN refresh/reordering redirects selected endpoint");
    require(ui.activate(item(MenuOp::LogonAccept)).effect==FrontEffect::Connect,"original Logon button fails to join");ui.open(Screen::Online);
    for(int i=0;i<6;++i)ui.key(SDLK_PAGEDOWN,SDL_SCANCODE_PAGEDOWN);
    require(ui.view().items[item(MenuOp::LanSelect,31)].selected && ui.activate(item(MenuOp::LanJoin)).effect==FrontEffect::None && ui.serverAddress()=="127.0.0.1:28031" && ui.keyboardPopup(),"LAN list keyboard paging cannot reach its final server");
    require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::Connect,"paged original callsign fails to join");ui.open(Screen::Online);
    auto locked=std::find_if(listings.begin(),listings.end(),[](const auto& l){return l.address=="127.0.0.1:28031";});locked->info.flags=Net::ServerLocked;ui.gameListings(listings);
    require(!ui.view().items[item(MenuOp::LanJoin)].enabled && ui.pointerPress(item(MenuOp::LanSelect,31),true).effect==FrontEffect::None,"LAN join bypasses live admission state");
    std::erase_if(listings,[](const auto& l){return l.address=="127.0.0.1:28031";});ui.gameListings(listings);
    require(!ui.view().items[item(MenuOp::LanJoin)].enabled && ui.activate(item(MenuOp::LanJoin)).effect==FrontEffect::None,"departed LAN server retains a stale join target");
    ui.gameListings({});require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.text=="Searching...";}),"empty LAN refresh loses original Searching... row");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(ui.screen()==Screen::Home,"LAN cancellation does not return to original main menu");
    auto enterLogon=[&]{ui.open(Screen::Online);ui.gameListings(listings);ui.activate(item(MenuOp::LanSelect));require(ui.activate(item(MenuOp::LanJoin)).effect==FrontEffect::None && ui.screen()==Screen::Home && ui.keyboardPopup(),"LAN Join creates a connection before callsign acceptance");};
    auto nameEdit=[&]() -> const OriginalControl& {auto bounds=authoredControlBounds(ui.view().artwork,"nameEditPane");auto i=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.bounds==bounds && c.textMode==OriginalControl::TextMode::Edit;});require(i!=ui.view().artwork.end(),"original callsign editor missing");return *i;};
    enterLogon();
    require(!ui.wantsText() && !nameEdit().editFocused,"prefilled original logon focus differs before Name click/Tab");
    auto originalName=settings.pilot;ui.text("hidden input");require(nameEdit().text==originalName,"prefilled hidden-password focus edits the visible name");
    require(std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.name=="passwordPane" || c.name=="passwordEditPane" || c.name=="savePasswordButtonPane" || c.name=="cdKeyButtonPane" || c.name=="signUpButtonPane";}),"LAN callsign dialog exposes account/password/CD-key controls");
    OriginalUi authored(assets);auto layout=authored.layout("logon","popup",{{"Callsign",0},{"ZoneID",1},{"PassportID",2},{"logonNameType",0}},{{"promptText",Allegiance::LanLogonPrompt}});
    auto origin=glm::floor((glm::vec2(800,600)-glm::vec2(layout.front().bounds.z,layout.front().bounds.w))*.5f);
    for(auto name:{"nameEditPane","logonButtonPane","abortButtonPane"})require(authoredControl(ui.view().artwork,name).bounds==authoredControl(layout,name).bounds+glm::vec4(origin,0,0),"callsign control geometry differs from original MDL");
    require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.text==Allegiance::LanLogonPrompt;}),"original LAN logon prompt missing");
    ui.key(SDLK_TAB,SDL_SCANCODE_TAB);require(ui.wantsText() && nameEdit().editFocused,"original LAN Tab does not focus Name");
    for(size_t i=0;i<originalName.size();++i)ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);
    require(!ui.view().items[item(MenuOp::LogonAccept)].enabled,"empty callsign leaves original Logon enabled");
    require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None && !ui.keyboardPopup() && settings.pilot==originalName,"empty-name Enter differs from original Abort or changes saved name");
    enterLogon();ui.activate(item(MenuOp::LogonName));for(size_t i=0;i<originalName.size();++i)ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);ui.text(" leading");
    require(ui.view().items[item(MenuOp::LogonAccept)].enabled && ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::None && ui.keyboardPopup(),"leading-space callsign differs from original enabled-but-refused Logon");
    ui.activate(item(MenuOp::LogonCancel));require(!ui.keyboardPopup() && settings.pilot==originalName,"Abort commits a staged callsign");
    enterLogon();ui.activate(item(MenuOp::LogonName));for(size_t i=0;i<originalName.size();++i)ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);ui.text("Veteran012345678901234567890123456789");
    auto acceptedName=nameEdit().text;require(acceptedName.size()==23,"callsign ignores original name/wire-record limit");
    auto endpoint=ui.serverAddress();std::reverse(listings.begin(),listings.end());ui.gameListings(listings);
    require(ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN).effect==FrontEffect::Connect && settings.pilot==acceptedName && ui.session().pilot==acceptedName && ui.serverAddress()==endpoint && !ui.keyboardPopup(),"callsign acceptance fails identity commit or discovery refresh redirects the selected endpoint");
    enterLogon();ui.text("discarded");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(!ui.keyboardPopup() && ui.screen()==Screen::Home && settings.pilot==acceptedName,"callsign Escape loses original main-menu return or changes saved identity");
    settings.pilot.clear();enterLogon();require(ui.wantsText() && nameEdit().editFocused && !ui.view().items[item(MenuOp::LogonAccept)].enabled,"empty saved callsign does not focus Name and disable Logon");
    ui.key(SDLK_TAB,SDL_SCANCODE_TAB);ui.text("\xc3\xa9");ui.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);require(ui.wantsText() && nameEdit().text.empty(),"LAN Tab loses Name focus or Backspace leaves a partial UTF-8 codepoint");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    settings.pilot=std::string(32,'x');enterLogon();require(nameEdit().text==std::string(23,'x') && !ui.wantsText(),"saved callsign ignores original SetMaxLength truncation or prefilled focus");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);require(settings.pilot==std::string(32,'x'),"Abort commits saved-name truncation without Logon");
    settings.pilot=acceptedName;
    puts("Original LAN callsign: authored popup/artwork/prompt, account-control hiding, prefilled Name/Tab/click focus, Logon/Abort/Escape, empty/leading-space policy, 23-byte limit, staged identity and discovery-refresh endpoint retention passed");
    puts("Original LAN dialog: authored rows, Searching/disabled Join, down selection/double-click join, endpoint retention, full paging, admission changes, departure and cancellation passed");
}
}
