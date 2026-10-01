#pragma once
#include "../frontend.hpp"
#include "../../ClientCore/CommandDescriptions.h"
#include <cstdio>

namespace NativeTest {
// Drive the real client event loop; calling Frontend::key directly would miss
// the SDL modifier loss and raw-joystick/gamepad duplicate dispatch bugs.
class OriginalInputRoutingSmoke {
    std::optional<Native::Settings> saved_;
    SDL_JoystickID device_{};
    SDL_Joystick* joystick_{};
    std::string guid_;
    bool passed_{};
    static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
    static void key(SDL_Keycode keycode,SDL_Scancode scan,SDL_Keymod mod=SDL_KMOD_NONE){
        SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=keycode;event.key.scancode=scan;event.key.mod=mod;event.key.down=true;
        require(SDL_PushEvent(&event),"Input routing fixture could not push key down");
        event.type=SDL_EVENT_KEY_UP;event.key.down=false;require(SDL_PushEvent(&event),"Input routing fixture could not push key up");
    }
    static constexpr SDL_Keymod modifiers_[]={SDL_KMOD_NONE,SDL_KMOD_LSHIFT,SDL_KMOD_RCTRL,SDL_Keymod(SDL_KMOD_RSHIFT|SDL_KMOD_LCTRL),SDL_KMOD_LALT,SDL_Keymod(SDL_KMOD_RALT|SDL_KMOD_RSHIFT),SDL_Keymod(SDL_KMOD_LALT|SDL_KMOD_LCTRL),SDL_Keymod(SDL_KMOD_LCTRL|SDL_KMOD_RSHIFT|SDL_KMOD_RALT)};
    static int selection(const Native::Frontend& ui){
        for(const auto& item:ui.view().items)if(item.action.op==Native::MenuOp::MappingSelect && item.selected)return item.action.value;
        return -1;
    }
    static void select(Native::Frontend& ui,TrekKey command){
        auto begin=std::begin(Allegiance::CommandDescriptions),end=std::end(Allegiance::CommandDescriptions);
        auto row=std::find_if(begin,end,[&](const auto& description){return description.m_tk==command;});require(row!=end,"Input routing fixture command is missing");
        int target=int(row-begin),current=selection(ui);
        while(current!=target){ui.key(current<target?SDLK_DOWN:SDLK_UP,current<target?SDL_SCANCODE_DOWN:SDL_SCANCODE_UP);current+=current<target?1:-1;}
        require(selection(ui)==target,"Input routing fixture could not select original mapping row");
    }
    static void checkBindings(const Native::Settings& settings){
        Native::KeyboardBindings keyboard(settings);
        for(unsigned i=0;i<std::size(modifiers_);++i)require(keyboard.command(SDL_SCANCODE_F13,modifiers_[i])==(i==Native::KeyControl?TK_FireWeapon:TK_StartChat),"Actual SDL mapping loses a modifier combination or changes an unrelated chord");
        require(keyboard.command(SDL_SCANCODE_F14,SDL_KMOD_ALT)==TK_NoKeyMapping,"Canceled SDL mapping was committed");
    }
    static void mouse(Native::Frontend& ui,SDL_Window* window,glm::ivec2 drawable,std::string_view name,bool down){
        auto item=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& row){return row.control==name && !row.blocked;});require(item!=ui.view().items.end(),"SDL secondary-button fixture control is missing");
        auto position=Native::originalMenuLayout(drawable,ui.view().classicScale,ui.view().canvas).pixel(glm::vec2(item->bounds)+glm::vec2(2));
        int width,height;SDL_GetWindowSize(window,&width,&height);SDL_Event event{};event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID=SDL_GetWindowID(window);event.button.button=SDL_BUTTON_RIGHT;event.button.down=down;event.button.clicks=1;
        event.button.x=position.x*width/drawable.x;event.button.y=position.y*height/drawable.y;
        require(SDL_PushEvent(&event),"SDL secondary-button fixture could not push mouse edge");
    }
    void button(SDL_GamepadButton button,bool down){require(SDL_SetJoystickVirtualButton(joystick_,int(button),down),"Input routing fixture could not move virtual gamepad button");SDL_UpdateJoysticks();}
    void detach(){if(device_){SDL_DetachVirtualJoystick(device_);device_=0;}if(joystick_){SDL_CloseJoystick(joystick_);joystick_=nullptr;}}
