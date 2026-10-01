#pragma once
#include "authored_hosts.hpp"
#include "../../ClientCore/TrainingMenuPresentation.h"
#include <fstream>

namespace NativeTest {
inline void checkTrainingProgression(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.open(Screen::Training);
    auto index=[&](MenuOp op,int value=0){
        const auto& items=ui.view().items;auto it=std::find_if(items.begin(),items.end(),[&](const auto& item){return item.action.op==op && item.action.value==value;});
        require(it!=items.end(),"training progression action missing");return size_t(it-items.begin());
    };
    auto selected=[&](int lesson){
        const auto& items=ui.view().items;
        require(std::count_if(items.begin(),items.end(),[](const auto& item){return item.action.op==MenuOp::TrainingSelect && item.selected;})==1 && items[index(MenuOp::TrainingSelect,lesson)].selected,"training progression does not check exactly the expected lesson");
        const auto& launch=items[index(MenuOp::TrainingStart,lesson)];require(launch.enabled && launch.selected,"training launch no longer matches the selected lesson");
    };
    auto select=[&](int lesson){require(ui.activate(index(MenuOp::TrainingSelect,lesson)).effect==FrontEffect::None,"training selection starts a lesson");selected(lesson);};
    auto returnFromSlides=[&]{ui.trainingSlides(true);ui.trainingSlides(false);ui.open(Screen::Training);};
    // Frozen TrainingScreen::m_iMissionNext progression, including removed TM7
    // and the final lesson's self-loop. Choosing/replaying other lessons must
    // not advance this independent state.
    constexpr int lessons[]{1,2,3,4,5,6,8};
    for(int next:lessons){
        selected(next);
        for(int choice:lessons){
            select(choice);ui.open(Screen::Training);selected(choice);
            ui.activate(index(MenuOp::Navigate,int(Screen::Home)));require(ui.screen()==Screen::Home,"training Back does not return to the main menu");
            ui.activate(index(MenuOp::Navigate,int(Screen::Training)));require(ui.hasMessageBox(),"main-menu training entry skips its welcome");
            ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);require(ui.screen()==Screen::Training && !ui.hasMessageBox(),"training welcome does not create the lesson screen");selected(next);
        }
        int replay=next==1?8:1;select(replay);
        for(auto popup:{Screen::Settings,Screen::Pause,Screen::Controls,Screen::Credits}){
            ui.open(popup);require(ui.screen()==popup,"training popup did not open");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
            require(ui.screen()==Screen::Training,"training popup does not return to its parent");selected(replay);
        }
        ui.open(Screen::Pause);ui.open(Screen::Controls);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
        require(ui.screen()==Screen::Pause,"nested mappings lose the training menu parent");ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);selected(replay);
        ui.open(Screen::Help);ui.toggleHelp();require(ui.screen()==Screen::Training,"training Help changes the lesson screen");selected(replay);
        ui.confirm("Cancel training selection?",{FrontEffect::Leave});require(ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE).effect==FrontEffect::None && !ui.hasMessageBox(),"training confirmation cancellation leaves the screen");selected(replay);
        require(ui.activate(index(MenuOp::TrainingStart,replay)).effect==FrontEffect::Start && ui.session().trainingMission==replay,"out-of-order replay does not start the selected lesson");
        returnFromSlides();selected(next);
        select(next);require(ui.activate(index(MenuOp::TrainingStart,next)).effect==FrontEffect::Start && ui.session().trainingMission==next,"progression launches the wrong lesson");
        returnFromSlides();selected(next>=6?8:next+1);
    }
    require(ui.activate(index(MenuOp::TrainingStart,8)).effect==FrontEffect::Start && ui.session().trainingMission==8,"final lesson replay launches an unavailable successor");returnFromSlides();selected(8);
    Frontend restarted(settings);restarted.originalInterface(assets);restarted.open(Screen::Training);
    require(std::any_of(restarted.view().items.begin(),restarted.view().items.end(),[](const auto& item){return item.action.op==MenuOp::TrainingSelect && item.action.value==1 && item.selected;}),"training progression persists beyond the original execution lifetime");
    puts("Original training progression: 49 selection/Back/welcome pairs, all seven lessons, out-of-order replay, final-lesson self-loop, popup/nested mapping/Help/cancel preservation and execution reset passed");
}
inline void checkTrainingMenu(Native::Assets& assets){
    using namespace Native;
    for(bool checked:{false,true})for(bool accept:{false,true})for(bool keyboard:{false,true}){
        Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.startupTrainingSuggestion();
        auto item=[&](MenuOp op,int delta=1){const auto& rows=ui.view().items;auto i=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.action.op==op && row.action.delta==delta;});require(i!=rows.end(),"training dialog action missing");return size_t(i-rows.begin());};
        require(ui.screen()==Screen::Home && ui.hasMessageBox() && ui.keyboardPopup() && !ui.wantsText() && authoredControl(ui.view().artwork,"messageText").text==Allegiance::TrainingSuggestionMessage,"original startup suggestion or modal capture missing");
        auto background=authoredControl(ui.view().artwork,"messageBackground");auto checkbox=authoredControl(ui.view().artwork,"trainingDontAskAgain");auto image=assets.image("btndontaskmeagainbmp",true);
        require(checkbox.image==image && checkbox.faces==255 && glm::vec2(checkbox.bounds)==glm::vec2(background.bounds)+glm::vec2(183,140) && checkbox.bounds.w==image->height/8,"original don't-ask artwork/faces/offset differ");
        ui.pointerPress({});ui.pointerRelease({});ui.text("ignored");ui.key(SDLK_SPACE,SDL_SCANCODE_SPACE);
        require(ui.hasMessageBox() && ui.chatText().empty(),"training suggestion leaks outside clicks/text/other keys");
        if(checked){auto i=item(MenuOp::TrainingDontAskAgain);ui.pointerPress(i);ui.pointerRelease(i);require(ui.view().items[item(MenuOp::TrainingDontAskAgain)].selected,"don't-ask checkbox does not toggle on release");}
        ui.startupTrainingSuggestion();require(ui.view().items[item(MenuOp::TrainingDontAskAgain)].selected==checked,"repeated Home visit resets the current checkbox");
        auto result=keyboard?ui.key(accept?SDLK_RETURN:SDLK_ESCAPE,accept?SDL_SCANCODE_RETURN:SDL_SCANCODE_ESCAPE):ui.activate(item(MenuOp::MessageDismiss,accept?1:0));
        require(settings.showStartupTrainingSuggestion==!checked && result.effect==(checked?FrontEffect::ApplyOptions:FrontEffect::None),"suggestion dismiss changes or fails to persist original preference");
        if(accept){
            require(ui.hasMessageBox() && ui.screen()==Screen::Home && authoredControl(ui.view().artwork,"messageText").text==Allegiance::TrainingWelcomeMessage,"suggestion OK skips original training welcome");
            require(std::none_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& row){return row.control=="messageCancel" || row.action.op==MenuOp::TrainingDontAskAgain;}),"welcome inherits Cancel or don't-ask controls");
            ui.key(keyboard?SDLK_ESCAPE:SDLK_RETURN,keyboard?SDL_SCANCODE_ESCAPE:SDL_SCANCODE_RETURN);
            require(!ui.hasMessageBox() && ui.screen()==Screen::Training,"welcome Return/Escape does not enter lesson selection");
        }else require(!ui.hasMessageBox() && ui.screen()==Screen::Home,"suggestion Cancel unexpectedly enters training");
        ui.open(Screen::Home);ui.startupTrainingSuggestion();require(!ui.hasMessageBox(),"suggestion repeats during the same execution");
        Frontend next(settings);next.originalInterface(assets);next.startupTrainingSuggestion();require(next.hasMessageBox()==!checked,"next execution ignores original persistent suggestion choice");
    }
    // An unrelated replacement must not inherit either the welcome transition
    // or a checked suggestion that was never acknowledged.
    for(int replacement=0;replacement<7;++replacement){
        Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.startupTrainingSuggestion();
        auto checkbox=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& row){return row.action.op==MenuOp::TrainingDontAskAgain;});ui.activate(size_t(checkbox-ui.view().items.begin()));
        switch(replacement){
        case 0:ui.error("Unrelated error");break;
        case 1:ui.confirm("Unrelated confirmation",{FrontEffect::Leave});break;
        case 2:ui.connecting();break;
        case 3:ui.purchasing(false,1);break;
        case 4:ui.transferring(Allegiance::CrewTransfer::Teleport,"Base",1);break;
        case 5:ui.trainingStarting();break;
        case 6:ui.configuring();break;
        }
        auto result=ui.key(SDLK_RETURN,SDL_SCANCODE_RETURN);ui.connectionFinished();
        require(settings.showStartupTrainingSuggestion && ui.screen()==Screen::Home && !ui.hasMessageBox() && result.effect==(replacement==1?FrontEffect::Leave:FrontEffect::None),"replacement dialog inherits training transition/preference or cannot complete");
    }
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.open(Screen::Training);
    auto lesson=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& row){return row.action.op==MenuOp::TrainingSelect && row.action.value==2;});ui.activate(size_t(lesson-ui.view().items.begin()));
    auto launch=std::find_if(ui.view().items.begin(),ui.view().items.end(),[](const auto& row){return row.action.op==MenuOp::TrainingStart;});require(ui.activate(size_t(launch-ui.view().items.begin())).effect==FrontEffect::Start && ui.session().trainingMission==2 && !settings.showStartupTrainingSuggestion,"launching a lesson fails to clear original suggestion preference");
    auto path=std::filesystem::temp_directory_path()/("allegiance-training-menu-check-"+std::to_string(getpid()));settings.save(path);auto persisted=Settings::load(path);require(!persisted.showStartupTrainingSuggestion,"training preference does not survive native settings reload");
    for(bool trained:{false,true}){
        {std::ofstream out(path);out<<"{\"HasTrained\":"<<std::boolalpha<<trained<<'}';}
        Settings imported;require(imported.importOriginal(path)==1 && imported.showStartupTrainingSuggestion==!trained,"legacy HasTrained fallback differs from WinTrek");
        for(bool explicitPreference:{false,true}){
            {std::ofstream out(path);out<<"{\"HasTrained\":"<<std::boolalpha<<trained<<",\"Ui.ShowStartupTrainingSuggestion\":"<<explicitPreference<<'}';}
            require(imported.importOriginal(path)==2 && imported.showStartupTrainingSuggestion==explicitPreference,"explicit legacy suggestion preference loses precedence over HasTrained");
        }
    }
    std::filesystem::remove(path);
    checkTrainingProgression(assets);
    puts("Original training entry: eight checkbox/accept/keyboard combinations, exact messages/artwork, modal capture, once-per-execution visits, seven replacement dialogs, lesson-launch persistence and legacy trained/explicit preference precedence passed");
}
}
