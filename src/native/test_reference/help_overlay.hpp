#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include <functional>

namespace NativeTest {
inline void checkHelpOverlay(Native::Assets& assets,glm::ivec2 pixelSize={1000,700},
    const std::function<void(const Native::MenuView&,const Native::MenuView&,std::string_view)>& comparePixels={}){
    using namespace Native;
    // Frozen HelpPosition Evaluate/Toggle/OnEvent equations; the host evaluates
    // at an event boundary before changing its target. No production motion
    // helper is used to compute the reference positions.
    struct Reference {
        bool on{};float value{},initial{};double started{};
        void evaluate(double time){value=on?std::min(1.f,initial+float(time*2-started)):std::max(0.f,initial-float(time*2-started));}
        void toggle(bool force,double time){on=force || !on;started=time*2;initial=value;}
        void close(double time){on=false;started=time*2;initial=value;}
    } reference;
    Allegiance::HelpMotion motion;size_t samples=0;
    for(int step=0;step<120;++step){
        double time=step*.037;reference.evaluate(time);
        if(step%5==0){bool force=step%3==0;motion.toggle(force,time);reference.toggle(force,time);}
        if(step%11==0){motion.close(time);reference.close(time);}
        for(double offset:{0.,.007,.021}){
            reference.evaluate(time+offset);
            require(std::abs(motion.x(time+offset)-(-1300+1330*reference.value))<.001f && motion.on()==reference.on,"Help animation changes speed, endpoints or reversal continuity");++samples;
        }
    }
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);OriginalUi authored(assets);
    auto root=authored.layout("helppane","helpPane");auto& off=ui.helpView();
    require(!ui.helpOn() && !ui.helpVisible() && off.canvas==glm::ivec2(1300,700) && off.artwork.size()==root.size() && off.items.size()==3,"Help loses its authored root/buttons or starts visible");
    auto* records=off.artwork.data();auto* items=off.items.data();
    const double start=uiTime();ui.showHelp(true,start);ui.clearSounds();
    for(int requested:{1,2})for(auto size:{glm::ivec2(800,600),glm::ivec2(1600,900),glm::ivec2(3000,1800)}){
        const auto& pane=ui.helpView(requested,start+.6);auto layout=originalMenuLayout(size,requested,pane.canvas);
        auto top=layout.pixel(menuSlideOffset(pane,size,start+.6));
        require(top==glm::vec2(30*layout.scale,size.y-(700+30)*layout.scale),"Help is centered instead of anchored at original image-space (30,30)");
        auto pixel=top+glm::vec2(3*layout.scale,300*layout.scale);
        require(ui.helpPointerInside(size,pixel) && !ui.helpPointerInside(size,{0,0}),"Help does not restrict mouse ownership to its translated visible pane");
        require(records==pane.artwork.data() && items==pane.items.data(),"Help evaluation rebuilds authored controls");
    }
    for(float phase:{.25f,.5f,1.f}){
        const auto& actual=ui.helpView(1,start+phase*.5);
        MenuView expected;expected.original=expected.overlay=true;expected.canvas=actual.canvas;expected.artwork=root;
        auto layout=originalMenuLayout(pixelSize,1,expected.canvas);
        auto at=glm::vec2(std::round(-1300+1330*phase),pixelSize.y-700-30)-layout.origin;
        for(auto& c:expected.artwork)c.translate(at);
        if(comparePixels)comparePixels(actual,expected,"help-phase-"+std::to_string(int(phase*100)));
    }
    // Actual authored close/community buttons, retained capture and no flight
    // resume/menu action on closure. No URLs are opened by this check.
    ui.helpView(1,start+.6);auto close=std::find_if(off.items.begin(),off.items.end(),[](const auto& i){return i.action.op==MenuOp::SecondaryClose;});
    require(close!=off.items.end(),"Help close button is unbound");size_t index=size_t(close-off.items.begin());
    ui.helpPress(index);require(ui.helpPointerCaptured(),"Help button does not retain pointer capture");
    require(ui.helpRelease({}).effect==FrontEffect::None && ui.helpOn() && !ui.helpPointerCaptured(),"Help activates a captured button after release outside");
    for(size_t i=0;i<off.items.size();++i)if(off.items[i].action.op==MenuOp::Web){ui.helpPress(i);auto result=ui.helpRelease(i);require(result.effect==FrontEffect::Web && result.value==off.items[i].action.value && ui.helpOn(),"Help community buttons change screen or dispatch the wrong destination");}
    ui.helpPress(index);require(ui.helpRelease(index).effect==FrontEffect::None && !ui.helpOn() && ui.screen()==Screen::Home,"Help close button replaces/resumes the underlying screen");
    ui.showHelp(true);ui.open(Screen::Training);require(ui.helpOn() && ui.screen()==Screen::Training,"ordinary screen navigation dismisses independent Help");
    ui.helpPress(index);ui.cancelHelpPointer();require(!ui.helpPointerCaptured() && ui.helpRelease(index).effect==FrontEffect::None && ui.helpOn(),"focus cancellation replays a stale Help click");
    ui.clearSounds();ui.toggleHelp();require(ui.sounds().size()==1 && ui.sounds()[0]==positiveButtonClickSound,"translated Help loses its original OnHelp sound");
    ui.showHelp(true);ui.open(Screen::Home);
    size_t popupStates=0;
    for(auto screen:{Screen::Pause,Screen::Settings,Screen::Controls,Screen::Credits,Screen::Online}){
        ui.open(Screen::Home);auto background=ui.view().artwork.size();ui.open(screen);
        const auto& scene=ui.sceneView();const auto& popup=ui.menuPopupView();
        if(!(ui.helpOn() && ui.keyboardPopup() && !ui.mainMenuInput() && scene.artwork.size()==background && !popup.artwork.empty() && scene.artwork.size()+popup.artwork.size()==ui.view().artwork.size()))throw std::runtime_error("Help/popup layer state "+std::to_string(int(screen))+": base "+std::to_string(background)+", scene "+std::to_string(scene.artwork.size())+", popup "+std::to_string(popup.artwork.size())+", combined "+std::to_string(ui.view().artwork.size())+", on "+std::to_string(ui.helpOn())+", captures "+std::to_string(ui.keyboardPopup())+", menu "+std::to_string(ui.mainMenuInput()));
        if(screen==Screen::Credits)require(!scene.scroll && popup.scroll,"Credits text is left in the background below independent Help");
        auto button=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& i){return i.captureOnPress && !i.blocked;});
        if(button!=ui.view().items.end()){
            ui.pointerPress(size_t(button-ui.view().items.begin()));
            require(ui.menuPopupView().capturedButton() && ui.menuPopupView().capturedButton()->control==button->control,"split popup loses its actual retained button capture");
            ui.pointerRelease({});
        }
        ++popupStates;
    }
    // An options/menu/mapping stack opened over Credits keeps both original
    // PopupContainer layers above Help, including the scrolling text.
    ui.open(Screen::Home);auto background=ui.view().artwork.size();ui.open(Screen::Credits);ui.open(Screen::Pause);ui.open(Screen::Controls);
    require(ui.sceneView().artwork.size()==background && ui.menuPopupView().artwork.size()+background==ui.view().artwork.size(),"nested popup backdrop loses its layer boundary above Help");
    printf("Original Help overlay: %zu motion/reversal samples, six original/integer-scale anchors/hits, %d authored pixel comparisons, retained buttons/capture, close/web/focus sounds and nonmodal screen navigation passed without a game\n",samples,comparePixels?3:0);
    printf("Original Help/popup ordering: %zu direct popup states, preserved foreground capture, separate Credits scrolling and retained nested popup boundary passed\n",popupStates);
}
}