public:
    ~OriginalInputRoutingSmoke(){detach();}
    bool passed()const{return passed_;}
    void step(unsigned frame,Native::Frontend& ui,Native::Settings& settings,Native::Controller& controller,const std::filesystem::path& config,SDL_Window* window,glm::ivec2 drawable){
        if(frame==40){saved_=settings;ui.open(Native::Screen::Controls);select(ui,TK_StartChat);}
        for(unsigned i=0;i<std::size(modifiers_);++i){
            auto base=50+i*8;
            if(frame==base)key(SDLK_RETURN,SDL_SCANCODE_RETURN);
            if(frame==base+1){require(ui.capturingKey(),"Actual SDL Enter does not start mapping capture");key(SDLK_LCTRL,SDL_SCANCODE_LCTRL,SDL_KMOD_LCTRL);}
            if(frame==base+2){require(ui.capturingKey(),"Original mapping editor must ignore modifier-only press/release");key(SDLK_F13,SDL_SCANCODE_F13,modifiers_[i]);}
            if(frame==base+3)require(!ui.capturingKey(),"Actual SDL chord failed to finish capture (or collided after losing modifiers)");
        }
        if(frame==120)select(ui,TK_FireWeapon);
        if(frame==122 || frame==128)key(SDLK_RETURN,SDL_SCANCODE_RETURN);
        if(frame==124 || frame==130)key(SDLK_F13,SDL_SCANCODE_F13,SDL_KMOD_RCTRL);
        if(frame==125 || frame==131){
            require(ui.capturingKey(),"Actual SDL conflicting chord bypasses original confirmation");
            require(std::any_of(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& control){return control.text.find("Key in use")!=std::string::npos;}),"Actual SDL conflict loses authored explanation");
        }
        if(frame==126)key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
        if(frame==127)require(!ui.capturingKey() && ui.screen()==Native::Screen::Controls,"Conflict cancellation closes the whole mapping dialog");
        if(frame==132)key(SDLK_RETURN,SDL_SCANCODE_RETURN);
        if(frame==133)require(!ui.capturingKey(),"Actual SDL conflict confirmation did not finish capture");
        if(frame==134 || frame==164)key(SDLK_SPACE,SDL_SCANCODE_SPACE);
        if(frame==135){checkBindings(settings);checkBindings(Native::Settings::load(config));}
        if(frame==136){ui.open(Native::Screen::Controls);select(ui,TK_FireWeapon);}
        if(frame==138)key(SDLK_RETURN,SDL_SCANCODE_RETURN);
        if(frame==140)key(SDLK_F14,SDL_SCANCODE_F14,SDL_KMOD_LALT);
        if(frame==142)key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
        if(frame==143){checkBindings(settings);checkBindings(Native::Settings::load(config));}
        if(frame==150){
            SDL_VirtualJoystickDesc description;SDL_INIT_INTERFACE(&description);description.type=SDL_JOYSTICK_TYPE_GAMEPAD;
            description.naxes=SDL_GAMEPAD_AXIS_COUNT;description.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;description.name="Allegiance classic UI routing fixture";description.vendor_id=0x1209;description.product_id=0xA11E;
            description.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;description.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
            device_=SDL_AttachVirtualJoystick(&description);require(device_,"Could not attach process-local SDL test gamepad");joystick_=SDL_OpenJoystick(device_);require(joystick_,"Could not open SDL test gamepad");
            char guid[33];SDL_GUIDToString(SDL_GetJoystickGUID(joystick_),guid,sizeof(guid));guid_=guid;
            settings.controller.enabled=true;settings.controller.device=guid_;settings.controller.mapped=false;settings.controller.mapButtons.clear();controller.configure(settings.controller);
            require(controller.gamepad(),"SDL routing fixture is not recognized as a gamepad");
            SDL_Event pad{};pad.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;pad.gbutton.which=device_;pad.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH;
            require(controller.menuKey(pad)==SDLK_RETURN,"SDL fixture does not exercise optional gamepad menu synthesis");
        }
        if(frame==152){ui.open(Native::Screen::Controls);select(ui,TK_StartChat);}
        if(frame==154)button(SDL_GAMEPAD_BUTTON_DPAD_DOWN,true);
        if(frame==155)require(selection(ui)==int(std::find_if(std::begin(Allegiance::CommandDescriptions),std::end(Allegiance::CommandDescriptions),[](const auto& c){return c.m_tk==TK_StartChat;})-std::begin(Allegiance::CommandDescriptions)),"Classic UI receives unauthored gamepad list navigation");
        if(frame==156)button(SDL_GAMEPAD_BUTTON_DPAD_DOWN,false);
        if(frame==158){select(ui,TK_FireMissile);key(SDLK_RETURN,SDL_SCANCODE_RETURN);}
        if(frame==159)require(ui.capturingKey(),"SDL gamepad mapping capture did not begin");
        if(frame==160)button(SDL_GAMEPAD_BUTTON_SOUTH,true);
        if(frame==161)require(!ui.capturingKey(),"Gamepad mapping was lost or synthesized Return restarted capture");
        if(frame==162)button(SDL_GAMEPAD_BUTTON_SOUTH,false);
        if(frame==165){
            auto check=[&](const auto& preferences){return std::any_of(preferences.controller.mapButtons.begin(),preferences.controller.mapButtons.end(),[&](const auto& route){return route.device>=0 && route.device<16 && preferences.controller.mapDevices[route.device]==guid_ && route.button==SDL_GAMEPAD_BUTTON_SOUTH && route.binding==Native::Binding::Missile;});};
            require(check(settings) && check(Native::Settings::load(config)),"Actual SDL gamepad mapping is not committed/persisted against its device GUID");checkBindings(settings);
        }
        if(frame==180){
            settings=*saved_;controller.configure(settings.controller);settings.save(config);detach();
            fprintf(stderr,"Original SDL input routing: eight left/right modifier combinations, exact conflict cancel/replace, whole-dialog cancellation, persistence, classic gamepad non-navigation and raw-button capture passed without a game\n");
        }
        if(frame==181)ui.open(Native::Screen::Home);
        if(frame==182 || frame==184)mouse(ui,window,drawable,"optionsButtonPane",frame==182);
        if(frame==183)require(ui.contextButtonCaptured() && ui.view().capturedButton() && ui.screen()==Native::Screen::Home,"SDL secondary ordinary-button press loses capture or activates");
        if(frame==185)require(!ui.buttonCaptured() && ui.screen()==Native::Screen::Home,"SDL captured right release invokes ordinary-button left callback");
        if(frame==186){Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;ui.networkSession(options);ui.open(Native::Screen::Lobby);}
        if(frame==187 || frame==189)mouse(ui,window,drawable,"friendlyFireCheckboxPane",frame==187);
        if(frame==188)require(ui.contextButtonCaptured() && !ui.session().rules.friendlyFire,"SDL right checkbox changes setup before release");
        if(frame==190){
            auto item=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& row){return row.control=="friendlyFireCheckboxPane";});
            require(!ui.buttonCaptured() && item!=ui.view().items.end() && item->selected && !ui.session().rules.friendlyFire,"SDL captured right checkbox release loses silent widget toggle");
            auto cancel=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& row){return row.control=="cancelButtonPane";});require(cancel!=ui.view().items.end(),"SDL setup cancellation fixture missing");ui.activate(size_t(cancel-ui.view().items.begin()));
        }
        if(frame==195){require(!ui.session().rules.friendlyFire,"SDL canceled setup commits a silent checkbox");ui.open(Native::Screen::Home);passed_=true;
            fprintf(stderr,"Original SDL secondary input: ordinary right capture/release, no left callback, captured checkbox toggle, staged state and cancellation passed without a game\n");}
    }
};
}
