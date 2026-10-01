#include "keyboard.hpp"
#include "input_bindings.hpp"
#include "mouse.hpp"
#include "mdl_text.hpp"
#include "test_reference/key_map.hpp"
#include <fstream>
#include <cstdio>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
#include <stdexcept>
#include <tuple>
#include <map>
using namespace Native;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
SDL_Keymod chord(uint8_t bits){return SDL_Keymod((bits&1?SDL_KMOD_SHIFT:0)|(bits&2?SDL_KMOD_CTRL:0)|(bits&4?SDL_KMOD_ALT:0));}
size_t compareInstalledMap(const std::filesystem::path& path){
    std::ifstream in(path);require(bool(in),"installed input map is missing");std::string text{std::istreambuf_iterator<char>(in),{}};Mdl::Parser parser(text);OriginalKeyMap::Map original;
    while(!parser.end()){
        auto name=parser.word();if(name=="use"){parser.expression();parser.need(';');continue;}
        parser.need('=');auto value=parser.expression();parser.need(';');
        if(name=="keyCommandMap")for(auto& row:value.args)original.add({int(row.args[0].number),int(row.args[1].number),int(row.args[2].number)});
    }
    // Independent Windows/SDL fixtures, including all keys used by the shipped
    // maps. Reference command selection and held evaluation stay in VK space.
    std::map<int,SDL_Scancode> scans{{8,SDL_SCANCODE_BACKSPACE},{9,SDL_SCANCODE_TAB},{13,SDL_SCANCODE_RETURN},
        {16,SDL_SCANCODE_LSHIFT},{17,SDL_SCANCODE_LCTRL},{18,SDL_SCANCODE_LALT},{19,SDL_SCANCODE_PAUSE},
        {27,SDL_SCANCODE_ESCAPE},{32,SDL_SCANCODE_SPACE},{33,SDL_SCANCODE_PAGEUP},{34,SDL_SCANCODE_PAGEDOWN},
        {35,SDL_SCANCODE_END},{36,SDL_SCANCODE_HOME},{37,SDL_SCANCODE_LEFT},{38,SDL_SCANCODE_UP},
        {39,SDL_SCANCODE_RIGHT},{40,SDL_SCANCODE_DOWN},{44,SDL_SCANCODE_PRINTSCREEN},{45,SDL_SCANCODE_INSERT},
        {46,SDL_SCANCODE_DELETE},{107,SDL_SCANCODE_KP_PLUS},{108,SDL_SCANCODE_KP_ENTER},{109,SDL_SCANCODE_KP_MINUS},
        {186,SDL_SCANCODE_SEMICOLON},{187,SDL_SCANCODE_EQUALS},{188,SDL_SCANCODE_COMMA},{189,SDL_SCANCODE_MINUS},
        {190,SDL_SCANCODE_PERIOD},{191,SDL_SCANCODE_SLASH},{192,SDL_SCANCODE_GRAVE},{219,SDL_SCANCODE_LEFTBRACKET},
        {220,SDL_SCANCODE_BACKSLASH},{221,SDL_SCANCODE_RIGHTBRACKET},{222,SDL_SCANCODE_APOSTROPHE}};
    for(int i=0;i<26;++i)scans.emplace(65+i,SDL_Scancode(SDL_SCANCODE_A+i));
    for(int i=0;i<12;++i)scans.emplace(112+i,SDL_Scancode(SDL_SCANCODE_F1+i));
    for(int i=1;i<=9;++i)scans.emplace(48+i,SDL_Scancode(SDL_SCANCODE_1+i-1));scans.emplace(48,SDL_SCANCODE_0);
    Settings settings;settings.importInputMap(path);KeyboardBindings keyboard(settings);
    std::array<int,TK_Max> commands;commands.fill(-1);
    for(size_t i=0;i<settings.keys.size();++i)if(settings.keyCommands[i]>=0)commands[settings.keyCommands[i]]=int(i);
    for(auto a:settings.keyAliases)if(a.command>=0)commands[a.command]=int(a.binding);
    size_t comparisons=0;
    for(auto [vk,scan]:scans)for(uint8_t m=0;m<8;++m){
        int expected=original.translate(vk,m&1,m&2,m&4);if(expected>=0 && commands[expected]<0)expected=-1;
        if(keyboard.command(scan,chord(m))!=expected)throw std::runtime_error("Installed map press differs from WinTrek: VK "+std::to_string(vk)+" modifiers "+std::to_string(m));
        std::array<bool,256> originalKeys{};originalKeys[vk]=true;std::array<bool,SDL_SCANCODE_COUNT> keys{};keys[scan]=true;
        KeyboardBindings::Down wanted;
        for(int tk=0;tk<TK_Max;++tk)if(commands[tk]>=0 && original.held(tk,originalKeys,m&1,m&2,m&4))wanted.set(commands[tk]);
        auto actual=keyboard.sample(keys,chord(m));
        // Native-only extensions have no TrekKey and are outside this original
        // command comparison (HUD scale / diagnostics / forward look).
        for(size_t i=0;i<settings.keys.size();++i)if(trekKey(Binding(i))<0)actual.reset(i);
        require(actual==wanted,"installed map held controls differ from original Boolean inputs");++comparisons;
    }
    for(auto row:original.rows)require(scans.contains(row.vk),"installed map has a key missing from the independent platform fixtures");
    return comparisons;
}
}
int main(int argc,char** argv){try{
    require(argc>1,"Usage: allegiance-keyboard-check ARTWORK");
    Settings settings;KeyboardBindings keyboard(settings);std::array<bool,SDL_SCANCODE_COUNT> keys{};
    MouseBindings mouse(settings);
    require(mouse.pressed(SDL_BUTTON_LEFT)[size_t(Binding::Fire)] && mouse.pressed(SDL_BUTTON_RIGHT)[size_t(Binding::Missile)] && mouse.pressed(SDL_BUTTON_MIDDLE).none(),"native default mouse assignments");
    require(mouse.wheel(1)[size_t(Binding::ThrottleUp)] && mouse.wheel(-1)[size_t(Binding::ThrottleDown)] && mouse.wheel(0).none(),"native default wheel assignments");
    require(MouseBindings::index(SDL_BUTTON_MIDDLE)==2 && MouseBindings::index(SDL_BUTTON_RIGHT)==1 && MouseBindings::index(SDL_BUTTON_X1)==3 && MouseBindings::index(8)==7 && MouseBindings::index(9)==-1,"original and SDL mouse button ordering");
    require(mouse.sample(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK,true).count()==2,"simultaneous held mouse controls");
    require(mouse.sample(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK,false).none() && mouse.sample(SDL_BUTTON_LMASK,true).none() && mouse.pressed(SDL_BUTTON_LEFT).none(),"focus/menu resume must suppress buttons held across the transition");
    mouse.released(SDL_BUTTON_LEFT);require(mouse.sample(SDL_BUTTON_LMASK,true)[size_t(Binding::Fire)],"release and repress restores mouse controls");
    require(mouse.wheelTicks(.4f)==0 && mouse.wheelTicks(.4f)==0 && mouse.wheelTicks(.4f)==1 && mouse.wheelTicks(-2.2f)==-2,"fractional wheel motion preserves detents and direction");
    mouse.sample(0,false);require(mouse.wheelTicks(.4f)==0,"fresh command wheel fixture");
    require(mouse.sample(SDL_BUTTON_LMASK,false,true).none() && mouse.wheelTicks(.4f)==0,"command view must suppress held fire without clearing its wheel");
    mouse.sample(0,false,true);require(mouse.wheelTicks(.4f)==1,"command view loses partial wheel detents across frames");
    mouse.sample(0,false);require(mouse.wheelTicks(.8f)==0,"lost input focus retains an old wheel fraction");
    mouse.sample(0,false);
    require(keyboard.pressed(SDL_SCANCODE_1,SDL_KMOD_NONE)==Binding::Weapon1 && keyboard.pressed(SDL_SCANCODE_1,SDL_KMOD_CTRL)==Binding::SwapWeapon1,"default weapon selection and configurable cargo swap must resolve separately");
    require(keyboard.pressed(SDL_SCANCODE_F8,SDL_KMOD_NONE)==Binding::Teleport && keyboard.pressed(SDL_SCANCODE_F8,SDL_KMOD_SHIFT)==Binding::EventLog,"teleport and event log shortcuts must resolve independently");
    require(keyboard.pressed(SDL_SCANCODE_PERIOD,SDL_KMOD_CTRL)==Binding::ToggleGrid && keyboard.pressed(SDL_SCANCODE_PRINTSCREEN,SDL_KMOD_NONE)==Binding::Screenshot,"grid chord and interactive screenshot defaults");
    require(!keyboard.keyDown(SDL_SCANCODE_LCTRL,SDL_KMOD_CTRL) && keyboard.keyDown(SDL_SCANCODE_PERIOD,SDL_KMOD_CTRL)==Binding::ToggleGrid,
            "original Any press table must not dispatch a Ctrl-bound vector lock while starting a chord");
    keyboard.keyUp(SDL_SCANCODE_PERIOD);keyboard.keyUp(SDL_SCANCODE_LCTRL);
    for(uint8_t wanted=0;wanted<=KeyAny;++wanted){
        settings.keys.fill(SDL_SCANCODE_UNKNOWN);settings.keys[size_t(Binding::Boost)]=SDL_SCANCODE_F10;settings.modifiers[size_t(Binding::Boost)]=wanted;keyboard.configure(settings);
        for(uint8_t modifiers=0;modifiers<8;++modifiers){
            keys.fill(false);keys[SDL_SCANCODE_F10]=true;
            OriginalKeyMap::Map original;original.add({121,wanted,TK_FireBooster});std::array<bool,256> originalKeys{};originalKeys[121]=true;
            bool expected=original.held(TK_FireBooster,originalKeys,modifiers&1,modifiers&2,modifiers&4);
            auto down=keyboard.sample(keys,chord(modifiers));
            require(down[size_t(Binding::Boost)]==expected && keyboard.command(SDL_SCANCODE_F10,chord(modifiers))==original.translate(121,modifiers&1,modifiers&2,modifiers&4),"Shift/Control/Alt held and press tables must match original independent policies");
            require(keyboard.sample(keys,SDL_Keymod(chord(modifiers)|SDL_KMOD_CAPS|SDL_KMOD_NUM))==down,"lock modifiers must not change flight chords");
        }
    }
    require(!keyboard.pressed(SDL_SCANCODE_UNKNOWN,SDL_KMOD_NONE) && !keyboard.sample(std::span(keys).first(0),SDL_KMOD_NONE).any(),"unknown/truncated keyboard input must be inert");
    settings=Settings{};size_t imported=settings.importInputMap(std::filesystem::path(argv[1])/"defaultinputmap.mdl");keyboard.configure(settings);
    require(keyboard.keyDown(SDL_SCANCODE_L,SDL_KMOD_NONE)==Binding::Coast && keyboard.keyDown(SDL_SCANCODE_L,SDL_KMOD_NONE)==Binding::Coast &&
            !keyboard.keyDown(SDL_SCANCODE_L,SDL_KMOD_CTRL),"original key-down and repeat dispatch immediately; modifier chords do not fall back");keyboard.keyUp(SDL_SCANCODE_L);
    require(!keyboard.keyIsDown(SDL_SCANCODE_L),"key-up releases held state without dispatching an action");
    require(imported>70 && !settings.keyAliases.empty(),"installed original keyboard map must retain aliases");
    require(bindingFor(TK_ViewBase)==Binding::CameraBase && trekKey(Binding::CameraBase)==TK_ViewBase,"original View Base command imports and keeps its logical identity");
    require(bindingFor(TK_VoteYes)==Binding::VoteYes && bindingFor(TK_VoteNo)==Binding::VoteNo && keyboard.pressed(SDL_SCANCODE_Y,SDL_KMOD_NONE)==Binding::VoteYes && keyboard.pressed(SDL_SCANCODE_N,SDL_KMOD_NONE)==Binding::VoteNo,"original Y/N ballot commands import and dispatch");
    require(keyboard.pressed(SDL_SCANCODE_SLASH,SDL_KMOD_NONE)==Binding::GameState && keyboard.command(SDL_SCANCODE_SLASH,SDL_KMOD_NONE)==TK_ConModeGameState,"original slash toggles the Game State console");
    mouse.configure(settings);mouse.sample(0,true);
    auto pointerButtons=MouseBindings::pilotButtons(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK|SDL_BUTTON_MMASK,true);
    require(pointerButtons==SDL_BUTTON_MMASK && MouseBindings::pilotButtons(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK,false)==(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK),"original cursor owns left/right selection without suppressing other mapped buttons");
    auto pointerDown=mouse.sample(pointerButtons,true);
    require(pointerDown[size_t(Binding::Missile)] && !pointerDown[size_t(Binding::Fire)] && !pointerDown[size_t(Binding::TargetCenter)] && mouse.wheel(.5f)[size_t(Binding::ThrottleUp)],"original cursor mode loses middle-button/wheel commands or fires through left/right scene selection");mouse.sample(0,true);
    require(mouse.pressed(SDL_BUTTON_LEFT)[size_t(Binding::Fire)] && mouse.pressed(SDL_BUTTON_RIGHT)[size_t(Binding::TargetCenter)] && mouse.pressed(SDL_BUTTON_MIDDLE)[size_t(Binding::Missile)] &&
        mouse.wheel(1)[size_t(Binding::ThrottleUp)] && mouse.wheel(-1)[size_t(Binding::ThrottleDown)],"installed original mouse fire, missile, center target and wheel throttle mappings");
    require(imported==107 && settings.controller.mapped && settings.controller.mapButtons.size()==2 && settings.controller.mapAxes[0]==ControllerAxisRoute{0,0,false,true,0} && settings.controller.mapAxes[2]==ControllerAxisRoute{0,3,false,true,3} && settings.controller.mapAxes[3]==ControllerAxisRoute{0,2,false,true,2} && settings.controller.mapAxes[4]==ControllerAxisRoute{0,0,true,false,4} && settings.controller.mapAxes[5].device==-1,"installed DirectInput device/value maps retain original roll/throttle order, look POV and inert debug-fill row");
    require(keyboard.command(SDL_SCANCODE_F3,SDL_KMOD_NONE)==TK_ConModeCommand && keyboard.command(SDL_SCANCODE_F3,SDL_KMOD_SHIFT)==TK_ToggleCommand &&
            keyboard.command(SDL_SCANCODE_F2,SDL_KMOD_NONE)==TK_ConModeCombat && keyboard.command(SDL_SCANCODE_F7,SDL_KMOD_NONE)==TK_ConModeNav,"import must preserve authored logical keys, including equivalent view aliases");
    for(auto [scan,normal,nearest,previous]:{std::tuple{SDL_SCANCODE_T,Binding::TargetFriendly,Binding::TargetFriendlyNearest,Binding::TargetFriendlyPrev},std::tuple{SDL_SCANCODE_F,Binding::TargetEnemy,Binding::TargetEnemyNearest,Binding::TargetEnemyPrev}}){
        require(keyboard.pressed(scan,SDL_KMOD_NONE)==normal && keyboard.pressed(scan,SDL_KMOD_SHIFT)==nearest && keyboard.pressed(scan,SDL_KMOD_CTRL)==previous,"installed original target cycle/nearest/previous chords");
    }
    require(keyboard.pressed(SDL_SCANCODE_KP_PLUS,SDL_KMOD_NONE)==Binding::CameraZoomIn && keyboard.pressed(SDL_SCANCODE_APOSTROPHE,SDL_KMOD_NONE)==Binding::CameraZoomIn,"original zoom key aliases must both work");
    for(int i=0;i<4;++i){
        require(bindingFor(TK_SwapTurret1+i)==Binding(int(Binding::SwapTurret1)+i) && bindingFor(TK_PromoteTurret1+i)==Binding(int(Binding::PromoteTurret1)+i),"all original turret swap and promotion commands retain their distinct logical bindings");
    }
    require(keyboard.pressed(SDL_SCANCODE_F8,SDL_KMOD_NONE)==Binding::Teleport && keyboard.pressed(SDL_SCANCODE_PAGEUP,SDL_KMOD_NONE)==Binding::ChatPageUp && keyboard.pressed(SDL_SCANCODE_PAGEDOWN,SDL_KMOD_NONE)==Binding::ChatPageDown,"original station teleport and message paging bindings");
    require(keyboard.pressed(SDL_SCANCODE_PERIOD,SDL_KMOD_CTRL)==Binding::ToggleGrid && keyboard.pressed(SDL_SCANCODE_PRINTSCREEN,SDL_KMOD_NONE)==Binding::Screenshot,"original sector grid and screenshot bindings");
    keys.fill(false);keys[SDL_SCANCODE_W]=true;require(keyboard.sample(keys,SDL_KMOD_ALT)[size_t(Binding::Up)],"original Any-modifier thrust must remain held");
    keyboard.configure(settings);keyboard.keyDown(SDL_SCANCODE_W,SDL_KMOD_NONE);
    require(keyboard.sample()[size_t(Binding::Up)],"accepted key-down starts continuous thrust");
    keyboard.setFocus(false);require(keyboard.sample().none() && !keyboard.keyDown(SDL_SCANCODE_W,SDL_KMOD_NONE),"focus loss clears held controls and refuses gameplay events");
    keyboard.setFocus(true);require(keyboard.sample().none(),"menu/focus resume must not sample keys pressed into another UI");
    keyboard.keyDown(SDL_SCANCODE_W,SDL_KMOD_NONE);keyboard.keyUp(SDL_SCANCODE_W);require(keyboard.sample().none(),"accepted key-up releases thrust");
    keyboard.keyDown(SDL_SCANCODE_D,SDL_KMOD_NONE);require(keyboard.sample()[size_t(Binding::Right)],"exact None continuous control starts without a modifier");
    keyboard.keyDown(SDL_SCANCODE_RCTRL,SDL_KMOD_RCTRL);require(!keyboard.sample()[size_t(Binding::Right)],"pressing a modifier changes exact held control while its key stays down");
    keyboard.keyUp(SDL_SCANCODE_RCTRL);require(keyboard.sample()[size_t(Binding::Right)],"releasing a modifier restores exact held control without another key-down");keyboard.keyUp(SDL_SCANCODE_D);
    auto art=std::filesystem::path(argv[1]);
    size_t comparisons=compareInstalledMap(art/"defaultinputmap.mdl")+compareInstalledMap(art/"inputmap1.mdl");
    auto file=std::filesystem::temp_directory_path()/("allegiance-keyboard-check-"+std::to_string(getpid()));
    settings.mouseAliases.push_back({3,Binding::Boost});settings.mouseAliases.push_back({3,Binding::Chaff});mouse.configure(settings);
    auto sides=mouse.pressed(SDL_BUTTON_X1);require(sides.count()==2 && sides[size_t(Binding::Boost)] && sides[size_t(Binding::Chaff)],"mouse aliases preserve all held logical controls");
    settings.save(file);auto restored=Settings::load(file,settings);require(restored.keys==settings.keys && restored.modifiers==settings.modifiers && restored.keyCommands==settings.keyCommands && restored.keyAliases==settings.keyAliases && restored.mouseButtons==settings.mouseButtons && restored.mouseAliases==settings.mouseAliases && restored.controller.mapButtons==settings.controller.mapButtons,"original keyboard commands, mouse and controller aliases survive settings reload without duplicate defaults");
    {std::ofstream out(file);out<<"binding 0 4\n";}restored=Settings::load(file);require(restored.keys[0]==SDL_SCANCODE_A && restored.modifiers[0]==KeyAny,"older two-column native bindings retain Any-modifier behavior");
    auto previous=settings;{std::ofstream out(file);out<<"keyCommandMap = [(70, 0, 80), (71, 9, 50)];";}
    bool failed=false;try{settings.importInputMap(file);}catch(const std::exception&){failed=true;}
    require(failed && settings.keys==previous.keys && settings.keyAliases==previous.keyAliases,"malformed keyboard imports must not partially change controls");
    {std::ofstream out(file);out<<"keyCommandMap = [(70,8,80),(70,0,81)];";}
    failed=false;try{settings.importInputMap(file);}catch(const std::exception&){failed=true;}
    require(failed && settings.keys==previous.keys && settings.keyCommands==previous.keyCommands,"Any and None cannot duplicate the same original press slot; failed import is atomic");
    {std::ofstream out(file);out<<"keyCommandMap = [(16,8,50),(17,2,57),(18,4,94),(108,8,209)];";}settings.importInputMap(file);keyboard.configure(settings);
    require(keyboard.command(SDL_SCANCODE_KP_ENTER,SDL_KMOD_NONE)==TK_TrackCommandView,"authored keypad Enter maps independently from the ordinary Enter key");
    for(auto scan:{SDL_SCANCODE_LSHIFT,SDL_SCANCODE_RSHIFT}){keyboard.keyDown(scan,SDL_KMOD_SHIFT);require(keyboard.sample()[size_t(Binding::Boost)] && !keyboard.pressed(scan,SDL_KMOD_SHIFT),"generic Windows Shift controls both physical sides and keeps Any held/press distinction");keyboard.keyUp(scan);}
    for(auto scan:{SDL_SCANCODE_LCTRL,SDL_SCANCODE_RCTRL}){require(keyboard.keyDown(scan,SDL_KMOD_CTRL)==Binding::Coast,"explicit modifier press fires immediately on either physical Control key");keyboard.keyUp(scan);}
    for(auto scan:{SDL_SCANCODE_LALT,SDL_SCANCODE_RALT}){require(keyboard.keyDown(scan,SDL_KMOD_ALT)==Binding::Cloak,"generic Windows Alt maps to both physical sides");keyboard.keyUp(scan);}
    settings=previous;
    {std::ofstream out(file);out<<"keyCommandMap = [(70, 0, 80)]; mouseButtonCommandMap = [(0,55), (10,56)];";}
    failed=false;try{settings.importInputMap(file);}catch(const std::exception&){failed=true;}
    require(failed && settings.keys==previous.keys && settings.mouseButtons==previous.mouseButtons && settings.mouseAliases==previous.mouseAliases,"invalid mouse import must roll back keyboard and mouse changes atomically");
    {std::ofstream out(file);out<<"keyCommandMap = []; mouseButtonCommandMap = [(3,55), (3,56), (9,12)];";}
    require(settings.importInputMap(file)==3,"saved original map with mouse aliases imports all logical controls");mouse.configure(settings);
    require(mouse.pressed(SDL_BUTTON_LEFT).none() && mouse.pressed(SDL_BUTTON_X1).count()==2 && mouse.wheel(1)[size_t(Binding::CameraZoomIn)],"import replaces default mouse mappings and retains aliases");
    {std::ofstream out(file);out<<"keyCommandMap = []; buttonCommandMap = [(1,2,55),(1,2,50),(0,-2,56)]; numericValues = [(1,0),(0,1),(1,2),(0,3),(1,4),(-1,0)];";}
    require(settings.importInputMap(file)==8 && settings.controller.mapAxes[2].index==3 && settings.controller.mapAxes[3].index==2 && settings.controller.mapButtons.size()==3 && settings.controller.mapButtons[2].button==-2,"old numericValues swap preserves multi-device axes, physical aliases and virtual hats");
    settings.controller.mapDevices[1]="fixture-guid#2";settings.save(file);restored=Settings::load(file);
    require(restored.controller.mapped && restored.controller.mapAxes==settings.controller.mapAxes && restored.controller.mapButtons==settings.controller.mapButtons && restored.controller.mapDevices==settings.controller.mapDevices,"controller routes and explicit device identities survive persistence");
    previous=settings;{std::ofstream out(file);out<<"keyCommandMap = [(70,0,80)]; mouseButtonCommandMap = []; buttonCommandMap = [(1,2,55)]; numericValuesNew = [(0,0),(16,1)];";}
    failed=false;try{settings.importInputMap(file);}catch(const std::exception&){failed=true;}
    require(failed && settings.keys==previous.keys && settings.mouseButtons==previous.mouseButtons && settings.controller.mapAxes==previous.controller.mapAxes && settings.controller.mapButtons==previous.controller.mapButtons,"invalid controller import must roll back every device map atomically");
    {std::ofstream out(file);out<<"keyCommandMap = []; buttonCommandMap = [(0,-5,55)];";}
    failed=false;try{settings.importInputMap(file);}catch(const std::exception&){failed=true;}require(failed && settings.controller.mapButtons==previous.controller.mapButtons,"invalid virtual hat button is rejected atomically");
    {std::ofstream out(file);out<<"keyCommandMap = [];";}settings.importInputMap(file);
    require(settings.controller.mapAxes==previous.controller.mapAxes && settings.controller.mapButtons==previous.controller.mapButtons && settings.mouseAliases==previous.mouseAliases,"absent device sections retain existing routes");
    std::filesystem::remove(file);printf("72 original modifier policies, %zu installed-map press/held comparisons, %zu keyboard/mouse/controller mappings, repeat/release/focus, generic modifiers, command identity, persistence and atomic import passed\n",comparisons,imported);return 0;
}catch(const std::exception& e){fprintf(stderr,"Keyboard: %s\n",e.what());return 1;}}
