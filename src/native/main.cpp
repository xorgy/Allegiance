#include "renderer.hpp"
#include "presentation.hpp"
#include "game.hpp"
#include "network_session.hpp"
#include "thread_affinity.hpp"
#include "smoke.hpp"
#include "audio.hpp"
#include "movie.hpp"
#include "training_slides.hpp"
#include "hud.hpp"
#include "original_cursor.hpp"
#include "../ClientCore/GlobalKeyFilter.h"
#include "chat_console.hpp"
#include "player_pane.hpp"
#include "game_state_pane.hpp"
#include "ui_pointer_capture.hpp"
#include "ui_console_panels.hpp"
#include "crew_pane.hpp"
#include "console_pane.hpp"
#include "ui_clock.hpp"
#include "primitives.hpp"
#include "controller.hpp"
#include "command_input.hpp"
#include "scene_pointer.hpp"
#include "input_bindings.hpp"
#include "test_reference/input_routing.hpp"
#include "lobby_network_actions.hpp"
#include "mouse.hpp"
#include "training_prompts.hpp"
#include "../ClientCore/Overlay.h"
#include "../ClientCore/MissionChatCommands.h"
#include "../ClientCore/Throttle.h"
#include "../ClientCore/SavedWing.h"
#include "../Igc/igc.h"
#include "../ClientCore/MouseJoystick.h"
#include "../ClientCore/KeyboardSteering.h"
#include "../ClientCore/CameraZoom.h"
#include "../ClientCore/RadarPresentation.h"
#include "../ClientCore/SectorMapPresentation.h"
#include "../ClientCore/CommunityLinks.h"
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <fstream>
#include <SDL3/SDL_main.h>

namespace {
void printUsage() {
                puts("Allegiance native client\n"
                     "  --connect IP:PORT  Join a native multiplayer server\n"
                     "  --server IP:PORT   Add an endpoint to the classic games browser\n"
                     "  --team N           Team to join (1 through 6)\n"
                     "  --practice         Launch directly into combat practice\n"
                     "  --enemies N / --allies N  AI wingmen for practice or skirmish\n"
                     "  --scale-test N     Diagnose N AI ships per team in a dense battle\n"
                     "  --scale-spread / --scale-idle  Spread fleets or disable AI for comparison\n"
                     "  --scale-command    Observe the scale match docked in command view\n"
                     "  --scale-distance M Observer/camera distance (default 1500 m; command 3000 m)\n"
                     "  --scale-csv FILE   Record CPU simulation, preparation and rendering costs\n"
                     "  --scenario N       Launch native scenario 0 through 5\n"
                     "  --training-intro      Play the authored introduction before --training\n"
                     "  --training N       Original narrated mission 2, 3, 4, 5, 6, 8 or 10\n"
                     "  --config FILE      Settings file (default: platform config directory)\n"
                     "  --import-config FILE  Import supported original config.json preferences\n"
                     "  --import-inputmap FILE  Import original keyboard, mouse and controller mappings\n"
                     "  --chat-smoke-test     Exercise wrapped history, censor and cached layout\n"
                     "  --elimination-smoke-test  Exercise station loss and continuing-team controls\n"
                     "  --frontend-smoke-test  Exercise menus and session restart\n"
                     "  --lobby-smoke-test     Exercise local seats, readiness and lifecycle\n"
                     "  --team-settings-smoke-test  Exercise the authored team dialog and briefing\n"
                     "  --lobby-chat-smoke-test  Exercise original lobby history, tabs and mapped paging\n"
                     "  --games-smoke-test  Exercise live classic browsing, join/leave and local creation\n"
                     "  --teleport-smoke-test  Exercise docked station travel on a split-base map\n"
                     "  --mouse-input-smoke-test  Exercise installed original mouse mappings and zoom\n"
                     "  --training-terminal-smoke-test  Exercise arena death, debrief and lesson restart\n"
                     "  --startup-movie-smoke-test  Exercise credits/intro sequencing and skip\n"
                     "  --training-entry-smoke-test  Exercise original suggestion/welcome dialogs\n"
                     "  --training-slides-menu-smoke-test  Check slideshow menu/quit routing without a game\n"
                     "  --classic-keyboard-smoke-test  Check classic menu/list/edit key routing without a game\n"
                     "  --classic-input-routing-smoke-test  Check SDL chord and gamepad mapping without a game\n"
                     "  --docking-smoke-test   Fly out, return, dock, refit and relaunch\n"
                     "  --loadout-smoke-test  Exercise persistent hangar loadout banks\n"
                     "  --refit-smoke-test    Exercise classic remote purchase/dock/refit/relaunch\n"
                     "  --cargo-smoke-test    Exercise flight cargo menus, swapping, reload and drop\n"
                     "  --surface-smoke-test  Exercise IGC mines, shield/hull hits and explosions\n"
                     "  --aleph-smoke-test    Inspect the original animated aleph funnel\n"
                     "  --construction-smoke-test  Fund and follow original station construction\n"
                     "  --crew-smoke-test      Board, fire and change turrets in a gunship\n"
                     "  --pings-smoke-test     Exercise original connection reports with --connect\n"
                     "  --custom-map-smoke-test  Exercise original catalog, preview, validation and launch\n"
                     "  --team-smoke-test      Exercise team accounts, donations and command handover\n"
                     "  --command-smoke-test   Select a group and issue a height waypoint\n"
                     "  --camera-smoke-test    Boost, turn, change view, look around\n"
                     "  --artwork PATH     Steam Artwork directory (auto-detected by default)\n"
                     "  --windowed         Smaller resizable borderless window\n"
                     "  --ui-scale N       Experimental UI multiplier, 0.5 to 2.5 (default 1)\n"
                     "  --classic-ui-scale N  Original UI integer scale 1 through 8, or auto (default)\n"
                     "  --modern-ui        Opt into the experimental native interface\n"
                     "  --validation       Vulkan core and synchronization validation\n"
                     "  --seed N           Reproduce a local simulation seed\n"
                     "  --frames N         Run N frames without capturing the mouse\n"
                     "  --smoke-test       Exercise combat, ships, resize, and movies\n"
                     "  --screenshot FILE  Capture the last --frames frame as PPM\n"
                     "  --timing-csv FILE  Record actual presentation feedback\n"
                     "  --movie FILE       Play an Artwork movie before practice\n"
                     "  --model NAME       Inspect an Artwork model\n"
                     "F1: controls. Esc: pause/menu. F10: target friendly. Quit from the menu or close the window.");
}
}

int main(int argc,char** argv) {
    try {
        for(int i=1;i<argc;++i)if(std::string_view(argv[i])=="--help"){printUsage();return 0;}
        Native::Affinity::initialize();
        Native::Affinity::ScopedUnpin startupWorkers(true);
        auto configPath=Native::Settings::defaultPath();
        auto artwork=Native::Assets::steamArtwork();std::filesystem::path importPath,inputMapPath;bool customConfig=false;
        for(int i=1;i+1<argc;++i) {
            std::string_view arg=argv[i];
            if(arg=="--config"){configPath=argv[i+1];customConfig=true;}
            else if(arg=="--artwork")artwork=argv[i+1];
            else if(arg=="--import-config")importPath=argv[i+1];
            else if(arg=="--import-inputmap")inputMapPath=argv[i+1];
        }
        Native::Settings defaultSettings;
        defaultSettings.importInputMap(artwork/"defaultinputmap.mdl");
        auto profileDefaults=defaultSettings;
        // WinTrek prefers its saved inputmap1 over the shipped default map.
        if(std::filesystem::is_regular_file(artwork/"inputmap1.mdl")) {
            try{profileDefaults.importInputMap(artwork/"inputmap1.mdl");}catch(const std::exception& e){fprintf(stderr,"Input map: %s; using default map.\n",e.what());}
        }
        auto settings=Native::Settings::load(configPath,customConfig?defaultSettings:profileDefaults);
        size_t imported=0;
        if(!importPath.empty())imported=settings.importOriginal(importPath);
        else if(!customConfig && !std::filesystem::exists(configPath) && std::filesystem::is_regular_file(artwork.parent_path()/"config.json")) {
            try{imported=settings.importOriginal(artwork.parent_path()/"config.json");}catch(const std::exception& e){fprintf(stderr,"Configuration import: %s\n",e.what());}
        }
        if(!inputMapPath.empty())imported+=settings.importInputMap(inputMapPath);
        bool windowed=!settings.fullscreen,validation=false,smoke=false,frontSmoke=false,teamSettingsSmoke=false,dockSmoke=false,cameraSmoke=false,crewSmoke=false,constructionSmoke=false,alephSmoke=false,surfaceSmoke=false,cargoSmoke=false,loadoutSmoke=false,practice=false;
        bool commandSmoke=false,teamSmoke=false,lobbySmoke=false,startupMovieSmoke=false,teleportSmoke=false,mouseSmoke=false,trainingTerminalSmoke=false,chatSmoke=false,consoleSmoke=false,playerPaneSmoke=false,playerPaneSmokePassed=false,gameStateSmoke=false,gameStateSmokePassed=false,crewUiSmoke=false,crewUiSmokePassed=false,resultsUiSmoke=false,resultsUiSmokePassed=false,eliminationSmoke=false;bool trainingIntro=false;int scenarioIndex=-1,trainingMission=0;std::optional<uint32_t> sessionSeed;std::string menuName;
        bool deviceSmoke=false,deviceSmokePassed=false,pingsSmoke=false,pingsSmokePassed=false,customMapSmoke=false,customMapSmokePassed=false;
        bool trainingEntrySmoke=false,trainingEntrySmokePassed=false;
        bool trainingSlidesMenuSmoke=false,trainingSlidesMenuSmokePassed=false;
        bool classicKeyboardSmoke=false,classicKeyboardSmokePassed=false;
        bool inputRoutingSmoke=false;
        bool dockViewSmoke=false,dockViewSmokePassed=false;
        bool findPlayerSmoke=false,findPlayerSmokePassed=false;
        std::string connectAddress;int networkSide=0;bool networkInitialized=false,networkSmoke=false,networkLaunched=false;
        std::vector<std::string> browseAddresses{"127.0.0.1:27500"};
        bool gamesSmoke=false,gamesLobbyCaptured=false,lanSmoke=false,lanSmokePassed=false;int gamesStage=0,gamesFilterStage=0,gamesCreateStage=0,gamesWingStage=0,gamesReadyStage=0;unsigned gamesStageFrame=0;
        bool refitSmoke=false,refitBusyCaptured=false;int refitStage=0,refitPilot=-1,refitCargo=-5;unsigned refitStageFrame=0;
        std::array<unsigned,2> refitLaunchFrames{};std::array<double,2> refitLaunchStart{-1,-1};std::array<int,2> refitLaunchCaptures{};
        std::array<unsigned,2> refitTransitionFrames{};std::array<double,2> refitTransitionStart{-1,-1};std::array<bool,2> refitTransitionCaptured{};
        std::array<bool,2> refitTransitionComplete{};
        bool lobbyChatSmoke=false,lobbyChatSmokePassed=false;double lobbyChatScrolledAt{};Native::GameState lobbyChatFixture;
        int scaleShips=0;bool scaleSpread=false,scaleIdle=false,scaleCommand=false,scaleDistanceSet=false;float scaleDistance=1500;std::optional<int> enemyCount,allyCount;std::string scaleFile;
        bool modernUi=false;int classicUiScale=settings.classicUiScale;
        float uiScale=settings.uiScale;unsigned frameLimit=0;std::string screenshot,modelName,movieName,timingFile;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            auto value=[&](){if(++i>=argc)throw std::runtime_error("Missing value for "+arg);return std::string(argv[i]);};
            if(arg=="--connect")connectAddress=value();
            else if(arg=="--server")browseAddresses.push_back(value());
            else if(arg=="--games-smoke-test")gamesSmoke=true;
            else if(arg=="--find-player-ui-smoke-test")findPlayerSmoke=true;
            else if(arg=="--lan-smoke-test")lanSmoke=true;
            else if(arg=="--refit-smoke-test")refitSmoke=true;
            else if(arg=="--lobby-chat-smoke-test")lobbyChatSmoke=true;
            else if(arg=="--network-smoke-test")networkSmoke=true;
            else if(arg=="--team")networkSide=std::stoi(value())-1;
            else if(arg=="--artwork")artwork=value();
            else if(arg=="--windowed")windowed=true;
            else if(arg=="--validation")validation=true;
            else if(arg=="--smoke-test"){smoke=true;practice=true;}
            else if(arg=="--chat-smoke-test"){chatSmoke=true;practice=true;}
            else if(arg=="--console-smoke-test"){consoleSmoke=true;practice=true;}
            else if(arg=="--player-pane-smoke-test"){playerPaneSmoke=true;practice=true;}
            else if(arg=="--game-state-smoke-test"){gameStateSmoke=true;practice=true;}
            else if(arg=="--crew-ui-smoke-test"){crewUiSmoke=true;practice=true;}
            else if(arg=="--dock-view-smoke-test"){dockViewSmoke=true;practice=true;}
            else if(arg=="--results-ui-smoke-test"){resultsUiSmoke=true;practice=true;}
            else if(arg=="--elimination-smoke-test"){eliminationSmoke=true;practice=true;}
            else if(arg=="--frontend-smoke-test")frontSmoke=true;
            else if(arg=="--team-settings-smoke-test")teamSettingsSmoke=true;
            else if(arg=="--lobby-smoke-test")lobbySmoke=true;
            else if(arg=="--device-smoke-test")deviceSmoke=true;
            else if(arg=="--pings-smoke-test")pingsSmoke=true;
            else if(arg=="--custom-map-smoke-test")customMapSmoke=true;
            else if(arg=="--teleport-smoke-test"){teleportSmoke=true;practice=true;}
            else if(arg=="--mouse-input-smoke-test"){mouseSmoke=true;practice=true;}
            else if(arg=="--training-terminal-smoke-test"){trainingTerminalSmoke=true;trainingMission=6;practice=true;}
            else if(arg=="--startup-movie-smoke-test")startupMovieSmoke=true;
            else if(arg=="--training-entry-smoke-test")trainingEntrySmoke=true;
            else if(arg=="--training-slides-menu-smoke-test")trainingSlidesMenuSmoke=true;
            else if(arg=="--classic-keyboard-smoke-test")classicKeyboardSmoke=true;
            else if(arg=="--classic-input-routing-smoke-test")inputRoutingSmoke=true;
            else if(arg=="--docking-smoke-test"){dockSmoke=true;practice=true;}
            else if(arg=="--loadout-smoke-test"){loadoutSmoke=true;practice=true;}
            else if(arg=="--cargo-smoke-test"){cargoSmoke=true;practice=true;}
            else if(arg=="--surface-smoke-test"){surfaceSmoke=true;practice=true;}
            else if(arg=="--aleph-smoke-test"){alephSmoke=true;practice=true;}
            else if(arg=="--construction-smoke-test"){constructionSmoke=true;practice=true;}
            else if(arg=="--crew-smoke-test"){crewSmoke=true;practice=true;}
            else if(arg=="--team-smoke-test"){teamSmoke=true;practice=true;}
            else if(arg=="--command-smoke-test"){commandSmoke=true;practice=true;}
            else if(arg=="--camera-smoke-test"){cameraSmoke=true;practice=true;}
            else if(arg=="--scale-test"){scaleShips=std::stoi(value());if(scaleShips<=0 || 2ll*scaleShips+1>Native::shipIdentityCapacity)throw std::runtime_error("Invalid scale team size");practice=true;}
            else if(arg=="--scale-spread")scaleSpread=true;
            else if(arg=="--scale-idle")scaleIdle=true;
            else if(arg=="--scale-command")scaleCommand=true;
            else if(arg=="--scale-distance"){scaleDistance=std::stof(value());scaleDistanceSet=true;if(!std::isfinite(scaleDistance) || scaleDistance<=0)throw std::runtime_error("Scale observer distance must be positive and finite");}
            else if(arg=="--scale-csv")scaleFile=value();
            else if(arg=="--enemies" || arg=="--allies"){int count=std::stoi(value());if(count<0 || count>=Native::shipIdentityCapacity)throw std::runtime_error("Invalid wingman count");(arg=="--enemies"?enemyCount:allyCount)=count;}
            else if(arg=="--practice")practice=true;
            else if(arg=="--training-intro")trainingIntro=true;
            else if(arg=="--training"){trainingMission=std::stoi(value());if(std::none_of(Native::originalTraining.begin(),Native::originalTraining.end(),[&](auto& t){return t.id==trainingMission;}))throw std::runtime_error("Invalid training mission ID");practice=true;}
            else if(arg=="--scenario"){scenarioIndex=std::stoi(value());if(scenarioIndex<0 || scenarioIndex>=int(Native::scenarios.size()))throw std::runtime_error("Invalid scenario index");practice=true;}
            else if(arg=="--menu")menuName=value();
            else if(arg=="--config")configPath=value();
            else if(arg=="--import-config" || arg=="--import-inputmap")static_cast<void>(value());
            else if(arg=="--ui-scale"){uiScale=std::stof(value());if(!std::isfinite(uiScale) || uiScale<0.5f || uiScale>2.5f)throw std::runtime_error("UI scale must be between 0.5 and 2.5");}
            else if(arg=="--classic-ui-scale"){auto v=value();if(v=="auto")classicUiScale=0;else{size_t end=0;classicUiScale=std::stoi(v,&end);if(end!=v.size() || classicUiScale<1 || classicUiScale>8)throw std::runtime_error("Classic UI scale must be auto or an integer between 1 and 8");}}
            else if(arg=="--modern-ui")modernUi=true;
            else if(arg=="--seed"){auto number=std::stoull(value());if(number>UINT32_MAX)throw std::runtime_error("Seed must fit in 32 bits");sessionSeed=uint32_t(number);}
            else if(arg=="--frames")frameLimit=std::stoul(value());
            else if(arg=="--screenshot")screenshot=value();
            else if(arg=="--model")modelName=value();
            else if(arg=="--movie")movieName=value();
            else if(arg=="--timing-csv")timingFile=value();
            else if(arg=="--help") {
                printUsage();
                return 0;
            }
            else throw std::runtime_error("Unknown argument: "+arg);
        }
        if(smoke && !frameLimit)frameLimit=1800;
        if(chatSmoke && !frameLimit)frameLimit=660;
        if(consoleSmoke && !frameLimit)frameLimit=1220;
        if(playerPaneSmoke && !frameLimit)frameLimit=3000;
        if(gameStateSmoke && !frameLimit)frameLimit=3000;
        if(crewUiSmoke && !frameLimit)frameLimit=3000;
        if(dockViewSmoke && !frameLimit)frameLimit=140;
        if(resultsUiSmoke && !frameLimit)frameLimit=400;
        if(eliminationSmoke && !frameLimit)frameLimit=260;
        if(frontSmoke && !frameLimit)frameLimit=1600;
        if(teamSettingsSmoke && !frameLimit)frameLimit=900;
        if(lobbyChatSmoke && !frameLimit)frameLimit=10000;
        if(gamesSmoke && !frameLimit)frameLimit=2400;
        if(findPlayerSmoke && !frameLimit)frameLimit=2000;
        if(lanSmoke && !frameLimit)frameLimit=2400;
        if(refitSmoke){if(connectAddress.empty())throw std::runtime_error("--refit-smoke-test requires --connect");if(!frameLimit)frameLimit=15000;}
        bool lobbySmokePassed=false;
        if(lobbySmoke && !frameLimit)frameLimit=930;
        if(deviceSmoke && !frameLimit)frameLimit=510;
        if(pingsSmoke){if(connectAddress.empty())throw std::runtime_error("--pings-smoke-test requires --connect");if(!frameLimit)frameLimit=3000;}
        if(customMapSmoke && !frameLimit)frameLimit=3000;
        if(teleportSmoke && !frameLimit)frameLimit=700;
        if(mouseSmoke){settings.importInputMap(artwork/"defaultinputmap.mdl");if(!frameLimit)frameLimit=1450;}
        if(trainingTerminalSmoke && !frameLimit)frameLimit=1120;
        if(startupMovieSmoke && !frameLimit)frameLimit=180;
        if(trainingEntrySmoke && !frameLimit)frameLimit=240;
        if(trainingSlidesMenuSmoke && !frameLimit)frameLimit=270;
        if(classicKeyboardSmoke && !frameLimit)frameLimit=480;
        if(inputRoutingSmoke && !frameLimit)frameLimit=200;
        if(dockSmoke && !frameLimit)frameLimit=16000;
        if(cameraSmoke && !frameLimit)frameLimit=2000;
        if(commandSmoke && !frameLimit)frameLimit=750;
        if(scaleCommand && !scaleShips)throw std::runtime_error("--scale-command requires --scale-test N");
        if(scaleCommand && !scaleDistanceSet)scaleDistance=3000;
        if(teamSmoke && !frameLimit)frameLimit=650;
        if(crewSmoke && !frameLimit)frameLimit=1800;
        if(loadoutSmoke && !frameLimit)frameLimit=500;
        if(cargoSmoke && !frameLimit)frameLimit=1000;
        if(surfaceSmoke && !frameLimit)frameLimit=9000;
        if(alephSmoke && !frameLimit)frameLimit=1800;
        if(constructionSmoke && !frameLimit)frameLimit=3600;
        settings.fullscreen=!windowed;settings.uiScale=uiScale;settings.classicUiScale=classicUiScale;
        if(imported){settings.save(configPath);fprintf(stderr,"Imported %zu original preferences and input mappings.\n",imported);}
        Native::Frontend frontend(settings,defaultSettings);float winMoney{};auto civilizationCatalog=Native::Game::civilizations(artwork,&winMoney);frontend.civilizations(std::move(civilizationCatalog),winMoney);
#ifdef __linux__
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"wayland");
#endif
        SDL_SetHint(SDL_HINT_APP_ID,"org.freeallegiance.Native");
        if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD))throw std::runtime_error(SDL_GetError());
        struct SdlQuit {~SdlQuit(){SDL_Quit();}} quit;
        Native::configureVulkanPlatform();
        if(!SDL_InitSubSystem(SDL_INIT_HAPTIC))fprintf(stderr,"Haptics unavailable: %s\n",SDL_GetError());
        Native::Controller controller(settings.controller,artwork);
        NativeTest::OriginalInputRoutingSmoke inputRoutingCheck;
        auto controllerInfo=[&]{frontend.controllers(controller.devices(),controller.name(),controller.axes(),controller.buttons(),controller.hats(),controller.gamepad());};
        controllerInfo();
        auto flags=SDL_WINDOW_VULKAN|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE;
        if(!windowed)flags|=SDL_WINDOW_FULLSCREEN;
        std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(SDL_CreateWindow("Allegiance",1280,720,flags),SDL_DestroyWindow);
        if(!window)throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window.get(),800,600);
        fprintf(stderr,"Window backend: %s\n",SDL_GetCurrentVideoDriver());
        Native::Assets assets(artwork);Native::Renderer renderer(window.get(),validation);
        std::unique_ptr<Native::OriginalCursor> originalCursor;
        if(!modernUi)originalCursor=std::make_unique<Native::OriginalCursor>(assets);
        if(!modernUi)frontend.originalInterface(assets);
        if(!timingFile.empty())renderer.traceTiming(timingFile);
        renderer.gamma(settings.gamma);
        settings.antialiasing=renderer.antialiasing(settings.antialiasing);renderer.textureLimit(settings.maxTextureSize);
        frontend.graphicsSamples(renderer.antialiasingOptions());
        frontend.graphicsDeviceService([&]{return renderer.deviceInfo();});
        std::shared_ptr<Native::ModelData> inspect;
        uint32_t inspectID=0;
        if(!modelName.empty()){inspect=assets.model(modelName);inspectID=renderer.addModel(inspect);}
        auto effect=renderer.addModel(Native::makeProjectile());
        uint32_t warp=0; // Aleph meshes are loaded with each world's authored texture.
        Native::Hud hud(renderer,assets,uiScale);
        hud.modernInterface(modernUi);
        float currentDisplayScale=1;
        auto updateDisplayScale=[&]{auto scale=SDL_GetWindowDisplayScale(window.get());currentDisplayScale=scale;frontend.displayScale(scale);hud.displayScale(scale);};
        updateDisplayScale();
        std::unordered_map<std::string,Native::ModelHandle> loaded;
        auto load=[&](const std::string& name,const std::string& skin) {
            bool useSkin=!skin.empty() && std::isalpha(static_cast<unsigned char>(skin[0])) && skin!=name;
            std::string key=name+":"+(useSkin?skin:"");auto it=loaded.find(key);if(it!=loaded.end())return it->second;
            auto model=assets.model(name);
            auto image=useSkin?assets.image(skin+"bmp"):std::shared_ptr<Native::ImageData>{};
            Native::ModelHandle handle{renderer.addModel(model,image),model->radius,model->frames,model->lights};loaded.emplace(key,handle);return handle;
        };
        std::unordered_map<std::string,Native::EffectTexture> effectTextures;
        auto loadEffect=[&](const std::string& name) {
            if(auto i=effectTextures.find(name);i!=effectTextures.end())return i->second;
            auto image=assets.image(name);image->mipmaps=false;
            Native::EffectTexture texture{renderer.addTexture(image),image->width,image->height,int(std::max(1u,image->width/image->height))};
            effectTextures.emplace(name,texture);return texture;
        };
        renderer.flushUploads();
        Native::KeyboardBindings keyboard(settings);
        Native::MouseBindings mouse(settings);
        Native::Audio audio(assets);audio.volume(settings.volume);audio.categoryGains(settings.effectsGain,settings.voiceGain);hud.configure(settings);
        audio.preload(Allegiance::ClientEventSoundIds);
        Native::QuickChatCatalog quickChat(assets);audio.preload(quickChat);frontend.quickChats(quickChat);
        Native::ChatConsole chatConsole(assets,quickChat);
        Native::PlayerPane playerPane(assets);
        Native::GameStatePane gameStatePane(assets);
        Native::UiPointerCapture pointerCapture(frontend,playerPane,gameStatePane,hud);
        Native::UiViewTransitions viewTransitions;
        Native::CrewPane crewPane(assets);
        Native::ConsolePane consolePane(assets);
        std::unique_ptr<Native::Game> game,preparedGame;
        frontend.researchService([&]{return game?game->researchChoices():std::vector<Native::ResearchChoice>{};});
        frontend.teleportService([&]{return game?game->teleportDestinations():std::vector<Native::TeleportChoice>{};});
        hud.sectorInfoService([&](int sector)->const Native::SectorInfo&{static const Native::SectorInfo empty;return game?game->sectorInfo(sector):empty;});
        frontend.loadoutServices(
            [&](const Native::Loadout& requested,bool defaults,int target){return game?game->previewLoadout(requested,defaults,target):Native::LoadoutPreview{};},
            [&](Native::PartKind kind,int mount,int hull){return game?game->loadoutPartChoices(kind,mount,hull):std::vector<Native::PartChoice>{};},{},
            [&](int part,int hull){return game?game->loadoutPartInfo(part,hull):std::optional<Native::LoadoutSlotPreview>{};});
        std::unique_ptr<Native::Net::Client> network;uint64_t displayedMatch=0;
        frontend.missionDetailsService([&]{
            if(!game)return Allegiance::MissionDetails{};
            auto info=Allegiance::DescribeMission(*game->core()->GetMissionParams());
            info.name=Native::sessionName(game->sessionOptions());info.server=network?network->serverName():"Standalone";
            return info;
        });
        Native::Screen connectionReturn=Native::Screen::Online;
        std::optional<Native::Net::Digest> contentDigest; // destroy authored sounds before their audio services
        std::unique_ptr<Native::Net::ServerBrowser> browser;uint64_t browserRevision{};bool browserActive{};
        {std::ifstream file(Native::Settings::defaultPath().parent_path()/"servers.txt");std::string address;
            while(browseAddresses.size()<64 && std::getline(file,address)){if(!address.empty() && address.back()=='\r')address.pop_back();if(!address.empty() && address.front()!='#')browseAddresses.push_back(std::move(address));}}
        bool wheelThrottleChanged=false;
        bool running=true,paused=false,help=false,thirdPerson=false,cursor=false,blockFire=false,menuVisible=!practice && !inspect;
        bool classicMouseEnabled=settings.virtualJoystick;
        auto mousePointerMode=[&]{return modernUi?cursor:!classicMouseEnabled;};
        auto investmentFlight=[&]{return menuVisible && !modernUi && game && frontend.screen()==Native::Screen::Research && !frontend.hasMessageBox() && (!game->state().docked || game->state().commandView);};
        auto consoleMap=[&]{return menuVisible && !modernUi && game && !frontend.hasMessageBox() && (frontend.screen()==Native::Screen::Research || frontend.screen()==Native::Screen::Teleport);};
        auto consoleFlags=[&]() -> uint32_t {return consoleMap()?(frontend.screen()==Native::Screen::Teleport?ofTeleportPane:ofInvestment):menuVisible && frontend.screen()==Native::Screen::ShipLoadout?ofInLoadout:0;};
        auto menuPointer=[&]{return menuVisible && (!investmentFlight() || frontend.freshInvestmentPane() || mousePointerMode() || game->state().commandView);};
        auto playerLayer=[&]{return !modernUi && game && !game->state().complete && !game->state().networkLobby && !game->state().cinematic && (!menuVisible || frontend.screen()==Native::Screen::Hangar || frontend.screen()==Native::Screen::ShipLoadout || frontend.screen()==Native::Screen::Teleport || investmentFlight());};
        auto flightConsole=[&]{return !modernUi && game && !game->state().complete && !game->state().networkLobby &&
            (!menuVisible || frontend.view().overlay || frontend.screen()==Native::Screen::Hangar || frontend.screen()==Native::Screen::ShipLoadout || consoleMap());};
        auto detailsPopup=[&]{return !modernUi && menuVisible && frontend.screen()==Native::Screen::GameDetails;};
        auto popupCapturesInput=[&]{return menuVisible && (frontend.hasMessageBox() || detailsPopup() || (!modernUi && frontend.keyboardPopup()));};
        auto popupCapturesPointer=[&]{return popupCapturesInput();};
        bool hadFlightConsole=false;
        // Synthetic held-key diagnostics must not depend on the compositor
        // granting focus to a test window. Ordinary play always uses real focus.
        auto keyboardFocus=[&]{return teleportSmoke || chatSmoke || consoleSmoke || playerPaneSmoke || gameStateSmoke || crewUiSmoke || resultsUiSmoke || (SDL_GetWindowFlags(window.get())&SDL_WINDOW_INPUT_FOCUS)!=0;};
        unsigned frames=0;float angle=0,throttle=0;std::string requestedScreenshot;uint64_t screenshotSerial=0;Native::GameState teamSettingsFixture;
        int teamRequester=-1,teamApprover=-1,teamRequestBalance{};
        int consoleRequester=-1,consoleRequestBalance{},consoleBallot=-1;
        float cockpitFov=settings.fieldOfView;
        std::unique_ptr<Native::TrainingSlides> slides;Native::SessionOptions slideSession;bool slidesPost{},debriefShown{},postgameShown{};double trainingStartAt{-1};
        Native::CameraInput cameraInput;Allegiance::ThrottleRepeat throttleRepeat;Allegiance::MouseJoystick mouseJoystick;
        frontend.mouseInputService([&]{return classicMouseEnabled;},[&](bool value){classicMouseEnabled=value;mouseJoystick.enable(false);});
        Native::Loadouts savedLoadouts;
        auto loadoutPath=[&](){return configPath.parent_path()/Native::sessionLoadoutFile(game->sessionOptions());};
        Allegiance::WingEntry wingEntry;
        auto savedWing=[&]{
            int selected=Allegiance::SavedWingAssignment(settings.defaultWing);
            if(selected!=settings.defaultWing){settings.defaultWing=selected;settings.save(configPath);}
            return selected;
        };
        auto restoreSavedWing=[&]{if(game && game->state().playerSide>=0){int selected=savedWing();if(game->state().wing!=selected)game->setWing(game->state().shipId,selected);}};
        auto enterWing=[&]{if(game){auto& state=game->state();auto selected=wingEntry.enter(state.shipId,state.playerSide,state.commander,savedWing());if(selected && state.wing!=*selected)game->setWing(state.shipId,*selected);}};
        auto startsInHangar=[&](Native::Scenario scenario){return !scaleShips && !smoke && !cameraSmoke && !mouseSmoke && !chatSmoke && !surfaceSmoke && scenario!=Native::Scenario::FlightTraining && scenario!=Native::Scenario::WeaponsTraining;};
        auto createSession=[&](const Native::SessionOptions& options,Native::GameRole role){return std::make_unique<Native::Game>(artwork,load,effect,warp,options,loadEffect,role);};
        auto startSession=[&](Native::SessionOptions options,Native::GameRole role=Native::GameRole::Offline) {
            frontend.trainingSlides(false);
            if(options.trainingMission && settings.showStartupTrainingSuggestion){settings.showStartupTrainingSuggestion=false;settings.save(configPath);}
            frontend.setPlaying(false);
            if(role==Native::GameRole::Offline){network.reset();networkInitialized=false;}
            audio.stopAll();game.reset();wingEntry.reset();chatConsole.reset();playerPane.reset();gameStatePane.reset();viewTransitions.reset();controller.reset();throttleRepeat.Reset();debriefShown=false;postgameShown=false;
            cameraInput={};cockpitFov=settings.fieldOfView;thirdPerson=settings.preferChase;mouseJoystick.enable(false);mouse.suppress(SDL_GetMouseState(nullptr,nullptr));
            bool prepared=role==Native::GameRole::Offline && preparedGame && preparedGame->sessionOptions()==options;
            if(role==Native::GameRole::Offline && !prepared)options.seed=sessionSeed.value_or(uint32_t(SDL_GetTicksNS()>>10));
            options.startInHangar=startsInHangar(options.scenario);
            game=prepared?std::move(preparedGame):createSession(options,role);preparedGame.reset();
            game->loadoutPresets(Native::Loadouts::load(loadoutPath()));
            if(!options.trainingMission){float coreMoney{};auto choices=game->civilizations(&coreMoney);frontend.civilizations(std::move(choices),coreMoney,Native::sessionCoreFile(game->sessionOptions()));}
            if(role==Native::GameRole::Replica){networkInitialized=false;return;}
            if(!options.trainingMission)enterWing();
            if(options.scenario==Native::Scenario::Conquest && !options.trainingMission)game->restoreDockedLoadout(true);
            game->particleQuality(settings.particles);game->showGrid(settings.showGrid);game->showCommandGrid(settings.showCommandGrid);game->visualPreferences(settings.visual);game->chatPreferences(settings.chat);game->quickChats(quickChat);hud.resetMessages();consolePane.reset();
            game->interfacePrompts([&](std::string_view text){return Native::interfacePrompt(text,settings,&keyboard);});
            if(options.trainingMission)game->startTraining(audio,controller.name().empty()?0:1,[&](std::string_view text){return Native::trainingPrompt(text,settings,&keyboard);});
            audio.preload(game->soundIds());
            if(!options.trainingMission){game->clearSounds();game->clearMessages();}hud.preloadIcons(game->radarIcons());hud.preloadInventory(game->inventoryScripts());hud.preload(game->contacts());renderer.flushUploads();
            frontend.setPlaying(true);paused=help=cursor=false;blockFire=true;throttle=0;menuVisible=game->state().docked && !game->state().commandView;
            if(menuVisible)frontend.open(Native::Screen::Hangar,&game->state());
        };
        frontend.prepareSessionService([&](Native::SessionOptions& options){
            options.seed=sessionSeed.value_or(uint32_t(SDL_GetTicksNS()>>10));options.startInHangar=startsInHangar(options.scenario);
            preparedGame=createSession(options,Native::GameRole::Offline);
            return preparedGame->missionBriefing(options.team);
        },[&]{preparedGame.reset();});
        auto joinServer=[&](const std::string& address,int side,std::array<uint64_t,2> creatorToken=std::array<uint64_t,2>{},Native::Net::Instance instance=Native::Net::Instance{}){
            if(side<0 || side>=6)throw std::runtime_error("Team must be 1 through 6");
            connectionReturn=frontend.screen()==Native::Screen::Games?Native::Screen::Games:Native::Screen::Online;
            frontend.setPlaying(false);
            network.reset();audio.stopAll();game.reset();networkInitialized=false;
            if(browser){browser->add(address);browser->stop();browserActive=false;}
            else if(browseAddresses.size()<64)browseAddresses.push_back(address);
            if(!contentDigest)contentDigest=Native::Net::contentDigest(artwork);
            network=std::make_unique<Native::Net::Client>(address,*contentDigest,settings.pilot,side,[&](const Native::SessionOptions& options)->Native::Game&{startSession(options,Native::GameRole::Replica);return *game;},creatorToken,instance);
            menuVisible=true;frontend.open(Native::Screen::Online);if(modernUi)frontend.error("Connecting to "+address+"...");else frontend.connecting();
        };
        if(!connectAddress.empty())joinServer(connectAddress,networkSide);
        if(practice && connectAddress.empty() && !inspect && !trainingIntro) {
            Native::SessionOptions options;options.pilot=settings.pilot;options.trainingMission=trainingMission;
            if(dockSmoke || crewSmoke || cargoSmoke || loadoutSmoke)options.enemies=0;
            if(cameraSmoke || commandSmoke || mouseSmoke){options.enemies=0;options.scenario=Native::Scenario::FreeFlight;}
            if(commandSmoke)options.allies=3;
            if(scenarioIndex>=0){auto& s=Native::scenarios[scenarioIndex];options.scenario=Native::Scenario(scenarioIndex);options.map=s.map;options.enemies=s.enemies;options.allies=s.allies;options.killLimit=s.killLimit;}
            if(alephSmoke){options.map=3;options.enemies=0;}
            if(teleportSmoke){options.scenario=Native::Scenario::Conquest;options.enemies=0;options.map=6;}
            if(teamSmoke){options.scenario=Native::Scenario::Conquest;options.rules.miners=0;options.rules.startingMoney=9;}
            if(eliminationSmoke){options.scenario=Native::Scenario::Conquest;options.map=6;options.enemies=0;options.rules.miners=0;options.rules.teams=3;options.rules.lives=0;options.rules.conquest=0;options.localPilots={{"Surviving pilot",1,false},{"Remaining pilot",2,false}};}
            if(chatSmoke || consoleSmoke || playerPaneSmoke || gameStateSmoke || crewUiSmoke || resultsUiSmoke){options.scenario=Native::Scenario::Conquest;options.enemies=0;options.rules.miners=0;}
            if(dockViewSmoke){options.scenario=Native::Scenario::Conquest;options.enemies=0;options.rules.miners=0;options.rules.lives=0;options.map=6;options.localPilots.push_back({"Surviving pilot",0,false});}
            if(constructionSmoke){options.scenario=Native::Scenario::Conquest;options.enemies=0;options.map=0;}
            if(enemyCount)options.enemies=*enemyCount;if(allyCount)options.allies=*allyCount;
            if(scaleShips){options.scenario=Native::Scenario::Skirmish;options.map=0;options.enemies=options.allies=scaleShips;options.killLimit=0;options.variedWingmen=true;sessionSeed=sessionSeed.value_or(1);if(scaleCommand)options.observerBattleDepth=scaleSpread?4000:1400;}
            startSession(options);
            if(scaleShips){Native::arrangeScaleBattle(*game,scaleSpread,scaleIdle,scaleDistance,scaleCommand);menuVisible=false;fprintf(stderr,"Scale diagnostic: %d AI per team, %s %s, %d total ships\n",scaleShips,scaleSpread?"spread":"dense",scaleIdle?"idle":"combat",Native::diagnosticShipCount(*game));}
            if(teamSmoke) {
                int pilot=game->addLocalPilot("Wing pilot",game->state().pilots.front().side);
                if(pilot<0 || !game->donateMoney(pilot,2000))throw std::runtime_error("Team smoke pilot provisioning failed");
                frontend.open(Native::Screen::Pilots,&game->state());menuVisible=true;
            }
            if(commandSmoke){game->launch();for(int i=0;i<32;++i)game->step({},.1);game->commandView(true);menuVisible=false;}
            if(cargoSmoke) {
                auto old=game->state().equipment.front().name;auto choices=game->partChoices(Native::PartKind::Weapon,0);
                auto gun=std::find_if(choices.begin(),choices.end(),[&](auto& p){return p.name!=old;});
                auto reserves=game->partChoices(Native::PartKind::Pack,-2);
                auto ammo=std::find_if(reserves.begin(),reserves.end(),[](auto& p){return p.name=="Ammo";});
                if(gun==choices.end() || ammo==reserves.end() || !game->replacePart(Native::PartKind::Pack,-1,gun->id) ||
                   !game->replacePart(Native::PartKind::Pack,-2,ammo->id) || !game->replacePart(Native::PartKind::Pack,-5,ammo->id))throw std::runtime_error("Cargo smoke provisioning failed");
                game->launch();menuVisible=false;renderer.flushUploads();
            }
            if(alephSmoke) {
                game->launch();menuVisible=false;
                game->step({},1./165); // Update original side visibility before selecting a warp.
                auto it=std::find_if(game->contacts().begin(),game->contacts().end(),[](auto& c){return c.type==Native::ContactWarp;});
                if(it==game->contacts().end())throw std::runtime_error("Aleph smoke has no visible warp");
                game->selectTarget(it->type,it->id);cameraInput.mode=Native::CameraMode::Target;cameraInput.distance=it->radius*4;
            }
            if(constructionSmoke) {
                int project=-1;
                for(auto& r:game->state().research)if(r.available && r.name=="Light Outpost")project=r.id;
                if(project<0 || !game->invest(project))throw std::runtime_error("Construction smoke project unavailable");
                int builder=-1;
                for(int i=0;i<18000 && builder<0;++i){game->step({},.1);for(auto& u:game->state().units)if(u.type==Native::builderPilot)builder=u.id;}
                if(builder<0)throw std::runtime_error("Construction smoke builder missing");
                // Original CommandID build = 9; select a build site in the visible sector.
                auto sites=game->orderTargets(builder,9);int sector=-1;for(auto& s:game->state().sectors)if(s.current)sector=s.id;
                auto site=std::find_if(sites.begin(),sites.end(),[&](auto& s){return s.sectorId==sector;});
                if(site==sites.end() || !game->order(builder,site->type,site->id,9))throw std::runtime_error("Construction smoke has no local build site");
                bool building=false;
                for(int i=0;i<24000 && !building;++i){game->step({},.1);for(auto& d:game->scene(true))building|=d.blend==Native::ModelBlend::Alpha;}
                if(!building)throw std::runtime_error("Construction smoke did not reach the build effect");
                game->launch();for(int i=0;i<32;++i)game->step({},.1);game->selectTarget(site->type,site->id);cameraInput.mode=Native::CameraMode::Target;cameraInput.distance=600;
                audio.preload(game->soundIds());game->clearSounds();renderer.flushUploads();menuVisible=false;
            }
            if(crewSmoke) {
                int gunner=game->state().shipId,pilot=game->addLocalPilot("Gunship pilot",0,290);
                if(pilot<0 || !game->boardShip(pilot) || !game->controlLocalPilot(pilot))throw std::runtime_error("Crew smoke boarding failed");
                game->launch();game->controlLocalPilot(gunner);audio.preload(game->soundIds());renderer.flushUploads();menuVisible=false;
            }
        }
        if(menuName=="help" && game){game->launch();help=true;menuVisible=false;}
        else if(!menuName.empty()) {
            const std::unordered_map<std::string,Native::Screen> screens{{"home",Native::Screen::Home},{"credits",Native::Screen::Credits},{"training",Native::Screen::Training},{"hangar",Native::Screen::Hangar},{"loadout",Native::Screen::ShipLoadout},{"scenarios",Native::Screen::Scenarios},{"games",Native::Screen::Games},{"lobby",Native::Screen::Lobby},
                {"chat-preferences",Native::Screen::ChatPreferences},{"mouse",Native::Screen::Mouse},{"mouse-buttons",Native::Screen::MouseButtons},{"visuals",Native::Screen::Visuals},{"audio",Native::Screen::Audio},{"interface",Native::Screen::Interface},{"movies",Native::Screen::Movies},{"teams",Native::Screen::Teams},{"lobby-pilots",Native::Screen::LobbyPilots},{"settings",Native::Screen::Settings},{"controls",Native::Screen::Controls},{"controller",Native::Screen::Controller},{"controller-map",Native::Screen::ControllerMap},{"online",Native::Screen::Online},{"rules",Native::Screen::MissionRules},{"scores",Native::Screen::Scores},{"research",Native::Screen::Research},{"orders",Native::Screen::Orders},{"pilot-orders",Native::Screen::PilotOrders},{"quickchat",Native::Screen::QuickChat},{"chat",Native::Screen::Chat}};
            auto it=screens.find(menuName);if(it==screens.end())throw std::runtime_error("Unknown menu");frontend.open(it->second,game?&game->state():nullptr);menuVisible=true;
        }
        std::unique_ptr<Native::Movie> movie;
        std::string playingMovieName;
        uint32_t movieTexture=0;
        std::unordered_map<uint64_t,uint32_t> movieTextures;
        std::shared_ptr<Native::ImageData> lastMovieFrame;
        auto playMovie=[&](const std::string& name) {
            movie=std::make_unique<Native::Movie>(assets.find(name));
            playingMovieName=name;
            auto size=movie->size();auto blank=std::make_shared<Native::ImageData>();
            blank->width=size.x;blank->height=size.y;blank->pixels.resize(size_t(size.x)*size.y*4);blank->mipmaps=false;
            uint64_t key=(uint64_t(size.x)<<32)|uint32_t(size.y);
            auto [it,inserted]=movieTextures.try_emplace(key,0);
            if(inserted)it->second=renderer.addTexture(blank);
            movieTexture=it->second;lastMovieFrame.reset();
            audio.synchronize({});
            audio.paused(true,false);SDL_SetWindowRelativeMouseMode(window.get(),false);
        };
        Native::StartupMovies startupMovies;
        if(!movieName.empty())playMovie(movieName);
        else if(!practice && !inspect && menuName.empty() && (!frameLimit || startupMovieSmoke)) {
            startupMovies=Native::StartupMovies(settings.startupCredits,settings.startupIntro);
            if(settings.startupIntro){settings.startupIntro=false;settings.save(configPath);}
            if(auto name=startupMovies.next();!name.empty())playMovie(std::string(name));
        }
        if(!modernUi && !practice && !inspect && connectAddress.empty() && menuName.empty() && movieName.empty() && (!frameLimit || trainingEntrySmoke))frontend.startupTrainingSuggestion();
        auto finishMovie=[&]{
            movie.reset();lastMovieFrame.reset();
            if(auto name=startupMovies.next();!name.empty())playMovie(std::string(name));
            else SDL_SetWindowRelativeMouseMode(window.get(),!menuVisible&&!paused&&!frameLimit);
        };
        std::vector<Native::HudQuad> movieQuads;
        auto showLoadouts=[&]() {
            std::vector<Native::PartChoice> choices;auto current=game->currentLoadout();
            for(size_t bank=0;bank<Native::Loadouts::count;++bank)if(auto* saved=savedLoadouts.find(bank,current.hull))
                choices.push_back({int(bank),game->loadoutName(*saved)+" / "+std::to_string(saved->parts.size())+" parts"});
            frontend.loadouts(std::move(choices),game->state());
        };
        frontend.loadoutServices(
            [&](const Native::Loadout& requested,bool defaults,int target){return game?game->previewLoadout(requested,defaults,target):Native::LoadoutPreview{};},
            [&](Native::PartKind kind,int mount,int hull){return game?game->loadoutPartChoices(kind,mount,hull):std::vector<Native::PartChoice>{};},
            [&](int bank,int hull)->std::optional<Native::Loadout>{if(!game)return {};auto saved=Native::Loadouts::load(loadoutPath());if(auto* loadout=saved.findExact(bank,hull))return *loadout;return {};},
            [&](int part,int hull){return game?game->loadoutPartInfo(part,hull):std::optional<Native::LoadoutSlotPreview>{};});
        frontend.loadoutMemory([&](const Native::Loadout& loadout){if(game)game->rememberEditorLoadout(loadout);});
        auto startSlides=[&](Native::SessionOptions options,bool post) {
            slides.reset();audio.stopAll();movie.reset();audio.paused(false);
            trainingStartAt=-1;
            slideSession=options;slidesPost=post;slides=std::make_unique<Native::TrainingSlides>(assets,audio,options.trainingMission,post);
            hud.preloadSlides(slides->all());renderer.flushUploads();frontend.trainingSlides(true);menuVisible=false;
        };
        auto finishPurchase=[&](Native::PurchaseResult result,uint64_t action=0){
            bool current=frontend.purchaseFinished(action);
            if(!result.accepted)return;
            audio.preload(game->soundIds());hud.preloadInventory(game->inventoryScripts());hud.preload(game->contacts());renderer.flushUploads();
            if(!current)return;
            if(result.launched){menuVisible=false;paused=help=cursor=false;blockFire=true;throttle=0;controller.reset();}
            else {
                frontend.stageLoadout(game->currentLoadout());frontend.sync(game->state());
                if(!result.complete)frontend.error("Some of the parts you requested are no longer available");
            }
        };
        auto flushUiSounds=[&]{
            for(auto id:frontend.sounds())audio.play(id,{},{},true);frontend.clearSounds();
            for(auto id:playerPane.sounds())audio.play(id,{},{},true);playerPane.clearSounds();
            for(auto id:gameStatePane.sounds())audio.play(id,{},{},true);gameStatePane.clearSounds();
        };
        auto dismissPanes=[&](uint32_t mask){Native::FrontResult action;action.dismissPanes=mask;Native::dismissConsolePanes(action,playerPane,gameStatePane,game.get());};
        auto frontAction=[&](Native::FrontResult result) {
            flushUiSounds(); // Start feedback before an action loads assets or joins a match.
            if(!modernUi)dismissPanes(result.dismissPanes);
            try {
                switch(result.effect) {
                case Native::FrontEffect::None:break;
                case Native::FrontEffect::CreateGame:
                    if(!browser){if(!contentDigest)contentDigest=Native::Net::contentDigest(artwork);browser=std::make_unique<Native::Net::ServerBrowser>(*contentDigest);}
                    if(browser->createGame(frontend.createAddress(),frontend.createName(),settings.pilot,frontend.createCore()))frontend.connecting();
                    else frontend.error("The game creation request could not be started.");break;
                case Native::FrontEffect::FindPlayer:
                    if(!browser){if(!contentDigest)contentDigest=Native::Net::contentDigest(artwork);browser=std::make_unique<Native::Net::ServerBrowser>(*contentDigest);for(const auto& address:browseAddresses)browser->add(address);}
                    browser->findPlayer(frontend.playerSearchName());break;
                case Native::FrontEffect::QuickChat:
                    if(game && result.value>=0 && result.value<int(quickChat.commands.size()) &&
                       game->sendQuickChat(Native::ChatChannel(result.command),quickChat.commands[result.value],result.command==int(Native::ChatChannel::Target) && (result.voiceOnly || result.targetType==Native::ContactShip)?result.targetId:-1,result.voiceOnly)){if(!result.voiceOnly)menuVisible=false;blockFire=true;}
                    else if(!game && result.voiceOnly && result.value>=0 && result.value<int(quickChat.commands.size())){
                        frontend.lobbyQuickChatSent(result.value);audio.play(quickChat.commands[result.value].sound,{}, {},true);blockFire=true;
                    }
                    else frontend.error("Choose a valid quick-chat recipient.");break;
                case Native::FrontEffect::SendChat:
                    if((game && game->sendChat(Native::ChatChannel(result.value),frontend.outgoingChatText(),result.targetId)) || (!game && !frontend.chatText().empty())){if(frontend.screen()==Native::Screen::Chat)menuVisible=false;frontend.chatSent();blockFire=true;}
                    else frontend.error("Choose a valid recipient and enter a message.");break;
                case Native::FrontEffect::SaveStats:
                    if(game){SYSTEMTIME time;GetLocalTime(&time);auto file=configPath.parent_path()/("scores-"+std::to_string(time.wYear)+"-"+std::to_string(time.wMonth)+"-"+std::to_string(time.wDay)+"_"+std::to_string(time.wHour)+"."+std::to_string(time.wMinute)+".xml");
                        try{if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());game->saveEndgame(file,frontend.results(),frontend.resultReason(),network?frontend.serverAddress():"Standalone");frontend.statsSaved("scores saved to "+file.string());}
                        catch(const std::exception& error){frontend.statsSaved(error.what());}}
                    break;
                case Native::FrontEffect::Donate:case Native::FrontEffect::AutoDonate:case Native::FrontEffect::RequestMoney:
                case Native::FrontEffect::MoneyResponse:case Native::FrontEffect::Wing:case Native::FrontEffect::HandOver:case Native::FrontEffect::MakeLeader:
                    if(!game && result.effect==Native::FrontEffect::Wing && result.value==-1){settings.defaultWing=result.command;settings.save(configPath);frontend.open(frontend.screen());break;}
                    if(game) {
                        bool ok=false;
                        switch(result.effect) {
                        case Native::FrontEffect::Donate:ok=game->donateMoney(result.value,result.command);break;
                        case Native::FrontEffect::AutoDonate:if(result.value>=0 && result.command>=0)restoreSavedWing();ok=game->setAutoDonate(result.value,result.command);break;
                        case Native::FrontEffect::RequestMoney:ok=game->requestMoney(result.value,frontend.screen()==Native::Screen::ShipLoadout?result.command:-1);break;
                        case Native::FrontEffect::MoneyResponse:ok=game->respondMoneyRequest(result.value!=0);break;
                        case Native::FrontEffect::Wing:ok=game->setWing(result.value,result.command);if(ok && result.value==game->state().shipId){settings.defaultWing=result.command;settings.save(configPath);}break;
                        case Native::FrontEffect::HandOver:ok=game->handOverCommand(result.value);if(ok)restoreSavedWing();break;
                        case Native::FrontEffect::MakeLeader:ok=game->makeTeamLeader(result.value);if(ok)restoreSavedWing();break;
                        default:break;
                        }
                        frontend.open(result.effect==Native::FrontEffect::MoneyResponse?Native::Screen::Pilots:frontend.screen(),&game->state());
                        if(!ok)frontend.error("The account or team action is no longer available.");
                    }break;
                case Native::FrontEffect::MutePilot:if(game){bool ok=game->mutePilot(result.value,result.command!=0);frontend.open(frontend.screen(),&game->state());if(!ok)frontend.error("That pilot is no longer available.");}break;
                case Native::FrontEffect::ControlPilot:if(game){if(game->controlLocalPilot(result.value)){controller.reset();throttle=0;blockFire=true;menuVisible=game->state().docked;frontend.open(menuVisible?Native::Screen::Hangar:Native::Screen::Pilots,&game->state());}else frontend.error("That local pilot cannot be controlled.");}break;
                case Native::FrontEffect::RemovePilot:if(game){bool removed=game->removeLocalPilot(result.value);frontend.open(Native::Screen::Pilots,&game->state());if(!removed)frontend.error("That local pilot cannot be removed.");}break;
                case Native::FrontEffect::SelectUnit:if(game){game->pickCommandUnit(result.value);if(!modernUi)chatConsole.selectPeer(*game,result.value);}break;
                case Native::FrontEffect::PlayerPane:if(game){gameStatePane.close();playerPane.toggle(*game,result.value!=0);blockFire=true;}break;
                case Native::FrontEffect::ConfirmBoot:if(game){
                    auto data=game->playerPaneData();auto pilot=std::find_if(data.rows.begin(),data.rows.end(),[&](const auto& row){return row.id==result.value;});
                    if(pilot!=data.rows.end()){
                        frontend.confirm("Boot "+pilot->name+"?",{Native::FrontEffect::BootPilot,result.value,result.command},!menuVisible);
                        menuVisible=true;blockFire=true;playerPane.cancelPointer();
                    }
                }break;
                case Native::FrontEffect::BootPilot:if(game){
                    bool accepted=game->bootPlayer(result.value,result.command);
                    if(result.resumeAfter){controller.reset();menuVisible=game->state().docked && !game->state().commandView;if(menuVisible)frontend.open(Native::Screen::Hangar,&game->state());else frontend.closeConsoleToFlight();paused=help=false;blockFire=true;}
                    if(!accepted){frontend.error("That pilot can no longer be booted.",!menuVisible);menuVisible=true;}
                    playerPane.sync(*game,true);
                }break;
                case Native::FrontEffect::TakeMeTo:if(game && game->takeMeTo(result.value)){if(!modernUi)chatConsole.selectPeer(*game,result.value);menuVisible=false;blockFire=true;}break;
                case Native::FrontEffect::NetworkConfigure:
                    if(network){if(network->configure(frontend.session()))frontend.configuring();else frontend.error("Unable to change mission parameters.");}break;
                case Native::FrontEffect::NetworkJoinTeam:if(network){restoreSavedWing();Native::emitLobbyNetworkActions(result,!modernUi,!game || game->state().networkReady,[&](const Native::Net::Action& a){
                    if(network->action(a)){if(a.op==Native::Net::Operation::Team && a.args[0]==Native::noTeam)frontend.quittingTeam(network->lastActionNumber());}
                    else if(a.op==Native::Net::Operation::Team)frontend.error("Unable to change teams.");
                });}break;
                case Native::FrontEffect::NetworkNewRound:if(network){Native::Net::Action a{};a.op=Native::Net::Operation::NewRound;network->action(a);}break;
                case Native::FrontEffect::LobbyAction:case Native::FrontEffect::LobbyVisit:case Native::FrontEffect::ChangeAlliance:
                    if(network)Native::emitLobbyNetworkActions(result,!modernUi,!game || game->state().networkReady,[&](const Native::Net::Action& a){
                        if(network->action(a)){if(result.effect==Native::FrontEffect::LobbyAction && a.op==Native::Net::Operation::Lobby && a.args[0]==int(Native::LobbyAction::CancelRequest))frontend.cancelingRequest(network->lastActionNumber());}
                        else if(a.op==Native::Net::Operation::Lobby && a.args[0]==int(Native::LobbyAction::CancelRequest))frontend.error("Unable to cancel the join request.");
                    });break;
                case Native::FrontEffect::TeamSettings:if(network){Native::Net::Action a{};a.op=Native::Net::Operation::Lobby;a.args[0]=int(Native::LobbyAction::TeamSettings);a.args[1]=result.value;Native::Net::text(a.text,frontend.teamSettingsName());network->action(a);}break;
                case Native::FrontEffect::NetworkReady:if(network){Native::Net::Action a{};a.op=Native::Net::Operation::Ready;a.args[0]=result.value;network->action(a);}break;
                case Native::FrontEffect::NetworkStart:if(network){Native::Net::Action a{};a.op=Native::Net::Operation::Start;network->action(a);}break;
                case Native::FrontEffect::Connect:try{settings.save(configPath);joinServer(frontend.serverAddress(),frontend.networkSide(),{},frontend.serverInstance());}catch(const std::exception& e){frontend.error(e.what());menuVisible=true;}break;
                case Native::FrontEffect::Start:if(frontend.session().trainingMission){settings.save(configPath);startSlides(frontend.session(),frontend.session().trainingMission==1);}else startSession(frontend.session());break;
                case Native::FrontEffect::Web:if(auto url=Allegiance::CommunityLink(result.value))SDL_OpenURL(url);break;
                case Native::FrontEffect::CommandView:if(game){game->commandView(true,game->state().commandTopDown);menuVisible=false;paused=help=cursor=false;blockFire=true;}break;
                case Native::FrontEffect::Help:if(!modernUi){frontend.open(Native::Screen::Help,game?&game->state():nullptr);if(result.resumeAfter)menuVisible=false;}else if(game){help=true;menuVisible=false;}else frontend.open(Native::Screen::Controls);break;
                case Native::FrontEffect::AcceptOrder:case Native::FrontEffect::ExecuteOrder:
                    if(game){bool ok=game->acceptCommand(result.effect==Native::FrontEffect::ExecuteOrder);frontend.open(Native::Screen::PilotOrders,&game->state());if(!ok)frontend.error("The queued command is no longer legal.");}break;
                case Native::FrontEffect::RejectOrder:if(game){game->rejectCommand();frontend.open(Native::Screen::PilotOrders,&game->state());}break;
                case Native::FrontEffect::ClearOrder:if(game){game->clearCommand();frontend.open(Native::Screen::PilotOrders,&game->state());}break;
                case Native::FrontEffect::TargetOrder:if(game){game->targetCommand();frontend.open(Native::Screen::PilotOrders,&game->state());}break;
                case Native::FrontEffect::Resume:controller.reset();if(slides){menuVisible=false;paused=help=false;blockFire=true;}else if(game){menuVisible=game->state().docked && !game->state().commandView;if(menuVisible)frontend.open(Native::Screen::Hangar,&game->state());else frontend.closeConsoleToFlight();paused=help=false;blockFire=true;}break;
                case Native::FrontEffect::ReturnToBase:if(game && game->returnToBase()){menuVisible=false;paused=help=cursor=false;blockFire=true;}break;
                case Native::FrontEffect::Launch:controller.reset();if(game){
                    if(frontend.screen()==Native::Screen::Teleport || frontend.screen()==Native::Screen::Research)frontend.open(Native::Screen::Hangar,&game->state());
                    bool passenger=game->state().passenger;auto source=game->state().sourcePilot;
                    auto purchase=game->purchaseLaunchLoadout();
                    if(!purchase.accepted){frontend.error("This loadout cannot be purchased here.");break;}
                    if(network){if(passenger)frontend.transferring(Allegiance::CrewTransfer::Disembark,source,network->lastActionNumber());else frontend.purchasing(true,network->lastActionNumber());}else finishPurchase(purchase);
                }break;
                case Native::FrontEffect::Leave:{
                    auto destination=slides || (game && game->state().trainingMission)?Native::Screen::Training:network && frontend.classicInterface()?connectionReturn:frontend.classicInterface()?(!game || game->sessionOptions().scenario==Native::Scenario::Conquest?Native::Screen::Games:Native::Screen::Home):Native::Screen::Scenarios;
                    slides.reset();trainingStartAt=-1;frontend.trainingSlides(false);
                    network.reset();networkInitialized=false;audio.stopAll();game.reset();controller.reset();mouseJoystick.enable(false);frontend.setPlaying(false);frontend.open(destination);menuVisible=true;break;
                }
                case Native::FrontEffect::Quit:running=false;break;
                case Native::FrontEffect::Intro:playMovie("intro_movie.avi");break;
                case Native::FrontEffect::ChangeHull:if(game){auto screen=frontend.screen();game->selectHull(result.value);audio.preload(game->soundIds());hud.preloadInventory(game->inventoryScripts());hud.preload(game->contacts());frontend.open(screen==Native::Screen::ShipLoadout?screen:Native::Screen::Hangar,&game->state());throttle=0;}break;
                case Native::FrontEffect::PartOptions:if(game) {
                    if(result.value<0)frontend.parts(Native::PartKind::Pack,result.value,game->partChoices(Native::PartKind::Pack,result.value));
                    else if(result.value<int(game->state().equipment.size())){auto& e=game->state().equipment[result.value];frontend.parts(e.kind,e.mount,game->partChoices(e.kind,e.mount));}
                }break;
                case Native::FrontEffect::EquipPart:if(game){
                    if(!game->replacePart(frontend.partKind(),frontend.partMount(),result.value))throw std::runtime_error("That part cannot be mounted here.");
                    audio.preload(game->soundIds());frontend.open(frontend.partMount()<0?Native::Screen::Cargo:Native::Screen::Hangar,&game->state());if(modernUi)frontend.error(result.value<0?"Cargo slot cleared.":"Equipment installed.");
                }break;
                case Native::FrontEffect::CargoSelect:if(game){game->selectCargo(result.value);frontend.open(Native::Screen::Cargo,&game->state());}break;
                case Native::FrontEffect::CargoNext:if(game){game->nextCargo();frontend.open(Native::Screen::Cargo,&game->state());}break;
                case Native::FrontEffect::CargoDrop:if(game){bool dropped=game->dropCargo();frontend.open(Native::Screen::Cargo,&game->state());if(!dropped)frontend.error("No item to drop.");}break;
                case Native::FrontEffect::CargoMounts:if(game)frontend.cargoMounts(game->cargoMounts(),game->state());break;
                case Native::FrontEffect::CargoSwap:if(game){bool changed=game->swapCargo(Native::PartKind(result.value/32),result.value%32);frontend.open(Native::Screen::Cargo,&game->state());if(!changed)frontend.error("No compatible reserve or free cargo slot.");}break;
                case Native::FrontEffect::Reload:if(game){game->reload();frontend.open(Native::Screen::Cargo,&game->state());}break;
                case Native::FrontEffect::Loadouts:if(game){savedLoadouts=Native::Loadouts::load(loadoutPath());showLoadouts();}break;
                case Native::FrontEffect::RememberLoadout:
                case Native::FrontEffect::SaveLoadout:if(game){
                    bool staged=frontend.screen()==Native::Screen::ShipLoadout;auto loadout=staged?frontend.stagedLoadout().loadout:game->currentLoadout();
                    auto next=Native::Loadouts::load(loadoutPath());next.remember(result.value,loadout);next.save(loadoutPath());savedLoadouts=std::move(next);game->loadoutPresets(savedLoadouts);
                    if(!staged)showLoadouts();
                    if(result.effect==Native::FrontEffect::SaveLoadout){if(modernUi)frontend.error("Loadout saved.");else game->loadoutPresetFeedback(loadout,result.value,true);}
                }break;
                case Native::FrontEffect::DeleteLoadout:if(game && result.value>=0 && result.value<int(Native::Loadouts::count)){auto next=savedLoadouts;next.banks[result.value].clear();next.save(loadoutPath());savedLoadouts=std::move(next);game->loadoutPresets(savedLoadouts);showLoadouts();if(modernUi)frontend.error("Loadout bank cleared.");}break;
                case Native::FrontEffect::UseLoadout:if(game){
                    if(frontend.screen()==Native::Screen::ShipLoadout){if(frontend.stageLoadoutPreset(result.value) && !modernUi)game->loadoutPresetFeedback(frontend.stagedLoadout().loadout,result.value,false);break;}
                    auto* saved=savedLoadouts.find(result.value,game->currentLoadout().hull);
                    if(!saved || !game->applyLoadout(*saved))throw std::runtime_error("Cannot fit that saved loadout here.");
                    audio.preload(game->soundIds());hud.preloadInventory(game->inventoryScripts());hud.preload(game->contacts());renderer.flushUploads();showLoadouts();
                    if(modernUi)frontend.error("Available equipment fitted within your budget.");
                }break;
                case Native::FrontEffect::CommitLoadout:if(game){
                    bool passenger=game->state().passenger;auto source=game->state().sourcePilot;
                    auto staged=frontend.stagedLoadout().loadout;auto purchase=game->purchaseLoadout(staged,result.value!=0);
                    if(!purchase.accepted){frontend.error("This loadout cannot be purchased here.");break;}
                    if(!result.value){auto next=Native::Loadouts::load(loadoutPath());next.remember(0,staged);next.save(loadoutPath());savedLoadouts=std::move(next);game->loadoutPresets(savedLoadouts);}
                    if(network){if(passenger)frontend.transferring(Allegiance::CrewTransfer::Disembark,source,network->lastActionNumber());else frontend.purchasing(result.value!=0,network->lastActionNumber());}else finishPurchase(purchase);
                }break;
                case Native::FrontEffect::Invest:if(game){bool funded=game->invest(result.value,result.command>0?result.command:-1);if(!funded && modernUi)throw std::runtime_error("Cannot fund that project.");frontend.sync(game->state());}break;
                case Native::FrontEffect::Order:if(game){auto& s=game->state();if(!game->order(result.value,result.targetType,result.targetId,result.command))throw std::runtime_error("That ship cannot carry out this order on the selected target.");frontend.open(Native::Screen::Orders,&s);}break;
                case Native::FrontEffect::DroneStandby:if(game){if(!game->droneStandby(result.value,result.command!=0))throw std::runtime_error("That drone cannot change its docking orders.");if(modernUi)frontend.open(Native::Screen::Orders,&game->state());}break;
                case Native::FrontEffect::OrderTargets:if(game)frontend.orderTargets(result.value,result.command,game->orderTargets(result.value,result.command));break;
                case Native::FrontEffect::Teleport:if(game){gameStatePane.close();if(!modernUi && game->state().docked)game->commandView(false);frontend.teleports(modernUi?game->teleportChoices():game->teleportDestinations(),game->state());}break;
                case Native::FrontEffect::TeleportTo:if(game){
                    bool passenger=game->state().passenger;auto source=game->state().sourcePilot;
                    auto* destination=game->core()->GetStation(result.value);std::string name=destination?destination->GetName():"";
                    if(game->teleportStation(result.value)){controller.reset();throttle=0;frontend.open(Native::Screen::Hangar,&game->state());if(network)frontend.transferring(passenger?Allegiance::CrewTransfer::Disembark:Allegiance::CrewTransfer::Teleport,passenger?source:name,network->lastActionNumber());}
                    else {frontend.open(Native::Screen::Hangar,&game->state());frontend.error("Teleportation failed.");}
                }break;
                case Native::FrontEffect::Crew:if(game)frontend.crew(game->crewChoices(),game->state());break;
                case Native::FrontEffect::Board:if(game){
                    bool teleport=frontend.screen()==Native::Screen::Teleport;
                    bool passenger=game->state().passenger;auto source=game->state().sourcePilot;
                    auto name=game->pilotName(result.value);
                    bool boarded=game->boardShip(result.value,false,!modernUi && teleport);
                    if(teleport)frontend.open(Native::Screen::Hangar,&game->state());else frontend.crew(game->crewChoices(),game->state());
                    if(!boarded){auto message=game->boardingFailure(result.value);if(!message.empty())frontend.error(std::move(message));}
                    else{if(network)frontend.transferring(passenger?Allegiance::CrewTransfer::Disembark:Allegiance::CrewTransfer::Board,passenger?source:name,network->lastActionNumber());blockFire=true;controller.reset();throttle=0;}
                }break;
                case Native::FrontEffect::Disembark:if(game){
                    auto name=game->state().sourcePilot;
                    bool left=game->leaveShip();frontend.open(Native::Screen::Hangar,&game->state());
                    if(!left){auto message=game->boardingFailure(-1);if(!message.empty())frontend.error(std::move(message));break;}
                    if(network)frontend.transferring(Allegiance::CrewTransfer::Disembark,name,network->lastActionNumber());
                }break;
                case Native::FrontEffect::Promote:if(game){if(!game->promoteGunner(result.value))throw std::runtime_error("Only your own gunner can be promoted.");frontend.crew(game->crewChoices(),game->state());blockFire=true;}break;
                case Native::FrontEffect::ApplyOptions:
                    if(result.value&1){mouseJoystick.enable(false);controller.configure(settings.controller);}
                    if(result.value&2)audio.categoryGains(settings.effectsGain,settings.voiceGain);
                    if(result.value&4){hud.configure(settings);if(game){game->particleQuality(settings.particles);game->visualPreferences(settings.visual);game->chatPreferences(settings.chat);}}
                    if(result.value&16){renderer.gamma(settings.gamma);frontend.refreshGraphicsDevice();}
                    if(result.value&32){settings.antialiasing=renderer.antialiasing(settings.antialiasing);frontend.refreshGraphicsOptions();}
                    if(result.value&64)renderer.textureLimit(settings.maxTextureSize);
                    settings.save(configPath);break;
                case Native::FrontEffect::PlayerPings:
                    if(result.value){menuVisible=false;blockFire=true;}
                    if(network)network->queryConnections();break;
                case Native::FrontEffect::ApplySettings:
                    settings.antialiasing=renderer.antialiasing(settings.antialiasing);renderer.textureLimit(settings.maxTextureSize);frontend.refreshGraphicsOptions();
                    mouseJoystick.enable(false);cockpitFov=settings.fieldOfView;controller.configure(settings.controller);keyboard.configure(settings);mouse.configure(settings);controllerInfo();hud.configure(settings);renderer.gamma(settings.gamma);audio.volume(settings.volume);audio.categoryGains(settings.effectsGain,settings.voiceGain);if(game){game->particleQuality(settings.particles);game->showGrid(settings.showGrid);game->showCommandGrid(settings.showCommandGrid);game->visualPreferences(settings.visual);game->chatPreferences(settings.chat);}
                    if(bool(SDL_GetWindowFlags(window.get())&SDL_WINDOW_FULLSCREEN)!=settings.fullscreen) {
                        if(!SDL_SetWindowFullscreen(window.get(),settings.fullscreen))throw std::runtime_error(SDL_GetError());
                        if(!settings.fullscreen)SDL_SetWindowSize(window.get(),1280,720);
                    }
                    settings.save(configPath);if(result.value==1){menuVisible=false;blockFire=true;}else if(!frontend.view().original)frontend.error("Configuration saved.");break;
                }
                if(game){for(auto& sound:game->sounds())audio.play(sound);game->clearSounds();}
            }catch(const std::exception& e){bool resume=!menuVisible;menuVisible=true;frontend.setPlaying(game!=nullptr);frontend.error(e.what(),resume);fprintf(stderr,"Session: %s\n",e.what());}
        };
        auto action=[&](Native::HudAction a) {
            if(a.command==Native::HudCommand::Menu){frontAction(frontend.activate(a.value));return;}
            if(!game)return;
            if(a.command==Native::HudCommand::Weapon && !game->recordKey(a.value<0?TK_ToggleGroupFire:TK_ToggleWeapon1+a.value))return;
            if(a.command==Native::HudCommand::Autopilot && !game->recordKey(TK_ToggleAutoPilot))return;
            switch(a.command) {
                case Native::HudCommand::Cargo:game->selectCargo(a.value);break;
                case Native::HudCommand::Target:
                    if(!modernUi && chatConsole.mode()==Allegiance::ComposeState::Shell)break;
                    if(!modernUi && chatConsole.mode()==Allegiance::ComposeState::Command){chatConsole.pickTarget(*game,a.type,a.value);break;}
                    game->selectTarget(a.type,a.value);if(game->state().commandView && a.type==Native::ContactShip && game->pickCommandUnit(a.value,(SDL_GetModState()&SDL_KMOD_SHIFT)!=0))if(!modernUi)chatConsole.selectUnits(*game);break;
                case Native::HudCommand::Sector:{
                    bool wasCommand=game->state().commandView;
                    if(consoleMap() && frontend.screen()==Native::Screen::Teleport){
                        auto& sectors=game->state().sectors;auto sector=std::find_if(sectors.begin(),sectors.end(),[&](auto& s){return s.id==a.value;});
                        if(sector!=sectors.end()){
                            if(sector->current)frontend.error("You are already in this cluster.");
                            else if(sector->restartStation<0)frontend.error("You do not have an appropriate station in this cluster.");
                            else frontAction({Native::FrontEffect::TeleportTo,sector->restartStation});
                        }
                    }else if(game->selectMapSector(a.value) && game->state().commandView && !wasCommand){menuVisible=false;help=cursor=false;blockFire=true;}break;}
                case Native::HudCommand::SectorChat:if(!modernUi)chatConsole.pickCluster(*game,a.value);else {menuVisible=true;frontend.chatSector(a.value,game->state());}blockFire=true;break;
                case Native::HudCommand::SectorHighlight:game->highlightSector(a.value);break;
                case Native::HudCommand::CommandOrder:if(!modernUi)chatConsole.verb(*game,a.value);else game->commandOrder(a.value);break;
                case Native::HudCommand::CommandRecipient:chatConsole.recipient(*game,a.value);blockFire=true;break;
                case Native::HudCommand::CommandWing:chatConsole.wings(*game);blockFire=true;break;
                case Native::HudCommand::Team:menuVisible=true;frontend.open(Native::Screen::Orders,&game->state());break;
                case Native::HudCommand::Scores:menuVisible=true;frontend.open(Native::Screen::Scores,&game->state());break;
                case Native::HudCommand::SessionMenu:menuVisible=true;frontend.open(Native::Screen::Pause,&game->state());break;
                case Native::HudCommand::Weapon:game->selectWeapon(a.value);break;
                case Native::HudCommand::Autopilot:game->toggleAutopilot();break;
                case Native::HudCommand::Respawn:game->recoverToHangar();throttle=0;break;
                case Native::HudCommand::NextHull:if(game->state().docked){menuVisible=true;frontend.open(Native::Screen::Hangar,&game->state());}else if(game->returnToBase()){paused=help=cursor=false;blockFire=true;}break;
                case Native::HudCommand::Launch:game->launch();break;
                case Native::HudCommand::Menu:break;
            }
        };
        auto bindingAction=[&](Native::Binding binding,TrekKey logicalKey=TK_NoKeyMapping) {
            using Native::Binding;
            if(logicalKey<0)logicalKey=Native::trekKey(binding);
            if(binding==Binding::Screenshot){
                auto directory=configPath.parent_path()/"screenshots";std::filesystem::create_directories(directory);
                requestedScreenshot=(directory/("allegiance-"+std::to_string(SDL_GetTicksNS())+"-"+std::to_string(++screenshotSerial)+".ppm")).string();return true;
            }
            if(binding==Binding::ChatPageUp || binding==Binding::ChatPageDown){
                if(menuVisible && frontend.pageChat(binding==Binding::ChatPageUp))return true;
                if(game)hud.pageMessages(game->state(),binding==Binding::ChatPageUp);return true;
            }
            if(binding==Binding::Pause){menuVisible=true;help=false;controller.reset();frontend.open(Native::Screen::Pause,!slides && game?&game->state():nullptr);return true;}
            if(binding==Binding::Help){if(!modernUi)frontAction(frontend.toggleHelp());else if(game)help=!help;return true;}
            if(!game)return false;
            if(!modernUi && binding==Binding::PaneInventory && game->state().docked && !game->state().networkLobby){
                if(game->state().ghost){if(game->recordKey(logicalKey)){dismissPanes(ofTeam|ofExpandedTeam|ofGameState);frontAction({Native::FrontEffect::Resume});}return true;}
                if(game->viewCamera(Native::CameraMode::Cockpit,logicalKey)){
                    dismissPanes(ofTeam|ofExpandedTeam|ofGameState);menuVisible=true;help=cursor=false;controller.reset();blockFire=true;
                    frontend.open(frontend.screen()==Native::Screen::ShipLoadout?Native::Screen::Hangar:Native::Screen::ShipLoadout,&game->state());
                }
                return true;
            }
            if(binding==Binding::CommandView || (!modernUi && (binding==Binding::UnitOrders || binding==Binding::PilotOrders))) {
                dismissPanes(ofTeam|ofExpandedTeam|ofGameState);
                game->commandView(!game->state().commandView,game->state().commandTopDown,logicalKey);
                menuVisible=game->state().docked && !game->state().commandView;
                if(menuVisible)frontend.open(Native::Screen::Hangar,&game->state());
                help=cursor=false;return true;
            }
            if(binding==Binding::CommandTopDown){game->toggleCommandPerspective();return true;}
            if((binding>=Binding::CameraExternal && binding<=Binding::CameraChase) || binding==Binding::CameraBase) {
                constexpr Native::CameraMode modes[]={Native::CameraMode::External,Native::CameraMode::Target,Native::CameraMode::FlyBy,
                    Native::CameraMode::Orbit,Native::CameraMode::Station,Native::CameraMode::Missile,Native::CameraMode::Cockpit,Native::CameraMode::Chase};
                auto mode=binding==Binding::CameraBase?Native::CameraMode::Base:modes[int(binding)-int(Binding::CameraExternal)];
                if(mode==Native::CameraMode::Target && game->state().docked)return true;
                if(mode!=Native::CameraMode::Cockpit && Native::cameraMode(cameraInput)==mode)mode=Native::CameraMode::Cockpit;
                if(game->viewCamera(mode,logicalKey)){
                    cameraInput.mode=mode;cameraInput.chase=thirdPerson=false;help=cursor=false;
                    if(game->state().docked){menuVisible=true;frontend.open(Native::Screen::Hangar,&game->state());}
                }
                return true;
            }
            if(binding==Binding::ToggleView) {
                auto current=Native::cameraMode(cameraInput);
                auto mode=current==Native::CameraMode::Chase?Native::CameraMode::Cockpit:Native::CameraMode::Chase;
                if(game->viewCamera(mode,logicalKey)){
                    cameraInput.mode=mode;cameraInput.chase=thirdPerson=false;help=cursor=false;
                    if(game->state().docked){menuVisible=true;frontend.open(Native::Screen::Hangar,&game->state());}
                }
                return true;
            }
            if(logicalKey>=0 && !game->recordKey(logicalKey))return true;
            if(binding==Binding::VoteYes || binding==Binding::VoteNo){game->vote(binding==Binding::VoteYes);return true;}
            if(binding==Binding::PaneContacts && !modernUi){
                if(!game->state().networkLobby && !game->state().complete && !game->state().cinematic){
                    if(menuVisible && (frontend.screen()==Native::Screen::Research || frontend.screen()==Native::Screen::Teleport))frontAction({Native::FrontEffect::Resume});
                    dismissPanes(ofGameState);playerPane.toggle(*game,logicalKey==TK_ConModeMiniTeam);blockFire=true;
                }
                return true;
            }
            if(binding==Binding::GameState){
                if(modernUi){menuVisible=true;frontend.open(Native::Screen::Scores,&game->state());return true;}
                if(!game->state().networkLobby && !game->state().complete && !game->state().cinematic){
                    if(menuVisible && (frontend.screen()==Native::Screen::Research || frontend.screen()==Native::Screen::Teleport))frontAction({Native::FrontEffect::Resume});
                    dismissPanes(ofTeam|ofExpandedTeam);gameStatePane.toggle(*game);blockFire=true;
                    if(game->state().trainingMission)game->setTrainingPane(ofGameState,gameStatePane.active());
                }return true;
            }
            if(binding==Binding::Cursor){
                if(modernUi){if(!investmentFlight() || !frontend.releaseInvestmentPointer())cursor=!cursor;}
                else {
                    bool wasEnabled=classicMouseEnabled;
                    auto change=Native::toggleConsoleMouse(frontend,classicMouseEnabled,playerPane,gameStatePane,*game,menuVisible);
                    if(change.effect!=Native::FrontEffect::None){change.dismissPanes=0;frontAction(change);}
                    if(game->state().cinematic && classicMouseEnabled && !wasEnabled)mouse.suppress(SDL_GetMouseState(nullptr,nullptr));
                }
                blockFire=true;return true;
            }
            if(binding==Binding::PauseTraining)return true;
            if(binding==Binding::PaneInventory && game->state().trainingMission){game->setTrainingPane(ofInventory,!(game->state().trainingPanes&ofInventory));return true;}
            if(binding==Binding::SectorMap && game->state().trainingMission){game->setTrainingPane(ofSectorPane,!(game->state().trainingPanes&ofSectorPane));return true;}
            if(binding==Binding::Teleport){
                if(!modernUi)dismissPanes(ofTeam|ofExpandedTeam|ofGameState);
                if(game->state().docked && !game->state().ghost && !game->state().trainingMission){
                    menuVisible=true;controller.reset();blockFire=true;
                    if(frontend.screen()==Native::Screen::Teleport)frontend.open(Native::Screen::Hangar,&game->state());
                    else frontAction({Native::FrontEffect::Teleport});
                }
                return true;
            }
            if(binding==Binding::ToggleGrid){
                if(game->state().commandView){settings.showCommandGrid=!settings.showCommandGrid;game->showCommandGrid(settings.showCommandGrid);}
                else {settings.showGrid=!settings.showGrid;game->showGrid(settings.showGrid);}
                settings.save(configPath);return true;
            }
            if(binding==Binding::RadarDetail){
                if(game->state().trainingMission)game->setTrainingRadar(Allegiance::CycleRadarDetail(game->state().trainingRadar));
                else if(game->state().commandView || game->state().camera==Native::CameraMode::Cockpit){
                    int mode=hud.cycleRadar(game->state().commandView);
                    (game->state().commandView?settings.commandRadar:settings.cockpitRadar)=mode;settings.save(configPath);
                }
                return true;
            }
            if(binding==Binding::PaneContacts && game->state().commandView){menuVisible=true;frontend.open(Native::Screen::Orders,&game->state());return true;}
            if((binding==Binding::HudLarger || binding==Binding::HudSmaller) && game->state().commandView)return true;
            if(hud.binding(binding)){if(binding==Binding::SectorMap || binding==Binding::HudVisible){hud.cancelPointer();SDL_CaptureMouse(pointerCapture.active());}return true;}
            if(paused || help)return false;
            if(binding>=Binding::SwapTurret1 && binding<=Binding::SwapTurret4){game->swapTurret(int(binding)-int(Binding::SwapTurret1));return true;}
            if(binding>=Binding::PromoteTurret1 && binding<=Binding::PromoteTurret4){game->promoteTurret(int(binding)-int(Binding::PromoteTurret1));return true;}
            for(auto target:Native::targetBindings)if(target.binding==binding){game->targetKey(target.key);return true;}
            if(binding>=Binding::SwapWeapon1 && binding<=Binding::SwapDispenser){
                int index=int(binding)-int(Binding::SwapWeapon1);
                constexpr Native::PartKind kinds[]={Native::PartKind::Missile,Native::PartKind::Chaff,Native::PartKind::Shield,Native::PartKind::Cloak,Native::PartKind::Booster,Native::PartKind::Dispenser};
                game->swapCargo(index<4?Native::PartKind::Weapon:kinds[index-4],index<4?index:0);return true;
            }
            switch(binding) {
            case Binding::Chat:if(!modernUi)chatConsole.startChat(*game);else {menuVisible=true;frontend.open(Native::Screen::Chat,&game->state());}blockFire=true;break;
            case Binding::QuickChat:if(!modernUi)chatConsole.startQuick(*game);else {menuVisible=true;frontend.open(Native::Screen::QuickChat,&game->state());}blockFire=true;break;
            case Binding::UnitOrders:menuVisible=true;frontend.open(Native::Screen::Orders,&game->state());break;
            case Binding::Research:if(game->state().economy && game->state().developments){
                if(!modernUi)dismissPanes(ofTeam|ofExpandedTeam|ofGameState);
                if(menuVisible && frontend.screen()==Native::Screen::Research)frontAction({Native::FrontEffect::Resume});
                else {menuVisible=true;frontend.open(Native::Screen::Research,&game->state());}
            }break;
            case Binding::Intro:playMovie("intro_movie.avi");break;
            case Binding::TrackCommand:game->trackCommandView();break;
            case Binding::BoardTarget:if(game->state().parentId>=0)game->boardShip(game->state().parentId);else if(game->state().targetObjectType==Native::ContactShip)game->boardShip(game->state().targetId);break;
            case Binding::LeaveShip:game->leaveShip();break;
            case Binding::PilotOrders:menuVisible=true;frontend.open(Native::Screen::PilotOrders,&game->state());break;
            case Binding::AcceptOrder:game->acceptCommand();break;
            case Binding::ExecuteOrder:game->acceptCommand(true);break;
            case Binding::RejectOrder:game->rejectCommand();break;
            case Binding::ClearOrder:game->clearCommand();break;
            case Binding::TargetOrder:game->targetCommand();break;
            case Binding::Inventory:menuVisible=true;frontend.open(Native::Screen::Cargo,&game->state());break;
            case Binding::NextCargo:game->nextCargo();break;
            case Binding::DropCargo:game->dropCargo();break;
            case Binding::Reload:game->reload();break;
            case Binding::NextWeapon:game->cycleWeapon();break;
            case Binding::PreviousWeapon:game->cycleWeapon(true);break;
            case Binding::GroupFire:game->selectWeapon(-1);break;
            case Binding::Weapon1:case Binding::Weapon2:case Binding::Weapon3:case Binding::Weapon4:game->selectWeapon(int(binding)-int(Binding::Weapon1));break;
            case Binding::Autopilot:game->toggleAutopilot();break;
            case Binding::Cloak:game->toggleCloak();break;
            case Binding::Coast:game->toggleVectorLock();break;
            case Binding::Ripcord:game->ripcord();break;
            case Binding::Eject:game->eject();throttle=0;break;
            default:return false;
            }return true;
        };
        bool suppressGlobalText=false;
        auto globalKeyFilter=[&](const SDL_KeyboardEvent& event){
            // TrekInput's super filter records physical held state before a
            // popup/global command can consume the translated key message.
            keyboard.keyDown(event.scancode,event.mod);
            if(modernUi || frontend.capturingKey())return false;
            if(popupCapturesInput())return false;
            auto binding=keyboard.pressed(event.scancode,event.mod);if(!binding)return false;
            auto command=keyboard.command(event.scancode,event.mod);
            auto parent=frontend.consoleScreen().value_or(frontend.screen());
            bool lobby=parent==Native::Screen::RemoteLobby || parent==Native::Screen::LobbyPilots || parent==Native::Screen::Pilots;
            bool inGame=game && !game->state().networkLobby && !game->state().complete;
            Allegiance::GlobalKeyContext context{bool(movie),inGame,game && game->state().cinematic,frontend.hasMessageBox(),chatConsole.active(),bool(game) || lobby || parent==Native::Screen::Results,
                lobby || parent==Native::Screen::Results || inGame};
            auto disposition=Allegiance::FilterGlobalKey(command,context);
            if(disposition==Allegiance::GlobalKeyDisposition::Pass)return false;
            if(command==TK_MainMenu && !frontend.mainMenuInput())return false;
            if(disposition==Allegiance::GlobalKeyDisposition::Dispatch){
                if(command!=TK_QuickChatMenu || !menuVisible || !frontend.lobbyQuickChat())bindingAction(*binding,command);
            }
            // SDL text events are separate from key-down. A consumed printable
            // shortcut must not insert its character after opening/closing UI.
            suppressGlobalText=event.key>=SDLK_SPACE && event.key<SDLK_DELETE && !(event.mod&(SDL_KMOD_CTRL|SDL_KMOD_ALT));
            return true;
        };
        if(trainingIntro){Native::SessionOptions options;options.trainingMission=trainingMission;startSlides(options,false);}
        std::optional<Native::Settings> slidesMenuKeyboard,classicKeyboardSettings;
        if(trainingSlidesMenuSmoke){
            slidesMenuKeyboard=settings;
            std::erase_if(settings.keyAliases,[](const auto& alias){return alias.binding==Native::Binding::Pause || alias.binding==Native::Binding::Help;});
            settings.keys[size_t(Native::Binding::Pause)]=SDL_SCANCODE_F12;
            settings.modifiers[size_t(Native::Binding::Pause)]=Native::KeyControl;
            settings.keyCommands[size_t(Native::Binding::Pause)]=TK_MainMenu;
            settings.keys[size_t(Native::Binding::Help)]=SDL_SCANCODE_F11;
            settings.modifiers[size_t(Native::Binding::Help)]=Native::KeyControl;
            settings.keyCommands[size_t(Native::Binding::Help)]=TK_Help;
            keyboard.configure(settings);
        }
        Native::CommandGesture commandGesture;glm::mat4 commandProjection{1};Native::CameraState commandCamera;bool commandProjectionValid{};
        auto pilotMouseControl=[&]{return !menuPointer() && !(playerLayer() && (playerPane.active() || gameStatePane.active())) && !chatConsole.keyboard() && !paused && !help && !mousePointerMode() && !movie && !slides;};
        auto scenePointerInput=[&]{return game && Native::scenePointerEnabled(game->state(),!modernUi,!pilotMouseControl()) && !movie && !slides && !paused && !help && !popupCapturesPointer() && chatConsole.mode()!=Allegiance::ComposeState::Shell && (!menuVisible || investmentFlight());};
        Native::MouseBindings::Down mousePulses;
        auto syncCombatView=[&]{
            if(modernUi){viewTransitions.reset();return;}
            bool available=game && (!network || network->ready());
            bool retired=available && frontend.releaseDockedView(game->state());
            if(retired){
                const auto& state=game->state();menuVisible=state.docked && !state.commandView && !state.cinematic;
                if(!menuVisible)frontend.closeConsoleToFlight();
                paused=help=cursor=false;blockFire=true;controller.reset();
            }
            auto change=viewTransitions.observe(frontend,available?&game->state():nullptr,menuVisible);
            if(retired){change.changed=true;change.switchView|=!game->state().cinematic;}
            if(change.changed){
                Native::applyUiViewChange(change,playerPane,gameStatePane,pointerCapture,chatConsole,commandGesture,mouse,mousePulses,SDL_GetMouseState(nullptr,nullptr),game.get());
                SDL_CaptureMouse(false);
                commandProjectionValid=false;cursor=false;blockFire=true;
            }
        };
        bool previousCinematic=false;
        uint64_t previous=Native::monotonicNs();
        std::vector<Native::DrawItem> draws;draws.reserve(1024);
        unsigned surfaceMask=0,cargoStage=0;
        glm::vec3 teleportInvestmentHeading{};
        int dockingStage=0;unsigned dockedFrame=0;int teleportHome=-1,teleportHomeSector=-1,teleportDestination=-1,teleportIdentity=-1,teleportTotal=0;
        int trainingTerminalStage=0;unsigned trainingTerminalFrame=0;
        uint64_t chatLayoutUpdates=0;size_t chatHistoryOffset=0;double chatPauseClock=0,chatPauseSimulation=0;bool chatWallChecked=false;
        float chatSmokeThrottle{};int chatSmokePlayer{},chatSmokeOther{-1};
        glm::vec3 chatSmokeEye{};
        int mouseSmokeAmmo=0,mouseSmokeMissiles=0,mouseSmokeTarget=-1;float mouseSmokeCommandDistance=0;
        bool mouseSmokeGrid{};
        using ScaleClock=std::chrono::steady_clock;
        auto scaleMilliseconds=[](auto begin,auto end){return std::chrono::duration<double,std::milli>(end-begin).count();};
        std::ofstream scaleOutput;if(!scaleFile.empty()){scaleOutput.open(scaleFile);scaleOutput<<"frame,step_ms,prepare_ms,render_ms,ships,contacts,draw_items,particles,draws,triangles,misses,eye_x,eye_y,eye_z,view_fov,pose_sampled_ns,pose_target_ns,presentation_lead_ms,local_fraction_ms,remote_time_s\n";}
        startupWorkers.restore();
        Native::Affinity::pinCurrentThread(Native::Affinity::Lane::Render);
        while(running && (!frameLimit || frames<frameLimit)) {
            bool browse=(frontend.screen()==Native::Screen::Games || frontend.screen()==Native::Screen::Online) && !network && !game;
            if(browse){
                if(!browser){if(!contentDigest)contentDigest=Native::Net::contentDigest(artwork);browser=std::make_unique<Native::Net::ServerBrowser>(*contentDigest);for(const auto& address:browseAddresses)browser->add(address);}
                browserActive=true;browser->poll(frontend.screen()==Native::Screen::Online);if(browserRevision!=browser->revision()){browserRevision=browser->revision();frontend.gameListings(browser->listings());frontend.gameAuthorities(browser->authorities());}
                if(auto result=browser->takePlayerSearch())frontend.foundPlayer(std::move(result->game));
                if(auto created=browser->takeCreation()){
                    frontend.connectionFinished();
                    if(!created->error.empty())frontend.error(created->error);
                    else try{joinServer(created->address,0,created->token);}catch(const std::exception& e){frontend.error(e.what());}
                }
            }else if(browserActive){browser->stop();browserActive=false;}
            if(network){
                network->poll();
                syncCombatView();
                if(network->closed()){auto reason=network->error();network.reset();audio.stopAll();game.reset();frontend.setPlaying(false);frontend.open(connectionReturn);frontend.error(reason);menuVisible=true;networkInitialized=false;}
                else if(network->ready()){
                    enterWing();
                    if(displayedMatch!=network->match()){displayedMatch=network->match();networkInitialized=false;postgameShown=false;frontend.networkSession(game->sessionOptions(),network->serverName());}
                    if(!networkInitialized){frontend.connectionFinished();networkInitialized=true;game->rememberEditorLoadout(game->currentLoadout(),true);game->particleQuality(settings.particles);game->showGrid(settings.showGrid);game->showCommandGrid(settings.showCommandGrid);game->visualPreferences(settings.visual);game->chatPreferences(settings.chat);game->quickChats(quickChat);game->interfacePrompts([&](std::string_view text){return Native::interfacePrompt(text,settings,&keyboard);});audio.preload(game->soundIds());hud.resetMessages();consolePane.reset();hud.preloadIcons(game->radarIcons());hud.preloadInventory(game->inventoryScripts());hud.preload(game->contacts());frontend.setPlaying(true);menuVisible=game->state().docked;bool results=game->state().networkLobby && game->state().endgame.frozen;postgameShown=results;frontend.open(results?Native::Screen::Results:game->state().networkLobby?Native::Screen::RemoteLobby:menuVisible?Native::Screen::Hangar:Native::Screen::Pause,&game->state());renderer.flushUploads();}
                    else if(frontend.screen()==Native::Screen::RemoteLobby && !game->state().networkLobby){menuVisible=true;frontend.open(Native::Screen::Hangar,&game->state());}
                    if(networkSmoke && game->state().networkLobby){
                        if(!game->state().networkReady){Native::Net::Action ready{};ready.op=Native::Net::Operation::Ready;ready.args[0]=1;network->action(ready);}
                        else if(game->state().networkHost && game->state().networkCountdown<0){Native::Net::Action start{};start.op=Native::Net::Operation::Start;network->action(start);}
                    }
                    if(networkSmoke && !networkLaunched && !game->state().networkLobby && game->state().docked){game->launch();networkLaunched=true;throttle=.6f;}
                    if(game->state().complete && !postgameShown){menuVisible=true;postgameShown=true;frontend.open(Native::Screen::Results,&game->state());}frontend.sync(game->state());
                    if(auto action=network->takeDockRefitStarted())frontend.purchasing(false,action);
                    uint64_t rejectedModalAction{};auto rejection=network->takeRejection(&rejectedModalAction);
                    if(!rejection.empty() || rejectedModalAction){
                        bool silentCurrent=frontend.classicInterface() && rejection.empty() && rejectedModalAction && frontend.pendingAction(rejectedModalAction);
                        if(frontend.actionRejected(rejectedModalAction,std::move(rejection),!menuVisible))menuVisible=true;
                        else if(silentCurrent && !game->state().docked){menuVisible=false;blockFire=true;}
                    }
                    uint64_t purchaseAction{};
                    while(auto purchase=network->takePurchase(&purchaseAction))finishPurchase(*purchase,purchaseAction);
                    while(auto transfer=network->takeTransfer())frontend.transferFinished(*transfer);
                    if(auto report=network->takeConnections()){frontend.connectionInfo(*report,game->state());menuVisible=true;blockFire=true;}
                }
            }

            syncCombatView();
            bool acquired=renderer.beginFrame();
            if(startupMovieSmoke && acquired) {
                if(frames==60 || frames==120) {
                    const char* expected=frames==60?"intro_microsoft.avi":"intro_movie.avi";
                    if(!movie || playingMovieName!=expected)throw std::runtime_error("Startup movie order did not follow preferences");
                    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_SPACE;SDL_PushEvent(&event);
                }
                if(frames==150) {
                    auto persisted=Native::Settings::load(configPath);
                    if(movie || game || frontend.screen()!=Native::Screen::Home || !menuVisible || persisted.startupIntro || !persisted.startupCredits)
                        throw std::runtime_error("Startup movie skip did not restore Home and one-shot preferences");
                    fprintf(stderr,"Startup movie smoke: credits, introduction, both skips and persisted one-shot preference passed\n");
                }
            }
            if(inputRoutingSmoke && acquired){
                if(game || network || modernUi)throw std::runtime_error("Classic SDL input routing diagnostic must remain UI-only");
                inputRoutingCheck.step(frames,frontend,settings,controller,configPath,window.get(),renderer.size());
            }
            if(deviceSmoke && acquired){
                auto key=[&](SDL_Keycode value,SDL_Scancode scan){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=value;e.key.scancode=scan;SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto capture=[&](const char* stage){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-"+stage+".ppm")).string();}};
                if(frames==30){frontend.open(Native::Screen::Settings);menuVisible=true;}
                if(frames==60)key(SDLK_D,SDL_SCANCODE_D);
                if(frames==90){auto info=renderer.deviceInfo();for(const auto& line:info)if(std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[&](const auto& item){return item.label==line;}))throw std::runtime_error("Device SDL menu loses actual Vulkan state");capture("device");}
                if(frames==91)key(SDLK_U,SDL_SCANCODE_U);
                if(frames==120){
                    auto saved=Native::Settings::load(configPath);if(std::abs(settings.gamma-1.13f*1.01f)>1e-6 || saved.gamma!=settings.gamma)throw std::runtime_error("Actual brightness accelerator did not apply/persist original multiplier");
                    auto info=renderer.deviceInfo();if(std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[&](const auto& item){return item.label==info.back();}))throw std::runtime_error("Brightness SDL action leaves the original device state one change behind");capture("bright");
                }
                if(frames==121)key(SDLK_D,SDL_SCANCODE_D);
                if(frames==150){if(std::abs(settings.gamma-1.13f)>1e-6)throw std::runtime_error("Actual brightness Down did not restore startup gamma");key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);key(SDLK_G,SDL_SCANCODE_G);key(SDLK_E,SDL_SCANCODE_E);}
                if(frames==180)key(SDLK_C,SDL_SCANCODE_C);
                if(frames==210){if(!settings.controller.autoCenter || !Native::Settings::load(configPath).controller.autoCenter || std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.label=="Auto Center On";}))throw std::runtime_error("Actual Auto Center accelerator loses its preference or label");capture("feedback");}
                if(frames==211){key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);}
                if(frames==240){if(frontend.screen()!=Native::Screen::Home || frontend.keyboardPopup())throw std::runtime_error("Original device/feedback menus lose their Home parent");frontend.open(Native::Screen::Settings);key(SDLK_O,SDL_SCANCODE_O);key(SDLK_A,SDL_SCANCODE_A);}
                if(frames==270)key(SDLK_A,SDL_SCANCODE_A);
                if(frames==300){auto options=renderer.antialiasingOptions();int selected=options.size()>1?options[1]:0;if(settings.antialiasing!=selected || Native::Settings::load(configPath).antialiasing!=selected)throw std::runtime_error("Actual antialiasing accelerator does not apply supported sample counts");capture("quality");}
                if(frames>=301 && frames<=304)key(SDLK_X,SDL_SCANCODE_X);
                if(frames==330){if(settings.maxTextureSize!=0 || Native::Settings::load(configPath).maxTextureSize!=0)throw std::runtime_error("Actual texture limit accelerator does not cycle original levels");key(SDLK_X,SDL_SCANCODE_X);}
                if(frames==360){if(settings.maxTextureSize!=1 || Native::Settings::load(configPath).maxTextureSize!=1 || std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.label=="Max Texture Size (512) ";}))throw std::runtime_error("Actual texture limit preference or label does not refresh");capture("texture");}
                if(frames==361){auto options=renderer.antialiasingOptions();for(size_t i=1;i<options.size();++i)key(SDLK_A,SDL_SCANCODE_A);}
                if(frames==390){if(settings.antialiasing)throw std::runtime_error("Actual antialiasing cycle does not return to No AA");key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);}
                if(frames==420){if(frontend.screen()!=Native::Screen::Home || frontend.keyboardPopup())throw std::runtime_error("Original quality menus lose their Home parent");deviceSmokePassed=true;fprintf(stderr,"Original device SDL: actual device/brightness, Auto Center, supported AA cycle, original texture limit cycle, persistence and popup parent restoration passed\n");}
            }
            if(dockSmoke && acquired) {
                if(frames==60){frontAction({Native::FrontEffect::Launch});dockingStage=1;thirdPerson=true;}
                if(frames==330){frontAction({Native::FrontEffect::ReturnToBase});dockingStage=2;}
                if(dockingStage==2 && game->state().docked) {
                    if(!menuVisible || frontend.screen()!=Native::Screen::Hangar)throw std::runtime_error("Docking did not open hangar");
                    dockingStage=3;dockedFrame=frames;fprintf(stderr,"Docking smoke: natural docking opened %s hangar at frame %u\n",game->state().station.c_str(),frames);
                }
                if(dockingStage==3 && frames==dockedFrame+60)frontAction({Native::FrontEffect::ChangeHull,1});
                if(dockingStage==3 && frames==dockedFrame+120) {
                    frontAction({Native::FrontEffect::Launch});if(game->state().docked || menuVisible)throw std::runtime_error("Docked refit/relaunch failed");dockingStage=4;
                }
                if(dockingStage==4 && frames==dockedFrame+250)frameLimit=frames+1;
            }
            if(trainingSlidesMenuSmoke && acquired){
                if(game || modernUi || network)throw std::runtime_error("Slideshow menu smoke requires the standalone classic UI without a game");
                auto key=[](SDL_Keycode code,SDL_Keymod mod=SDL_KMOD_NONE){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;e.key.scancode=SDL_GetScancodeFromKey(code,nullptr);e.key.mod=mod;SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto menuKey=[&]{key(SDLK_F12,SDL_KMOD_CTRL);};
                auto checkSlide=[&](size_t index){if(!slides || slides->index()!=index || menuVisible || frontend.keyboardPopup() || frontend.screen()!=Native::Screen::Slideshow)throw std::runtime_error("Slideshow menu return/advance fails at frame "+std::to_string(frames)+": slide "+std::to_string(slides?int(slides->index()):-1)+", screen "+std::to_string(int(frontend.screen()))+", visible "+std::to_string(menuVisible)+", popup "+std::to_string(frontend.keyboardPopup()));};
                if(frames==10){Native::SessionOptions options;options.trainingMission=3;startSlides(options,false);if(slides->all().size()<3)throw std::runtime_error("Slideshow menu fixture needs a multi-slide introduction");}
                if(frames==12 || frames==14 || frames==40)key(SDLK_F11,SDL_KMOD_CTRL);
                if(frames==13 && (!slides || slides->index()!=0 || menuVisible || frontend.screen()!=Native::Screen::Slideshow || !frontend.helpOn()))throw std::runtime_error("Mapped Help advances/replaces a slide or requires a game");
                if(frames==15)checkSlide(0);
                if(frames==20 || frames==50 || frames==100 || frames==130 || frames==170)menuKey();
                if(frames==21 || frames==101){
                    if(!slides || slides->index()!=(frames==21?0u:2u) || !menuVisible || frontend.screen()!=Native::Screen::Pause ||
                       std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.label=="Mission";}))throw std::runtime_error("Mapped slideshow main-menu key skips a slide or loses its original Mission entry");
                }
                if(frames==30)key(SDLK_H);
                if(frames==31 && (!slides || slides->index()!=0 || menuVisible || frontend.screen()!=Native::Screen::Slideshow || !frontend.helpOn()))throw std::runtime_error("Slideshow menu Help replaces training or retains the popup");
                if(frames==70 || frames==80 || frames==120)key(SDLK_ESCAPE);
                if(frames==41 || frames==71)checkSlide(0);
                if(frames==60)key(SDLK_G);
                if(frames==65)key(SDLK_C);
                if(frames==66 && (!slides || slides->index()!=0 || frontend.screen()!=Native::Screen::Controls))throw std::runtime_error("Slideshow Game Options cannot open original key mapping");
                if(frames==81)checkSlide(1); // Unmapped Escape remains an ordinary slideshow key.
                if(frames==90){SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_MIDDLE;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);}
                if(frames==91 || frames==121)checkSlide(2);
                if(frames==110 || frames==140 || frames==180)key(SDLK_Q);
                if(frames==111 && (!frontend.hasMessageBox() || slides->index()!=2))throw std::runtime_error("Training quit prompt advances the slideshow");
                if(frames==150 || frames==190)key(SDLK_RETURN);
                if(frames==151 || frames==191){if(slides || trainingStartAt>=0 || !menuVisible || frontend.screen()!=Native::Screen::Training || frontend.hasMessageBox())throw std::runtime_error("Confirmed slideshow quit fails to stop narration and return to original lesson selection");}
                if(frames==160){Native::SessionOptions options;options.trainingMission=1;startSlides(options,true);}
                if(frames==200){Native::SessionOptions options;options.trainingMission=1;startSlides(options,true);}
                if(frames==205 || frames==220)menuKey();
                if(frames==210)slides->skip(); // Completion while a foreground popup is open.
                if(frames==211){if(slides || !menuVisible || frontend.screen()!=Native::Screen::Training || frontend.keyboardPopup())throw std::runtime_error("Finished debrief retains a stale foreground popup");}
                if(frames==221){if(std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.label=="Mission";}))throw std::runtime_error("Completed slideshow leaves a stale Mission entry");}
                if(frames==230){settings=*slidesMenuKeyboard;keyboard.configure(settings);}
                if(frames==231){Native::SessionOptions options;options.trainingMission=3;startSlides(options,false);}
                if(frames==232 || frames==240)key(SDLK_F1);
                if(frames==233 && (!frontend.helpOn() || menuVisible || slides->index()!=0))throw std::runtime_error("Help replaces ordinary slideshow keyboard focus");
                if(frames==234)key(SDLK_RIGHT);
                if(frames==235 && (!frontend.helpOn() || menuVisible || slides->index()!=1))throw std::runtime_error("Help blocks an ordinary slideshow advance key");
                if(frames==250)frontAction({Native::FrontEffect::Leave});
                if(frames==260){trainingSlidesMenuSmokePassed=true;fprintf(stderr,"Original slideshow SDL: remapped Ctrl+F12 menu, independent Help/ordinary-key advance and mapping return, Escape/middle-click advance, pre/post quit/cancel, stopped narration, foreground completion and lesson selection passed without a game\n");}
            }
            if(classicKeyboardSmoke && acquired){
                if(game || modernUi || network)throw std::runtime_error("Classic keyboard smoke requires the standalone classic UI without a game");
                auto key=[](SDL_Keycode code,SDL_Keymod mod=SDL_KMOD_NONE){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;e.key.scancode=SDL_GetScancodeFromKey(code,nullptr);e.key.mod=mod;SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto shortcutText=[](){SDL_Event e{};e.type=SDL_EVENT_TEXT_INPUT;e.text.text="h";SDL_PushEvent(&e);};
                auto menuKey=[&]{auto i=size_t(Native::Binding::Pause);auto bits=settings.modifiers[i];auto mod=SDL_Keymod(((bits&Native::KeyShift)?SDL_KMOD_SHIFT:0)|((bits&Native::KeyControl)?SDL_KMOD_CTRL:0)|((bits&Native::KeyAlt)?SDL_KMOD_ALT:0));SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=settings.keys[i];e.key.key=SDL_GetKeyFromScancode(e.key.scancode,mod,true);e.key.mod=mod;SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto navigation=[&]{for(auto code:{SDLK_TAB,SDLK_UP,SDLK_DOWN,SDLK_SPACE,SDLK_RETURN})key(code);};
                auto item=[&](Native::MenuOp op,int id=-1){const auto& items=frontend.view().items;auto found=std::find_if(items.begin(),items.end(),[&](const auto& i){return i.action.op==op && (id<0 || i.action.delta==id);});if(found==items.end())throw std::runtime_error("Classic keyboard SDL fixture control missing");return size_t(found-items.begin());};
                if(frames==10){
                    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;frontend.networkSession(options);frontend.setPlaying(true);
                    Native::GameState state;state.networked=state.networkLobby=state.networkHost=state.commander=state.docked=true;state.playerSide=0;state.shipId=40;state.networkHostPilot=40;
                    state.networkPilots={{40,0,"Commander",true,true,true,true,false,Native::noTeamRequest,1},{41,0,"Private recipient",true,false,false,false,false,Native::noTeamRequest,1}};
                    frontend.open(Native::Screen::RemoteLobby,&state);frontend.activate(item(Native::MenuOp::RosterSelect,41));frontend.activate(item(Native::MenuOp::ComposeChat));frontend.text("SDL private draft");
                }
                if(frames==30 || frames==120)menuKey();
                if(frames==31 || frames==121)if(frontend.screen()!=Native::Screen::Pause || !frontend.keyboardPopup() || frontend.chatText()!="SDL private draft")throw std::runtime_error("One mapped main-menu key fails above the original lobby/results EditPane");
                if(frames==40 || frames==100 || frames==130)key(SDLK_ESCAPE);
                if(frames==70)key(SDLK_F1);
                if(frames==41 || frames==71 || frames==101 || frames==131){
                    auto parent=frames==131?Native::Screen::Results:Native::Screen::RemoteLobby;
                    if(frontend.screen()!=parent || !frontend.wantsText() || frontend.chatText()!="SDL private draft")throw std::runtime_error("Original popup return at frame "+std::to_string(frames)+" has screen "+std::to_string(int(frontend.screen()))+", text focus "+std::to_string(frontend.wantsText())+", draft '"+frontend.chatText()+"'");
                }
                if(frames==50)frontend.open(Native::Screen::Help);
                if(frames==60)shortcutText();
                if(frames==61 && (!frontend.helpOn() || frontend.screen()!=Native::Screen::RemoteLobby || !frontend.wantsText() || frontend.chatText()!="SDL private drafth"))throw std::runtime_error("Independent Help blocks ordinary lobby text input");
                if(frames==62)key(SDLK_BACKSPACE);
                if(frames==90 || frames==150 || frames==170)navigation();
                if(frames==80)frontend.open(Native::Screen::Credits);
                if(frames==91 && frontend.screen()!=Native::Screen::Credits)throw std::runtime_error("Keyboard navigation dismisses the original credits");
                if(frames==110){Native::GameState state;state.networked=state.networkLobby=state.docked=state.complete=true;state.playerSide=0;state.endgame.frozen=true;frontend.open(Native::Screen::Results,&state);}
                if(frames==140){frontend.setPlaying(false);frontend.open(Native::Screen::Home);}
                if(frames==151 && frontend.screen()!=Native::Screen::Home)throw std::runtime_error("Classic main menu receives an unauthored keyboard activation");
                if(frames==160)frontend.open(Native::Screen::Training);
                if(frames==171 && frontend.screen()!=Native::Screen::Training)throw std::runtime_error("Classic training receives an unauthored keyboard activation");
                if(frames==180){
                    classicKeyboardSettings=settings;settings.keys.fill(SDL_SCANCODE_UNKNOWN);settings.modifiers.fill(0);settings.keyCommands.fill(TK_NoKeyMapping);settings.keyAliases.clear();
                    settings.keys[size_t(Native::Binding::Help)]=SDL_SCANCODE_F12;settings.modifiers[size_t(Native::Binding::Help)]=Native::KeyControl;settings.keyCommands[size_t(Native::Binding::Help)]=TK_Help;
                    settings.keys[size_t(Native::Binding::Pause)]=SDL_SCANCODE_F10;settings.modifiers[size_t(Native::Binding::Pause)]=Native::KeyControl;settings.keyCommands[size_t(Native::Binding::Pause)]=TK_MainMenu;
                    settings.keys[size_t(Native::Binding::RollRight)]=SDL_SCANCODE_F12;settings.modifiers[size_t(Native::Binding::RollRight)]=Native::KeyAny;settings.keyCommands[size_t(Native::Binding::RollRight)]=TK_RollRight;
                    settings.keyAliases.push_back({SDL_SCANCODE_F11,Native::KeyShift,Native::Binding::Help,TK_Help});settings.keyAliases.push_back({SDL_GetScancodeFromKey(SDLK_H,nullptr),0,Native::Binding::Help,TK_Help});keyboard.configure(settings);
                }
                if(frames==181 || frames==200)key(SDLK_F1);
                if(frames==182 && frontend.screen()!=Native::Screen::Training)throw std::runtime_error("Unbound F1 still opens Help from a menu");
                if(frames==190 || frames==192){SDL_Event e{};e.type=frames==190?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.key=SDLK_F12;e.key.scancode=SDL_SCANCODE_F12;e.key.mod=SDL_KMOD_CTRL;SDL_PushEvent(&e);}
                if(frames==191 && !keyboard.sample()[size_t(Native::Binding::RollRight)])throw std::runtime_error("Consumed global chord loses the original simultaneous continuous bind");
                if(frames==193 && keyboard.sample()[size_t(Native::Binding::RollRight)])throw std::runtime_error("Consumed global chord loses its physical key release");
                if(frames==250 || frames==280)key(SDLK_F12,SDL_KMOD_CTRL);
                if((frames==191 || frames==201 || frames==251 || frames==271) && (!frontend.helpOn() || frontend.screen()!=(frames<220?Native::Screen::Training:Native::Screen::RemoteLobby)))throw std::runtime_error("Translated Help changes its underlying screen");
                if(frames==210)key(SDLK_F11,SDL_KMOD_SHIFT);
                if(frames==211 && (frontend.helpOn() || frontend.screen()!=Native::Screen::Training))throw std::runtime_error("Help alias fails to retain training selection");
                if(frames==220){
                    Native::GameState state;state.networked=state.networkLobby=state.networkHost=state.commander=state.docked=true;state.playerSide=0;state.shipId=40;state.networkHostPilot=40;
                    state.networkPilots={{40,0,"Commander",true,true,true,true,false,false,1},{41,0,"Private recipient",true,false,false,false,false,false,1}};
                    frontend.setPlaying(true);frontend.open(Native::Screen::RemoteLobby,&state);frontend.activate(item(Native::MenuOp::RosterSelect,41));frontend.activate(item(Native::MenuOp::ComposeChat));
                    while(!frontend.chatText().empty())frontend.key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);frontend.text("SDL private draft");
                }
                if(frames==230 || frames==240 || frames==301 || frames==310){key(SDLK_H);shortcutText();}
                if(frames==231 && (!frontend.helpOn() || frontend.screen()!=Native::Screen::RemoteLobby))throw std::runtime_error("Printable Help alias does not run before the lobby EditPane");
                if((frames==241 || frames==281) && (frontend.screen()!=Native::Screen::RemoteLobby || !frontend.wantsText() || frontend.chatText()!="SDL private draft"))throw std::runtime_error("Help toggle leaks shortcut text or loses a private draft");
                if(frames==260)key(SDLK_F10,SDL_KMOD_CTRL);
                if(frames==261 && frontend.screen()!=Native::Screen::Pause)throw std::runtime_error("Independent Help blocks the mapped global main menu");
                if(frames==270)key(SDLK_ESCAPE);
                if(frames==290)frontend.open(Native::Screen::Controls);
                if(frames==291)key(SDLK_F12,SDL_KMOD_CTRL);
                if(frames==292 && frontend.screen()!=Native::Screen::Controls)throw std::runtime_error("Mapped Help bypasses key-mapping popup ownership");
                if(frames==300){frontend.setPlaying(false);frontend.open(Native::Screen::Home);}
                if(frames==302 && (!frontend.helpOn() || frontend.screen()!=Native::Screen::Home))throw std::runtime_error("Mapped Help still requires an instantiated game");
                if(frames==311 && (frontend.helpOn() || frontend.screen()!=Native::Screen::Home))throw std::runtime_error("Mapped Help loses the Home parent");
                if(frames==330){settings=*classicKeyboardSettings;keyboard.configure(settings);}
                auto helpButton=[&](bool outsideRelease=false,bool release=true){
                    auto size=renderer.size();const auto& pane=frontend.helpView(Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                    auto close=std::find_if(pane.items.begin(),pane.items.end(),[](const auto& i){return i.action.op==Native::MenuOp::SecondaryClose;});if(close==pane.items.end())throw std::runtime_error("SDL Help close fixture missing");
                    auto p=Native::originalMenuLayout(size,pane.classicScale,pane.canvas).pixel(glm::vec2(close->bounds)+glm::vec2(close->bounds.z,close->bounds.w)*.5f+Native::menuSlideOffset(pane,size,Native::uiTime()));
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x*w/size.x;e.button.y=p.y*h/size.y;SDL_PushEvent(&e);
                    if(release){e.type=SDL_EVENT_MOUSE_BUTTON_UP;if(outsideRelease)e.button.x=e.button.y=-1;SDL_PushEvent(&e);}
                };
                if(frames==335 || frames==390)frontend.showHelp(true);
                if(frames==380)helpButton();
                if(frames==381 && (frontend.helpOn() || frontend.screen()!=Native::Screen::Home || frontend.helpPointerCaptured()))throw std::runtime_error("SDL Help close does not operate independently of its underlying screen");
                if(frames==420)helpButton(true);
                if(frames==421 && (!frontend.helpOn() || frontend.helpPointerCaptured()))throw std::runtime_error("SDL Help activates a captured button after release outside");
                if(frames==440)helpButton(false,false);
                if(frames==441 && !frontend.helpPointerCaptured())throw std::runtime_error("SDL Help does not retain its own button capture");
                if(frames==442){SDL_Event e{};e.type=SDL_EVENT_WINDOW_FOCUS_LOST;SDL_PushEvent(&e);}
                if(frames==443 && frontend.helpPointerCaptured())throw std::runtime_error("SDL Help retains a stale capture after focus loss");
                if(frames==460)key(SDLK_F1);
                if(frames==470){if(frontend.helpOn())throw std::runtime_error("Default mapped Help does not hide the independent pane");classicKeyboardSmokePassed=true;fprintf(stderr,"Classic keyboard SDL: lobby/results main menu, nonmodal Help text/focus, original no-op button keys, default/remapped/aliased Help, unbound F1, printable-shortcut suppression, popup/mapping priority, animated button hit/capture/outside release and focus cancellation passed without a game\n");}
            }
            if((frontSmoke || gamesSmoke || refitSmoke || teamSettingsSmoke || lobbyChatSmoke || lobbySmoke || cargoSmoke || loadoutSmoke || teleportSmoke || trainingEntrySmoke) && acquired) {
                std::function<void(Native::MenuOp,int)> routedClick;
                auto clickMenu=[&](Native::MenuOp op,int value=0) {
                    if(frontend.view().original && frontend.screen()==Native::Screen::Scenarios && op==Native::MenuOp::Scenario){
                        auto item=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[&](auto& item){return item.label==Native::scenarios.at(value).name && item.enabled;});
                        if(item==frontend.view().items.end())throw std::runtime_error("Local scenario popup is missing an entry");frontAction(frontend.activate(size_t(item-frontend.view().items.begin())));return;
                    }
                    if(frontend.view().original && (frontend.screen()==Native::Screen::Hangar || frontend.screen()==Native::Screen::ShipLoadout)){
                        if(op==Native::MenuOp::Loadouts){frontAction({Native::FrontEffect::Loadouts});return;}
                        if(op==Native::MenuOp::Navigate && value==int(Native::Screen::Cargo)){frontend.open(Native::Screen::Cargo,&game->state());return;}
                        if(op==Native::MenuOp::HangarHull || op==Native::MenuOp::EquipmentSlot){
                            if(frontend.screen()==Native::Screen::Hangar)routedClick(Native::MenuOp::Navigate,int(Native::Screen::ShipLoadout));
                            if(op==Native::MenuOp::HangarHull){routedClick(Native::MenuOp::LoadoutPopup,1);routedClick(Native::MenuOp::PreviewHull,game->state().economy?value:Native::hullIds.at(value));routedClick(Native::MenuOp::CommitLoadout,0);}
                            else {auto& slots=frontend.stagedLoadout().slots;auto it=std::find_if(slots.begin(),slots.end(),[&](auto& s){return s.mount==value && (value<0 || s.kind==Native::PartKind::Weapon);});if(it==slots.end())throw std::runtime_error("Frontend smoke: missing original equipment slot");routedClick(Native::MenuOp::PreviewSlot,int(it-slots.begin()));}
                            return;
                        }
                        if(op==Native::MenuOp::EquipPart){routedClick(Native::MenuOp::PreviewPart,value);routedClick(Native::MenuOp::CommitLoadout,0);return;}
                        if(op==Native::MenuOp::PreviewHull || op==Native::MenuOp::PreviewPart){
                            auto visible=[&]{return std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[&](auto& item){return item.enabled && item.action.op==op && item.action.value==value;});};
                            size_t count=op==Native::MenuOp::PreviewHull?frontend.stagedLoadout().hulls.size():game->loadoutPartChoices(frontend.partKind(),frontend.partMount(),frontend.stagedLoadout().loadout.hull).size()+1;
                            auto scan=[&]{for(size_t i=0;i<count+1 && !visible();++i)frontAction(frontend.key(SDLK_DOWN,SDL_SCANCODE_DOWN));};
                            scan();
                            if(!visible() && op==Native::MenuOp::PreviewPart){
                                auto firstSelected=[&]{return std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[](auto& item){return item.popupLevel==0 && item.popupRow==0 && item.selected;});};
                                for(size_t i=0;i<count+1 && !firstSelected();++i)frontAction(frontend.key(SDLK_UP,SDL_SCANCODE_UP));
                                frontAction(frontend.key(SDLK_RIGHT,SDL_SCANCODE_RIGHT));scan();
                            }
                        }
                    }
                    for(auto& item:frontend.view().items)if(item.action.op==op && item.action.value==value) {
                        auto size=renderer.size();auto layout=frontend.view().original?Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas):Native::menuLayout(size,settings.uiScale);
                        auto local=glm::vec2(item.bounds)+glm::vec2(item.bounds.z,item.bounds.w)*.5f;
                        if(frontend.view().slide && frontend.view().slide->movesItem(size_t(&item-frontend.view().items.data())))local+=Native::menuSlideOffset(frontend.view(),size,Native::uiTime());
                        auto p=layout.pixel(local);
                        if(auto hit=Native::menuHit(frontend.view(),size,settings.uiScale,p)){
                            if(teamSettingsSmoke || gamesSmoke || refitSmoke || lobbyChatSmoke || trainingEntrySmoke || (loadoutSmoke && !modernUi) || (lobbySmoke && !modernUi)){
                                int w,h;SDL_GetWindowSize(window.get(),&w,&h);p*=glm::vec2(w,h)/glm::vec2(size);
                                SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;event.button.clicks=1;event.button.x=p.x;event.button.y=p.y;SDL_PushEvent(&event);event.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&event);
                            }else {action({Native::HudCommand::Menu,int(*hit)});if(!lobbySmoke && frontend.screen()==Native::Screen::Countdown)frontAction(frontend.tick(61));}
                            return;
                        }
                        if(!item.enabled)return; // a disabled hit must remain inert
                        throw std::runtime_error("Frontend smoke: menu hit test failed op="+std::to_string(int(op))+" frame="+std::to_string(frames)+" control="+item.control);
                    }
                    throw std::runtime_error("Frontend smoke: missing menu control op="+std::to_string(int(op))+" value="+std::to_string(value)+" screen="+std::to_string(int(frontend.screen()))+" frame="+std::to_string(frames));
                };
                routedClick=clickMenu;
                if(trainingEntrySmoke){
                    auto key=[](SDL_Keycode value){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=value;e.key.scancode=SDL_GetScancodeFromKey(value,nullptr);SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                    auto capture=[&](const char* state){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-"+state+".ppm")).string();}};
                    if(frames==30){if(!frontend.hasMessageBox() || frontend.screen()!=Native::Screen::Home || std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::TrainingDontAskAgain;}))throw std::runtime_error("Original startup suggestion/checkbox missing");capture("suggestion");}
                    if(frames==60)clickMenu(Native::MenuOp::TrainingDontAskAgain);
                    if(frames==70){if(std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::TrainingDontAskAgain && i.selected;}))throw std::runtime_error("Actual don't-ask checkbox click is not checked");capture("checked");}
                    if(frames==75)key(SDLK_ESCAPE);
                    if(frames==90){if(frontend.hasMessageBox() || settings.showStartupTrainingSuggestion || Native::Settings::load(configPath).showStartupTrainingSuggestion)throw std::runtime_error("Cancel loses the original don't-ask preference");frontend.startupTrainingSuggestion();if(frontend.hasMessageBox())throw std::runtime_error("Suggestion repeats in the same execution");}
                    if(frames==105)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Training));
                    if(frames==135){if(!frontend.hasMessageBox() || frontend.screen()!=Native::Screen::Home || std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.control=="messageCancel";}))throw std::runtime_error("Training welcome loses original single-button modal");capture("welcome");}
                    if(frames==150)key(SDLK_ESCAPE);
                    if(frames==180){if(frontend.hasMessageBox() || frontend.screen()!=Native::Screen::Training || game || slides)throw std::runtime_error("Welcome Escape fails to enter original lesson selection");capture("training");}
                    if(frames==190)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Home));
                    if(frames==210){if(frontend.screen()!=Native::Screen::Home || frontend.hasMessageBox())throw std::runtime_error("Training Back loses Home or repeats suggestion");trainingEntrySmokePassed=true;fprintf(stderr,"Training entry SDL: original suggestion, authored checkbox click/checked face, Cancel/persistence, welcome single-button/Escape, lesson selection and Home return passed\n");}
                }
                if(gamesSmoke){
                    auto advance=[&](int stage){gamesStage=stage;gamesStageFrame=frames;};
                    auto capture=[&](const char* state){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-"+state+".ppm")).string();}};
                    auto filterKey=[](SDL_Keycode key,SDL_Scancode scan){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=key;event.key.scancode=scan;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);};
                    auto filterPointer=[&](Native::MenuOp op,int choice,bool down){
                        const auto& items=frontend.view().items;auto found=std::find_if(items.begin(),items.end(),[&](const auto& item){return item.action.op==op && item.action.value==(op==Native::MenuOp::LobbyWingChoice?choice:0) && (op!=Native::MenuOp::GameFilterChoice || item.action.delta==choice);});
                        if(found==items.end())throw std::runtime_error("Games filter smoke: missing authored control or choice");
                        auto size=renderer.size();auto p=Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas).pixel(glm::vec2(found->bounds)+glm::vec2(3));int w,h;SDL_GetWindowSize(window.get(),&w,&h);p*=glm::vec2(w,h)/glm::vec2(size);
                        SDL_Event event{};event.type=SDL_EVENT_MOUSE_MOTION;event.motion.x=p.x;event.motion.y=p.y;SDL_PushEvent(&event);event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;event.button.button=SDL_BUTTON_LEFT;event.button.x=p.x;event.button.y=p.y;SDL_PushEvent(&event);
                    };
                    auto filterCaption=[&]{auto combo=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.action.op==Native::MenuOp::GameFilterCombo && item.action.value==0;});
                        if(combo!=frontend.view().items.end())for(const auto& c:frontend.view().artwork)if(c.kind==Native::OriginalControl::Kind::Text && c.visibleBounds()==combo->bounds)return c.text;
                        throw std::runtime_error("Games filter smoke: lost original combo caption");};
                    if(frames==gamesStageFrame+15){
                        if(gamesStage==2)capture("games");else if(gamesStage==3)capture(gamesFilterStage==1 || gamesFilterStage==3 || gamesFilterStage==5?"filter-popup":gamesFilterStage==6?"filter-draft":"filter");else if(gamesStage==8)capture(gamesCreateStage==2?"create-empty-error":gamesCreateStage==5?"create-long-error":"create");else if(gamesStage==9)capture("local-settings");
                    }
                    if(gamesStage==5 && gamesWingStage==1 && frames==gamesStageFrame+15)capture("wing-popup");
                    if(gamesStage==6 && gamesWingStage==5 && frames==gamesStageFrame+15)capture("quit-mission");
                    if(gamesStage==5 && !gamesLobbyCaptured && network && network->ready() && frontend.screen()==Native::Screen::RemoteLobby){capture("remote-lobby");gamesLobbyCaptured=true;gamesStageFrame=frames;}
                    if(gamesStage==0 && frames>=30){clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Games));advance(1);}
                    else if(gamesStage==1 && frames>gamesStageFrame+30 && frontend.screen()==Native::Screen::Games){
                        auto live=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.action.op==Native::MenuOp::GameSelect;});
                        if(live!=frontend.view().items.end()){clickMenu(Native::MenuOp::GameSelect,live->action.value);advance(2);}
                    }else if(gamesStage==2 && frames>gamesStageFrame+30){clickMenu(Native::MenuOp::GameFilter);advance(3);}
                    else if(gamesStage==3 && frames>gamesStageFrame+30){
                        if(gamesFilterStage==0)clickMenu(Native::MenuOp::GameFilterCombo);
                        else if(gamesFilterStage==1){if(frontend.wantsText())throw std::runtime_error("Games filter popup fails to capture SDL text");filterKey(SDLK_UP,SDL_SCANCODE_UP);filterKey(SDLK_RETURN,SDL_SCANCODE_RETURN);}
                        else if(gamesFilterStage==2){if(!frontend.wantsText() || filterCaption()!="DontCare")throw std::runtime_error("Games filter popup Enter accepts its parent or loses focus");clickMenu(Native::MenuOp::GameFilterCombo);}
                        else if(gamesFilterStage==3)filterKey(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
                        else if(gamesFilterStage==4){if(!frontend.wantsText())throw std::runtime_error("Games filter popup Escape cancels its parent");filterPointer(Native::MenuOp::GameFilterCombo,0,true);}
                        else if(gamesFilterStage==5)filterPointer(Native::MenuOp::GameFilterChoice,0,false);
                        else if(gamesFilterStage==6){if(!frontend.wantsText() || filterCaption()!="No")throw std::runtime_error("Games filter drag/release fails to select a row");SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text="MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM tail";SDL_PushEvent(&event);}
                        else if(gamesFilterStage==7){filterKey(SDLK_LEFT,SDL_SCANCODE_LEFT);filterKey(SDLK_DELETE,SDL_SCANCODE_DELETE);}
                        else if(gamesFilterStage==8){auto edit=std::find_if(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.textMode==Native::OriginalControl::TextMode::Edit && c.editFocused;});if(edit==frontend.view().artwork.end() || edit->text!="MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM tail")throw std::runtime_error("Original end-only filter edit changes text on Left/Delete or loses its authored focus");filterKey(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);}
                        else {auto edit=std::find_if(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.textMode==Native::OriginalControl::TextMode::Edit && c.editFocused;});if(edit==frontend.view().artwork.end() || !edit->text.ends_with(" tai"))throw std::runtime_error("Original filter edit fails append/backspace through SDL");clickMenu(Native::MenuOp::GameFilterCancel);advance(4);fprintf(stderr,"Games filter smoke: actual SDL popup keys, scoped Escape/Enter, restored edit focus, drag/release, long end-only editing and staged cancellation passed\n");}
                        ++gamesFilterStage;gamesStageFrame=frames;
                    }
                    else if(gamesStage==4 && frames>gamesStageFrame+30){clickMenu(Native::MenuOp::GameJoin);advance(5);}
                    else if(gamesStage==5 && network && network->ready() && frontend.screen()==Native::Screen::RemoteLobby && frames>gamesStageFrame+30){
                        if(gamesReadyStage==0){if(game->state().networkReady){clickMenu(Native::MenuOp::NetworkReady);gamesReadyStage=1;}else{clickMenu(Native::MenuOp::LobbyAction,int(Native::LobbyAction::ForceReady));gamesReadyStage=2;}gamesStageFrame=frames;}
                        else if(gamesReadyStage==1){if(!game->state().networkReady){clickMenu(Native::MenuOp::LobbyAction,int(Native::LobbyAction::ForceReady));gamesReadyStage=2;gamesStageFrame=frames;}}
                        else if(gamesReadyStage==2){if(game->state().networkReady && (game->state().forceReady&(1u<<game->state().playerSide))){
                            auto afk=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.control=="awayFromKeyboardButtonPane";});
                            if(afk==frontend.view().items.end() || afk->selected)throw std::runtime_error("Authoritative Team Ready retains the commander's AFK face");
                            gamesReadyStage=3;gamesStageFrame=frames;fprintf(stderr,"Original Team Ready SDL: actual AFK-to-ready command sequence, encrypted personal/team replication and cleared AFK face passed\n");}}
                        else if(gamesWingStage==0){filterPointer(Native::MenuOp::LobbyWing,0,true);gamesWingStage=1;gamesStageFrame=frames;}
                        else if(gamesWingStage==1){filterKey(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);gamesWingStage=2;gamesStageFrame=frames;}
                        else if(gamesWingStage==2){filterPointer(Native::MenuOp::LobbyWing,0,true);gamesWingStage=3;gamesStageFrame=frames;}
                        else if(gamesWingStage==3){filterPointer(Native::MenuOp::LobbyWingChoice,7,false);gamesWingStage=4;gamesStageFrame=frames;}
                        else if(game->state().wing==7){if(settings.defaultWing!=7 || Native::Settings::load(configPath).defaultWing!=7)throw std::runtime_error("Wing choice is not persisted");if(gamesWingStage==4){capture("wing-selected");gamesWingStage=5;gamesStageFrame=frames;}
                            else {fprintf(stderr,"Original wing SDL combo: drag/release, Escape, persisted choice and encrypted pregame replication passed\n");clickMenu(Native::MenuOp::Navigate,int(Native::Screen::ConfirmLeave));advance(6);}}
                    }
                    else if(gamesStage==6 && frames>gamesStageFrame+30){
                        if(frontend.screen()!=Native::Screen::RemoteLobby || frontend.sceneView().artwork.front().image!=assets.image("teamscreenbmp",false) || (gamesWingStage!=6 && !frontend.hasMessageBox()))throw std::runtime_error("Quit mission replaced its lobby parent");
                        if(gamesWingStage==5){filterKey(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);gamesWingStage=6;gamesStageFrame=frames;}
                        else if(gamesWingStage==6){if(frontend.hasMessageBox())throw std::runtime_error("Quit mission Escape failed to cancel");clickMenu(Native::MenuOp::Navigate,int(Native::Screen::ConfirmLeave));gamesWingStage=7;gamesStageFrame=frames;}
                        else {clickMenu(Native::MenuOp::MessageDismiss);advance(7);}
                    }
                    else if(gamesStage==7 && frames>gamesStageFrame+30){if(network || game || frontend.screen()!=Native::Screen::Games)throw std::runtime_error("Original network leave failed to return to the games list");clickMenu(Native::MenuOp::GameCreate);advance(8);}
                    else if(gamesStage==8 && frames>gamesStageFrame+30){
                        auto draft=[&]()->std::string{for(const auto& c:frontend.view().artwork)if(c.textMode==Native::OriginalControl::TextMode::Edit && c.editFocused)return c.text;throw std::runtime_error("Create Game loses original edit focus");};
                        auto error=[&](std::string_view text){return frontend.hasMessageBox() && std::any_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[&](const auto& c){return c.kind==Native::OriginalControl::Kind::Text && c.text==text;});};
                        if(gamesCreateStage==0){clickMenu(Native::MenuOp::GameCreateServer,0);clickMenu(Native::MenuOp::GameCreateCore,0);for(size_t i=0,n=draft().size();i<n;++i)filterKey(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);}
                        else if(gamesCreateStage==1){if(!draft().empty())throw std::runtime_error("Create Game SDL Backspace fails to clear its name");clickMenu(Native::MenuOp::GameCreateAccept);}
                        else if(gamesCreateStage==2){if(!error("Error : Game name is empty"))throw std::runtime_error("Create Game loses original empty-name validation");filterKey(SDLK_RETURN,SDL_SCANCODE_RETURN);}
                        else if(gamesCreateStage==3){if(frontend.hasMessageBox() || !frontend.wantsText())throw std::runtime_error("Create Game error dismissal loses field focus");SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text="MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM";SDL_PushEvent(&event);}
                        else if(gamesCreateStage==4){if(draft().size()!=65)throw std::runtime_error("Create Game prematurely truncates long SDL input");clickMenu(Native::MenuOp::GameCreateAccept);}
                        else if(gamesCreateStage==5){if(!error("Error : Game name is too long"))throw std::runtime_error("Create Game loses original long-name validation");filterKey(SDLK_RETURN,SDL_SCANCODE_RETURN);}
                        else if(gamesCreateStage==6)filterKey(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);
                        else {if(draft().size()!=64)throw std::runtime_error("Create Game does not retain its rejected name");clickMenu(Native::MenuOp::GameCreateAccept);advance(9);}
                        ++gamesCreateStage;gamesStageFrame=frames;
                    }
                    else if(gamesStage==9 && frames>gamesStageFrame+30){
                        if(frontend.screen()!=Native::Screen::Lobby || frontend.session().name!=std::string(64,'M'))throw std::runtime_error("Local named game creation loses its original 64-byte name after network leave");clickMenu(Native::MenuOp::SetupCancel);advance(10);
                    }else if(gamesStage==10 && frames>gamesStageFrame+30){if(frontend.screen()!=Native::Screen::Games)throw std::runtime_error("Local creation cancel failed to return to the games list");advance(11);frameLimit=frames+10;fprintf(stderr,"Games UI smoke: actual SDL entry, live query/selection/filter, encrypted join, original remote lobby/leave and named local creation/cancel passed\n");}
                }else if(refitSmoke){
                  if(network && network->ready() && !game->state().networkLobby){
                    auto hasCargo=[&]{return std::any_of(game->state().equipment.begin(),game->state().equipment.end(),[&](const auto& part){return part.mount==refitCargo && !part.empty;});};
                    auto advance=[&](int stage){refitStage=stage;refitStageFrame=frames;};
                    auto capture=[&](const char* state){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-"+state+".ppm")).string();}};
                    bool settled=frames>refitStageFrame+30;
                    if(frames==refitStageFrame+15){if(refitStage==1)capture("hangar");else if(refitStage==2)capture("loadout");else if(refitStage==3)capture("cargo-menu");else if(refitStage==4)capture("staged");else if(refitStage==6)capture("purchased");else if(refitStage==9)capture("redocked");else if(refitStage==10)capture("refit");}
                    if((refitStage==5 || refitStage==7 || refitStage==11) && frontend.hasMessageBox() && !refitBusyCaptured){capture("purchase-pending");refitBusyCaptured=true;}
                    if(refitStage==0 && game->state().docked && frontend.screen()==Native::Screen::Hangar && !frontend.hasMessageBox()){refitPilot=game->state().shipId;advance(1);}
                    else if(refitStage==1 && settled){clickMenu(Native::MenuOp::Navigate,int(Native::Screen::ShipLoadout));advance(2);}
                    else if(refitStage==2 && settled){auto& slots=frontend.stagedLoadout().slots;auto it=std::find_if(slots.begin(),slots.end(),[](auto& slot){return slot.mount<0 && slot.id>=0;});if(it==slots.end())throw std::runtime_error("Remote refit fixture has no cargo");refitCargo=it->mount;clickMenu(Native::MenuOp::PreviewSlot,int(it-slots.begin()));advance(3);}
                    else if(refitStage==3 && settled){clickMenu(Native::MenuOp::PreviewPart,-1);advance(4);}
                    else if(refitStage==4 && settled){if(!hasCargo())throw std::runtime_error("Staging a remote refit changed the live ship");clickMenu(Native::MenuOp::CommitLoadout,0);advance(5);}
                    else if(refitStage==5 && !frontend.hasMessageBox() && !hasCargo()){if(!frontend.stagedLoadout().equivalent)throw std::runtime_error("Purchase completion retains the old loadout preview");advance(6);}
                    else if(refitStage==6 && settled){clickMenu(Native::MenuOp::CommitLoadout,1);advance(7);}
                    else if(refitStage==7 && !game->state().docked && !menuVisible && !frontend.hasMessageBox()){capture("flight");advance(8);}
                    else if(refitStage==8 && refitTransitionComplete[0]){if(!refitLaunchFrames[0])throw std::runtime_error("Remote purchase launch skips its external camera");if(!game->returnToBase())throw std::runtime_error("Remote return-to-base command rejected");advance(14);}
                    else if(refitStage==14 && game->state().docked && frontend.screen()==Native::Screen::Hangar){advance(9);}
                    else if(refitStage==9 && settled && !frontend.hasMessageBox()){clickMenu(Native::MenuOp::Navigate,int(Native::Screen::ShipLoadout));advance(10);}
                    else if(refitStage==10 && settled){clickMenu(Native::MenuOp::PreviewDefault);advance(12);}
                    else if(refitStage==12 && settled){clickMenu(Native::MenuOp::CommitLoadout,1);advance(11);}
                    else if(refitStage==11 && !game->state().docked && !menuVisible && !frontend.hasMessageBox()){
                        if(game->state().shipId!=refitPilot || !hasCargo())throw std::runtime_error("Remote refit/relaunch loses cargo or pilot identity");
                        advance(13);
                    }else if(refitStage==13 && refitTransitionComplete[1]){
                        if(!refitLaunchFrames[1] || refitTransitionFrames[0]<2 || refitTransitionFrames[1]<2)throw std::runtime_error("Remote refit relaunch skips its external camera or visible cockpit transition");
                        advance(15);frameLimit=frames+30;fprintf(stderr,"Remote refit UI: actual SDL staging, purchase completion, full launch, autopilot docking, default refit and full relaunch passed; visible ship in %u/%u cinematic and %u/%u transition frames\n",refitLaunchFrames[0],refitLaunchFrames[1],refitTransitionFrames[0],refitTransitionFrames[1]);
                    }
                  }
                }else if(teleportSmoke) {
                if(frames==60){auto choices=game->teleportChoices();auto home=std::find_if(choices.begin(),choices.end(),[](auto& c){return c.current;});if(home==choices.end())throw std::runtime_error("Teleport smoke missing origin");teleportHome=home->id;teleportHomeSector=game->state().sectorId;teleportIdentity=game->state().shipId;teleportTotal=game->state().money+game->shipValue();clickMenu(Native::MenuOp::Teleport);}
                if(frames>=100 && teleportDestination<0 && (!frontend.view().slide || frontend.view().slide->displacement(Native::uiTime()).x==0)){auto destination=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[&](auto& item){return item.enabled && (modernUi?item.action.op==Native::MenuOp::TeleportTo:item.action.op==Native::MenuOp::TeleportSelect && !item.action.delta && item.action.value>=0 && item.action.value!=teleportHome);});
                    if(destination==frontend.view().items.end())throw std::runtime_error("Split-base teleport smoke has no other destination");
                    teleportDestination=destination->action.value;teleportTotal=game->state().money+game->shipValue();
                    if(modernUi)clickMenu(Native::MenuOp::TeleportTo,teleportDestination);
                    else {clickMenu(Native::MenuOp::TeleportSelect,teleportDestination);auto choices=game->teleportChoices();if(std::none_of(choices.begin(),choices.end(),[&](auto& row){return row.current && row.id==teleportHome;}))throw std::runtime_error("Selecting a teleport row travels before TELEPORT is clicked");clickMenu(Native::MenuOp::TeleportActivate);}
                    if(game->state().money+game->shipValue()!=teleportTotal)throw std::runtime_error("Teleport menu changed wallet/ship value");}
                if(frames==180){
                    auto choices=game->teleportChoices();auto station=std::find_if(choices.begin(),choices.end(),[](auto& c){return c.current;});
                    if(!game->state().docked || station==choices.end() || station->id==teleportHome || station->id!=teleportDestination || game->state().shipId!=teleportIdentity || frontend.screen()!=Native::Screen::Hangar)
                        throw std::runtime_error("Teleport HUD input did not retain docked identity, loadout, wallet and destination");
                    fprintf(stderr,"Teleport smoke: original split-base map, mouse destination selection, docked identity/loadout/credits and destination hangar passed\n");
                }
                if(!modernUi && frames==185){game->commandView(true,true);game->commandView(false,true);}
                if(!modernUi && frames==190)clickMenu(Native::MenuOp::CommandView);
                if(!modernUi && frames==195){
                    if(!game->state().commandView || !game->state().commandTopDown || menuVisible || game->state().sectorId!=game->playerShip()->GetStation()->GetCluster()->GetObjectID())throw std::runtime_error("Hangar Command does not preserve the remembered perspective and view the current base sector");
                    game->commandView(false,true);menuVisible=true;frontend.open(Native::Screen::Hangar,&game->state());
                    fprintf(stderr,"Hangar Command: actual authored button retains the original remembered perspective and base sector\n");
                }
                auto press=[&](SDL_Scancode scan){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=scan;event.key.key=SDL_GetKeyFromScancode(scan,SDL_KMOD_NONE,false);SDL_PushEvent(&event);};
                auto clickHomeSector=[&]{
                    auto size=renderer.size();auto layout=Native::originalMenuLayout(size,frontend.view().classicScale);auto sector=std::find_if(game->state().sectors.begin(),game->state().sectors.end(),[&](auto& s){return s.id==teleportHomeSector;});
                    if(sector==game->state().sectors.end())throw std::runtime_error("Sector smoke lost origin sector");std::vector<Allegiance::SectorMapPoint> points;
                    for(auto& s:game->state().sectors)if(s.known)points.push_back({s.position.x,s.position.y});
                    auto bounds=Allegiance::SectorMapBounds::Calculate(points,120,96);auto node=bounds.Pixel({sector->position.x,sector->position.y},120,96);
                    auto pixel=(glm::vec2(49, float(size.y)/layout.scale-177)+glm::vec2(9+node.x,25+node.y))*layout.scale;
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);SDL_Event event{};event.button.button=SDL_BUTTON_LEFT;event.button.clicks=1;event.button.x=pixel.x*w/size.x;event.button.y=pixel.y*h/size.y;
                    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;SDL_PushEvent(&event);event.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&event);
                };
                if(!modernUi){
                    if(frames==200)press(settings.keys[size_t(Native::Binding::Research)]);
                    if(frames==235)clickHomeSector();
                    if(frames==260){auto choices=game->teleportChoices();if(!game->state().commandView || menuVisible || game->state().sectorId!=teleportHomeSector || std::none_of(choices.begin(),choices.end(),[&](auto& row){return row.current && row.id==teleportDestination;}))throw std::runtime_error("F5 map click must open the picked command sector without moving the docked pilot");press(settings.keys[size_t(Native::Binding::CommandView)]);}
                    if(frames==360)clickHomeSector();
                    if(frames==390){auto choices=game->teleportChoices();if(frontend.screen()!=Native::Screen::Hangar || std::none_of(choices.begin(),choices.end(),[&](auto& row){return row.current && row.id==teleportHome;}) || game->state().shipId!=teleportIdentity)throw std::runtime_error("F8 map click must teleport to the picked restart sector and retain pilot identity");fprintf(stderr,"Sector map smoke: actual SDL clicks route F5 to docked command view and F8 to station transfer, without moving a viewing pilot\n");}
                    if(frames==400)press(settings.keys[size_t(Native::Binding::Teleport)]);
                }
                if(frames==300 || frames==420)press(settings.keys[size_t(Native::Binding::Teleport)]);
                if(frames==320)press(settings.keys[size_t(Native::Binding::Screenshot)]);
                if(frames==350 && frontend.screen()!=Native::Screen::Teleport)throw std::runtime_error("Original teleport key did not open destination screen");
                if(frames==450 && frontend.screen()!=Native::Screen::Hangar)throw std::runtime_error("Original teleport key did not toggle back to hangar");
                if(frames==460 || frames==500 || frames==540 || frames==570 || frames==610)press(settings.keys[size_t(Native::Binding::Research)]);
                if(frames==480){
                    if(frontend.screen()!=Native::Screen::Research || !menuVisible)throw std::runtime_error("F5 did not open original investment console from hangar");
                    if(frontend.classicInterface()){
                        Native::OriginalUi authored(assets);Native::UiOverlayLayout overlays(authored,{"MergedInvestmentsPaneImage"},settings.hudStyle);
                        auto* overlay=overlays.get(0,{},glm::vec2(frontend.view().canvas));const auto& slide=frontend.view().slide;
                        if(!overlay || !slide || slide->screenAnchor!=overlay->anchor || slide->off!=overlay->off-overlay->on)throw std::runtime_error("Investment console ignores its authored anchor/slide instead of using dialog.mdl");
                        if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-investment.ppm")).string();}
                    }
                }
                if(frames==520 && frontend.screen()!=Native::Screen::Hangar)throw std::runtime_error("F5 did not toggle investments back to hangar");
                if(frames==550)clickMenu(Native::MenuOp::Launch);
                if(frames==560 && game->state().docked)throw std::runtime_error("Launching underneath investments did not launch the ship");
                // Ship rotation accelerates gradually; require actual movement, not a
                // large turn within fifteen frames on a high-refresh display.
                if(frames==580){teleportInvestmentHeading=game->state().up;press(settings.keys[size_t(Native::Binding::RollRight)]);}
                if(frames==595){SDL_Event event{};event.type=SDL_EVENT_KEY_UP;event.key.scancode=settings.keys[size_t(Native::Binding::RollRight)];SDL_PushEvent(&event);if(glm::length(teleportInvestmentHeading-game->state().up)<.0001f)throw std::runtime_error("Fresh F5 console suppresses original keyboard flight controls");}
                if(frames==601)press(settings.keys[size_t(Native::Binding::Cursor)]);
                if(frames==608 && frontend.classicInterface() && frontend.freshInvestmentPane())throw std::runtime_error("Original ToggleMouse did not release fresh F5 pointer while retaining the pane");
                if(frames==600 && (!menuVisible || frontend.screen()!=Native::Screen::Research || !frontend.view().overlay))throw std::runtime_error("F5 did not open investment console over the flight scene");
                if(frames==620){if(menuVisible || (frontend.classicInterface() && !frontend.closingConsole()))throw std::runtime_error("Closing F5 must resume flight and retain the inactive exit animation");fprintf(stderr,"Investment smoke: actual F5 toggles, launch underneath console, flight overlay and immediate input return passed\n");}

                } else if(lobbyChatSmoke){
                    auto visible=[&](std::string_view text){return std::any_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[&](auto& c){return c.name.starts_with("lobbyChat/") && c.text.find(text)!=c.text.npos;});};
                    auto add=[&](std::string text,int target,bool leader=false){Native::GameMessage message{};message.serial=lobbyChatFixture.messages.empty()?1:lobbyChatFixture.messages.back().serial+1;message.text=std::move(text);message.chatTarget=target;message.chat=message.fromPlayer=true;message.leader=leader;message.color=leader?glm::vec4(.8f,.2f,.8f,1):glm::vec4(1);lobbyChatFixture.messages.push_back(std::move(message));};
                    auto capture=[&](const char* name){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-"+name+".ppm")).string();}};
                    auto pageUp=[&]{SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=settings.keys[size_t(Native::Binding::ChatPageUp)];event.key.key=SDL_GetKeyFromScancode(event.key.scancode,SDL_KMOD_NONE,false);event.key.mod=frames>220?SDL_Keymod(SDL_KMOD_CTRL|SDL_KMOD_SHIFT):SDL_KMOD_NONE;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);};
                    if(frames==30){
                        Native::SessionOptions session;session.scenario=Native::Scenario::Conquest;frontend.networkSession(session);auto civs=Native::Game::civilizations(artwork);
                        auto& state=lobbyChatFixture;state.networked=state.networkLobby=state.commander=state.networkHost=state.networkReady=true;state.playerSide=0;state.networkHostPilot=10;
                        state.networkPilots={{10,0,"Commander",true,true,true,true,false},{20,0,"Wing pilot",true,false,false,false,false},{30,1,"Opponent",true,false,true,false,false}};
                        state.teamScores.resize(2);for(int i=0;i<2;++i){state.teamScores[i].name=i?"Blue team":"Green team";state.teamScores[i].active=true;state.teamScores[i].civilization=civs[i].id;state.teamScores[i].civilizationIcon=civs[i].icon;state.teamScores[i].color=i?glm::vec4(.2f,.5f,1,1):glm::vec4(.2f,1,.2f,1);}
                        for(int i=0;i<30;++i)add("Commander (all): Lobby history message "+std::to_string(i),CHAT_EVERYONE,i%3==0);
                        add("Commander (team): Team-only report",CHAT_TEAM,true);add("Commander (leaders): Leader-only report",CHAT_LEADERS,true);add("Wing pilot (me): Private report",CHAT_INDIVIDUAL);
                        add("Commander (all): This long message should wrap with the original bitmap font, preserving the commander color and two-space indentation on continuation lines.",CHAT_EVERYONE,true);
                        frontend.setPlaying(true);frontend.open(Native::Screen::RemoteLobby,&state);menuVisible=true;
                    }
                    if(frames==40)clickMenu(Native::MenuOp::LobbyChatChannel,0);
                    if(frames==60){if(!visible("Private report") || visible("Team-only") || visible("Leader-only"))throw std::runtime_error("Rendered All chat channel filter differs from WinTrek");capture("latest");}
                    if(frames==70)pageUp();
                    if(frames==90){if(!visible("Lobby history") || visible("Private report"))throw std::runtime_error("Actual SDL Page Up does not scroll original lobby chat");}
                    if(frames==100){SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text="Unsent draft";SDL_PushEvent(&event);}
                    if(frames==110){add("Wing pilot (all): New report while reading",CHAT_EVERYONE);frontend.sync(lobbyChatFixture);}
                    if(frames==130){if(frontend.chatText()!="Unsent draft" || visible("New report"))throw std::runtime_error("New lobby message steals scroll position or draft");capture("history");}
                    if(frames==150)clickMenu(Native::MenuOp::LobbyChatChannel,1);
                    if(frames==175){if(!visible("Team-only") || !visible("Private report") || visible("Lobby history"))throw std::runtime_error("Rendered Team history filter differs from WinTrek");capture("team");}
                    if(frames==190)clickMenu(Native::MenuOp::LobbyChatChannel,2);
                    if(frames==210){if(!visible("Leader-only") || !visible("Private report") || visible("Team-only"))throw std::runtime_error("Rendered Leaders history filter differs from WinTrek");capture("leaders");}
                    if(frames==220)clickMenu(Native::MenuOp::LobbyChatChannel,0);
                    if(frames==225){auto i=size_t(Native::Binding::ChatPageUp);settings.keys[i]=SDL_SCANCODE_F11;settings.modifiers[i]=Native::KeyControl|Native::KeyShift;settings.keyCommands[i]=TK_ChatPageUp;
                        std::erase_if(settings.keyAliases,[](auto& route){return route.binding==Native::Binding::ChatPageUp;});keyboard.configure(settings);}
                    if(frames==230){pageUp();lobbyChatScrolledAt=Native::uiTime();}
                    if(frames==250){add("Wing pilot (all): Final fresh report",CHAT_EVERYONE);frontend.sync(lobbyChatFixture);}
                    if(frames==275)clickMenu(Native::MenuOp::RosterSelect,1);
                    if(frames==280 || frames==290)clickMenu(Native::MenuOp::RosterSort,0);
                    if(frames==300){
                        auto first=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](auto& p){return p.action.op==Native::MenuOp::RosterSelect;});
                        if(first==frontend.view().items.end() || first->action.delta!=20 || !first->selected || frontend.chatText()!="Unsent draft" || !frontend.wantsText())throw std::runtime_error("Actual lobby name sort loses its descending order, selected pilot, draft or text focus");
                        capture("roster-sorted");
                    }
                    if(frames==310 || frames==320)clickMenu(Native::MenuOp::RosterSort,4);
                    if(frames==330 || frames==340)clickMenu(Native::MenuOp::RosterSort,5);
                    if(frames==350 || frames==360)clickMenu(Native::MenuOp::RosterSort,3);
                    if(frames==370)clickMenu(Native::MenuOp::RosterSort,1);
                    if(frames==380)clickMenu(Native::MenuOp::RosterSort,2);
                    if(frames==400){
                        auto selected=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](auto& p){return p.action.op==Native::MenuOp::RosterSelect && p.selected;});
                        if(selected==frontend.view().items.end() || selected->action.delta!=20 || frontend.chatText()!="Unsent draft")throw std::runtime_error("Actual lobby sort columns lose stable private selection or draft");
                        fprintf(stderr,"Lobby roster UI: actual SDL clicks on all six sort headers, reverse name order, zero ranks and retained private selection/draft passed\n");
                    }
                    if(frames>260 && !lobbyChatSmokePassed && Native::uiTime()-lobbyChatScrolledAt>10.25){
                        if(!visible("Final fresh report") || frontend.chatText()!="Unsent draft")throw std::runtime_error("Original ten-second autoscroll does not return to latest chat while preserving draft");
                        lobbyChatSmokePassed=true;capture("autoscroll");frameLimit=frames+10;fprintf(stderr,"Lobby chat UI: actual SDL paging/remapped chord/text/channel clicks, authored wrapping/filters, stable reading/draft and ten-second autoscroll passed\n");
                    }
                } else if(teamSettingsSmoke) {
                    if(frames==30){
                        Native::SessionOptions session;session.scenario=Native::Scenario::Conquest;session.team=1;frontend.networkSession(session);
                        auto civs=Native::Game::civilizations(artwork);auto& state=teamSettingsFixture;
                        state.networked=state.networkLobby=state.commander=state.networkReady=true;state.playerSide=1;state.networkHostPilot=10;
                        state.networkPilots={{10,0,"Owner",true,true,true,false,false},{20,1,"Commander",true,false,true,true,false}};
                        state.teamScores.resize(2);state.teamScores[0].name="Veterans";state.teamScores[1].name="Seasoned pilots";
                        state.teamScores[0].color={200.f/255,15.f/255,200.f/255,1};state.teamScores[1].color={8.f/255,184.f/255,184.f/255,1};
                        for(int i=0;i<2;++i){state.teamScores[i].active=true;state.teamScores[i].civilization=civs[i].id;state.teamScores[i].civilizationIcon=civs[i].icon;}
                        frontend.open(Native::Screen::RemoteLobby,&state);menuVisible=true;
                    }
                    if(frames==70 || frames==215)clickMenu(Native::MenuOp::TeamSettingsOpen);
                    if((frames==100 || frames==140) && !screenshot.empty()){
                        auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+(frames==100?"-team-settings.ppm":"-team-factions.ppm"))).string();
                    }
                    if(frames==110)clickMenu(Native::MenuOp::TeamFaction);
                    if(frames==150){auto civs=Native::Game::civilizations(artwork);clickMenu(Native::MenuOp::TeamFactionChoice,civs[0].id);}
                    if(frames==170)clickMenu(Native::MenuOp::TeamName);
                    if(frames==180){SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text=" edit";SDL_PushEvent(&event);}
                    if(frames==190)clickMenu(Native::MenuOp::TeamSettingsCancel);
                    if(frames==210 && (frontend.screen()!=Native::Screen::RemoteLobby || frontend.chatText()!="" || !frontend.wantsText()))throw std::runtime_error("Rendered team dialog loses its lobby/draft/focus on cancel");
                    if(frames==245){
                        if(frontend.teamSettingsName()!="Seasoned pilots" || std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.control=="teamNameEditPane";}))throw std::runtime_error("Rendered original team settings do not restore authoritative values");
                        fprintf(stderr,"Team settings smoke: actual SDL authored dialog/dropdown clicks, name editing, cancel, focus and faction watermark passed\n");
                    }
                    if(frames==270)clickMenu(Native::MenuOp::TeamSettingsCancel);
                    if(frames==280){teamSettingsFixture.networkCountdown=9;teamSettingsFixture.missionStage=STAGE_STARTING;teamSettingsFixture.sector="Wasp";frontend.sync(teamSettingsFixture);}
                    if(frames==300){
                        if(frontend.wantsText() || std::none_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.name=="missionBriefingCountDown" && c.text=="9";}))throw std::runtime_error("Rendered original briefing did not isolate input or display countdown");
                        if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-briefing.ppm")).string();}
                    }
                    if(frames==330){teamSettingsFixture.networkCountdown=8;teamSettingsFixture.missionStage=STAGE_STARTING;frontend.sync(teamSettingsFixture);}
                    if(frames==350){teamSettingsFixture.playerSide=Native::noTeam;teamSettingsFixture.commander=false;teamSettingsFixture.networkPilots[1].team=Native::noTeam;teamSettingsFixture.networkPilots[1].commander=false;frontend.sync(teamSettingsFixture);}
                    if(frames==370){
                        if(std::none_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.name=="textTimer" && c.text=="8";}) || std::any_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.name=="missionBriefingCountDown";}))throw std::runtime_error("Rendered No Team countdown did not replace the briefing");
                        if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-countdown.ppm")).string();}
                        fprintf(stderr,"Lobby briefing smoke: authored faction briefing, map/tip/countdown and No Team timer passed\n");
                    }
                    if(frames==410){
                        auto& s=teamSettingsFixture;s.playerSide=1;s.commander=s.networkReady=true;s.forceReady=2;s.networkCountdown=-1;s.missionStage=STAGE_NOTSTARTED;
                        s.networkPilots[1].team=1;s.networkPilots[1].commander=s.networkPilots[1].ready=true;s.networkPilots[1].requested=0;
                        s.networkPilots.push_back({21,1,"Wing pilot",true,false,false,false,false});
                        frontend.open(Native::Screen::Home);frontend.open(Native::Screen::RemoteLobby,&s);
                        SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text="Unsent settings draft";SDL_PushEvent(&event);
                    }
                    if(frames==420)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Lobby));
                    if(frames==440){
                        if(frontend.screen()!=Native::Screen::Lobby || frontend.wantsText() || std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::SetupAccept;}))throw std::runtime_error("Original commander game details are editable or retain lobby chat input");
                        if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-details.ppm")).string();}
                        SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_ESCAPE;event.key.scancode=SDL_SCANCODE_ESCAPE;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);
                    }
                    if(frames==450){
                        // WinTrek's global TK_MainMenu filter consumes Escape
                        // before NewGameScreen. Its Back button returns to teams.
                        if(frontend.screen()!=Native::Screen::Pause || frontend.chatText()!="Unsent settings draft")throw std::runtime_error("Original details Escape does not open its main-menu popup or retains no draft");
                        SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_ESCAPE;event.key.scancode=SDL_SCANCODE_ESCAPE;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);
                    }
                    if(frames==460){
                        if(frontend.screen()!=Native::Screen::Lobby)throw std::runtime_error("Closing the original main menu loses game details");
                        clickMenu(Native::MenuOp::SetupCancel);
                    }
                    if(frames==470){
                        auto& items=frontend.view().items;auto away=std::find_if(items.begin(),items.end(),[](const auto& i){return i.control=="awayFromKeyboardButtonPane";});auto ready=std::find_if(items.begin(),items.end(),[](const auto& i){return i.control=="sideReadyButtonPane";});
                        if(frontend.screen()!=Native::Screen::RemoteLobby || !frontend.wantsText() || frontend.chatText()!="Unsent settings draft" || away==items.end() || !away->selected || ready==items.end() || ready->selected)throw std::runtime_error("Original details Escape restores readiness or loses its lobby/draft");
                    }
                    if(frames==480)clickMenu(Native::MenuOp::RosterSelect,1);
                    if(frames==490)clickMenu(Native::MenuOp::MakeLeader,21);
                    if(frames==510){
                        if(frontend.chatText()!="Unsent settings draft" || std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::RosterSelect && i.selected;}))throw std::runtime_error("Actual Make Leader click retains private selection or loses the unsent draft");
                        if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-details-return.ppm")).string();}
                        fprintf(stderr,"Lobby actions smoke: actual Details click, read-only original setup, global Escape menu, Back, retained AFK/draft, Make Leader click and cleared private selection passed\n");
                    }
                    auto rightTeam=[&](int side){
                        auto& items=frontend.view().items;auto row=std::find_if(items.begin(),items.end(),[&](const auto& item){return item.control=="team"+std::to_string(side);});
                        if(row==items.end())throw std::runtime_error("Rendered alliance team row absent");
                        auto p=Native::originalMenuLayout(renderer.size(),frontend.view().classicScale,frontend.view().canvas).pixel(glm::vec2(row->bounds)+glm::vec2(row->bounds.z,row->bounds.w)*.5f);
                        SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_RIGHT;event.button.x=p.x;event.button.y=p.y;SDL_PushEvent(&event);event.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&event);
                    };
                    auto captureAlliance=[&](const char* suffix){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+suffix+".ppm")).string();}};
                    if(frames==600){
                        frontend.setPlaying(false);auto session=frontend.session();session.team=0;session.rules.teams=4;session.alliances.fill(NA);session.localPilots={{"Second team",1,true},{"Third team",2,true},{"Fourth team",3,true}};
                        frontend.networkSession(session);frontend.open(Native::Screen::Home);frontend.open(Native::Screen::LobbyPilots);
                    }
                    if(frames==610)rightTeam(0);
                    if(frames==630){
                        if(std::count_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.popupLevel>=0 && item.action.op==Native::MenuOp::ChangeAlliance;})!=3)throw std::runtime_error("Actual team right-click misses original solo alliance choices");
                        captureAlliance("-alliance-choices");
                    }
                    if(frames==650)clickMenu(Native::MenuOp::ChangeAlliance,0);
                    if(frames==670){
                        const auto& session=frontend.session();if(session.alliances[0]!=0 || session.alliances[1]!=0 || !session.rules.alliedSight || !session.rules.alliedRipcord || session.rules.maxImbalance!=32767)throw std::runtime_error("Actual alliance mouse release does not form its selected group/rules");captureAlliance("-alliance-pair");
                    }
                    if(frames==690)rightTeam(2);
                    if(frames==710){
                        if(std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& item){return item.popupLevel>=0 && item.label=="Ally with Teams 1,2";}))throw std::runtime_error("Original grouped alliance menu absent after formation");captureAlliance("-alliance-group");
                    }
                    if(frames==730){
                        for(auto key:{SDLK_DOWN,SDLK_RETURN}){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=key;event.key.scancode=key==SDLK_DOWN?SDL_SCANCODE_DOWN:SDL_SCANCODE_RETURN;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);}
                    }
                    if(frames==750){const auto& groups=frontend.session().alliances;if(groups[0]!=groups[1] || groups[0]!=groups[2] || groups[0]<0 || groups[3]!=NA)throw std::runtime_error("Actual arrow/Enter alliance selection does not join the selected group");captureAlliance("-alliance-merged");}
                    if(frames==770)rightTeam(0);
                    if(frames==790)clickMenu(Native::MenuOp::ChangeAlliance,0);
                    if(frames==810){const auto& groups=frontend.session().alliances;if(groups[0]!=NA || groups[1]<0 || groups[1]!=groups[2])throw std::runtime_error("Removing one side destroys its remaining allies");captureAlliance("-alliance-remaining");}
                    if(frames==830)rightTeam(1);
                    if(frames==850)clickMenu(Native::MenuOp::ChangeAlliance,1);
                    if(frames==870){
                        const auto& session=frontend.session();if(std::any_of(session.alliances.begin(),session.alliances.end(),[](char group){return group!=NA;}) || session.rules.allowDefections || session.rules.alliedSight || session.rules.alliedRipcord || session.rules.maxImbalance!=32766)throw std::runtime_error("Actual last-alliance removal does not restore original automatic balance/rules");captureAlliance("-alliance-removed");
                        fprintf(stderr,"Alliance smoke: actual original team right-click, solo/group menus, mouse release, arrow/Enter group join, partial removal and final rule restoration passed\n");
                    }
                } else if(lobbySmoke) {
                  if(!modernUi){
                    auto key=[](SDL_Keycode key,SDL_Scancode scan){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=key;event.key.scancode=scan;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);};
                    auto text=[](const char* value){SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text=value;SDL_PushEvent(&event);};
                    auto capture=[&](const char* stage){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-"+stage+".ppm")).string();}};
                    if(frames==30)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Games));
                    if(frames==60)clickMenu(Native::MenuOp::GameCreate);
                    if(frames==90){clickMenu(Native::MenuOp::GameCreateServer,0);clickMenu(Native::MenuOp::GameCreateCore,0);for(size_t i=0,n=settings.pilot.size()+7;i<n;++i)key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);}
                    if(frames==120){static const std::string name(64,'M');text(name.c_str());}
                    if(frames==150)clickMenu(Native::MenuOp::GameCreateAccept);
                    if(frames==180){
                        if(frontend.screen()!=Native::Screen::Lobby || frontend.session().name!=std::string(64,'M'))throw std::runtime_error("Local lobby diagnostic did not reach original named game settings");
                        // Supply additional seats as fixture state. Standard entry,
                        // settings and team controls remain the original UI.
                        auto options=frontend.session();options.localPilots={{"Friendly seat",0,false},{"Opposing seat",1,true}};frontend.networkSession(options);capture("settings");
                    }
                    if(frames==210)clickMenu(Native::MenuOp::SetupAccept);
                    if(frames==240){if(frontend.screen()!=Native::Screen::LobbyPilots)throw std::runtime_error("Original local creation bypasses its team lobby");clickMenu(Native::MenuOp::Ready);}
                    if(frames==270){if(game || frontend.screen()!=Native::Screen::LobbyPilots)throw std::runtime_error("Unready local seats escaped the team lobby");clickMenu(Native::MenuOp::Start);}
                    if(frames==300){if(game || frontend.screen()!=Native::Screen::LobbyPilots)throw std::runtime_error("Disabled Start launched unready local seats");clickMenu(Native::MenuOp::LobbyAction,int(Native::LobbyAction::ForceReady));}
                    if(frames==330){
                        auto afk=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.control=="awayFromKeyboardButtonPane";});
                        if(afk==frontend.view().items.end() || afk->selected)throw std::runtime_error("Team Ready leaves the local commander marked AFK");clickMenu(Native::MenuOp::TeamSettingsOpen);
                    }
                    if(frames==360){for(size_t i=0,n=frontend.teamSettingsName().size();i<n;++i)key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);}
                    if(frames==390)text("Known complete crew");
                    if(frames==420)clickMenu(Native::MenuOp::TeamSettingsAccept);
                    if(frames==450){
                        if(std::none_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.name=="textPrompt" && c.text.find("KNOWN COMPLETE CREW")!=c.text.npos;}))throw std::runtime_error("Accepted local team name is missing from the original prompt");capture("teams");
                    }
                    if(frames==480)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Lobby));
                    if(frames==510)clickMenu(Native::MenuOp::MissionSetting,10);
                    if(frames==540)clickMenu(Native::MenuOp::SetupCancel);
                    if(frames==570){if(frontend.screen()!=Native::Screen::LobbyPilots || frontend.session().rules.friendlyFire)throw std::runtime_error("Local settings Cancel loses the team parent or commits its draft");clickMenu(Native::MenuOp::Start);}
                    if(frames==600){if(frontend.screen()!=Native::Screen::Countdown || game || frontend.wantsText())throw std::runtime_error("Original local countdown does not isolate its briefing");capture("briefing");}
                    if(frames==630)key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
                    if(frames==660){if(frontend.screen()!=Native::Screen::LobbyPilots || game || !frontend.wantsText())throw std::runtime_error("Countdown Cancel does not restore original team lobby and chat focus");clickMenu(Native::MenuOp::Start);}
                    if(frames==690)frontAction(frontend.tick(61));
                    if(frames==720){
                        if(!game || game->state().networkPilots.size()!=3 || game->state().pilots.size()!=2 || !game->state().docked || frontend.screen()!=Native::Screen::Hangar || game->sessionOptions().name!=std::string(64,'M'))throw std::runtime_error("Original local flow did not start its full-name game with three public seats and private allied accounts");capture("hangar");clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Pilots));
                    }
                    if(frames==750){if(frontend.screen()!=Native::Screen::Hangar || playerPane.mode()!=Allegiance::PlayerPaneMode::Expanded)throw std::runtime_error("Original hangar Team button did not overlay the expanded player pane");capture("players");}
                    if(frames==751)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Pilots));
                    if(frames==780){if(playerPane.active() || frontend.screen()!=Native::Screen::Hangar)throw std::runtime_error("Original hangar Team button did not dismiss its player pane");clickMenu(Native::MenuOp::Navigate,int(Native::Screen::ConfirmLeave));}
                    if(frames==810){if(!frontend.hasMessageBox() || frontend.screen()!=Native::Screen::Hangar)throw std::runtime_error("Local hangar Exit loses its original quit confirmation/parent");capture("quit");}
                    if(frames==811)key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
                    if(frames==840){if(!game || frontend.hasMessageBox() || frontend.screen()!=Native::Screen::Hangar)throw std::runtime_error("Local hangar quit Cancel leaves the game");clickMenu(Native::MenuOp::Navigate,int(Native::Screen::ConfirmLeave));}
                    if(frames==870)clickMenu(Native::MenuOp::MessageDismiss);
                    if(frames==900){if(game || frontend.screen()!=Native::Screen::Games)throw std::runtime_error("Original local quit did not return to Games");lobbySmokePassed=true;fprintf(stderr,"Local lobby SDL smoke: original creation, full 64-byte game name, three seats, readiness/AFK, team edit, settings Cancel, briefing Cancel/restart, real docked game, hangar Team overlay and quit Cancel/accept passed\n");}
                  }else{
                    if(frames==30)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Scenarios));
                    if(frames==60)clickMenu(Native::MenuOp::Scenario,int(Native::Scenario::Conquest));
                    if(frames==90)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::LobbyPilots));
                    if(frames==120 || frames==150)clickMenu(Native::MenuOp::AddLobbyPilot);
                    if(frames==180)clickMenu(Native::MenuOp::LobbyPilotReady,0);
                    if(frames==210)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Lobby));
                    if(frames==240)clickMenu(Native::MenuOp::Ready);
                    if(frames==270){clickMenu(Native::MenuOp::Start);if(game || frontend.screen()!=Native::Screen::Lobby)throw std::runtime_error("Unready lobby pilot started a session");}
                    if(frames==300)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::LobbyPilots));
                    if(frames==330)clickMenu(Native::MenuOp::LobbyPilotReady,1);
                    if(frames==450)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Lobby));
                    if(frames==480){clickMenu(Native::MenuOp::Start);if(frontend.screen()!=Native::Screen::Countdown)throw std::runtime_error("Local lobby countdown absent");}
                    if(frames==570)clickMenu(Native::MenuOp::CancelCountdown);
                    if(frames==600){clickMenu(Native::MenuOp::Start);frontAction(frontend.tick(61));if(!game || game->state().pilots.size()!=3)throw std::runtime_error("Local lobby pilot identities absent");}
                    if(frames==660){clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Pilots));clickMenu(Native::MenuOp::Account,game->state().pilots.back().id);}
                    if(frames==700){int id=game->state().pilots.back().id;clickMenu(Native::MenuOp::ControlPilot,id);if(game->state().shipId!=id || !game->state().docked)throw std::runtime_error("Local lobby control seat failed");}
                    if(frames==750){clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Pilots));auto pilot=std::find_if(game->state().pilots.begin(),game->state().pilots.end(),[](auto& p){return p.commander;});if(pilot==game->state().pilots.end())throw std::runtime_error("Local commander absent");clickMenu(Native::MenuOp::Account,pilot->id);clickMenu(Native::MenuOp::ControlPilot,pilot->id);}
                    if(frames==800){clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Pilots));int id=game->state().pilots.back().id;clickMenu(Native::MenuOp::Account,id);clickMenu(Native::MenuOp::RemovePilot,id);if(game->state().pilots.size()!=2)throw std::runtime_error("Local lobby departure failed");lobbySmokePassed=true;}
                  }
                } else if(loadoutSmoke) {
                  if(!modernUi){
                    static Native::Loadout owned,draft;auto presetKey=[&](int bank,bool save){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_0+bank;event.key.scancode=SDL_GetScancodeFromKey(event.key.key,nullptr);event.key.mod=save?SDL_KMOD_CTRL:SDL_KMOD_NONE;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);};
                    auto tip=[&](const std::string& text){return std::count_if(game->state().messages.begin(),game->state().messages.end(),[&](const auto& m){return m.tip && !m.warning && m.text==text;});};
                    auto name=[&]{return std::string(game->core()->GetHullType(frontend.stagedLoadout().loadout.hull)->GetName());};
                    if(frames==60){owned=game->currentLoadout();clickMenu(Native::MenuOp::Navigate,int(Native::Screen::ShipLoadout));}
                    if(frames==90){draft=frontend.stagedLoadout().loadout;presetKey(9,true);}
                    if(frames==120){auto saved=Native::Loadouts::load(loadoutPath());if(!saved.find(9,draft.hull) || saved.find(9,draft.hull)->hull!=draft.hull || tip("Saved "+name()+" loadout 9")!=1)throw std::runtime_error("Classic preset Ctrl+9 loses persistent editor fitting or original console feedback");}
                    if(frames==150)clickMenu(Native::MenuOp::PreviewDefault);
                    if(frames==180)presetKey(9,false);
                    if(frames==210){if(tip("Loaded "+name()+" loadout 9")!=1 || game->currentLoadout()!=owned || !game->state().docked)throw std::runtime_error("Classic preset 9 loses original feedback or changes the live fitting");clickMenu(Native::MenuOp::PreviewDefault);}
                    if(frames==240){if(tip("Saved "+name()+" loadout 0"))throw std::runtime_error("Default button invents a keyboard preset save notification");presetKey(0,true);}
                    if(frames==270)presetKey(0,false);
                    if(frames==300){if(tip("Saved "+name()+" loadout 0")!=1 || tip("Loaded "+name()+" loadout 0")!=1)throw std::runtime_error("Classic default bank loses saved/loaded feedback");if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-presets.ppm")).string();}}
                    if(frames==330)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Hangar));
                    if(frames==360){if(frontend.screen()!=Native::Screen::Hangar || game->currentLoadout()!=owned)throw std::runtime_error("Classic preset Back loses the original hangar/fitting");fprintf(stderr,"Original loadout preset SDL: Ctrl+9/9/Ctrl+0/0, original tips, persistent banks, silent automatic defaults, immutable owned ship and Back passed\n");}
                  }else{
                    if(frames==60)clickMenu(Native::MenuOp::Loadouts);
                    if(frames==90)clickMenu(Native::MenuOp::LoadoutSlot,9);
                    if(frames==120)clickMenu(Native::MenuOp::SaveLoadout,9);
                    if(frames==150)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Hangar));
                    if(frames==180)clickMenu(Native::MenuOp::HangarHull,1);
                    if(frames==210)clickMenu(Native::MenuOp::Loadouts);
                    if(frames==250){clickMenu(Native::MenuOp::UseLoadout,9);if(game->currentLoadout().hull!=Native::hullIds[0])throw std::runtime_error("Loadout smoke fit failed");}
                    if(frames==280){auto saved=Native::Loadouts::load(loadoutPath());if(!saved.find(9,-1) || saved.find(9,-1)->hull!=Native::hullIds[0])throw std::runtime_error("Loadout smoke persistence failed");}
                  }
                } else if(cargoSmoke) {
                    if(frames==450){menuVisible=true;frontend.open(Native::Screen::Cargo,&game->state());}
                    if(frames==500)clickMenu(Native::MenuOp::CargoSelect,-1);
                    if(frames==550)clickMenu(Native::MenuOp::CargoMounts);
                    if(frames==600){auto old=game->state().equipment.front().name;clickMenu(Native::MenuOp::CargoSwap,32);if(game->state().equipment.front().name==old)throw std::runtime_error("Cargo smoke swap failed");cargoStage=1;}
                    if(frames==650)clickMenu(Native::MenuOp::CargoSelect,-2);
                    if(frames==700){int ammo=game->state().ammo;clickMenu(Native::MenuOp::Reload);if(game->state().ammo<=ammo)throw std::runtime_error("Cargo smoke manual reload failed");cargoStage=2;}
                    if(frames==750){clickMenu(Native::MenuOp::CargoDrop);if(std::any_of(game->state().equipment.begin(),game->state().equipment.end(),[](auto& e){return e.mount==-5;}))throw std::runtime_error("Cargo smoke drop failed");cargoStage=3;}
                } else if(frontSmoke) {
                if(frames==30)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Settings));
                if(frames==60){frontAction(frontend.key(SDLK_G,SDL_SCANCODE_G));frontAction(frontend.key(SDLK_K,SDL_SCANCODE_K));}
                if(frames==90){frontAction(frontend.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE));frontAction(frontend.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE));}
                if(frames==120)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Scenarios));
                if(frames==150)clickMenu(Native::MenuOp::Scenario,int(Native::Scenario::Skirmish));
                if(frames==200)clickMenu(Native::MenuOp::Ready);
                if(frames==230){clickMenu(Native::MenuOp::Start);if(!game)throw std::runtime_error("Frontend smoke: start failed");}
                if(frames==500 && !game->state().docked)throw std::runtime_error("Frontend smoke: missing starting hangar");
                if(frames==560)clickMenu(Native::MenuOp::HangarHull,2);
                if(frames==568)clickMenu(Native::MenuOp::EquipmentSlot,0);
                if(frames==578){auto& items=frontend.view().items;auto it=std::find_if(items.begin(),items.end(),[](auto& i){return (i.action.op==Native::MenuOp::EquipPart || i.action.op==Native::MenuOp::PreviewPart) && i.action.value>=0;});if(it==items.end())throw std::runtime_error("No compatible equipment");clickMenu(Native::MenuOp::EquipPart,it->action.value);}
                if(frames==582)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Cargo));
                if(frames==590)clickMenu(Native::MenuOp::EquipmentSlot,-5);
                if(frames==600) {
                    auto& items=frontend.view().items;auto it=std::find_if(items.begin(),items.end(),[](auto& i){return i.action.op==Native::MenuOp::EquipPart && i.action.value>=0;});
                    if(it==items.end())throw std::runtime_error("Frontend smoke: no cargo choices");clickMenu(Native::MenuOp::EquipPart,it->action.value);
                    if(frontend.screen()!=Native::Screen::Cargo || std::none_of(game->state().equipment.begin(),game->state().equipment.end(),[](auto& e){return e.mount==-5;}))
                        throw std::runtime_error("Frontend smoke: cargo installation failed");
                }
                if(frames==620)clickMenu(Native::MenuOp::EquipmentSlot,-5);
                if(frames==630) {
                    clickMenu(Native::MenuOp::EquipPart,-1);
                    if(std::any_of(game->state().equipment.begin(),game->state().equipment.end(),[](auto& e){return e.mount==-5;}))throw std::runtime_error("Frontend smoke: cargo clearing failed");
                }
                if(frames==645)clickMenu(Native::MenuOp::EquipmentSlot,-5);
                if(frames==650) {
                    auto& items=frontend.view().items;auto it=std::find_if(items.begin(),items.end(),[](auto& i){return i.action.op==Native::MenuOp::EquipPart && i.action.value>=0;});
                    if(it==items.end())throw std::runtime_error("Frontend smoke: no reserve for flight");clickMenu(Native::MenuOp::EquipPart,it->action.value);
                }
                if(frames==660)clickMenu(Native::MenuOp::Navigate,int(Native::Screen::Hangar));
                if(frames==700){clickMenu(Native::MenuOp::Launch);if(game->state().docked)throw std::runtime_error("Frontend smoke: launch failed");}
                if(frames==750){menuVisible=true;frontend.open(Native::Screen::Pause,&game->state());}
                if(frames==780)frontAction(frontend.key(SDLK_Q,SDL_SCANCODE_Q));
                if(frames==810){clickMenu(Native::MenuOp::Leave);if(game)throw std::runtime_error("Frontend smoke: leave failed");}
                if(frames==850)clickMenu(Native::MenuOp::Scenario,int(Native::Scenario::FlightTraining));
                if(frames==880)clickMenu(Native::MenuOp::Ready);
                if(frames==910){clickMenu(Native::MenuOp::Start);if(!game)throw std::runtime_error("Frontend smoke: restart failed");}
                if(frames==1300){menuVisible=true;frontend.open(Native::Screen::Pause,&game->state());}
                if(frames==1340)frontAction(frontend.key(SDLK_G,SDL_SCANCODE_G));
                if(frames==1380)frontAction(frontend.key(SDLK_C,SDL_SCANCODE_C));
                }
            }
            if(teamSmoke && game && acquired) {
                auto choose=[&](Native::MenuOp op,int value=0) {
                    auto& items=frontend.view().items;
                    for(size_t i=0;i<items.size();++i)if(items[i].action.op==op && items[i].action.value==value && items[i].enabled){frontAction(frontend.activate(i));return;}
                    throw std::runtime_error("Team smoke menu action unavailable");
                };
                if(frames==120){frontend.open(Native::Screen::Pilots,&game->state());choose(Native::MenuOp::Account,game->state().pilots.back().id);}
                if(frames==180)choose(Native::MenuOp::Donate,game->state().pilots.back().id);
                if(frames==240)choose(Native::MenuOp::Wing,game->state().pilots.back().id);
                if(frames==360)choose(Native::MenuOp::HandOver,game->state().pilots.back().id);
                if(frames==420 && game->state().commander)throw std::runtime_error("Team smoke command handover failed");
                if(frames==450){
                    teamRequester=game->state().shipId;teamApprover=game->state().pilots.back().id;teamRequestBalance=game->pilotBalance(teamRequester);
                    if(!game->requestMoney(500) || !game->controlLocalPilot(teamApprover) || game->state().moneyRequest.sender!=teamRequester)throw std::runtime_error("Team smoke financial request did not follow pilot control");
                    frontend.open(Native::Screen::PilotOrders,&game->state());
                }
                if(frames==480)choose(Native::MenuOp::AcceptOrder);
                if(frames==510 && (game->state().moneyRequest.sender>=0 || game->pilotBalance(teamRequester)!=teamRequestBalance+500))throw std::runtime_error("Team smoke command menu failed to approve money");
                if(frames==530){
                    teamRequestBalance=game->pilotBalance(teamRequester);
                    if(!game->controlLocalPilot(teamRequester) || !game->requestMoney(500) || !game->controlLocalPilot(teamApprover))throw std::runtime_error("Team smoke in-flight financial fixture failed");
                    game->launch();for(int i=0;i<32;++i)game->step({},.1);menuVisible=false;
                    auto display=settings;display.visual.centerHud=false;hud.configure(display);
                }
                if(frames==570){
                    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=settings.keys[size_t(Native::Binding::AcceptOrder)];event.key.key=SDL_GetKeyFromScancode(event.key.scancode,SDL_KMOD_NONE,false);SDL_PushEvent(&event);
                    event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);
                }
                if(frames==590 && (game->state().moneyRequest.sender>=0 || game->pilotBalance(teamRequester)!=teamRequestBalance+500))throw std::runtime_error("Team smoke accept key failed to approve in-flight money request");
                if(frames==620){
                    if(!game->controlLocalPilot(teamRequester) || !game->requestMoney(250) || !game->controlLocalPilot(teamApprover))throw std::runtime_error("Team smoke final request display fixture failed");
                }
                if(frames==630)fprintf(stderr,"Team smoke: accounts, command handover, money/order menu and in-flight accept key with center HUD disabled passed\n");
            }
            if(commandSmoke && game && network) {
                if(frames==30){SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.key=SDLK_F2;key.key.scancode=SDL_SCANCODE_F2;SDL_PushEvent(&key);}
                if(frames==60 && (!game->state().docked || !game->state().commandView || menuVisible))throw std::runtime_error("Network command smoke: F2 did not open docked command view");
            }
            if(commandSmoke && game && !network) {
                int w,h;SDL_GetWindowSize(window.get(),&w,&h);SDL_Event input{};
                auto button=[&](bool down,Uint8 button,float x,float y){input.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;input.button.button=button;input.button.x=x*w;input.button.y=y*h;SDL_PushEvent(&input);};
                auto move=[&](float x,float y){input.type=SDL_EVENT_MOUSE_MOTION;input.motion.x=x*w;input.motion.y=y*h;SDL_PushEvent(&input);};
                if(frames==60)button(true,SDL_BUTTON_LEFT,.18f,.24f);
                if(frames==100)move(.7f,.72f);
                if(frames==140)button(false,SDL_BUTTON_LEFT,.7f,.72f);
                if(frames==160 && game->commandUnits().size()<2)throw std::runtime_error("Commander smoke: group rectangle failed");
                if(frames==240){game->commandOrder(5);button(true,SDL_BUTTON_RIGHT,.55f,.55f);}
                if(frames==280)move(.55f,.35f);
                if(frames==640)button(false,SDL_BUTTON_RIGHT,.55f,.35f);
            }
            if(trainingTerminalSmoke && acquired){
                auto escape=[] {SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_ESCAPE;SDL_PushEvent(&e);};
                auto advance=[&](int stage){trainingTerminalStage=stage;trainingTerminalFrame=frames;};
                if(frames==20){game->eject();advance(1);}
                if(frames==30 && (!game->state().cinematicDeath || game->state().complete))throw std::runtime_error("Training terminal fixture omitted the death camera");
                // Follow the authored transitions, not a frame count calibrated
                // for one monitor. The Mac reaches and can finish the debrief
                // much earlier in frame numbers than the 165 Hz Linux display.
                if(trainingTerminalStage==1 && game && game->state().complete && slides && slidesPost) {
                    if(game->state().result!="Training ended: ship destroyed")throw std::runtime_error("Arena death entered the wrong authored debrief");
                    advance(2);
                } else if(trainingTerminalStage==2 && frames>=trainingTerminalFrame+2) {
                    escape();advance(3);
                } else if(trainingTerminalStage==3 && frames>=trainingTerminalFrame+2) {
                    if(game || slides || !menuVisible || frontend.screen()!=Native::Screen::Training)throw std::runtime_error("Training debrief did not release the session and return to training");
                    auto& items=frontend.view().items;auto lesson=std::find_if(items.begin(),items.end(),[](auto& item){return item.enabled && (item.action.op==Native::MenuOp::Training || item.action.op==Native::MenuOp::TrainingSelect) && item.action.value==8;});
                    if(lesson==items.end())throw std::runtime_error("Training menu has no nanite lesson");
                    auto size=renderer.size();auto layout=frontend.view().original?Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas):Native::menuLayout(size,settings.uiScale);auto p=layout.pixel(glm::vec2(lesson->bounds)+glm::vec2(lesson->bounds.z,lesson->bounds.w)*.5f);
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);SDL_Event e{};e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x*w/size.x;e.button.y=p.y*h/size.y;
                    // Exercise the menu's persistence separately from the
                    // prior CLI training start which has already cleared it.
                    settings.showStartupTrainingSuggestion=true;settings.save(configPath);
                    // A real double click first selects the lesson on single
                    // click release, then emits its separate launch event.
                    for(int clicks:{1,2}){e.button.clicks=clicks;e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);}advance(4);
                } else if(trainingTerminalStage==4 && frames>=trainingTerminalFrame+2) {
                    if(game || !slides || slidesPost || slides->index()!=0 || slides->all().size()!=5)throw std::runtime_error("Training menu did not start the selected authored introduction");
                    if(settings.showStartupTrainingSuggestion || Native::Settings::load(configPath).showStartupTrainingSuggestion)throw std::runtime_error("Training menu launch did not persist the original suggestion preference");
                    if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-nanite-introduction.ppm")).string();}
                    escape();advance(8);
                } else if(trainingTerminalStage>=8 && trainingTerminalStage<=11 && frames>=trainingTerminalFrame+2) {
                    size_t expected=trainingTerminalStage-7;
                    if(!slides || slides->finished() || slides->index()!=expected || game)throw std::runtime_error("Original slideshow key/button down did not advance exactly one slide");
                    SDL_Event e{};
                    if(trainingTerminalStage==8){e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_ESCAPE;e.key.repeat=true;}
                    else if(trainingTerminalStage==9 || trainingTerminalStage==10){e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=trainingTerminalStage==9?SDL_BUTTON_RIGHT:SDL_BUTTON_MIDDLE;}
                    else {e.type=SDL_EVENT_KEY_DOWN;e.key.key=SDLK_SPACE;}
                    SDL_PushEvent(&e);advance(trainingTerminalStage+1);
                } else if(trainingTerminalStage==12 && slides && slides->finished() && trainingStartAt>=0) {
                    if(!frontend.hasMessageBox() || game || std::none_of(frontend.messageView().artwork.begin(),frontend.messageView().artwork.end(),[](const auto& c){return c.text=="Training simulation initiated...";}))throw std::runtime_error("Original training startup message is absent before mission loading");
                    if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-starting.ppm")).string();}
                    escape();advance(5);
                } else if(trainingTerminalStage==5 && !slides && game) {
                    if(game->state().trainingMission!=8 || game->state().complete || frontend.hasMessageBox())throw std::runtime_error("Finishing the introduction did not release its startup message and start a fresh nanite lesson");
                    frontAction({Native::FrontEffect::Leave});advance(6);
                } else if(trainingTerminalStage==6 && frames>=trainingTerminalFrame+2) {
                    if(game || slides || frontend.screen()!=Native::Screen::Training)throw std::runtime_error("Leaving an authored lesson did not return to training");
                    advance(7);frameLimit=frames+1;
                    fprintf(stderr,"Training terminal smoke: arena death/debrief, authored menu double click, five-slide nanite introduction, Escape/repeated key/right/middle/Space advance, presented startup lockdown, fresh lesson and explicit exit passed\n");
                }
            }
            if(eliminationSmoke && acquired) {
                if(frames==20) {
                    Native::prepareEliminationSmoke(*game);menuVisible=false;paused=help=false;
                }
                if(frames==80)Native::loseEliminationSmokeStations(*game);
                auto modalHit=[&](float x,Native::HudCommand command) {
                    auto size=renderer.size();
                    float scale=Native::interfaceScale(size,settings.uiScale);
                    auto hit=hud.click(glm::vec2(size)*.5f+glm::vec2(x,59.f)*scale);
                    if(!hit || hit->command!=command)throw std::runtime_error("Eliminated HUD control cannot be hit");action(*hit);
                };
                if(frames==120){modalHit(-145,Native::HudCommand::Scores);if(frontend.screen()!=Native::Screen::Scores)throw std::runtime_error("Eliminated HUD cannot open team scores");}
                if(frames==170){frontAction(frontend.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE));if(menuVisible)throw std::runtime_error("Eliminated observer was mistaken for a finished match");}
                if(frames==200){modalHit(145,Native::HudCommand::SessionMenu);if(frontend.screen()!=Native::Screen::Pause)throw std::runtime_error("Eliminated HUD cannot open its session menu");}
                if(frames==220){
                    for(auto& item:frontend.view().items)if(item.action.op==Native::MenuOp::ReturnToBase && item.enabled)throw std::runtime_error("Eliminated session menu offers an unusable return-to-base action");
                    fprintf(stderr,"Elimination smoke: observer relocation, crew reset, continuing teams, HUD score/menu hits and inactive controls passed\n");
                }
            }
            if(resultsUiSmoke && acquired) {
                auto outside=[&](Uint8 button){auto layout=Native::originalMenuLayout(renderer.size(),frontend.view().classicScale,frontend.view().canvas);auto at=layout.pixel({790,590});int w,h;SDL_GetWindowSize(window.get(),&w,&h);at*=glm::vec2(w,h)/glm::vec2(renderer.size());SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=at.x;e.motion.y=at.y;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=button;e.button.clicks=1;e.button.x=at.x;e.button.y=at.y;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);};
                auto click=[&](Native::MenuOp op,int value=0,int delta=-999,Uint8 button=SDL_BUTTON_LEFT,Uint8 clicks=1){auto& items=frontend.view().items;auto found=std::find_if(items.begin(),items.end(),[&](auto& i){return i.enabled && i.action.op==op && i.action.value==value && (delta==-999 || i.action.delta==delta);});if(found==items.end())throw std::runtime_error("Results UI smoke: missing authored action");auto layout=Native::originalMenuLayout(renderer.size(),frontend.view().classicScale,frontend.view().canvas);auto local=glm::vec2(found->bounds)+(op==Native::MenuOp::ListClear?glm::vec2(3,found->bounds.w-3):glm::vec2(found->bounds.z,found->bounds.w)*.5f);glm::vec2 at=layout.pixel(local);int w,h;SDL_GetWindowSize(window.get(),&w,&h);at*=glm::vec2(w,h)/glm::vec2(renderer.size());SDL_Event event{};event.type=SDL_EVENT_MOUSE_MOTION;event.motion.x=at.x;event.motion.y=at.y;SDL_PushEvent(&event);event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=button;event.button.clicks=clicks;event.button.x=at.x;event.button.y=at.y;SDL_PushEvent(&event);event.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&event);};
                if(frames==20){for(int i=0;i<32;++i)if(game->addLocalPilot("Test pilot "+std::to_string(i),i%2)<0)throw std::runtime_error("Results UI pilot fixture failed");}
                if(frames==30){Native::completeResultsUiSmoke(*game);frontend.open(Native::Screen::Results,&game->state());menuVisible=true;}
                if(frames==60 && (!frontend.view().original || frontend.screen()!=Native::Screen::Results || !game->state().endgame.frozen || game->state().endgame.players.size()!=33))throw std::runtime_error("Victory did not show authored postgame tables");
                if((frames==80 || frames==150) && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+(frames==80?"-table.ppm":"-scrolled.ppm"))).string();}
                if(frames==90){auto row=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::ResultsSelect && i.action.delta==0;});if(row==frontend.view().items.end())throw std::runtime_error("Results UI player-selection fixture missing");click(Native::MenuOp::ResultsSelect,row->action.value);}
                if(frames==95){SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text="Original postgame draft";SDL_PushEvent(&event);}
                if(frames==100)click(Native::MenuOp::ResultsSort,1);
                if(frames==105 && (!frontend.wantsText() || frontend.chatText()!="Original postgame draft"))throw std::runtime_error("Original postgame row/header selection loses actual SDL chat input");
                if(frames==110){auto row=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::ResultsSelect && i.action.delta==0;});if(row==frontend.view().items.end())throw std::runtime_error("Results right-click fixture missing");click(Native::MenuOp::ResultsSelect,row->action.value,0,SDL_BUTTON_RIGHT,2);}
                if(frames==115){auto first=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::ResultsSelect && i.action.delta==0;});if(first==frontend.view().items.end() || !first->selected || frontend.keyboardPopup() || frontend.chatText()!="Original postgame draft")throw std::runtime_error("Actual results double-right click fails original selection/draft/no-popup behavior");}
                if(frames==120)click(Native::MenuOp::ScrollPage,-3,1);
                if(frames==130)click(Native::MenuOp::ResultsSelect,1,1,SDL_BUTTON_RIGHT);
                if(frames==140)click(Native::MenuOp::ListClear,int(Native::MenuOp::ResultsSelect),1,SDL_BUTTON_RIGHT);
                if(frames==145 && (std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::ResultsSelect && i.action.delta==1 && i.selected;}) || frontend.chatText()!="Original postgame draft"))throw std::runtime_error("Actual blank results right-click retains team selection or loses its draft");
                if(frames==160)click(Native::MenuOp::SaveStats);
                if(frames==200){if(std::none_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](auto& c){return c.name.starts_with("resultChat/") && c.text.find("scores saved to")!=std::string::npos;}))throw std::runtime_error("Original Save Stats did not write and report its result");click(Native::MenuOp::ResultsBack);}
                if(frames==240){if(game || frontend.screen()!=Native::Screen::Games || !menuVisible)throw std::runtime_error("Standalone Conquest postgame Back did not return to Games");frontend.open(Native::Screen::Settings);}
                if(frames==245)click(Native::MenuOp::PopupItem,0,0,SDL_BUTTON_MIDDLE);
                if(frames==250){if(frontend.screen()!=Native::Screen::Settings || !frontend.keyboardPopup())throw std::runtime_error("Inside middle click dismisses the actual original menu");if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-menus.ppm")).string();}outside(SDL_BUTTON_RIGHT);}
                if(frames==260){if(frontend.screen()!=Native::Screen::Games || frontend.keyboardPopup())throw std::runtime_error("Outside right click fails to dismiss the actual original menu cascade");frontend.open(Native::Screen::Settings);frontend.key(SDLK_G,SDL_SCANCODE_G);frontend.key(SDLK_Q,SDL_SCANCODE_Q);}
                if(frames==265)outside(SDL_BUTTON_MIDDLE);
                if(frames==275){if(frontend.screen()!=Native::Screen::Games || frontend.keyboardPopup())throw std::runtime_error("Outside middle click fails to dismiss the actual original menu cascade");resultsUiSmokePassed=true;frameLimit=frames+10;fprintf(stderr,"Results UI smoke: actual victory, original artwork/tables, header sorting, scrollbar, single/double/blank right clicks, preserved chat, XML Save Stats, Back to Games and actual inside/outside right/middle menu routing passed\n");}
            }
            if(lanSmoke && acquired){
                static int stage=0,row=-1;static unsigned changed=0;
                auto click=[&](Native::MenuOp op,int value,int clicks=1){
                    const auto& view=frontend.view();auto i=std::find_if(view.items.begin(),view.items.end(),[&](const auto& i){return i.enabled && i.action.op==op && i.action.value==value;});
                    if(i==view.items.end())throw std::runtime_error("LAN SDL smoke control missing");
                    auto size=renderer.size();auto p=Native::originalMenuLayout(size,view.classicScale,view.canvas).pixel(glm::vec2(i->bounds)+glm::vec2(i->bounds.z,i->bounds.w)*.5f);int w,h;SDL_GetWindowSize(window.get(),&w,&h);p*=glm::vec2(w,h)/glm::vec2(size);
                    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.clicks=clicks;e.button.x=p.x;e.button.y=p.y;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);
                };
                if(stage==0 && frames>=10){click(Native::MenuOp::Navigate,int(Native::Screen::Online));stage=1;}
                else if(stage==1 && frontend.screen()==Native::Screen::Online){
                    auto found=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.enabled && i.action.op==Native::MenuOp::LanSelect && i.action.value>=0;});
                    if(found!=frontend.view().items.end()){row=found->action.value;click(Native::MenuOp::LanSelect,row);changed=frames;stage=2;}
                }else if(stage==2 && frames>=changed+30){
                    if(!frontend.view().original || frontend.wantsText() || frontend.pointerCaptured() || std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::LanJoin && i.enabled;}))throw std::runtime_error("LAN discovered row fails original selection/Join state");
                    if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-lan.ppm")).string();}
                    changed=frames;stage=3;
                }else if(stage==3 && frames>=changed+30){click(Native::MenuOp::LanSelect,row,2);changed=frames;stage=4;}
                else if(stage==4 && frames>=changed+10){
                    if(network || game || frontend.screen()!=Native::Screen::Home || !frontend.keyboardPopup() || std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::LogonAccept && i.enabled;}))throw std::runtime_error("LAN selection bypasses original callsign dialog");
                    if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-logon.ppm")).string();}
                    click(Native::MenuOp::LogonName,0);changed=frames;stage=6;
                }else if(stage==6 && frames>=changed+5){
                    if(!frontend.wantsText())throw std::runtime_error("Actual callsign Name click does not focus text input");
                    SDL_Event e{};e.type=SDL_EVENT_TEXT_INPUT;e.text.text=" Pilot";SDL_PushEvent(&e);changed=frames;stage=7;
                }else if(stage==7 && frames>=changed+5){click(Native::MenuOp::LogonAccept,0);stage=8;}
                else if(stage==8 && network && network->ready() && game && game->state().networkLobby){
                    if(frontend.screen()!=Native::Screen::RemoteLobby || !frontend.view().original || network->metrics().missingBaselines)throw std::runtime_error("LAN SDL double-click join fails to reach a coherent original team lobby");
                    if(game->sessionOptions().pilot!=settings.pilot || !settings.pilot.ends_with(" Pilot") || Native::Settings::load(configPath).pilot!=settings.pilot)throw std::runtime_error("Actual callsign Logon fails to persist identity or use it in the encrypted admission");
                    lanSmokePassed=true;stage=5;frameLimit=frames+30;fprintf(stderr,"LAN SDL smoke: original main menu/discovery/list selection, callsign artwork/Name click/text/Logon, saved identity, encrypted admission and original remote lobby passed\n");
                }
            }
            if(customMapSmoke && acquired){
                static int stage=0,teams=2;static unsigned changed=0;static std::string filename,name;static const Native::Game* preparedIdentity=nullptr;
                auto advance=[&](int value){stage=value;changed=frames;};
                auto key=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;e.key.scancode=SDL_GetScancodeFromKey(code,nullptr);SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto capture=[&](const char* suffix){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+suffix+".ppm")).string();}};
                auto click=[&](std::string_view control){const auto& menu=frontend.view();auto item=std::find_if(menu.items.begin(),menu.items.end(),[&](const auto& i){return i.control==control && i.enabled;});if(item==menu.items.end())throw std::runtime_error("Custom-map SDL smoke: original control absent or disabled: "+std::string(control));
                    auto size=renderer.size();auto at=Native::originalMenuLayout(size,menu.classicScale,menu.canvas).pixel(glm::vec2(item->bounds)+glm::vec2(item->bounds.z,item->bounds.w)*.5f);int w,h;SDL_GetWindowSize(window.get(),&w,&h);at*=glm::vec2(w,h)/glm::vec2(size);
                    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.clicks=1;e.button.x=at.x;e.button.y=at.y;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);
                };
                auto select=[&](std::string_view control,int row){
                    auto item=std::find_if(frontend.view().items.begin(),frontend.view().items.end(),[&](const auto& i){return i.control==control && i.action.op==Native::MenuOp::SetupCombo;});
                    if(item==frontend.view().items.end())throw std::runtime_error("Custom-map SDL smoke: missing combo");
                    // Small original menus support arrows/Enter, not Home.
                    int current=0;if(control=="teamCountComboPane")current=frontend.session().rules.teams-2;
                    click(control);for(int i=0;i<current;++i)key(SDLK_UP);for(int i=0;i<row;++i)key(SDLK_DOWN);key(SDLK_RETURN);
                };
                if(stage==0 && frames==30){auto maps=Native::Game::customMaps(artwork);if(maps.empty())throw std::runtime_error("Custom-map SDL smoke requires the installed catalog");filename=maps.back().file;name=maps.back().name;teams=maps.back().teams;
                    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.name="Custom maps";options.rules.miners=0;options.rules.startCountdown=1;frontend.networkSession(options);frontend.open(Native::Screen::Scenarios);frontend.open(Native::Screen::Lobby);menuVisible=true;click("mapTypeComboPane");advance(1);
                }else if(stage==1 && frames==changed+5)capture("-catalog");
                else if(stage==1 && frames>changed+10){if(!frontend.keyboardPopup())throw std::runtime_error("Original custom-map combo did not open; screen="+std::to_string(int(frontend.screen()))+", relative="+std::to_string(SDL_GetWindowRelativeMouseMode(window.get())));key(SDLK_END);key(SDLK_RETURN);advance(2);}
                else if(stage==2 && frames==changed+5)capture("-map");
                else if(stage==2 && frames>changed+15){if(frontend.session().customMap!=filename)throw std::runtime_error("Original map combo cannot reach its final catalog entry");select("teamCountComboPane",teams==2?1:0);advance(3);}
                else if(stage==3 && frames>changed+10){click("createButtonPane");advance(4);}
                else if(stage==4 && frames==changed+5)capture("-error");
                else if(stage==4 && frames>changed+10){if(!frontend.hasMessageBox() || frontend.screen()!=Native::Screen::Lobby)throw std::runtime_error("Wrong custom-map team count did not retain setup with its original message box");key(SDLK_RETURN);advance(5);}
                else if(stage==5 && frames>changed+10){select("teamCountComboPane",teams-2);click("createButtonPane");advance(6);}
                else if(stage==6 && frames>changed+10){if(frontend.screen()!=Native::Screen::LobbyPilots || frontend.session().customMap!=filename)throw std::runtime_error("Valid custom map did not reach original team lobby");click("sideReadyButtonPane");advance(7);}
                else if(stage==7 && frames>changed+10){click("startButtonPane");advance(8);}
                else if(stage==8 && frames>changed+15){if(frontend.screen()!=Native::Screen::Countdown || !std::any_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[&](const auto& c){return c.name=="missionBriefingMapStr" && c.text==name;}))throw std::runtime_error("Custom-map briefing loses the catalog's friendly name");if(!preparedGame)throw std::runtime_error("Briefing did not prepare the original IGC mission");preparedIdentity=preparedGame.get();auto info=preparedGame->missionBriefing(frontend.session().team);if(!info || std::none_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[&](const auto& c){return c.name=="missionBriefingTextBody" && c.text.find(info->text)!=std::string::npos;}))throw std::runtime_error("Briefing loses the actual prepared home sector");capture("-briefing");advance(9);}
                else if(stage==9 && game && game->state().docked && menuVisible && frontend.screen()==Native::Screen::Hangar){if(game.get()!=preparedIdentity || game->sessionOptions().customMap!=filename)throw std::runtime_error("Custom-map launch replaced the prepared briefing world");capture("-hangar");advance(12);}
                else if(stage==12 && frames>changed+10){click("launchButtonPane");advance(10);}
                else if(stage==10 && frames==changed+30){if(!game->state().cinematic || !game->playerShip()->GetVisibleF() || game->scene(true).size()!=game->scene(false).size()+1)throw std::runtime_error("Custom-map launch cinematic omits the player's ship");capture("-launch");}
                else if(stage==10 && !game->state().docked && !game->state().cinematic){customMapSmokePassed=true;frameLimit=frames+30;advance(11);fprintf(stderr,"Original custom-map SDL smoke: full catalog, End selection, authored preview, team-count rejection, corrected acceptance, lobby/briefing, actual file-backed hangar and visible launch passed\n");}
            }
            if(pingsSmoke && acquired && network && network->ready()){
                static int stage=0;static unsigned changed=0;
                auto advance=[&](int value){stage=value;changed=frames;};
                auto key=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;e.key.scancode=SDL_GetScancodeFromKey(code,nullptr);SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto pause=[&]{auto i=size_t(Native::Binding::Pause);SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=settings.keys[i];e.key.key=SDL_GetKeyFromScancode(e.key.scancode,SDL_KMOD_NONE,true);SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto capture=[&](const char* suffix){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+suffix+".ppm")).string();}};
                auto check=[&](bool flight){
                    const auto& view=frontend.view();auto has=[&](std::string_view text){return std::any_of(view.artwork.begin(),view.artwork.end(),[&](const auto& c){return c.name=="detailsText" && c.text.find(text)!=std::string::npos;});};
                    if(!menuVisible || frontend.screen()!=Native::Screen::GameDetails || !view.original || view.overlay!=flight || frontend.wantsText() || !has("Connection Info of Players") || !has("Your Connection Info"))throw std::runtime_error("Original Player Pings popup loses its report, input capture or docked/flight parent");
                };
                if(stage==0 && frames>120 && game->state().docked && frontend.screen()==Native::Screen::Hangar && !frontend.hasMessageBox()){pause();advance(1);}
                else if((stage==1 || stage==6) && frames>changed+5){if(!frontend.keyboardPopup())throw std::runtime_error("Player Pings SDL main menu absent");key(SDLK_P);advance(stage+1);}
                else if(stage==2 && frontend.screen()==Native::Screen::GameDetails){check(false);capture("-docked");SDL_Event text{};text.type=SDL_EVENT_TEXT_INPUT;text.text.text="Ignored report input";SDL_PushEvent(&text);advance(3);}
                else if(stage==3 && frames>changed+15){check(false);key(SDLK_ESCAPE);advance(4);}
                else if(stage==4 && frames>changed+10){
                    if(frontend.screen()!=Native::Screen::Hangar || !menuVisible || !game->state().docked)throw std::runtime_error("Connection report close loses hangar parent");
                    const auto& menu=frontend.view();auto item=std::find_if(menu.items.begin(),menu.items.end(),[](const auto& i){return i.enabled && i.action.op==Native::MenuOp::Launch;});if(item==menu.items.end())throw std::runtime_error("Original hangar Launch control absent");
                    auto size=renderer.size();auto at=Native::originalMenuLayout(size,menu.classicScale,menu.canvas).pixel(glm::vec2(item->bounds)+glm::vec2(item->bounds.z,item->bounds.w)*.5f);int w,h;SDL_GetWindowSize(window.get(),&w,&h);at*=glm::vec2(w,h)/glm::vec2(size);
                    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.clicks=1;e.button.x=at.x;e.button.y=at.y;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);advance(5);
                }else if(stage==5 && !game->state().docked && !game->state().cinematic){pause();advance(6);}
                else if(stage==7 && frontend.screen()==Native::Screen::GameDetails){check(true);capture("-flight");advance(8);}
                else if(stage==8 && frames>changed+15){check(true);key(SDLK_RETURN);advance(9);}
                else if(stage==9 && frames>changed+10){if(menuVisible || game->state().docked)throw std::runtime_error("Player Pings Enter does not resume flight");pingsSmokePassed=true;frameLimit=frames+30;advance(10);fprintf(stderr,"Player Pings SDL smoke: actual Escape/P, encrypted report, original colors/fonts/artwork, read-only capture, docked Escape, original Launch and flight Enter/resume passed\n");}
            }
            if(findPlayerSmoke && acquired){
                static int stage=0;static unsigned changed=0;
                auto advance=[&](int next){stage=next;changed=frames;};
                auto click=[&](std::string_view control){
                    const auto& menu=frontend.view();auto item=std::find_if(menu.items.begin(),menu.items.end(),[&](const auto& i){return i.control==control && i.enabled && !i.blocked;});
                    if(item==menu.items.end())throw std::runtime_error("Find Player SDL control absent: "+std::string(control));
                    auto size=renderer.size();auto at=Native::originalMenuLayout(size,menu.classicScale,menu.canvas).pixel(glm::vec2(item->bounds)+glm::vec2(item->bounds.z,item->bounds.w)*.5f);int w,h;SDL_GetWindowSize(window.get(),&w,&h);at*=glm::vec2(w,h)/glm::vec2(size);
                    SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;event.button.clicks=1;event.button.x=at.x;event.button.y=at.y;SDL_PushEvent(&event);event.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&event);
                };
                auto key=[&](SDL_Keycode key,SDL_Scancode scan){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=key;event.key.scancode=scan;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);};
                auto type=[&](const char* text){SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.text=text;SDL_PushEvent(&event);};
                auto capture=[&](const char* suffix){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+suffix+".ppm")).string();}};
                auto message=[&](std::string_view caption){return std::any_of(frontend.messageView().artwork.begin(),frontend.messageView().artwork.end(),[&](const auto& c){return c.text==caption;});};
                if(stage==0 && frames==20){frontend.open(Native::Screen::Games);menuVisible=true;}
                if(stage==0 && frames>20 && std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::GameSelect && i.action.value>=0;})){click("findPlayerButtonPane");advance(1);}
                else if(stage==1 && frames>changed+5){if(!frontend.wantsText())throw std::runtime_error("Actual Find dialog lacks edit focus");type("lOOKUPfIXTURE");advance(2);}
                else if(stage==2){if(frames==changed+10)capture("-dialog");if(frames>changed+15){capture("-pending");key(SDLK_RETURN,SDL_SCANCODE_RETURN);advance(3);}}
                else if(stage==3 && message("Player found!")){
                    if(frontend.waitCursor() || !std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[](const auto& i){return i.action.op==Native::MenuOp::GameSelect && i.selected;}))throw std::runtime_error("Actual Find response retains wait cursor or fails to select its authoritative game");
                    capture("-found");advance(4);
                }
                else if(stage==4 && frames>changed+10){key(SDLK_RETURN,SDL_SCANCODE_RETURN);advance(12);}
                else if(stage==12 && frames>changed+5){if(frontend.hasMessageBox())throw std::runtime_error("Find acknowledgment Enter fails to close its message");click("findPlayerButtonPane");advance(5);}
                else if(stage==5 && frames>changed+5){for(size_t i=0;i<std::string_view("lOOKUPfIXTURE").size();++i)key(SDLK_BACKSPACE,SDL_SCANCODE_BACKSPACE);type("MissingLookupPilot");advance(6);}
                else if(stage==6 && frames>changed+15){key(SDLK_RETURN,SDL_SCANCODE_RETURN);advance(7);}
                else if(stage==7 && message("The player was not found.")){capture("-not-found");advance(13);}
                else if(stage==13 && frames>changed+10){key(SDLK_RETURN,SDL_SCANCODE_RETURN);advance(8);}
                else if(stage==8 && frames>changed+10){click("findPlayerButtonPane");advance(9);}
                else if(stage==9 && frames>changed+5){if(frontend.playerSearchName()!="MissingLookupPilot")throw std::runtime_error("Find dialog reopening loses its retained name");click("findCancelButtonPane");advance(10);}
                else if(stage==10 && frames>changed+10){
                    if(frontend.keyboardPopup() || frontend.wantsText() || frontend.screen()!=Native::Screen::Games)throw std::runtime_error("Find cancel retains a popup/edit or leaves the original Games screen");
                    findPlayerSmokePassed=true;frameLimit=frames+10;fprintf(stderr,"Find Player SDL smoke: authored dialog and retained edit, actual clicks/text/Enter/Backspace, encrypted exact-name success and failure, selected game, original messages/wait-cursor policy, acknowledgment and cancel passed\n");advance(11);
                }
            }
            if(dockViewSmoke && acquired){
                if(frames==25){
                    frontend.open(Native::Screen::ShipLoadout,&game->state());frontend.open(Native::Screen::Settings,&game->state());frontend.open(Native::Screen::Controls,&game->state());
                    frontend.key(SDLK_DOWN,SDL_SCANCODE_DOWN);frontend.key(SDLK_RETURN,SDL_SCANCODE_RETURN);
                    if(!frontend.capturingKey())throw std::runtime_error("Dock-view smoke must enter live mapping capture");
                    frontend.purchasing(false,901);
                    TRef<IstationIGC> station=game->playerShip()->GetStation();game->core()->GetIgcSite()->KillStationEvent(station,nullptr,1);
                }
                if(frames==35 && (menuVisible || game->state().docked || frontend.hasMessageBox() || frontend.capturingKey() || frontend.pointerCaptured()))throw std::runtime_error("Base loss leaves the actual client in a docked editor/modal or mapping capture");
                if(frames==60){
                    auto options=game->sessionOptions();startSession(options);
                    frontend.open(Native::Screen::Research,&game->state());frontend.open(Native::Screen::GameDetails,&game->state());
                    game->launch();game->eject();game->eject();
                }
                if(frames==75 && (menuVisible || !game->state().ghost || !game->state().commandView || !game->state().docked || game->state().complete || frontend.keyboardPopup()))throw std::runtime_error("Exhausted-life rescue leaves the actual client beneath a stale docked popup");
                if(frames==100){
                    frontend.open(Native::Screen::GameDetails,&game->state());menuVisible=true;
                    TRef<IstationIGC> station=game->playerShip()->GetStation();game->core()->GetIgcSite()->KillStationEvent(station,nullptr,1);
                    game->step({},.001);
                    if(!game->state().docked || !game->state().ghost || !game->state().commandView || game->playerShip()->GetStation()==station)throw std::runtime_error("Exhausted observer fails to relocate to a surviving own base");
                }
                if(frames==110){
                    if(!menuVisible || frontend.screen()!=Native::Screen::GameDetails)throw std::runtime_error("Observer base-to-base evacuation incorrectly discards its read-only popup");
                    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_RETURN;event.key.scancode=SDL_SCANCODE_RETURN;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);
                }
                if(frames==120){
                    if(menuVisible || !game->state().commandView || !game->state().ghost)throw std::runtime_error("Original Details Enter fails to resume the evacuated observer");
                    dockViewSmokePassed=true;frameLimit=frames+10;
                    fprintf(stderr,"Dock-view SDL smoke: actual base loss, nested loadout/options/mapping retirement, pending-dialog and key capture release, exhausted-life command recovery, observer relocation and Details Enter/resume passed\n");
                }
            }
            if(crewUiSmoke && acquired) {
                static int ownHull=-1,parent=-1;static unsigned flight=0;
                auto click=[&](Native::MenuOp op,int value){auto size=renderer.size();const auto& menu=frontend.view();auto item=std::find_if(menu.items.begin(),menu.items.end(),[&](auto& i){return i.enabled && i.action.op==op && i.action.value==value;});
                    if(item==menu.items.end())throw std::runtime_error("Crew UI smoke: required original control missing or disabled");auto p=Native::originalMenuLayout(size,menu.classicScale,menu.canvas).pixel(glm::vec2(item->bounds)+glm::vec2(item->bounds.z,item->bounds.w)*.5f);int w,h;SDL_GetWindowSize(window.get(),&w,&h);p*=glm::vec2(w,h)/glm::vec2(size);
                    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.clicks=1;e.button.x=p.x;e.button.y=p.y;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);};
                auto key=[&](Native::Binding binding){auto i=size_t(binding);auto bits=settings.modifiers[i];SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=settings.keys[i];e.key.mod=SDL_Keymod(((bits&Native::KeyControl)?SDL_KMOD_CTRL:0)|((bits&Native::KeyShift)?SDL_KMOD_SHIFT:0));e.key.key=SDL_GetKeyFromScancode(e.key.scancode,e.key.mod,true);SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                if(frames==1){ownHull=game->currentLoadout().hull;parent=Native::prepareCrewUiSmoke(*game);frontend.open(Native::Screen::Hangar,&game->state());
                    auto i=size_t(Native::Binding::CameraBase);settings.keys[i]=SDL_SCANCODE_F10;settings.modifiers[i]=Native::KeyControl|Native::KeyShift;settings.keyCommands[i]=TK_ViewBase;keyboard.configure(settings);}
                if((frames==120 || frames==250 || frames==360 || frames==470 || (flight && frames==flight+80)) && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+(frames==120?"-turret-loadout.ppm":frames==250?"-staged.ppm":frames==360?"-bought.ppm":frames==470?"-launch.ppm":"-base.ppm"))).string();}
                if(frames==20)key(Native::Binding::PaneInventory);
                if(frames==40){if(frontend.screen()!=Native::Screen::ShipLoadout || !game->state().passenger)throw std::runtime_error("Mapped inventory key cannot open a boarded loadout");key(Native::Binding::PaneInventory);}
                if(frames==45){if(frontend.screen()!=Native::Screen::Hangar)throw std::runtime_error("Mapped inventory key fails to return to hangar");key(Native::Binding::CommandView);}
                if(frames==50){if(menuVisible || !game->state().commandView)throw std::runtime_error("Docked command key fails to enter command view");key(Native::Binding::PaneInventory);}
                if(frames==55){if(frontend.screen()!=Native::Screen::ShipLoadout || !menuVisible || game->state().commandView)throw std::runtime_error("Inventory key from docked command view fails to open loadout");key(Native::Binding::CameraCockpit);}
                if(frames==60){if(frontend.screen()!=Native::Screen::Hangar)throw std::runtime_error("Mapped cockpit key fails to return from loadout to hangar");auto params=*game->core()->GetMissionParams();params.bAllowDevelopments=false;game->core()->SetMissionParams(&params);}
                if(frames==65)key(Native::Binding::Research);
                if(frames==70){if(frontend.screen()!=Native::Screen::Hangar || game->state().developments || std::any_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.name=="investButtonPane";}))throw std::runtime_error("Research key or hangar artwork ignores no-development mission policy");auto params=*game->core()->GetMissionParams();params.bAllowDevelopments=true;game->core()->SetMissionParams(&params);}
                if(frames==75)key(Native::Binding::Research);
                if(frames==80){if(frontend.screen()!=Native::Screen::Research)throw std::runtime_error("Research key fails after restoring mission developments");key(Native::Binding::PaneInventory);}
                if(frames==100 && (frontend.screen()!=Native::Screen::ShipLoadout || !frontend.stagedLoadout().valid || !game->state().passenger))throw std::runtime_error("Docked gunner cannot open the original loadout screen");
                if(frames==180)click(Native::MenuOp::LoadoutPopup,1);
                if(frames==210)click(Native::MenuOp::PreviewHull,ownHull);
                if(frames==250 && (frontend.stagedLoadout().loadout.hull!=ownHull || !game->state().passenger))throw std::runtime_error("Gunner hull browsing changed the live crew relationship");
                if(frames==280)click(Native::MenuOp::CommitLoadout,0);
                if(frames==330 && (game->state().passenger || game->currentLoadout().hull!=ownHull || frontend.screen()!=Native::Screen::ShipLoadout))throw std::runtime_error("Original passenger purchase did not disembark and fit their own hull");
                if(frames==400)click(Native::MenuOp::Navigate,int(Native::Screen::Hangar));
                if(frames==440)click(Native::MenuOp::Launch,0);
                if(frames==441 && (SDL_GetWindowFlags(window.get())&SDL_WINDOW_MOUSE_CAPTURE))throw std::runtime_error("Animated hangar Launch retains mouse capture after its down action");
                if(frames==470){auto external=game->scene(true).size();if(!game->state().cinematic || !game->playerShip()->GetVisibleF() || !game->camera(cameraInput).showPlayer || external!=game->scene(false).size()+1)throw std::runtime_error("Disembark/refit launch cinematic omits the player's actual ship model");}
                if(frames>460 && !flight && !game->state().cinematic && !game->state().docked){flight=frames;key(Native::Binding::CameraBase);}
                if(flight && frames==flight+40 && Native::cameraMode(cameraInput)!=Native::CameraMode::Base)throw std::runtime_error("View Base did not dispatch the original logical camera command");
                if(flight && frames==flight+100)key(Native::Binding::CameraBase);
                if(flight && frames==flight+150){if(Native::cameraMode(cameraInput)!=Native::CameraMode::Cockpit || game->state().passenger || game->state().docked)throw std::runtime_error("Repeated View Base did not return to cockpit after passenger purchase and launch");crewUiSmokePassed=true;frameLimit=frames+30;fprintf(stderr,"Crew UI smoke: original hangar loadout button, parent-name/turret art, staged hull browsing, disembark/purchase, back/launch and View Base/cockpit toggle passed\n");}
            }
            if(gameStateSmoke && acquired) {
                static unsigned flight=0;static glm::vec3 direction;
                auto key=[&](SDL_Keycode k){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=k==SDLK_SLASH?settings.keys[size_t(Native::Binding::GameState)]:SDL_GetScancodeFromKey(k,nullptr);e.key.key=SDL_GetKeyFromScancode(e.key.scancode,SDL_KMOD_NONE,true);SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto roll=[&](bool down){auto i=size_t(Native::Binding::RollRight);SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.scancode=settings.keys[i];e.key.key=SDL_GetKeyFromScancode(e.key.scancode,SDL_KMOD_NONE,true);SDL_PushEvent(&e);};
                auto clickClose=[&]{auto size=renderer.size();const auto& pane=gameStatePane.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                    if(pane.items.empty())throw std::runtime_error("Game State close button is missing");auto p=glm::vec2(pane.items[0].bounds)+glm::vec2(4);
                    if(pane.slide)p+=pane.slide->displacement(Native::uiTime());p=Native::originalMenuLayout(size,pane.classicScale,pane.canvas).pixel(p);int w,h;SDL_GetWindowSize(window.get(),&w,&h);p*=glm::vec2(w,h)/glm::vec2(size);
                    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.clicks=1;e.button.x=p.x;e.button.y=p.y;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);};
                if(frames==20)key(SDLK_SLASH);
                if(frames==120 && (!gameStatePane.active() || !menuVisible || frontend.screen()!=Native::Screen::Hangar))throw std::runtime_error("Slash did not overlay original Game State above hangar");
                if((frames==150 || (flight && frames==flight+150)) && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+(frames==150?"-hangar.ppm":"-flight.ppm"))).string();}
                if(frames==180)key(SDLK_F6);
                if(frames==210 && (gameStatePane.active() || !playerPane.active()))throw std::runtime_error("F6 did not banish Game State");
                if(frames==230)key(SDLK_SLASH);
                if(frames==260 && (!gameStatePane.active() || playerPane.active()))throw std::runtime_error("Game State did not banish F6");
                if(frames==300)key(SDLK_F5);
                if(frames==340 && (gameStatePane.active() || frontend.screen()!=Native::Screen::Research))throw std::runtime_error("F5 did not banish Game State");
                if(frames==360)key(SDLK_SLASH);
                if(frames==400 && (!gameStatePane.active() || frontend.screen()!=Native::Screen::Hangar))throw std::runtime_error("Game State did not banish investments and preserve hangar");
                if(frames==420)key(SDLK_F8);
                if(frames==460 && (gameStatePane.active() || frontend.screen()!=Native::Screen::Teleport))throw std::runtime_error("F8 did not banish Game State");
                if(frames==480)key(SDLK_SLASH);
                if(frames==570)clickClose();
                if(frames==600 && gameStatePane.active())throw std::runtime_error("Original Game State close button did not dismiss the console");
                if(frames==620)key(SDLK_SLASH);
                if(frames==650)key(SDLK_SLASH);
                if(frames==700){if(gameStatePane.active())throw std::runtime_error("Second slash did not hide Game State");game->launch();menuVisible=false;blockFire=true;}
                if(frames==720){if(!game->state().cinematic)throw std::runtime_error("Game State launch override fixture absent");key(SDLK_SLASH);}
                if(frames==721 && gameStatePane.active())throw std::runtime_error("Game State ignored the original override camera guard");
                if(frames>721 && !flight && !game->state().cinematic){flight=frames;key(SDLK_SLASH);}
                if(flight && frames==flight+40){direction=game->state().up;roll(true);}
                if(flight && frames==flight+140)roll(false);
                if(flight && frames==flight+170){if(menuVisible || !gameStatePane.active() || game->state().docked || glm::dot(direction,game->state().up)>.999f)throw std::runtime_error("Game State interrupted flight or held-key steering");gameStateSmokePassed=true;frameLimit=frames+60;fprintf(stderr,"Game State smoke: actual slash/F5/F6/F8, original close button, hangar/flight overlays, launch guard and continuous steering passed\n");}
            }
            if(playerPaneSmoke && acquired) {
                static int peer=-1;static unsigned flight=0;static glm::vec3 direction;auto key=[&](SDL_Keycode key,bool down=true,bool up=true){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=key;e.key.scancode=SDL_GetScancodeFromKey(key,nullptr);if(down)SDL_PushEvent(&e);if(up){e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);}};
                auto roll=[&](bool down){auto i=size_t(Native::Binding::RollRight);SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.scancode=settings.keys[i];e.key.key=SDL_GetKeyFromScancode(e.key.scancode,SDL_KMOD_NONE,true);SDL_PushEvent(&e);};
                auto click=[&](Native::MenuOp op,int value,int button=SDL_BUTTON_LEFT,int clicks=1){
                    auto size=renderer.size();const auto& menu=playerPane.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                    auto item=std::find_if(menu.items.begin(),menu.items.end(),[&](auto& i){return i.enabled && i.action.op==op && i.action.value==value;});
                    if(item==menu.items.end())throw std::runtime_error("Player-pane smoke: required authored control absent");
                    auto p=glm::vec2(item->bounds)+glm::vec2(item->bounds.z,item->bounds.w)*.5f;
                    if(menu.slide && menu.slide->movesItem(size_t(item-menu.items.begin())))p+=menu.slide->displacement(Native::uiTime());
                    p=Native::originalMenuLayout(size,menu.classicScale,menu.canvas).pixel(p);int w,h;SDL_GetWindowSize(window.get(),&w,&h);p*=glm::vec2(w,h)/glm::vec2(size);
                    SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=button;event.button.clicks=clicks;event.button.x=p.x;event.button.y=p.y;SDL_PushEvent(&event);event.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&event);
                };
                if(frames==1){peer=game->addLocalPilot("Player pane recipient",0);if(peer<0)throw std::runtime_error("Player-pane recipient fixture failed");}
                if(frames==20)key(SDLK_F6);
                if(frames==120 && (!playerPane.active() || playerPane.mode()!=Allegiance::PlayerPaneMode::Expanded || frontend.screen()!=Native::Screen::Hangar))throw std::runtime_error("F6 did not overlay original expanded pane above hangar");
                if(frames>=123 && frames<=137 && frames%2==1)click(Native::MenuOp::Order,int((frames-123)/2));
                if(frames==140){const auto& v=playerPane.view(renderer.size(),Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));if(std::none_of(v.items.begin(),v.items.end(),[](const auto& i){return i.action.op==Native::MenuOp::Order && i.action.value==7 && i.selected;}))throw std::runtime_error("Actual F6 header clicks fail to select the original objective sort");}
                if((frames==150 || frames==370 || (flight && frames==flight+150)) && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+(frames==150?"-expanded.ppm":frames==370?"-compact.ppm":"-flight.ppm"))).string();}
                if(frames==180)click(Native::MenuOp::RosterSelect,peer);
                if(frames==185)click(Native::MenuOp::RosterSelect,peer,SDL_BUTTON_LEFT,2);
                if(frames==190 && (!playerPane.active() || playerPane.selected()!=peer || game->state().commandView))throw std::runtime_error("F6 double click invokes an unauthored command-camera action");
                if(frames==195){
                    auto size=renderer.size();const auto& v=playerPane.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));glm::vec2 p{};
                    for(const auto& i:v.items)if(i.action.op==Native::MenuOp::RosterSelect){p.x=i.bounds.x+3;p.y=std::max(p.y,i.bounds.y+i.bounds.w+4);}
                    p=Native::originalMenuLayout(size,v.classicScale,v.canvas).pixel(p);int w,h;SDL_GetWindowSize(window.get(),&w,&h);p*=glm::vec2(w,h)/glm::vec2(size);
                    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.clicks=1;e.button.x=p.x;e.button.y=p.y;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);
                }
                if(frames==200 && playerPane.selected()>=0)throw std::runtime_error("Actual F6 blank click retains the former pilot selection");
                if(frames==205)click(Native::MenuOp::RosterSelect,peer);
                if(frames==220)click(Native::MenuOp::RosterSelect,peer,SDL_BUTTON_RIGHT);
                if(frames==240 && !playerPane.capturing())throw std::runtime_error("Original player context popup did not capture input");
                if(frames==250)key(SDLK_C);
                if(frames==270){if(chatConsole.audience().recipient!=peer || playerPane.capturing())throw std::runtime_error("Flight context Chat did not pick its actual pilot");key(SDLK_RETURN);}
                if(frames==280){SDL_Event text{};text.type=SDL_EVENT_TEXT_INPUT;text.text.text="Original player-pane chat";SDL_PushEvent(&text);}
                if(frames==290)key(SDLK_RETURN);
                if(frames==310 && (chatConsole.active() || game->state().messages.empty() || game->state().messages.back().text.find("Original player-pane chat")==std::string::npos))throw std::runtime_error("Selected player inline chat did not send");
                if(frames==320)click(Native::MenuOp::Navigate,0);
                if(frames==370 && playerPane.mode()!=Allegiance::PlayerPaneMode::Compact)throw std::runtime_error("Original collapse button did not switch to compact pane");
                if(frames==400)key(SDLK_F6);
                if(frames==460 && playerPane.mode()!=Allegiance::PlayerPaneMode::Expanded)throw std::runtime_error("F6 did not expand the compact player pane");
                if(frames==470)click(Native::MenuOp::ConfirmBoot,peer);
                if(frames==471 && (!frontend.hasMessageBox() || !menuVisible || !playerPane.active() || !game->core()->GetShip(peer)))throw std::runtime_error("Hangar Boot failed to open the original modal without changing membership");
                if(frames==475 && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-boot-hangar.ppm")).string();}
                if(frames==476)click(Native::MenuOp::RosterSelect,game->state().shipId);
                if(frames==477 && (!frontend.hasMessageBox() || playerPane.selected()!=peer))throw std::runtime_error("Modal Boot leaked a click into the independent F6 pane");
                if(frames==478)key(SDLK_F6);
                if(frames==479 && (!frontend.hasMessageBox() || !playerPane.active()))throw std::runtime_error("Boot modal leaked the F6 binding");
                if(frames==480)key(SDLK_ESCAPE);
                if(frames==481 && (frontend.hasMessageBox() || !menuVisible || frontend.screen()!=Native::Screen::Hangar || !game->core()->GetShip(peer)))throw std::runtime_error("Boot Escape failed to preserve hangar and recipient");
                if(frames==490)key(SDLK_F6);
                if(frames==600){if(playerPane.active())throw std::runtime_error("Second F6 did not hide expanded player pane");game->launch();menuVisible=false;blockFire=true;}
                if(frames==620){key(SDLK_F6);if(!game->state().cinematic)throw std::runtime_error("Player-pane launch camera fixture absent");}
                if(frames==621 && playerPane.active())throw std::runtime_error("F6 ignored the original launch-camera override guard");
                if(frames>621 && !flight && !game->state().cinematic){flight=frames;key(SDLK_F6);}
                if(flight && frames==flight+40){direction=game->state().up;roll(true);}
                if(flight && frames==flight+140)roll(false);
                if(flight && frames==flight+170 && (menuVisible || !playerPane.active() || game->state().docked || glm::dot(direction,game->state().up)>.999f))throw std::runtime_error("Original player overlay interrupted flight or held-key steering");
                if(flight && frames==flight+180){if(playerPane.selected()!=peer)click(Native::MenuOp::RosterSelect,peer);}
                if(flight && frames==flight+190)click(Native::MenuOp::ConfirmBoot,peer);
                if(flight && frames==flight+191 && (!frontend.hasMessageBox() || !frontend.view().overlay || !menuVisible || game->state().docked))throw std::runtime_error("Flight Boot must keep the real scene behind its modal");
                if(flight && frames==flight+195 && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-boot-flight.ppm")).string();}
                if(flight && frames==flight+205)key(SDLK_ESCAPE);
                if(flight && frames==flight+206 && (menuVisible || frontend.hasMessageBox() || !playerPane.active() || !game->core()->GetShip(peer)))throw std::runtime_error("Boot cancellation did not resume flight and retain F6");
                if(flight && frames==flight+210)click(Native::MenuOp::ConfirmBoot,peer);
                if(flight && frames==flight+212){
                    auto& menu=frontend.view();auto item=std::find_if(menu.items.begin(),menu.items.end(),[](auto& i){return i.control=="messageCancel";});if(item==menu.items.end())throw std::runtime_error("Original modal Cancel button missing");
                    auto size=renderer.size();auto p=Native::originalMenuLayout(size,menu.classicScale,menu.canvas).pixel(glm::vec2(item->bounds)+glm::vec2(item->bounds.z,item->bounds.w)*.5f);int w,h;SDL_GetWindowSize(window.get(),&w,&h);p*=glm::vec2(w,h)/glm::vec2(size);
                    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.clicks=1;e.button.x=p.x;e.button.y=p.y;SDL_PushEvent(&e);
                }
                if(flight && frames==flight+213){if(frontend.messageView().pressed!=1)throw std::runtime_error("Cached dialog lost the original pressed Cancel face");if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-boot-pressed.ppm")).string();}}
                if(flight && frames==flight+214){SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_UP;e.button.button=SDL_BUTTON_LEFT;e.button.x=1;e.button.y=1;SDL_PushEvent(&e);}
                if(flight && frames==flight+215)key(SDLK_RETURN);
                if(flight && frames==flight+225){auto* pilot=game->core()->GetShip(peer);if(menuVisible || frontend.hasMessageBox() || !playerPane.active() || !pilot || pilot->GetSide()->GetObjectID()!=SIDE_TEAMLOBBY)throw std::runtime_error("Confirmed Boot did not return to flight and move the recipient to No Team");playerPaneSmokePassed=true;frameLimit=frames+15;fprintf(stderr,"Player-pane smoke: actual F6, roster/chat, collapse/expand, hangar/flight overlays, steering and original Boot capture/cancel/confirm/resume passed\n");}
            }
            if(consoleSmoke && acquired) {
                auto press=[&](Native::Binding binding){auto i=size_t(binding);auto bits=settings.modifiers[i];auto mod=SDL_Keymod(((bits&Native::KeyShift)?SDL_KMOD_SHIFT:0)|((bits&Native::KeyControl)?SDL_KMOD_CTRL:0)|((bits&Native::KeyAlt)?SDL_KMOD_ALT:0));SDL_Event e{};
                    e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=settings.keys[i];e.key.key=SDL_GetKeyFromScancode(e.key.scancode,mod,true);e.key.mod=mod;SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto key=[&](SDL_Keycode code,const char* input=nullptr){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;SDL_PushEvent(&e);
                    // SDL produces text between key down and key up; popup accelerators
                    // suppress that text until the matching release.
                    if(input){SDL_Event t{};t.type=SDL_EVENT_TEXT_INPUT;t.text.text=input;SDL_PushEvent(&t);}e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto text=[&](const char* value){SDL_Event e{};e.type=SDL_EVENT_TEXT_INPUT;e.text.text=value;SDL_PushEvent(&e);};
                auto click=[&](glm::vec2 pixel){int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=pixel.x*w/size.x;e.button.y=pixel.y*h/size.y;e.button.clicks=1;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);};
                if(frames==10)press(Native::Binding::Pause);
                if(frames==15){if(frontend.screen()!=Native::Screen::Pause || !frontend.keyboardPopup())throw std::runtime_error("Mapped main-menu key did not open the original global popup above hangar");key(SDLK_ESCAPE);}
                if(frames==20 && (!menuVisible || frontend.screen()!=Native::Screen::Hangar))throw std::runtime_error("Original main menu lost its hangar parent");
                if(frames==60)press(Native::Binding::Chat);
                if(frames==61 && (!menuVisible || frontend.screen()!=Native::Screen::Hangar || chatConsole.mode()!=Allegiance::ComposeState::Chat))throw std::runtime_error("Inline chat replaced or failed to open above hangar");
                if(frames==90)text("Classic inline chat");
                if(frames==150 && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-chat.ppm")).string();}
                if(frames==180)key(SDLK_RETURN);
                if(frames==181 && (chatConsole.active() || !menuVisible || frontend.screen()!=Native::Screen::Hangar || game->state().messages.back().text.find("Classic inline chat")==std::string::npos))throw std::runtime_error("Inline chat did not send or retain hangar");
                if(frames==200)press(Native::Binding::CommandView);
                if(frames==201)SDL_Delay(510); // Let the authored command-bar entrance finish before clicking its final position.
                if(frames==240){auto size=renderer.size();float scale=Native::originalInterfaceScale(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));glm::vec2 extent=glm::vec2(size)/scale;click((glm::vec2(std::floor(extent.x*.5f)-135,extent.y-132)+glm::vec2(30,18))*scale);}
                if(frames==250){auto size=renderer.size();float scale=Native::originalInterfaceScale(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));glm::vec2 extent=glm::vec2(size)/scale;click((glm::vec2(std::floor(extent.x*.5f)-135,extent.y-132)+glm::vec2(139,53))*scale);}
                if(frames==251 && (!game->state().commandView || menuVisible || chatConsole.mode()!=Allegiance::ComposeState::Command || chatConsole.currentVerb()!=5 || chatConsole.audience().recipient!=game->state().shipId))throw std::runtime_error("Original recipient and verb artwork did not dispatch the clicked command");
                if(frames==280)text(game->state().station.c_str());
                if(frames==320 && (!chatConsole.target() || chatConsole.target()->type!=Native::ContactStation))throw std::runtime_error("Inline command did not complete the selected station");
                if((frames==320 || frames==400) && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+(frames==320?"-command.ppm":"-quick.ppm"))).string();}
                if(frames==340)key(SDLK_RETURN);
                if(frames==341 && chatConsole.active())throw std::runtime_error("Typed command retained keyboard capture after sending");
                if(frames==360)press(Native::Binding::QuickChat);
                if(frames==361 && (chatConsole.mode()!=Allegiance::ComposeState::Quick || chatConsole.view(renderer.size(),1).items.empty()))throw std::runtime_error("Original quick chat popup did not open");
                if(frames==390){auto child=std::find_if(quickChat.menus[0].items.begin(),quickChat.menus[0].items.end(),[](auto& item){return item.menu>=0;});if(child==quickChat.menus[0].items.end())throw std::runtime_error("Quick chat has no authored submenu");key(SDL_Keycode(child->key));}
                if(frames==391 && chatConsole.view(renderer.size(),1).items.size()<=quickChat.menus[0].items.size())throw std::runtime_error("Authored quick-chat key did not open a child popup");
                if(frames==420)key(SDLK_ESCAPE);
                if(frames==440)key(SDLK_ESCAPE);
                if(frames==480)bindingAction(Native::Binding::PilotOrders,TK_ConModeCommand);
                if(frames==481 && (game->state().commandView || !menuVisible || frontend.screen()!=Native::Screen::Hangar))throw std::runtime_error("Original ConModeCommand alias must toggle the command view back to hangar");
                if(frames==500)press(Native::Binding::Research);
                if(frames==550)press(Native::Binding::Chat);
                if(frames==560)text("Testing the original console");
                if(frames==610)key(SDLK_ESCAPE);
                if(frames==620){frontend.open(Native::Screen::Pilots,&game->state());frontend.text("Unsent lobby draft");menuVisible=true;}
                if(frames==630)press(Native::Binding::QuickChat);
                if(frames==631 && (frontend.screen()!=Native::Screen::Pilots || frontend.wantsText() || frontend.view().slide || std::none_of(frontend.view().items.begin(),frontend.view().items.end(),[](auto& item){return item.popupLevel==0 && item.bounds.x==11 && item.bounds.y==11;})))throw std::runtime_error("Actual quick-chat binding did not open a stationary popup at the original team-screen corner");
                if(frames==635 && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-lobby-quick.ppm")).string();}
                if(frames==640)key(SDLK_1,"1");
                if(frames==645 && (!menuVisible || frontend.screen()!=Native::Screen::Pilots || frontend.chatText()!="Unsent lobby draft" || std::any_of(frontend.view().items.begin(),frontend.view().items.end(),[](auto& item){return item.popupLevel>=0;}) || std::none_of(game->state().messages.begin(),game->state().messages.end(),[](auto& message){return message.text.find("Affirmative")!=std::string::npos;})))throw std::runtime_error("Lobby voice-only quick chat lost its screen/draft or failed to deliver");
                if(frames==650)frontend.open(Native::Screen::Research,&game->state());
                if(frames==655)press(Native::Binding::Chat);
                if(frames==660){if(frontend.screen()!=Native::Screen::Research || !menuVisible || chatConsole.mode()!=Allegiance::ComposeState::Chat)throw std::runtime_error("Inline composition replaced the investment console");fprintf(stderr,"Console smoke: actual SDL chat entry, authored recipient/verb clicks, typed orders, flight/lobby quick-chat cascades and F5 composition passed\n");}
                if(frames==666)text("Discard this research draft");
                if(frames==670 && chatConsole.typed()!="Discard this research draft")throw std::runtime_error("Docked investment chat did not receive actual text input");
                if(frames==675)key(SDLK_ESCAPE);
                if(frames==676 && (chatConsole.active() || frontend.screen()!=Native::Screen::Research || !menuVisible))throw std::runtime_error("Docked investment chat Escape closed its parent instead of cancelling composition");
                if(frames==680)press(Native::Binding::Research);
                if(frames==685)press(Native::Binding::CommandView);
                auto money=[&](int amount){auto* sender=game->core()->GetShip(ShipID(consoleRequester));game->core()->GetIgcSite()->GetMoneyRequest(sender,amount,game->playerShip()->GetBaseHullType()->GetObjectID());};
                auto capture=[&](const char* suffix){if(!screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+suffix+".ppm")).string();}};
                if(frames==690){
                    if(menuVisible || !game->state().commandView || chatConsole.active())throw std::runtime_error("Original command console did not regain controls for request feedback: menu="+std::to_string(menuVisible)+", view="+std::to_string(game->state().commandView)+", compose="+std::to_string(int(chatConsole.mode()))+", screen="+std::to_string(int(frontend.screen())));
                    consoleRequester=game->addLocalPilot("Requesting veteran",game->state().playerSide);
                    if(consoleRequester<0)throw std::runtime_error("Original request feedback pilot fixture failed");
                    consoleRequestBalance=game->pilotBalance(consoleRequester);money(125);
                }
                if(frames==695){if(game->state().moneyRequest.sender!=consoleRequester || !game->state().messages.back().tip)throw std::runtime_error("Money request did not enter the original critical-tip display");capture("-money");}
                if(frames==710)press(Native::Binding::AcceptOrder);
                if(frames==720 && (game->state().moneyRequest.sender>=0 || game->pilotBalance(consoleRequester)!=consoleRequestBalance+125))throw std::runtime_error("Mapped Insert did not approve money exactly once");
                if(frames==735){consoleRequestBalance=game->pilotBalance(consoleRequester);money(50);}
                if(frames==750)press(Native::Binding::RejectOrder);
                if(frames==760 && (game->state().moneyRequest.sender>=0 || game->pilotBalance(consoleRequester)!=consoleRequestBalance))throw std::runtime_error("Mapped Delete did not decline money without a transfer");
                if(frames==780){auto* sender=game->core()->GetShip(ShipID(consoleRequester));auto* station=game->playerShip()->GetStation();game->core()->GetIgcSite()->ReceiveChat(sender,CHAT_INDIVIDUAL,game->state().shipId,NA,"Defend our station",c_cidDefend,OT_station,station->GetObjectID(),sender);}
                if(frames==800){if(game->state().commands[0].command!=c_cidDefend)throw std::runtime_error("Original queued order display fixture failed");capture("-queued");}
                if(frames==820)press(Native::Binding::AcceptOrder);
                if(frames==830){if(game->state().commands[1].command!=c_cidDefend)throw std::runtime_error("Mapped Insert did not accept queued order");capture("-accepted");fprintf(stderr,"Console feedback smoke: original critical money/order prompts, authored queued/accepted display and actual mapped Insert/Delete passed\n");}
                if(frames==840)press(Native::Binding::Pause);
                if(frames==845)key(SDLK_I);
                if(frames==850){
                    std::string core="Core: "+std::string(game->core()->GetMissionParams()->szIGCStaticFile)+" ";
                    if(!menuVisible || frontend.screen()!=Native::Screen::GameDetails || !frontend.view().overlay ||
                        std::none_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[&](const auto& c){return c.name=="detailsText" && c.text==core;}))throw std::runtime_error("Escape-menu Game Details did not open the original read-only report over command view: menu="+std::to_string(menuVisible)+", screen="+std::to_string(int(frontend.screen()))+", overlay="+std::to_string(frontend.view().overlay)+", "+core);
                    capture("-details");
                }
                // A suspended composer must not receive the top popup's keys,
                // text, or scrollbar clicks. Retain it across popup dismissal.
                if(frames==851){chatConsole.startChat(*game);chatConsole.text(*game,"Retained draft");}
                if(frames==855){text(" should be ignored");key(SDLK_BACKSPACE);}
                if(frames==860){if(chatConsole.typed()!="Retained draft" || frontend.screen()!=Native::Screen::GameDetails)throw std::runtime_error("Game Details leaked text or keys into its suspended chat draft");chatConsole.wings(*game);}
                if(frames==870){
                    auto& menu=frontend.view();auto page=std::find_if(menu.items.begin(),menu.items.end(),[](const auto& i){return i.action.op==Native::MenuOp::ScrollPage && i.action.value==-7 && i.action.delta==1;});
                    if(page==menu.items.end())throw std::runtime_error("Original Game details scrollbar has no downward page");
                    auto p=Native::originalMenuLayout(renderer.size(),menu.classicScale,menu.canvas).pixel(glm::vec2(page->bounds)+glm::vec2(page->bounds.z,page->bounds.w)*.5f);click(p);
                }
                if(frames==880){
                    if(std::none_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.name=="detailsText" && c.text.starts_with("Experimental: ");}) || chatConsole.view(renderer.size(),frontend.view().classicScale).items.empty())throw std::runtime_error("Actual Game details scrollbar input leaked into its suspended wing popup or cannot reach the final fields");capture("-details-bottom");
                }
                if(frames==885)chatConsole.key(*game,SDLK_ESCAPE); // remove only the fixture's wing popup
                if(frames==890)key(SDLK_RETURN);
                if(frames==900){
                    if(menuVisible || !game->state().commandView || game->state().commands[1].command!=c_cidDefend || chatConsole.typed()!="Retained draft" || !chatConsole.keyboard())throw std::runtime_error("Closing original Game details lost command view, its accepted command or its suspended chat draft");
                    fprintf(stderr,"Game details smoke: actual Escape/I, original read-only mission snapshot, top-popup input capture, authored scrollbar and Enter back to command view with retained chat passed\n");
                }
                if(frames==910)key(SDLK_ESCAPE);
                if(frames==920){
                    int observer=game->state().shipId;
                    if(game->addLocalPilot("Abstaining veteran",game->state().playerSide)<0 || !game->controlLocalPilot(consoleRequester) ||
                        !game->sendChat(Native::ChatChannel::Team,"#draw") || !game->sendChat(Native::ChatChannel::Team,"#resign") || !game->controlLocalPilot(observer))throw std::runtime_error("Original ballot fixture failed");
                    game->commandView(true);consoleBallot=game->state().ballot.id;
                }
                if(frames==930){if(consoleBallot<0 || !game->state().ballot.text.starts_with("Requesting veteran has proposed offering a draw."))throw std::runtime_error("Original FIFO ballot display missing");capture("-vote");}
                if(frames==935)press(Native::Binding::Chat);
                if(frames==940){press(Native::Binding::VoteNo);text("n");}
                if(frames==945 && (game->state().ballot.id!=consoleBallot || chatConsole.typed()!="n"))throw std::runtime_error("Chat typing N accidentally voted on a ballot");
                if(frames==950)key(SDLK_ESCAPE);
                if(frames==955)press(Native::Binding::VoteNo);
                if(frames==960){if(game->state().ballot.id<0 || game->state().ballot.id==consoleBallot || !game->state().ballot.text.starts_with("Requesting veteran has proposed resigning."))throw std::runtime_error("Mapped N did not advance the original ballot FIFO");consoleBallot=game->state().ballot.id;}
                if(frames==965)press(Native::Binding::Pause);
                if(frames==970)key(SDLK_I);
                if(frames==975)press(Native::Binding::VoteYes);
                if(frames==980){if(frontend.screen()!=Native::Screen::GameDetails || game->state().ballot.id!=consoleBallot)throw std::runtime_error("Game Details leaked Y into ballot voting");key(SDLK_RETURN);}
                if(frames==990)press(Native::Binding::CommandView);
                if(frames==995)press(Native::Binding::Research);
                if(frames==1000)press(Native::Binding::VoteYes);
                if(frames==1005){if(game->state().ballot.id>=0 || !menuVisible || frontend.screen()!=Native::Screen::Research)throw std::runtime_error("Mapped Y did not vote above the original investment console");fprintf(stderr,"Ballot smoke: authored prompt, actual mapped N/Y, FIFO, chat typing capture, Game Details capture and investment parent retained passed\n");}
                if(frames==1010)press(Native::Binding::Chat);
                if(frames==1012)chatConsole.recipient(*game,2);
                if(frames==1015)text("#list");
                if(frames==1017)key(SDLK_RETURN);
                if(frames==1020){
                    auto& rows=game->state().messages;
                    if(chatConsole.active() || frontend.screen()!=Native::Screen::Research || rows.size()<2 || !rows.back().text.ends_with(Allegiance::MissionChatHelp) || rows.back().fromPlayer || !rows[rows.size()-2].text.ends_with("#list") || hud.messageLayoutStats().rows==0)throw std::runtime_error("Original inline list command lost its local echo, visible private help or investment parent");
                    capture("-mission-help");
                }
                if(frames==1035)press(Native::Binding::Chat);
                if(frames==1040)text("#autobalance a");
                if(frames==1045)key(SDLK_RETURN);
                if(frames==1050 && (game->state().maxImbalance!=32766 || frontend.session().rules.maxImbalance!=32766 || game->core()->GetMissionParams()->iMaxImbalance!=32766 || frontend.screen()!=Native::Screen::Research))throw std::runtime_error("Original typed balance command lost live IGC/frontend policy or investment parent");
                if(frames==1060)press(Native::Binding::Pause);
                if(frames==1070)key(SDLK_I);
                if(frames==1075){
                    auto& menu=frontend.view();auto page=std::find_if(menu.items.begin(),menu.items.end(),[](const auto& i){return i.action.op==Native::MenuOp::ScrollPage && i.action.value==-7 && i.action.delta==1;});
                    if(frontend.screen()!=Native::Screen::GameDetails || page==menu.items.end())throw std::runtime_error("Live policy Game details popup or scrollbar missing: screen="+std::to_string(int(frontend.screen())));
                    auto p=Native::originalMenuLayout(renderer.size(),menu.classicScale,menu.canvas).pixel(glm::vec2(page->bounds)+glm::vec2(page->bounds.z,page->bounds.w)*.5f);click(p);
                }
                if(frames==1085){
                    if(std::none_of(frontend.view().artwork.begin(),frontend.view().artwork.end(),[](const auto& c){return c.name=="detailsText" && c.text=="Max Team Imbalance: Auto ";}))throw std::runtime_error("Original Game details did not show live automatic balance policy");
                    capture("-mission-policy");
                }
                if(frames==1088)key(SDLK_RETURN);
                if(frames==1100 && (!menuVisible || frontend.screen()!=Native::Screen::Research))throw std::runtime_error("Original global main menu/Game details lost its investment parent");
                if(frames==1110)press(Native::Binding::Chat);
                if(frames==1115)text("#autoaccept");
                if(frames==1120)key(SDLK_RETURN);
                if(frames==1125 && (game->state().autoAccept&(1u<<game->state().playerSide)))throw std::runtime_error("Original typed commander automatic-admission toggle did not turn off");
                if(frames==1140)press(Native::Binding::Chat);
                if(frames==1145)text("#autoaccept");
                if(frames==1150)key(SDLK_RETURN);
                if(frames==1160){
                    if(!(game->state().autoAccept&(1u<<game->state().playerSide)) || chatConsole.active())throw std::runtime_error("Original typed commander automatic-admission toggle did not restore");
                    fprintf(stderr,"Mission chat smoke: actual mapped composer/Enter, original local echo/private help, live balance policy and authored Game details, commander admission toggles passed\n");
                }
            }
            if(chatSmoke && acquired) {
                auto chatPoint=[&]{auto b=hud.messageLayoutStats().listBounds;return glm::vec2(b)+glm::vec2(10);};
                auto chatWheel=[&](float amount){auto p=chatPoint();int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.y=amount;e.wheel.mouse_x=p.x*w/size.x;e.wheel.mouse_y=p.y*h/size.y;SDL_PushEvent(&e);};
                auto chatPage=[&](bool older){auto b=older?Native::Binding::ChatPageUp:Native::Binding::ChatPageDown;auto i=size_t(b);auto bits=settings.modifiers[i];auto mod=SDL_Keymod(((bits&Native::KeyShift)?SDL_KMOD_SHIFT:0)|((bits&Native::KeyControl)?SDL_KMOD_CTRL:0)|((bits&Native::KeyAlt)?SDL_KMOD_ALT:0));SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=settings.keys[i];e.key.key=SDL_GetKeyFromScancode(e.key.scancode,mod,true);e.key.mod=mod;SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);};
                auto longChat=[&](int index){
                    std::string text="Message "+std::to_string(index)+": ";
                    while(text.size()+32<240)text+="readable café / wrapped text ";
                    if(!game->sendChat(Native::ChatChannel::Team,text))throw std::runtime_error("Chat smoke could not send a long UTF-8 message");
                };
                if(frames==10){auto prefs=settings.chat;prefs.filterAll=false;prefs.censor=true;game->chatPreferences(prefs);for(int i=0;i<16;++i)longChat(i);game->sendChat(Native::ChatChannel::Team,"Display-only FUCK censor check");}
                if(frames==15){
                    auto yes=std::find_if(quickChat.commands.begin(),quickChat.commands.end(),[](auto& command){return command.sonic==15;});
                    if(yes==quickChat.commands.end() || !game->sendQuickChat(Native::ChatChannel::Team,*yes) || game->state().messages.back().text.find("Affirmative!")==std::string::npos || game->sounds().empty() || game->sounds().back().id!=quickChat.voiceForChat(15)->sound)throw std::runtime_error("Chat smoke confused logical sonic ID 15 with its audio resource");
                }
                if(frames==25){
                    auto count=game->state().messages.size();auto time=game->state().time;
                    if(!game->sendChat(Native::ChatChannel::Team,"Repeated affirmative") || !game->sendChat(Native::ChatChannel::Team,"Repeated affirmative") || game->state().messages.size()!=count+2)throw std::runtime_error("Chat smoke suppressed a repeated message");
                    auto& message=game->state().messages.back();if(message.text.find(" (")==std::string::npos || message.text.find("): Repeated affirmative")==std::string::npos)throw std::runtime_error("Chat smoke omitted its recipient label");
                    count=game->state().messages.size();std::string name=settings.pilot;
                    if(!game->sendChat(Native::ChatChannel::Target,"!mute "+name) || !game->sendChat(Native::ChatChannel::Team,"Self-muted echo") || game->state().messages.size()!=count || !game->sendChat(Native::ChatChannel::Target,"!unmute "+name) || game->state().time!=time)throw std::runtime_error("Chat smoke local shell changed history or advanced simulation");
                }
                if(frames==40)hud.binding(Native::Binding::EventLog);
                if(frames==70){auto stats=hud.messageLayoutStats();if(stats.rows<=game->state().messages.size())throw std::runtime_error("Chat smoke long messages remain clipped");chatLayoutUpdates=stats.updates;}
                if(frames==120 && hud.messageLayoutStats().updates!=chatLayoutUpdates)throw std::runtime_error("Chat layout is being recomputed without a message or font change");
                if(frames==140){hud.pageMessages(game->state(),true);chatHistoryOffset=hud.messageLayoutStats().offset;if(!chatHistoryOffset)throw std::runtime_error("Wrapped chat cannot page to older rows");}
                if(frames==180){longChat(17);longChat(18);}
                if(frames==200 && hud.messageLayoutStats().offset<=chatHistoryOffset)throw std::runtime_error("New wrapped messages move the historical page");
                if(frames==220){auto prefs=settings.chat;prefs.censor=false;game->chatPreferences(prefs);for(auto& message:game->state().messages)if(message.text!=message.displayText)throw std::runtime_error("Censor disable did not restore existing raw text");}
                if(frames==260){auto prefs=settings.chat;prefs.censor=true;game->chatPreferences(prefs);hud.setScale(1.25f);}
                if(frames==300){for(int i=0;i<4;++i)hud.pageMessages(game->state(),false);if(hud.messageLayoutStats().offset)throw std::runtime_error("Wrapped chat cannot return to newest rows");}
                if(frames==310){auto p=settings;p.chatLines=10;p.chatSize=3;hud.configure(p);}
                if(frames==312){auto stats=hud.messageLayoutStats();if(stats.pageRows!=10 || stats.lineHeight!=assets.font("hugeBoldFont")->height)throw std::runtime_error("Classic huge chat font or configured row count differs");chatLayoutUpdates=stats.updates;}
                if(frames==315){auto p=settings;p.chatLines=1;p.chatSize=3;hud.configure(p);}
                if(frames==318 && (hud.messageLayoutStats().pageRows!=1 || hud.messageLayoutStats().updates!=chatLayoutUpdates))throw std::runtime_error("Chat line count unnecessarily rewraps resident history");
                if(frames==320){auto p=settings;p.chatSize=1;hud.configure(p);}
                if(frames==322 && (hud.messageLayoutStats().lineHeight!=assets.font("smallBoldFont")->height || hud.messageLayoutStats().updates<=chatLayoutUpdates))throw std::runtime_error("Classic small chat font did not rewrap history");
                if(frames==325){auto p=settings;p.chatLines=7;p.chatSize=2;hud.configure(p);}
                if(frames==328){hud.build(renderer.size(),game->state(),game->contacts(),glm::mat4(1),renderer.stats(),false,false,false,{-1,-1},nullptr,nullptr,{},ofInLoadout);if(hud.messageLayoutStats().pageRows!=6)throw std::runtime_error("Original loadout chat does not cap rows at six");}
                if(frames==330 && (hud.messageLayoutStats().pageRows!=7 || hud.messageLayoutStats().lineHeight!=assets.font("largeBoldFont")->height))throw std::runtime_error("Leaving loadout lost the requested original chat lines or font");
                if(frames==340){for(int i=0;i<3;++i)hud.pageMessages(game->state(),true);if(frameLimit>=2200){paused=true;chatPauseClock=Native::uiTime();chatPauseSimulation=game->state().time;}}
                if(chatPauseClock && !chatWallChecked && Native::uiTime()-chatPauseClock>10.1){
                    if(hud.messageLayoutStats().offset || game->state().time!=chatPauseSimulation)throw std::runtime_error("Chat timeout did not restore newest rows while simulation was paused");
                    chatWallChecked=true;paused=false;fprintf(stderr,"Chat smoke: real ten-second autoscroll elapsed while IGC simulation remained paused\n");
                }
                if(frameLimit>=2200 && frames+1==frameLimit && !chatWallChecked)throw std::runtime_error("Extended chat smoke ended before the wall-clock timeout check");
                if(frames==400)fprintf(stderr,"Chat smoke: original audience labels, repeated records, local mute/unmute, UTF-8 wrapping, sparse-row history anchoring, live censor, font resize and resident layout reuse passed\n");
                if(frameLimit<2200){
                    if(frames==345){for(int i=0;i<30;++i)hud.pageMessages(game->state(),false);chatSmokeThrottle=throttle;}
                    if(frames==350 || frames==352 || frames==354)chatWheel(.4f);
                    if(frames==360 && (hud.messageLayoutStats().offset || std::abs(throttle-chatSmokeThrottle-.1f)>.001f))throw std::runtime_error("Original undetectable CommPane captures the wheel instead of pilot throttle");
                    if(frames==365)chatWheel(-1.2f);
                    if(frames==375)chatPage(true);
                    if(frames==380 && (hud.messageLayoutStats().offset!=hud.messageLayoutStats().pageRows || std::abs(throttle-chatSmokeThrottle)>.001f))throw std::runtime_error("Mapped flight chat paging or wheel reversal differs");
                    if(frames==385)chatPage(false);
                    if(frames==400)game->commandView(true);
                    if(frames==445){for(int i=0;i<30;++i)hud.pageMessages(game->state(),false);
                        // Settle the original two-second camera transition in
                        // simulation time without waiting through extra frames.
                        for(int i=0;i<22;++i)game->step({},.1);const auto& camera=game->camera(cameraInput);if(camera.animating)throw std::runtime_error("Command chat fixture camera transition did not settle");chatSmokeEye=camera.position;}
                    if(frames==450 || frames==452 || frames==454)chatWheel(.4f);
                    if(frames==460){if(hud.messageLayoutStats().offset || glm::length(game->state().eyePosition-chatSmokeEye)<1)throw std::runtime_error("Original undetectable CommPane captures the wheel instead of command zoom");chatSmokeEye=game->state().eyePosition;}
                    if(frames==465)chatPage(true);
                    if(frames==475){bindingAction(Native::Binding::Pause);chatHistoryOffset=hud.messageLayoutStats().offset;}
                    if(frames==480)chatWheel(1);
                    if(frames==481)chatPage(true);
                    if(frames==482){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=settings.keys[size_t(Native::Binding::PaneContacts)];e.key.key=SDL_GetKeyFromScancode(e.key.scancode,SDL_KMOD_NONE,true);SDL_PushEvent(&e);e.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&e);}
                    if(frames==485 && (!frontend.keyboardPopup() || playerPane.active() || hud.messageLayoutStats().offset!=chatHistoryOffset || glm::length(game->state().eyePosition-chatSmokeEye)>.01f))throw std::runtime_error("Original popup leaked wheel/F6/paging into background chat, players or command camera");
                    if(frames==490){frontAction(frontend.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE));chatSmokeOther=game->addLocalPilot("CommPane observer",game->state().playerSide);if(chatSmokeOther<0)throw std::runtime_error("CommPane player fixture rejected");playerPane.toggle(*game);playerPane.sync(*game,true);chatSmokePlayer=playerPane.selected();bindingAction(Native::Binding::Pause);}
                    if(frames==495){const auto& view=playerPane.view(renderer.size(),Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));auto row=std::find_if(view.items.begin(),view.items.end(),[&](const auto& item){return item.action.op==Native::MenuOp::RosterSelect && item.action.value!=chatSmokePlayer;});
                        if(row==view.items.end())throw std::runtime_error("Popup F6 capture fixture has no other player");auto p=Native::originalMenuLayout(renderer.size(),view.classicScale,view.canvas).pixel(glm::vec2(row->bounds)+glm::vec2(5));
                        int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x*w/size.x;e.button.y=p.y*h/size.y;SDL_PushEvent(&e);e.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&e);}
                    if(frames==500 && playerPane.selected()!=chatSmokePlayer)throw std::runtime_error("Options popup click selected a background F6 player");
                    if(frames==505){if(menuVisible && frontend.keyboardPopup())frontAction(frontend.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE));playerPane.close();game->commandView(false);}
                    if(frames==530){if(!game->controlLocalPilot(chatSmokeOther) || !game->state().docked)throw std::runtime_error("CommPane hangar fixture is not docked");frontend.open(Native::Screen::Hangar,&game->state());menuVisible=true;for(int i=0;i<30;++i)hud.pageMessages(game->state(),false);}
                    if(frames==540)chatPage(true);
                    if(frames==545 && (frontend.screen()!=Native::Screen::Hangar || hud.messageLayoutStats().offset!=hud.messageLayoutStats().pageRows))throw std::runtime_error("Original hangar consumes mapped paging before its CommPane");
                    if(frames==550 && !screenshot.empty()){auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-docked-history.ppm")).string();}
                    if(frames==560){hud.binding(Native::Binding::HudVisible);}
                    if(frames==565 && hud.messageLayoutStats().listBounds!=glm::vec4(0))throw std::runtime_error("Hidden chat retains a displayed list");
                    if(frames==570)hud.binding(Native::Binding::HudVisible);
                    if(frames==600)fprintf(stderr,"CommPane SDL smoke: original mouse-transparent pilot/command wheel, mapped flight/hangar pages, options/F6 capture and hidden-pane release passed\n");
                }
            }
            if(mouseSmoke && acquired){
                auto button=[&](bool down,Uint8 b){SDL_Event e{};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;e.button.button=b;SDL_PushEvent(&e);};
                auto wheel=[&](float y){SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.y=y;SDL_PushEvent(&e);};
                auto key=[&](bool down,SDL_Scancode scan,SDL_Keymod mod,bool repeat=false){SDL_Event e{};e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;e.key.scancode=scan;e.key.mod=mod;e.key.repeat=repeat;e.key.key=SDL_GetKeyFromScancode(scan,mod,false);SDL_PushEvent(&e);};
                if(frames==10){
                    if(!modernUi && !classicMouseEnabled)bindingAction(Native::Binding::Cursor,TK_ToggleMouse);
                    game->selectWeapon(0);mouseSmokeAmmo=game->state().ammo;game->targetKey(TK_TargetCenter);mouseSmokeTarget=game->state().targetId;
                    if(mouseSmokeTarget>=0)game->targetKey(TK_TargetNothing);
                    else {auto contacts=game->contacts();auto station=std::find_if(contacts.begin(),contacts.end(),[](auto& c){return c.type==Native::ContactStation;});
                        if(station==contacts.end())throw std::runtime_error("Mouse center-target fixture has no known base");game->selectTarget(station->type,station->id);}
                }
                if(frames==20)button(true,SDL_BUTTON_RIGHT);if(frames==21)button(false,SDL_BUTTON_RIGHT);
                if(frames==40 && (game->state().targetId!=mouseSmokeTarget || game->state().ammo!=mouseSmokeAmmo))throw std::runtime_error("Original right mouse assignment did not target center without firing");
                if(frames==50 || frames==60 || frames==70)wheel(.4f);
                if(frames==90 && std::abs(throttle-.1f)>.001f)throw std::runtime_error("Original fractional wheel throttle did not apply one detent");
                if(frames==100)wheel(-1.2f);
                if(frames==120 && std::abs(throttle)>.001f)throw std::runtime_error("Original wheel-down throttle did not restore zero throttle");
                if(frames==200){settings.mouseButtons[8]=int(Native::Binding::CameraZoomOut);settings.mouseButtons[9]=int(Native::Binding::CameraZoomIn);mouse.configure(settings);}
                if(frames==240 || frames==250 || frames==260)wheel(.4f);
                if(frames==280 && std::abs(game->state().viewFov-(settings.fieldOfView-glm::degrees(.1f)))>.02f)throw std::runtime_error("Configured fractional wheel zoom did not reach cockpit projection");
                if(frames==300)wheel(-1.2f);
                if(frames==320 && std::abs(game->state().viewFov-settings.fieldOfView)>.02f)throw std::runtime_error("Configured wheel-down zoom did not restore the cockpit field of view");
                if(frames==990){mouseSmokeMissiles=game->state().missiles;if(!mouseSmokeMissiles || game->state().missileReady<.99f)throw std::runtime_error("Original mouse missile fixture did not arm");}
                if(frames==1000)button(true,SDL_BUTTON_MIDDLE);if(frames==1001)button(false,SDL_BUTTON_MIDDLE);
                if(frames==1010 && game->state().missiles>=mouseSmokeMissiles)throw std::runtime_error("Original middle mouse assignment did not launch a real IGC missile");
                if(frames==1020){settings.mouseButtons[1]=int(Native::Binding::NextWeapon);mouse.configure(settings);}
                if(frames==1040)button(true,SDL_BUTTON_RIGHT);if(frames==1041)button(false,SDL_BUTTON_RIGHT);
                if(frames==1060 && game->state().selectedWeapon!=1)throw std::runtime_error("Configured right mouse assignment did not cycle the gun");
                if(frames==1150)game->commandView(true);
                if(frames==1160)mouseSmokeCommandDistance=glm::length(game->state().eyePosition-game->state().position);
                if(frames==1170)wheel(1);
                if(frames==1180){float distance=glm::length(game->state().eyePosition-game->state().position);
                    if(std::abs(distance/mouseSmokeCommandDistance-.8f)>.002f)throw std::runtime_error("Command wheel zoom still depends on frame time");
                    fprintf(stderr,"Mouse input smoke: original center target, fractional wheel throttle and middle-button missile; configured weapon cycle, cockpit zoom and command wheel zoom passed\n");
                }
                if(frames==1210){
                    if(game->state().coast)throw std::runtime_error("Modifier smoke started with vector lock already enabled");
                    key(true,SDL_SCANCODE_L,SDL_KMOD_NONE);
                }
                if(frames==1211 && !game->state().coast)throw std::runtime_error("Original L key-down failed to enable vector lock");
                if(frames==1212)key(true,SDL_SCANCODE_L,SDL_KMOD_NONE,true);
                if(frames==1213){if(game->state().coast)throw std::runtime_error("Original repeated key-down was incorrectly suppressed");key(false,SDL_SCANCODE_L,SDL_KMOD_NONE);}
                if(frames==1214 && game->state().coast)throw std::runtime_error("Key-up dispatched a press action");
                if(frames==1220){mouseSmokeGrid=settings.showCommandGrid;key(true,SDL_SCANCODE_LCTRL,SDL_KMOD_CTRL);}
                if(frames==1221){key(true,SDL_SCANCODE_PERIOD,SDL_KMOD_CTRL);key(false,SDL_SCANCODE_PERIOD,SDL_KMOD_CTRL);}
                if(frames==1222)key(false,SDL_SCANCODE_LCTRL,SDL_KMOD_NONE);
                if(frames==1223 && (game->state().coast || settings.showCommandGrid==mouseSmokeGrid))throw std::runtime_error("Ctrl + . changed flight vector lock or failed to toggle command grid");
                if(frames==1230){key(true,SDL_SCANCODE_LCTRL,SDL_KMOD_CTRL);button(true,SDL_BUTTON_LEFT);}
                if(frames==1231){button(false,SDL_BUTTON_LEFT);key(false,SDL_SCANCODE_LCTRL,SDL_KMOD_NONE);}
                if(frames==1232 && game->state().coast)throw std::runtime_error("Ctrl + mouse selection inadvertently enabled vector lock");
                if(frames==1240){key(true,SDL_SCANCODE_L,SDL_KMOD_CTRL);key(false,SDL_SCANCODE_L,SDL_KMOD_CTRL);}
                if(frames==1241 && game->state().coast)throw std::runtime_error("Any press action incorrectly fell through a Ctrl chord");
                if(frames==1250){key(true,SDL_SCANCODE_F3,SDL_KMOD_NONE);key(false,SDL_SCANCODE_F3,SDL_KMOD_NONE);}
                if(frames==1251 && game->state().commandView)throw std::runtime_error("Original F3 failed to exit command view");
                if(frames==1260){key(true,SDL_SCANCODE_F3,SDL_KMOD_SHIFT);key(false,SDL_SCANCODE_F3,SDL_KMOD_SHIFT);}
                if(frames==1261 && game->state().commandView)throw std::runtime_error("Shift + F3 entered command view instead of only changing perspective");
                if(frames==1270){key(true,SDL_SCANCODE_F3,SDL_KMOD_NONE);key(false,SDL_SCANCODE_F3,SDL_KMOD_NONE);}
                if(frames==1271 && !game->state().commandView)throw std::runtime_error("Original F3 failed to enter command view");
                if(frames==1280){key(true,SDL_SCANCODE_F2,SDL_KMOD_NONE);key(false,SDL_SCANCODE_F2,SDL_KMOD_NONE);}
                if(frames==1281){
                    if(game->state().commandView)throw std::runtime_error("Original F2 failed to return to cockpit");
                    fprintf(stderr,"Keyboard input smoke: original vector lock, repeat/release, exact chords, Ctrl + . grid, Ctrl + mouse, F3 / Shift + F3 / F2 passed\n");
                }
                if(frames==1300){menuVisible=true;frontend.open(Native::Screen::Settings,&game->state());frontend.open(Native::Screen::Interface);frontend.open(Native::Screen::Mouse);frontend.open(Native::Screen::MouseButtons);}
            }
            float mouseX=0,mouseY=0;
            mousePulses.reset();
            auto mouseActions=[&](Native::MouseBindings::Down bindings,bool wheel=false){
                for(size_t i=0;i<bindings.size();++i)if(bindings[i]){
                    auto binding=Native::Binding(i);if(bindingAction(binding)){if(menuVisible || movie)break;continue;}
                    if(wheel){
                        if(binding==Native::Binding::ThrottleUp || binding==Native::Binding::ThrottleDown){
                            controller.manualThrottle();wheelThrottleChanged=true;throttle=(Allegiance::StepThrottle(throttle*2-1,binding==Native::Binding::ThrottleUp)+1)*.5f;
                        }else if(binding==Native::Binding::CameraZoomIn || binding==Native::Binding::CameraZoomOut){
                            bool out=binding==Native::Binding::CameraZoomOut;
                            if(game->state().commandView)game->zoomCommandCamera(out,.1f);
                            else if(Native::cameraMode(cameraInput)==Native::CameraMode::Cockpit)
                                cockpitFov=glm::degrees(Allegiance::ZoomFieldOfView(glm::radians(cockpitFov),.1f,out,glm::radians(5.f),glm::radians(settings.fieldOfView)));
                            else cameraInput.distance=Allegiance::ZoomCameraDistance(cameraInput.distance,.1f,out,100,6000);
                        }else mousePulses.set(i);
                    }else mousePulses.set(i);
                }
            };
            if(!modernUi && game && (!network || network->ready())){
                bool available=flightConsole();if(hadFlightConsole && !available)chatConsole.reset();hadFlightConsole=available;
                if(available){chatConsole.sync(*game);chatConsole.view(renderer.size(),Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));hud.consoleControls(chatConsole.recipientButton(),chatConsole.allowedVerbs());}
            }
            else if(!game){hadFlightConsole=false;chatConsole.reset();playerPane.reset();gameStatePane.reset();}
            if(!modernUi && game && (!network || network->ready())){
                if(consoleMap())gameStatePane.close();
                if(game->state().trainingMission && bool(game->state().trainingPanes&ofGameState)!=gameStatePane.active()){
                    if(game->state().trainingPanes&ofGameState)gameStatePane.toggle(*game);else gameStatePane.close();
                }
                playerPane.sync(*game);playerPane.view(renderer.size(),Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                gameStatePane.sync(*game);gameStatePane.view(renderer.size(),Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
            }
            auto retireUnavailableCapture=[&]{
                bool held=pointerCapture.active() || frontend.helpPointerCaptured();
                if(movie)frontend.cancelHelpPointer();
                if(movie || slides){if(pointerCapture.active())pointerCapture.cancel();}
                else {
                    if(frontend.buttonCaptured() && !menuVisible && trainingStartAt<0)frontend.cancelPointer();
                    bool retired=!game || modernUi || game->state().complete || game->state().networkLobby || game->state().cinematic;
                    if(retired || (!playerLayer() && !popupCapturesPointer())){
                        if(playerPane.pointerCaptured())playerPane.cancelPointer();
                        if(gameStatePane.pointerCaptured())gameStatePane.cancelPointer();
                    }
                    if(!game && hud.pointerCaptured())hud.cancelPointer();
                }
                if(held && !pointerCapture.active() && !frontend.helpPointerCaptured())SDL_CaptureMouse(false);
            };
            syncCombatView();retireUnavailableCapture();
            SDL_Event event;while(SDL_PollEvent(&event)) {
                syncCombatView();retireUnavailableCapture();
                if(event.type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)updateDisplayScale();
                if(event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)frontend.refreshGraphicsDevice();
                // The diagnostic supplies an isolated raw-key focus context;
                // it does not create a Game or dispatch flight controls.
                keyboard.setFocus(((classicKeyboardSmoke && frames>=190 && frames<194) ||
                    ((!menuVisible || investmentFlight()) && !chatConsole.keyboard() && !playerPane.capturing() && !movie && !slides && !paused && !help && game && keyboardFocus())) && event.type!=SDL_EVENT_WINDOW_FOCUS_LOST);
                if(event.type==SDL_EVENT_MOUSE_BUTTON_UP)mouse.released(event.button.button);
                // Help owns only hits in its pane. PopupContainer precedes it
                // in the original image group; keyboard focus stays put.
                if(!modernUi && !movie && !SDL_GetWindowRelativeMouseMode(window.get()) &&
                   (frontend.helpPointerCaptured() || (frontend.helpVisible() && !popupCapturesPointer())) && !pointerCapture.active() &&
                   (event.type==SDL_EVENT_MOUSE_MOTION || event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP || event.type==SDL_EVENT_MOUSE_WHEEL)){
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();float x,y;
                    if(event.type==SDL_EVENT_MOUSE_MOTION){x=event.motion.x;y=event.motion.y;}
                    else if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP){x=event.button.x;y=event.button.y;}
                    else SDL_GetMouseState(&x,&y);
                    glm::vec2 p{x*size.x/std::max(1,w),y*size.y/std::max(1,h)};
                    const auto& pane=frontend.helpView(Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                    bool inside=frontend.helpPointerInside(size,p);auto hit=inside?Native::menuHit(pane,size,settings.uiScale,p):std::nullopt;
                    frontend.helpHover(hit);
                    if(inside || frontend.helpPointerCaptured()){
                        if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP){
                            if(event.button.button==SDL_BUTTON_LEFT || event.button.button==SDL_BUTTON_RIGHT)frontAction(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN?frontend.helpPress(hit,event.button.button,event.button.clicks>=2):frontend.helpRelease(hit,event.button.button));
                            mouse.suppress(SDL_BUTTON_MASK(event.button.button));SDL_CaptureMouse(frontend.helpPointerCaptured());
                        }
                        continue;
                    }
                }
                if(slides && !menuVisible && (event.type==SDL_EVENT_KEY_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_DOWN)) {
                    // WinTrek's global input filter precedes Slideshow::OnKey.
                    // A mapped main-menu key opens the menu without skipping a
                    // slide; the menu and its children then own their inputs.
                    if(event.type==SDL_EVENT_KEY_DOWN){
                        suppressGlobalText=false;if(globalKeyFilter(event.key))continue;
                        auto pressed=keyboard.pressed(event.key.scancode,event.key.mod);
                        if(pressed==Native::Binding::Screenshot || (!modernUi && pressed==Native::Binding::Pause && frontend.mainMenuInput())){
                            bindingAction(*pressed,keyboard.command(event.key.scancode,event.key.mod));continue;
                        }
                    }
                    slides->advance();continue;
                }
                if(controller.event(event))controllerInfo();
                if(menuVisible && !movie && frontend.mappingController(event))continue;
                if(modernUi && menuVisible && !investmentFlight() && !movie && !frontend.capturingKey())if(auto key=controller.menuKey(event)){frontAction(frontend.key(key,SDL_SCANCODE_UNKNOWN));continue;}
                if(event.type==SDL_EVENT_QUIT)running=false;
                if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST){pointerCapture.cancel();frontend.cancelHelpPointer();SDL_CaptureMouse(false);mouse.suppress(SDL_GetMouseState(nullptr,nullptr));if(!frameLimit && game && !menuVisible){menuVisible=true;frontend.open(Native::Screen::Pause,&game->state());SDL_SetWindowRelativeMouseMode(window.get(),false);}}
                if(event.type==SDL_EVENT_MOUSE_MOTION){
                    int mapW,mapH;SDL_GetWindowSize(window.get(),&mapW,&mapH);auto mapSize=renderer.size();
                    if(pointerCapture.move(mapSize,{event.motion.x*mapSize.x/std::max(1,mapW),event.motion.y*mapSize.y/std::max(1,mapH)}))continue;
                    const float dx=Allegiance::MouseDelta(event.motion.xrel,settings.sensitivity,settings.mouseAcceleration,6,10);
                    const float dy=Allegiance::MouseDelta(event.motion.yrel,settings.sensitivity,settings.mouseAcceleration,6,10);
                    mouseX+=dx;mouseY+=dy;mouseJoystick.move(dx,-dy,1.f/512);
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();
                    if(w>0 && h>0){commandGesture.pointer={event.motion.x*size.x/w,event.motion.y*size.y/h};if(!popupCapturesPointer())chatConsole.hover(size,commandGesture.pointer);if(playerLayer() && !popupCapturesPointer()){playerPane.hover(size,commandGesture.pointer);gameStatePane.hover(size,commandGesture.pointer);}if(menuPointer()){frontend.pointerMove(Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas).local(commandGesture.pointer),size);frontend.pointerHover(Native::menuHit(frontend.view(),size,settings.uiScale,commandGesture.pointer));}}
                }
                if(event.type==SDL_EVENT_MOUSE_WHEEL && pointerCapture.active())continue;
                if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP){
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();glm::vec2 p{event.button.x*size.x/std::max(1,w),event.button.y*size.y/std::max(1,h)};
                    if(pointerCapture.button(size,p,event.button.button,event.type==SDL_EVENT_MOUSE_BUTTON_DOWN,event.button.clicks>=2,(SDL_GetModState()&SDL_KMOD_CTRL)!=0,frontAction,action)){
                        if(game){if(!gameStatePane.active() && game->state().trainingMission)game->setTrainingPane(ofGameState,false);playerPane.sync(*game,true);}
                        mouse.suppress(SDL_BUTTON_MASK(event.button.button));blockFire=true;SDL_CaptureMouse(pointerCapture.active());continue;
                    }
                }
                if(!modernUi && game && !popupCapturesPointer() && !movie && !slides && (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP)) {
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();glm::vec2 p{event.button.x*size.x/std::max(1,w),event.button.y*size.y/std::max(1,h)};
                    chatConsole.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                    if(event.button.button==SDL_BUTTON_LEFT && chatConsole.pointer(*game,size,p,event.type==SDL_EVENT_MOUSE_BUTTON_DOWN)) {mouse.suppress(SDL_BUTTON_LMASK);blockFire=true;continue;}
                    gameStatePane.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                    if(playerLayer() && gameStatePane.pointer(size,p,event.button.button,event.type==SDL_EVENT_MOUSE_BUTTON_DOWN)){
                        if(!gameStatePane.active() && game->state().trainingMission)game->setTrainingPane(ofGameState,false);
                        mouse.suppress(SDL_BUTTON_MASK(event.button.button));blockFire=true;SDL_CaptureMouse(gameStatePane.pointerCaptured());continue;
                    }
                    playerPane.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                    if(playerLayer())if(auto result=playerPane.pointer(size,p,event.button.button,event.type==SDL_EVENT_MOUSE_BUTTON_DOWN,event.button.clicks>=2)){
                        frontAction(*result);mouse.suppress(SDL_BUTTON_MASK(event.button.button));blockFire=true;SDL_CaptureMouse(playerPane.pointerCaptured());playerPane.sync(*game,true);continue;
                    }
                }
                if(!scenePointerInput() || event.type==SDL_EVENT_WINDOW_FOCUS_LOST){
                    bool captured=commandGesture.selectionStart || commandGesture.orderStart;commandGesture.clear();
                    if(captured)SDL_CaptureMouse(false);
                }
                if(event.type==SDL_EVENT_MOUSE_WHEEL && !popupCapturesPointer() && playerLayer() && playerPane.wheel(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-event.wheel.y:event.wheel.y))continue;
                if(menuPointer() && !movie && event.type==SDL_EVENT_MOUSE_WHEEL){if(frontend.pointerWheel(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-event.wheel.y:event.wheel.y) || !investmentFlight())continue;}
                if(menuVisible && !movie && event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && frontend.mappingMouse(event.button.button)){mouse.suppress(SDL_BUTTON_MASK(event.button.button));continue;}
                // Console controls consume their own hits;
                // the uncovered scene retains the original selection/order gestures.
                if(consoleMap() && (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP)){
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();glm::vec2 p{event.button.x*size.x/std::max(1,w),event.button.y*size.y/std::max(1,h)};
                    auto hit=Native::menuHit(frontend.view(),size,settings.uiScale,p);
                    if(hit || frontend.pointerCaptured()){
                        frontend.pointerMove(Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas).local(p),size);
                        if(event.button.button==SDL_BUTTON_LEFT){if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN){frontAction(frontend.pointerPress(hit,event.button.clicks>=2));SDL_CaptureMouse(frontend.pointerCaptured());}else {frontAction(frontend.pointerRelease(hit,event.button.clicks>=2));SDL_CaptureMouse(false);}}
                        else if(event.button.button==SDL_BUTTON_RIGHT){
                            if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN){frontAction(frontend.pointerContext(hit,event.button.clicks>=2));SDL_CaptureMouse(frontend.pointerCaptured());}
                            else if(frontend.pointerCaptured()){frontAction(frontend.pointerRelease(hit,event.button.clicks>=2,event.button.button));SDL_CaptureMouse(frontend.pointerCaptured());}
                        }
                        mouse.suppress(SDL_BUTTON_MASK(event.button.button));continue;
                    }
                }
                if(game && (!menuVisible || consoleMap()) && !popupCapturesPointer() && !movie && !slides && !help && !paused && (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP)){
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();glm::vec2 p{event.button.x*size.x/std::max(1,w),event.button.y*size.y/std::max(1,h)};
                    auto result=hud.mapPointerButton(p,event.button.button,event.type==SDL_EVENT_MOUSE_BUTTON_DOWN,(SDL_GetModState()&SDL_KMOD_CTRL)!=0);
                    if(result.handled){if(result.action)action(*result.action);SDL_CaptureMouse(result.capture);mouse.suppress(SDL_BUTTON_MASK(event.button.button));blockFire=true;continue;}
                }
                if(event.type==SDL_EVENT_MOUSE_WHEEL && game && game->state().commandView && (!menuVisible || investmentFlight()) && !popupCapturesPointer() && !help && !paused){
                    float y=event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-event.wheel.y:event.wheel.y;int ticks=mouse.wheelTicks(y);
                    if(!commandGesture.selectionStart)for(int i=0;i<std::abs(ticks);++i)game->zoomCommandCamera(ticks<0,.1f);continue;
                }
                if((event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP) && (event.button.button==SDL_BUTTON_LEFT || event.button.button==SDL_BUTTON_RIGHT) && scenePointerInput() && commandProjectionValid) {
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();
                    if(w>0 && h>0) {
                        glm::vec2 p(event.button.x*size.x/w,event.button.y*size.y/h);commandGesture.pointer=p;
                        auto hit=hud.click(p);bool down=event.type==SDL_EVENT_MOUSE_BUTTON_DOWN;
                        if(!modernUi && commandProjectionValid && (!hit || hit->command==Native::HudCommand::Target)) {
                            bool composing=chatConsole.mode()==Allegiance::ComposeState::Command;
                            int individual=chatConsole.audience().channel==Native::ChatChannel::Target?chatConsole.audience().recipient:-1;
                            auto* pick=Native::commandHover(game->contacts(),p,size,commandProjection,[&](const auto& c){return !game->commandCursor(c.type,c.id,chatConsole.subjects(),composing,chatConsole.currentVerb(),individual,event.button.button==SDL_BUTTON_LEFT?0:1).empty();});
                            hit=pick?std::optional<Native::HudAction>({Native::HudCommand::Target,pick->id,pick->type}):std::nullopt;
                        }
                        if(!hit || hit->command==Native::HudCommand::Target || (!down && (commandGesture.selectionStart || commandGesture.orderStart))){
                            std::optional<Native::ScenePointerTarget> target;
                            if(hit && hit->command==Native::HudCommand::Target)target=Native::ScenePointerTarget{hit->type,hit->value};
                            if(Native::scenePointerButton(commandGesture,*game,chatConsole,p,size,commandProjection,commandCamera,event.button.button,down,(SDL_GetModState()&SDL_KMOD_SHIFT)!=0,!modernUi,target)){
                                SDL_CaptureMouse(commandGesture.selectionStart || commandGesture.orderStart);mouse.suppress(SDL_BUTTON_MASK(event.button.button));continue;
                            }
                        }else if(event.button.button==SDL_BUTTON_RIGHT)continue;
                    }
                }
                if(event.type==SDL_EVENT_MOUSE_WHEEL && game && !menuPointer() && !movie && (!modernUi || !cursor || game->state().commandView) && !help && !paused && !game->state().dead && !game->state().docked && !game->state().cinematicDeath){
                    float y=event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-event.wheel.y:event.wheel.y;
                    int ticks=mouse.wheelTicks(y);for(int i=0;i<std::abs(ticks) && !menuPointer() && !movie;++i)mouseActions(mouse.wheel(float(ticks)),true);
                }
                if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button==SDL_BUTTON_LEFT && !movie && !SDL_GetWindowRelativeMouseMode(window.get())) {
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();
                    if(w>0 && h>0) {
                        glm::vec2 p{event.button.x*size.x/w,event.button.y*size.y/h};
                        if(menuPointer()) {
                            frontend.pointerMove(Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas).local(p),size);
                            auto hit=Native::menuHit(frontend.view(),size,settings.uiScale,p);
                            if(frontend.view().original){frontAction(frontend.pointerPress(hit,event.button.clicks>=2));SDL_CaptureMouse(frontend.pointerCaptured());}
                            else if(hit){action({Native::HudCommand::Menu,int(*hit)});blockFire=true;}
                            mouse.suppress(SDL_BUTTON_LMASK);continue;
                        } else if(auto a=hud.click(p)){action(*a);blockFire=true;mouse.suppress(SDL_BUTTON_LMASK);continue;}
                    }
                }
                if(event.type==SDL_EVENT_MOUSE_BUTTON_UP && (event.button.button==SDL_BUTTON_LEFT || (event.button.button==SDL_BUTTON_RIGHT && frontend.pointerCaptured())) && menuPointer() && frontend.view().original && !movie) {
                    SDL_CaptureMouse(false);
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();
                    if(w>0 && h>0){glm::vec2 p{event.button.x*size.x/w,event.button.y*size.y/h};frontAction(frontend.pointerRelease(Native::menuHit(frontend.view(),size,settings.uiScale,p),event.button.clicks>=2,event.button.button));SDL_CaptureMouse(frontend.pointerCaptured());blockFire=true;}
                    continue;
                }
                if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button==SDL_BUTTON_RIGHT && menuPointer() && frontend.view().original && !movie){
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();
                    if(w>0 && h>0){glm::vec2 p{event.button.x*size.x/w,event.button.y*size.y/h};auto hit=Native::menuHit(frontend.view(),size,settings.uiScale,p);
                        frontend.pointerMove(Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas).local(p),size);frontAction(frontend.pointerContext(hit,event.button.clicks>=2));}
                    SDL_CaptureMouse(frontend.pointerCaptured());mouse.suppress(SDL_BUTTON_RMASK);blockFire=true;
                    continue;
                }
                if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button!=SDL_BUTTON_LEFT && event.button.button!=SDL_BUTTON_RIGHT && menuPointer() && frontend.view().original && !movie){
                    int w,h;SDL_GetWindowSize(window.get(),&w,&h);auto size=renderer.size();
                    if(w>0 && h>0){glm::vec2 p{event.button.x*size.x/w,event.button.y*size.y/h};frontend.pointerMove(Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas).local(p),size);frontAction(frontend.pointerOther());}
                    SDL_CaptureMouse(frontend.pointerCaptured());mouse.suppress(SDL_BUTTON_MASK(event.button.button));blockFire=true;
                    continue;
                }
                if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && game && !menuPointer() && !chatConsole.keyboard() && !movie &&
                   ((!game->state().commandView && !mousePointerMode() && !playerPane.active() && !gameStatePane.active()) || (!modernUi && event.button.button!=SDL_BUTTON_LEFT && event.button.button!=SDL_BUTTON_RIGHT)) && !help && !paused && !blockFire)
                    mouseActions(mouse.pressed(event.button.button));
                if(event.type==SDL_EVENT_TEXT_INPUT){
                    if(suppressGlobalText){suppressGlobalText=false;continue;}
                    if(popupCapturesInput()){if(!frontend.hasMessageBox() && !detailsPopup())frontend.text(event.text.text);}
                    else if(flightConsole() && chatConsole.wantsText())chatConsole.text(*game,event.text.text);
                    else if(menuVisible)frontend.text(event.text.text);
                }
                if(event.type==SDL_EVENT_KEY_UP && menuVisible && !movie)frontend.keyReleased(event.key.key,event.key.scancode);
                if(event.type==SDL_EVENT_KEY_UP)keyboard.keyUp(event.key.scancode);
                if(event.type==SDL_EVENT_KEY_DOWN) {
                    suppressGlobalText=false;
                    if(movie){if(!event.key.repeat && (event.key.key==SDLK_ESCAPE || event.key.key==SDLK_SPACE))finishMovie();continue;}
                    if(globalKeyFilter(event.key))continue;
                    if(popupCapturesInput()){frontAction(frontend.keyboardEvent(event.key));continue;}
                    auto pressed=keyboard.pressed(event.key.scancode,event.key.mod);
                    auto command=keyboard.command(event.key.scancode,event.key.mod);
                    if(pressed==Native::Binding::Screenshot && !frontend.capturingKey()){
                        bindingAction(*pressed,command);continue;
                    }
                    // WinTrek's global input filter pages the message log even
                    // while a text entry or another game pane owns keyboard focus.
                    if(!frontend.capturingKey() && !(menuVisible && frontend.screen()==Native::Screen::Controls) && (pressed==Native::Binding::ChatPageUp || pressed==Native::Binding::ChatPageDown)){
                        bindingAction(*pressed,command);continue;
                    }
                    if(flightConsole() && !slides && !frontend.capturingKey() && chatConsole.key(*game,event.key.key,(event.key.mod&SDL_KMOD_CTRL)!=0))continue;
                    if((playerLayer() || consoleMap()) && !slides && !frontend.capturingKey() && (pressed==Native::Binding::VoteYes || pressed==Native::Binding::VoteNo)){bindingAction(*pressed,command);continue;}
                    if(playerLayer() && !slides && !frontend.capturingKey() && gameStatePane.key(event.key.key)){
                        if(game->state().trainingMission)game->setTrainingPane(ofGameState,false);continue;
                    }
                    if(playerLayer() && !slides && !frontend.capturingKey())if(auto result=playerPane.key(event.key.key)){frontAction(*result);continue;}
                    if((playerLayer() || consoleMap()) && (pressed==Native::Binding::PaneContacts || pressed==Native::Binding::GameState) && !frontend.capturingKey()){bindingAction(*pressed,command);continue;}
                    if(!modernUi && game && menuVisible && !frontend.hasMessageBox() && (frontend.screen()==Native::Screen::Hangar || frontend.screen()==Native::Screen::ShipLoadout || consoleMap()) && (pressed==Native::Binding::Chat || pressed==Native::Binding::QuickChat)){bindingAction(*pressed,command);continue;}
                    if(consoleMap() && pressed==Native::Binding::SectorMap){bindingAction(*pressed,command);continue;}
                    if(!modernUi && menuVisible && game && pressed && frontend.consoleBinding(*pressed)){bindingAction(*pressed,command);continue;}
                    if(modernUi && menuVisible && game && !frontend.hasMessageBox() &&
                        ((pressed==Native::Binding::Research && (frontend.screen()==Native::Screen::Hangar || frontend.screen()==Native::Screen::Research)) ||
                         (pressed==Native::Binding::Teleport && (frontend.screen()==Native::Screen::Hangar || frontend.screen()==Native::Screen::Teleport)) ||
                         (pressed==Native::Binding::CommandView && frontend.screen()==Native::Screen::Hangar))){bindingAction(*pressed,command);continue;}
                    if(menuVisible && (!investmentFlight() || event.key.key==SDLK_ESCAPE)){frontAction(frontend.keyboardEvent(event.key));continue;}
                    if(pressed) {
                        bindingAction(*pressed,command);continue;
                    }
                    if(!game && event.key.key==SDLK_ESCAPE)running=false;
                }
            }
            syncCombatView();
            keyboard.setFocus((classicKeyboardSmoke && frames>=190 && frames<194) ||
                ((!menuVisible || investmentFlight()) && !chatConsole.keyboard() && !playerPane.capturing() && !movie && !slides && !paused && !help && game && keyboardFocus()));
            bool commandWheel=game && game->state().commandView && (!menuVisible || investmentFlight()) && !popupCapturesInput() && !movie && !slides && !paused && !help && keyboardFocus();
            if(menuPointer() || (playerLayer() && (playerPane.active() || gameStatePane.active())) || movie || slides || paused || help || (modernUi && cursor) || !keyboardFocus())mouse.sample(SDL_GetMouseState(nullptr,nullptr),false,commandWheel);
            bool wantsText=chatConsole.wantsText() || (menuVisible && !chatConsole.keyboard() && frontend.wantsText());
            if(wantsText!=SDL_TextInputActive(window.get())){if(wantsText)SDL_StartTextInput(window.get());else SDL_StopTextInput(window.get());}
            bool capture=pilotMouseControl() && !frameLimit && (inspect || (game && !game->state().dead && !game->state().docked && !game->state().commandView));
            mouseJoystick.enable(capture && (!modernUi || settings.virtualJoystick));
            if(SDL_GetWindowRelativeMouseMode(window.get())!=capture)SDL_SetWindowRelativeMouseMode(window.get(),capture);
            auto now=Native::monotonicNs();double wallDt=double(now-previous)*1e-9;float dt=std::clamp(float(wallDt),0.0001f,0.1f);previous=now;
            Native::FrameTiming frameTiming{now,acquired?renderer.targetPresentTime():0};
            frontAction(frontend.tick(wallDt,true));
            if(auto result=playerPane.tick(wallDt))frontAction(*result);
            flushUiSounds();
            for(auto id:chatConsole.sounds())audio.play(id,{},{},true);
            chatConsole.clearSounds();
            if(!acquired){if(network && network->ready()){Native::FlightInput neutral;neutral.active=false;network->step(neutral,wallDt);}SDL_Delay(10);continue;}
            if(smoke && game) {
                if(frames==120){game->nextHull();thirdPerson=true;}
                if(frames==200){hud.key(SDLK_F6);game->selectWeapon(0);}
                if(frames==300){game->nextHull();hud.key(SDLK_F8);}
                if(frames==500){hud.key(SDLK_F6);hud.key(SDLK_F8);game->selectWeapon(-1);}
                if(frames==760)hud.key(SDLK_F4);
                if(frames==820)hud.key(SDLK_F4);
                if(frames==900){hud.key(SDLK_F7);help=true;}
                if(frames==980){hud.key(SDLK_F7);help=false;}
                if(frames==700)game->respawn();
                if(frames==850){hud.setScale(uiScale*0.8f);if(windowed)SDL_SetWindowSize(window.get(),1024,768);}
                if(frames==1000){hud.setScale(uiScale);if(windowed)SDL_SetWindowSize(window.get(),1280,720);}
                if(frames==1100)playMovie("intro_microsoft.avi");
                if(frames==1500)movie.reset();
                if(frames==1600)game->respawn();
                if(frames>1600 && game->state().dead)game->respawn();
            }
            float fallbackThrottle;
            bool forcedThrottle=false;
            if(game && game->takeThrottleReset(fallbackThrottle,&forcedThrottle)) {
                if(forcedThrottle){controller.manualThrottle();throttle=fallbackThrottle;}
                else if(!controller.resumeThrottle())throttle=fallbackThrottle;
            }
            const auto& control=controller.sample((SDL_GetWindowFlags(window.get())&SDL_WINDOW_INPUT_FOCUS)!=0);
            if((!menuVisible || investmentFlight()) && !chatConsole.keyboard() && !playerPane.capturing() && !movie)for(size_t i=0;i<control.pressed.size();++i)if(control.pressed[i]) {
                bindingAction(Native::Binding(i));if(menuVisible && !investmentFlight())break;
            }
            auto size=renderer.size();auto projection=glm::perspectiveRH_ZO(glm::radians(settings.fieldOfView),float(size.x)/size.y,30000.0f,0.2f);projection[1][1]*=-1;
            Native::Game* soundGame=nullptr;
            Native::CameraState soundCamera;
            bool soundPaused=false,soundListener=false,clearWorldSounds=false;
            auto scalePrepareStart=ScaleClock::now();double scaleStepMs=0;
            glm::mat4 view(1);draws.clear();std::span<const Native::HudQuad> overlay;Native::HudBackground background;Native::RenderLighting lighting;const Native::ModelPreview* preview=nullptr;
            float pointerX,pointerY;SDL_GetMouseState(&pointerX,&pointerY);int windowWidth,windowHeight;SDL_GetWindowSize(window.get(),&windowWidth,&windowHeight);
            glm::vec2 pointer{pointerX*size.x/std::max(1,windowWidth),pointerY*size.y/std::max(1,windowHeight)};
            const auto visiblePointer=pointer;
            bool helpBlocksPointer=false;
            if(!modernUi && !movie && frontend.helpVisible()){
                frontend.helpView(Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                helpBlocksPointer=!capture && !pointerCapture.active() && (frontend.helpPointerCaptured() || !popupCapturesPointer()) && frontend.helpPointerInside(size,pointer);
                if(helpBlocksPointer)pointer={-1,-1};
            }
            bool capturedHover=pointerCapture.move(size,pointer);
            if(!capturedHover && !helpBlocksPointer && menuVisible && frontend.view().original)frontend.pointerMove(Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas).local(pointer),size);
            // SetEnabled also fires the original enabled-enter event when the
            // pointer has not moved. Observe the current retained layout once;
            // the feedback state suppresses unchanged hover/refresh events.
            if(!capturedHover && !helpBlocksPointer && !modernUi && !movie && menuPointer())frontend.pointerHover(Native::menuHit(frontend.view(),size,settings.uiScale,pointer));
            auto renderGame=[&](bool menuPaused){
                syncCombatView();
                cameraInput.chase=thirdPerson;cameraInput.fieldOfView=cockpitFov;cameraInput.viewportHeight=renderer.size().y;
                if(cameraSmoke)cameraInput.headYaw=frames>=1400 && frames<1600?glm::radians(180.f):frames>=1200 && frames<1400?glm::radians(90.f):0;
                if(previousCinematic && !game->state().cinematic){cameraInput.mode=Native::CameraMode::Cockpit;cameraInput.chase=thirdPerson=settings.preferChase;}
                previousCinematic=game->state().cinematic;
                const auto& camera=game->camera(cameraInput);
                auto& state=game->state();auto eye=camera.position;
                projection=glm::perspectiveRH_ZO(glm::radians(camera.fieldOfView),float(size.x)/size.y,30000.0f,0.2f);projection[1][1]*=-1;
                soundGame=game.get();soundCamera=camera;
                soundPaused=menuPaused || paused || help || state.trainingPaused;soundListener=true;clearWorldSounds=false;
                view=glm::lookAtRH(eye,eye+camera.forward,camera.up);
                background=hud.buildBackground(size,state,camera);lighting=state.lighting;
                const auto& scene=game->scene(camera.showPlayer);draws.insert(draws.end(),scene.begin(),scene.end());
                if(refitSmoke && state.cinematic && !state.cinematicDeath && !state.docked){
                    auto submitted=scene.size();
                    if(!game->playerShip()->GetVisibleF() || !camera.showPlayer || submitted!=game->scene(false).size()+1)
                        throw std::runtime_error("Remote launch camera loses the player's ship model");
                    auto projected=projection*view*glm::vec4(state.position,1);
                    if(!(projected.w>state.radius && std::abs(projected.x)<projected.w && std::abs(projected.y)<projected.w))
                        throw std::runtime_error("Remote launch camera leaves the player's ship outside the view");
                    size_t launch=refitStage>=11 && refitStage!=14?1:0;++refitLaunchFrames[launch];
                    if(refitLaunchStart[launch]<0)refitLaunchStart[launch]=state.time;
                    double age=state.time-refitLaunchStart[launch];int& captured=refitLaunchCaptures[launch];
                    if(!screenshot.empty() && captured<3 && age>=std::array<double,3>{.05,1.4,2.7}[captured]){
                        auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-launch"+std::to_string(launch+1)+"-"+std::to_string(++captured)+".ppm")).string();
                    }
                }
                if(refitSmoke && !state.cinematic && !state.docked && (refitStage==8 || refitStage==13)) {
                    size_t launch=refitStage==13?1:0;
                    if(!camera.animating)refitTransitionComplete[launch]=true;
                    else {
                        auto submitted=scene.size();
                        if(!game->playerShip()->GetVisibleF() || !camera.showPlayer || submitted!=game->scene(false).size()+1)
                            throw std::runtime_error("Remote launch-to-cockpit transition loses the player's ship model");
                        ++refitTransitionFrames[launch];
                        if(refitTransitionStart[launch]<0)refitTransitionStart[launch]=state.time;
                        if(!screenshot.empty() && !refitTransitionCaptured[launch] && state.time-refitTransitionStart[launch]>=.3) {
                            auto path=std::filesystem::path(screenshot);requestedScreenshot=(path.parent_path()/(path.stem().string()+"-transition"+std::to_string(launch+1)+".ppm")).string();refitTransitionCaptured[launch]=true;
                        }
                    }
                }
                commandProjection=projection*view;commandCamera=camera;commandProjectionValid=true;
                overlay=hud.build(size,state,game->contacts(),projection*view,renderer.stats(),paused && !menuVisible,help && !menuVisible,!capture,pointer,&commandGesture,&camera,game->investmentSectors(menuVisible?frontend.investmentAsteroid():0),consoleFlags());preview=hud.preview();
            };
            if(slides && slides->finished() && !slidesPost && trainingStartAt>=0 && Native::uiTime()>=trainingStartAt) {
                slides.reset();trainingStartAt=-1;frontend.connectionFinished();startSession(slideSession);
            }
            if(slides) {
                slides->tick(dt);
                if(slides->finished()) {
                    if(slidesPost){slides.reset();frontend.trainingSlides(false);audio.stopAll();game.reset();frontend.setPlaying(false);frontend.open(Native::Screen::Training);menuVisible=true;}
                    else if(modernUi){slides.reset();startSession(slideSession);}
                    else if(trainingStartAt<0){frontend.trainingStarting();trainingStartAt=Native::uiTime()+.1;}
                }
                if(slides){
                    overlay=hud.buildSlide(size,slides->current(),slides->index(),slides->all().size());view=glm::mat4(1);
                    if(menuVisible)overlay=hud.buildMenu(size,frontend.sceneView(),pointer,true);
                }
            } else if(movie) {
                movie->paused(!frameLimit && !(SDL_GetWindowFlags(window.get())&SDL_WINDOW_INPUT_FOCUS));
                auto image=movie->advance();
                if(image && image!=lastMovieFrame){renderer.updateTexture(movieTexture,*image);lastMovieFrame=image;}
                auto dimensions=movie->size();float aspect=float(dimensions.x)/dimensions.y;
                float sx=1,sy=1;
                if(float(size.x)/size.y>aspect)sx=aspect*size.y/size.x;else sy=size.x/(aspect*size.y);
                movieQuads={{{-sx,-sy,sx,sy},{0,0,1,1},glm::vec4(1),movieTexture}};
                overlay=movieQuads;view=glm::mat4(1);
                if(movie->finished()){
                    fprintf(stderr,"Movie finished\n");
                    finishMovie();
                }
            } else if(menuVisible && !investmentFlight()) {
                if(game && (!network || network->ready()) && !game->state().complete && (network || game->state().economy || game->state().trainingMission || frontend.screen()==Native::Screen::Chat || frontend.screen()==Native::Screen::QuickChat) && (network || (frontend.screen()!=Native::Screen::Pause && frontend.screen()!=Native::Screen::ConfirmLeave))) {
                    Native::FlightInput idle;idle.active=false;idle.throttle=throttle;if(network)network->step(idle,dt,frameTiming);else game->step(idle,dt);
                    soundGame=game.get();
                    syncCombatView();
                    frontend.sync(game->state());
                    if(game->state().complete)frontend.open(Native::Screen::Results,&game->state());
                }
                if(game && (!network || network->ready()) && !game->state().complete) {
                    auto& state=game->state();soundGame=game.get();
                    soundCamera.position=state.eyePosition;soundCamera.forward=state.forward;soundCamera.up=state.up;soundCamera.mode=state.camera;
                    soundListener=true;
                } else clearWorldSounds=true;
                bool gameBehindMenu=(!menuVisible || frontend.view().overlay) && game && (!network || network->ready());
                if(gameBehindMenu){
                    cameraInput.yaw=cameraInput.pitch=cameraInput.roll=cameraInput.panRight=cameraInput.panDown=cameraInput.zoom=0;
                    renderGame(frontend.screen()==Native::Screen::Pause && !network);
                }else{
                    soundPaused=frontend.screen()==Native::Screen::Pause || frontend.screen()==Native::Screen::ConfirmLeave || (game && game->state().trainingPaused);view=glm::mat4(1);
                }
                if(menuVisible){overlay=hud.buildMenu(size,frontend.sceneView(),pointer,gameBehindMenu);preview=hud.preview();}
                if(menuVisible && consoleMap() && !gameBehindMenu)overlay=hud.buildSectorMap(size,game->state(),pointer,consoleFlags(),game->investmentSectors(frontend.investmentAsteroid()));
                if(menuVisible && !modernUi && !gameBehindMenu && game && !game->state().complete && !game->state().networkLobby &&
                   (frontend.screen()==Native::Screen::Hangar || frontend.screen()==Native::Screen::ShipLoadout || consoleMap() || detailsPopup() || !frontend.menuPopupView().artwork.empty()))
                    {overlay=hud.appendStatus(size,game->state(),consoleFlags());overlay=hud.appendMessages(size,game->state(),consoleFlags());preview=hud.preview();}
            } else if(game && (!network || network->ready())) {
                auto keyboardDown=keyboard.sample();auto buttons=SDL_GetMouseState(nullptr,nullptr);
                Native::FlightInput input;input.deadZone=settings.controller.deadZone;input.throttleChanged=wheelThrottleChanged || control.throttleChanged;wheelThrottleChanged=false;
                cameraInput.yaw=cameraInput.pitch=cameraInput.roll=0;
                if(!buttons)blockFire=false;
                bool state_modal=game->state().dead || game->state().docked || game->state().cinematicDeath;
                bool flightMouse=!menuPointer() && !paused && !help && (!modernUi || !cursor) && !state_modal && !blockFire && keyboardFocus();
                auto mouseDown=mouse.sample(Native::MouseBindings::pilotButtons(buttons,!modernUi && !pilotMouseControl()),flightMouse && !game->state().commandView,flightMouse || commandWheel);
                mouseDown|=mousePulses;
                auto held=[&](Native::Binding binding){return !chatConsole.keyboard() && (keyboardDown[size_t(binding)] || control.down[size_t(binding)] || mouseDown[size_t(binding)]);};
                // TK_ToggleMouse controls the mouse stream, not keyboard or
                // physical joystick input. Keep the optional native policy.
                bool pilotControls=!modernUi || !cursor || investmentFlight();
                if(!paused && !help) {
                    throttleRepeat.Advance(dt,settings.throttleDelay);
                    if(pilotControls && !state_modal) {
                        if(control.useThrottle)throttle=control.throttle;
                        if((held(Native::Binding::ThrottleUp) || held(Native::Binding::ThrottleDown)) && throttleRepeat.Consume()) {
                            controller.manualThrottle();input.throttleChanged=true;throttle=(Allegiance::StepThrottle(throttle*2-1,held(Native::Binding::ThrottleUp))+1)*.5f;
                        }
                    }
                    for(int preset=0;preset<4;++preset)if(held(Native::Binding(int(Native::Binding::ThrottleZero)+preset))) {
                        controller.manualThrottle();input.throttleChanged=true;throttle=(Allegiance::ThrottlePreset(preset)+1)*.5f;break;
                    }
                    input.matchSpeed=held(Native::Binding::MatchSpeed);
                    if(input.matchSpeed){controller.manualThrottle();throttle=game->matchTargetThrottle(throttle);}
                    input.active=!chatConsole.keyboard() && !playerPane.capturing() && pilotControls && !state_modal;input.throttle=throttle;
                    if(pilotControls && !playerPane.capturing() && !help && !state_modal) {
                        bool relativeMouse=modernUi && !menuPointer() && !playerPane.active() && !gameStatePane.active();
                        auto axes=Allegiance::JoystickSteering(mouseJoystick.enabled,mouseJoystick.x,mouseJoystick.y,
                            std::clamp(control.axes[0]-(relativeMouse?mouseX:0)*0.002f/dt,-1.0f,1.0f),
                            std::clamp(control.axes[1]+(relativeMouse?mouseY:0)*0.002f/dt,-1.0f,1.0f),
                            control.axes[2],held(Native::Binding::RollModifier),settings.invertY);
                        input.yaw=axes.yaw;input.pitch=axes.pitch;input.roll=axes.roll;
                        if(!game->state().commandView)Allegiance::KeyboardSteering(input.yaw,input.pitch,input.roll,[&](TrekKey key){
                            switch(key){case TK_RollRight:return held(Native::Binding::RollRight);case TK_RollLeft:return held(Native::Binding::RollLeft);
                            case TK_RollModifier:return held(Native::Binding::RollModifier);case TK_YawRight:return held(Native::Binding::YawRight);
                            case TK_YawLeft:return held(Native::Binding::YawLeft);case TK_PitchDown:return held(Native::Binding::PitchDown);
                            case TK_PitchUp:return held(Native::Binding::PitchUp);default:return false;}
                        });
                        if((keyboard.keyIsDown(SDL_SCANCODE_LALT) || keyboard.keyIsDown(SDL_SCANCODE_RALT)) && Native::cameraMode(cameraInput)!=Native::CameraMode::Cockpit) {
                            cameraInput.yaw=float(held(Native::Binding::YawLeft))-float(held(Native::Binding::YawRight));
                            cameraInput.pitch=float(held(Native::Binding::PitchUp))-float(held(Native::Binding::PitchDown));cameraInput.roll=input.roll;
                            input.yaw=input.pitch=input.roll=0;
                        }
                        input.forward=held(Native::Binding::Forward);input.backward=held(Native::Binding::Backward);
                        if(!game->state().commandView){input.left=held(Native::Binding::Left);input.right=held(Native::Binding::Right);input.up=held(Native::Binding::Up);input.down=held(Native::Binding::Down);}
                        input.boost=held(Native::Binding::Boost);
                        input.fire=held(Native::Binding::Fire);input.missile=held(Native::Binding::Missile);
                        input.chaff=held(Native::Binding::Chaff);input.deploy=held(Native::Binding::Deploy);
                    }
                    if(smoke) {
                        const auto& state=game->state();input.throttle=0.2f;input.fire=true;input.missile=frames>400&&frames<650;
                        if(!state.target.empty()) {
                            auto direction=(state.hasWeaponSolution?state.leadDirection:glm::normalize(state.targetPosition-state.position));
                            input.yaw=std::clamp(-glm::dot(direction,state.right)*3,-1.0f,1.0f);
                            input.pitch=std::clamp(-glm::dot(direction,state.up)*3,-1.0f,1.0f);
                        }
                    }
                    if(cargoSmoke){input={};input.fire=frames<330;}
                    if(surfaceSmoke){input={};input.deploy=frames>=30 && frames<100;thirdPerson=true;}
                    if(dockSmoke && dockingStage==1){input.throttle=1;input.boost=true;}
                    if(cameraSmoke) {
                        input.throttle=.45f;input.boost=(frames>=120 && frames<400) || frames>=1800;
                        input.yaw=frames>400?.2f:0;input.pitch=frames>400?-.1f:0;input.roll=frames>400?.07f:0;
                        input.left=frames>=1200 && frames<1400;
                        thirdPerson=(frames>=450 && frames<1050) || frames>=1750;
                        if(frames>=2400) {
                            const Native::CameraMode modes[]={Native::CameraMode::Orbit,Native::CameraMode::Station,Native::CameraMode::Target,
                                Native::CameraMode::FlyBy,Native::CameraMode::External,Native::CameraMode::Missile};
                            cameraInput.mode=modes[std::min(5u,(frames-2400)/600)];thirdPerson=false;
                            input.missile=frames>=5400 && frames<5450;
                        }
                    }
                    if(crewSmoke) {
                        input.throttle=1;input.fire=(frames%300)<150;input.yaw=frames<600?.25f:0;input.pitch=frames>=600 && frames<900?.15f:0;
                        if(frames==900 && !game->boardShip(game->state().parentId))throw std::runtime_error("Crew smoke turret cycle failed");
                        if(frames==1200)thirdPerson=true;
                        if(frames==1500)thirdPerson=false;
                    }
                    if(networkSmoke){input.throttle=.6f;input.yaw=.08f;input.active=true;}
                    if(scaleShips){Native::maintainScaleObserver(*game);input={};}
                    auto scaleStepStart=ScaleClock::now();
                    if(network)network->step(input,dt,frameTiming);else game->step(input,scaleShips?1./165:dt);
                    scaleStepMs=scaleMilliseconds(scaleStepStart,ScaleClock::now());
                }
                if(network && (paused || help)){Native::FlightInput neutral;neutral.active=false;network->step(neutral,dt,frameTiming);}
                if(!paused && !help && pilotControls) {
                    bool left=held(Native::Binding::LookLeft) || held(Native::Binding::LookFrontLeft) || held(Native::Binding::LookRearLeft);
                    bool right=held(Native::Binding::LookRight) || held(Native::Binding::LookFrontRight) || held(Native::Binding::LookRearRight);
                    bool front=held(Native::Binding::LookFront) || held(Native::Binding::LookFrontLeft) || held(Native::Binding::LookFrontRight);
                    bool rear=held(Native::Binding::LookRear) || held(Native::Binding::LookRearLeft) || held(Native::Binding::LookRearRight);
                    cameraInput.headYaw=(left || right || front || rear)?std::atan2(float(right)-float(left),float(front)-float(rear)):control.hatActive?control.headYaw:0;
                    if(!game->state().commandView){
                        bool out=held(Native::Binding::CameraZoomOut),in=held(Native::Binding::CameraZoomIn);
                        if(Native::cameraMode(cameraInput)==Native::CameraMode::Cockpit){
                            if(in || out)cockpitFov=glm::degrees(Allegiance::ZoomFieldOfView(glm::radians(cockpitFov),dt,!in,glm::radians(5.f),glm::radians(settings.fieldOfView)));
                        }else if(in || out)cameraInput.distance=Allegiance::ZoomCameraDistance(cameraInput.distance,dt,out,100,6000);
                    }
                }
                cameraInput.panRight=cameraInput.panDown=cameraInput.zoom=0;
                if(game->state().commandView && !commandGesture.selectionStart && (!menuVisible || investmentFlight()) && !paused && !help && !frameLimit && (SDL_GetWindowFlags(window.get())&SDL_WINDOW_INPUT_FOCUS)) {
                    float px,py;SDL_GetMouseState(&px,&py);int width,height;SDL_GetWindowSize(window.get(),&width,&height);
                    auto edge=[](float p,float extent){return p>=0 && p<8?(p-8)/12:p<=extent && p>extent-8?(p-extent+8)/12:0.f;};
                    bool mouseFocus=(SDL_GetWindowFlags(window.get())&SDL_WINDOW_MOUSE_FOCUS)!=0;
                    cameraInput.panRight=(mouseFocus?edge(px,float(width)):0.f)+float(held(Native::Binding::YawRight))-float(held(Native::Binding::YawLeft))+
                        .5f*(float(held(Native::Binding::Right))-float(held(Native::Binding::Left)));
                    cameraInput.panDown=(mouseFocus?edge(py,float(height)):0.f)+float(held(Native::Binding::PitchDown))-float(held(Native::Binding::PitchUp))+
                        .5f*(float(held(Native::Binding::Down))-float(held(Native::Binding::Up)));
                    cameraInput.zoom=(held(Native::Binding::CameraZoomOut) || held(Native::Binding::HudSmaller))?1.f:(held(Native::Binding::CameraZoomIn) || held(Native::Binding::HudLarger))?-1.f:0.f;
                    cameraInput.roll=float(held(Native::Binding::RollRight))-float(held(Native::Binding::RollLeft));
                }
                renderGame(false);
                if(investmentFlight() || frontend.closingConsole()){frontend.sync(game->state());overlay=hud.buildMenu(size,frontend.view(),menuPointer()?pointer:glm::vec2(-1),true);preview=hud.preview();}
                const auto& state=game->state();
                if(state.complete){menuVisible=true;frontend.open(Native::Screen::Results,&state);}
                else if(state.docked && !state.commandView && !state.cinematic){menuVisible=true;throttle=0;frontend.open(Native::Screen::Hangar,&state);}
            } else if(inspect) {
                angle+=dt*0.15f;
                view=glm::lookAtRH(glm::vec3(0,inspect->radius*0.7f,inspect->radius*2.5f),glm::vec3(0),glm::vec3(0,1,0));
                draws.push_back({inspectID,glm::rotate(glm::mat4(1),angle,glm::vec3(0,1,0))});
            }
            bool pointerBlocked=false;
            auto blocksPointer=[&](const Native::MenuView& menu) {
                auto layout=Native::originalMenuLayout(size,menu.classicScale,menu.canvas);auto p=layout.local(pointer);
                for(size_t i=0;i<menu.artwork.size();++i) {
                    auto b=menu.artwork[i].bounds;
                    auto shift=menu.slide && menu.slide->movesArtwork(i)?Native::menuSlideOffset(menu,size,Native::uiTime()):glm::vec2(0);b.x+=shift.x;b.y+=shift.y;
                    if(auto clip=menu.artwork[i].clip){auto lo=glm::max(glm::vec2(b),glm::vec2(*clip)+shift),hi=glm::min(glm::vec2(b)+glm::vec2(b.z,b.w),glm::vec2(*clip)+shift+glm::vec2(clip->z,clip->w));b={lo,glm::max(hi-lo,glm::vec2(0))};}
                    if(p.x>=b.x && p.y>=b.y && p.x<b.x+b.z && p.y<b.y+b.w)return true;
                }
                return false;
            };
            if(menuPointer())pointerBlocked=!investmentFlight() || blocksPointer(frontend.view());
            if(!modernUi && game && (!network || network->ready()) && !movie && !slides && !game->state().complete) {
                bool crewLoadout=menuVisible && frontend.screen()==Native::Screen::ShipLoadout;
                bool crewShown=!game->state().networkLobby && (!menuVisible || frontend.screen()==Native::Screen::Hangar || crewLoadout || consoleMap()) &&
                    (crewLoadout || game->state().docked || game->state().commandView || game->state().cinematic || game->state().camera==Native::CameraMode::Cockpit || game->state().camera==Native::CameraMode::Chase);
                const auto& crew=crewPane.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale),game->state(),crewLoadout,crewShown,settings.hudStyle);
                if(!crew.artwork.empty()){overlay=hud.buildMenu(size,crew,pointer,true);pointerBlocked=pointerBlocked || blocksPointer(crew);}
                const auto& status=consolePane.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale),game->state(),settings.chatLines,crewLoadout,!game->state().networkLobby);
                if(!status.artwork.empty())overlay=hud.buildMenu(size,status,pointer,true);
                playerPane.sync(*game);auto& players=playerPane.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                if(!pointerCapture.active() && playerLayer() && !popupCapturesPointer())playerPane.hover(size,pointer);
                if(playerLayer() && !players.artwork.empty()){overlay=hud.buildMenu(size,players,pointer,true);pointerBlocked=pointerBlocked || blocksPointer(players);}
                gameStatePane.sync(*game);auto& standings=gameStatePane.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                if(!pointerCapture.active() && playerLayer() && !popupCapturesPointer())gameStatePane.hover(size,pointer);
                if(playerLayer() && !standings.artwork.empty()){overlay=hud.buildMenu(size,standings,pointer,true);pointerBlocked=pointerBlocked || blocksPointer(standings);}
                auto& console=chatConsole.view(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                if(!console.artwork.empty()){overlay=hud.buildMenu(size,console,pointer,true);pointerBlocked=pointerBlocked || blocksPointer(console);}
            }
            if(!modernUi && !movie && frontend.helpVisible()){
                const auto& pane=frontend.helpView(Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                auto helpPointer=capture || (popupCapturesPointer() && !frontend.helpPointerCaptured())?glm::vec2(-1):visiblePointer;
                if(helpBlocksPointer || frontend.helpPointerCaptured())frontend.helpHover(Native::menuHit(pane,size,settings.uiScale,helpPointer));
                overlay=hud.buildMenu(size,pane,helpPointer,true);pointerBlocked=pointerBlocked || helpBlocksPointer;
            }
            pointer=visiblePointer;
            if(!modernUi && menuVisible && !frontend.menuPopupView().artwork.empty())overlay=hud.buildMenu(size,frontend.menuPopupView(),pointer,true);
            if(detailsPopup())overlay=hud.buildMenu(size,frontend.detailsView(),pointer,true);
            if(!modernUi && (menuVisible || trainingStartAt>=0) && frontend.hasMessageBox())overlay=hud.buildMenu(size,frontend.messageView(),pointer,true);
            flushUiSounds();
            if(originalCursor && !movie && (!slides || menuVisible || frontend.helpVisible()) && !capture) {
                std::string_view name="default";const Native::Contact* hovered=nullptr;glm::vec4 color(1);
                if((menuVisible || trainingStartAt>=0) && frontend.waitCursor())name="wait";
                else if(menuPointer() && frontend.previewCursor(Native::originalMenuLayout(size,frontend.view().classicScale,frontend.view().canvas).local(pointer)))name="drag";
                else if(!pointerBlocked && game && (!network || network->ready()) && !game->state().complete) {
                    auto& state=game->state();bool composing=chatConsole.mode()==Allegiance::ComposeState::Command;
                    auto mapCursor=hud.mapCursor(pointer,!state.ripcord && !(consoleFlags()&ofTeleportPane) && (composing || !state.docked));
                    if(mapCursor)name=*mapCursor;
                    else if(commandProjectionValid && (!state.docked || state.commandView)) {
                        int individual=chatConsole.audience().channel==Native::ChatChannel::Target?chatConsole.audience().recipient:-1;
                        auto cursorFor=[&](const Native::Contact& c){return game->commandCursor(c.type,c.id,chatConsole.subjects(),composing,chatConsole.currentVerb(),individual);};
                        hovered=Native::commandHover(game->contacts(),pointer,size,commandProjection,[&](auto& c){return !cursorFor(c).empty();});
                        if(hovered){name=cursorFor(*hovered);if(hovered->side>=0 && hovered->side<int(state.sideColors.size()))color=state.sideColors[hovered->side];}
                    }
                }
                int scale=Native::originalInterfaceScale(size,Native::classicInterfaceScale(settings.classicUiScale,currentDisplayScale));
                bool hardware=originalCursor->set(name,scale,SDL_GetWindowPixelDensity(window.get()));
                if(hardware && !SDL_CursorVisible())SDL_ShowCursor();else if(!hardware && SDL_CursorVisible())SDL_HideCursor();
                if(hovered || !hardware)overlay=hud.appendPointer(size,pointer,hovered,hardware?nullptr:&originalCursor->artwork(name),color);
            }
            controller.feedback(game?game->takeFeedback():Native::Feedback{},game && !movie && !slides && (!menuVisible || investmentFlight()) && !paused && !help && (SDL_GetWindowFlags(window.get())&SDL_WINDOW_INPUT_FOCUS));
            auto synchronizeFrameAudio=[&] {
                if(soundGame && soundGame==game.get()) {
                    audio.paused(soundPaused);
                    if(soundListener) {
                        audio.listener(soundCamera.position,game->state().velocity,soundCamera.forward,soundCamera.up);
                        audio.synchronize(game->worldSounds(soundCamera.mode==Native::CameraMode::Cockpit));
                    } else if(clearWorldSounds)audio.synchronize({});
                    for(auto& sound:game->sounds())audio.play(sound);
                    game->clearSounds();
                } else if(clearWorldSounds) {
                    audio.synchronize({});audio.paused(soundPaused);
                }
                soundGame=nullptr;clearWorldSounds=false;
            };
            if(game && game->state().complete && game->state().trainingMission && !debriefShown && !slides) {
                // Debrief narration establishes its own paused world mix, and
                // leaving can retire the game. Finish its last sound update
                // first, preserving the original transition order.
                synchronizeFrameAudio();
                debriefShown=true;
                if(game->state().trainingPostSlideshow){Native::SessionOptions options;options.trainingMission=game->state().trainingMission;startSlides(options,true);}
                else frontAction({Native::FrontEffect::Leave});
            }
            renderer.flushUploads();
            auto particles=game && (!menuVisible || frontend.view().overlay) && !movie && !slides?game->particles():std::span<const Native::ParticleDraw>{};
            if(surfaceSmoke) {
                for(auto& d:draws)if(d.blend==Native::ModelBlend::Additive && !d.aleph)surfaceMask|=2;
                for(auto& p:particles) {
                    if(!(p.texture&Native::hudAdditive))surfaceMask|=1;
                    uint32_t id=p.texture&0x0fffffffu;
                    if(id==effectTextures.at("f103bmp").id)surfaceMask|=4;
                    if(id==effectTextures.at("fx18bmp").id)surfaceMask|=8;
                }
                if(surfaceMask==15)frameLimit=frames+1;
            }
            const std::string& capturePath=!requestedScreenshot.empty()?requestedScreenshot:frames+1==frameLimit?screenshot:requestedScreenshot;
            auto scaleRenderStart=ScaleClock::now();
            renderer.draw(projection*view,draws,overlay,capturePath,preview,particles,lighting,background);
            auto scaleRenderEnd=ScaleClock::now();
            // Spatial sound and mixer maintenance consume this frame's state,
            // but are not prerequisites for presenting it. Submit first so they
            // overlap GPU rendering instead of extending the input-to-present
            // budget. No state is advanced or queued for a later audio frame.
            synchronizeFrameAudio();
            audio.tick(dt);
            if(scaleOutput){auto stats=renderer.stats();scaleOutput<<frames<<','<<scaleStepMs<<','<<scaleMilliseconds(scalePrepareStart,scaleRenderStart)-scaleStepMs<<','<<scaleMilliseconds(scaleRenderStart,scaleRenderEnd)<<','<<(game?Native::diagnosticShipCount(*game):0)<<','<<(game?game->contacts().size():0)<<','<<draws.size()<<','<<particles.size()<<','<<stats.draws<<','<<stats.triangles<<','<<stats.missed<<','<<commandCamera.position.x<<','<<commandCamera.position.y<<','<<commandCamera.position.z<<','<<commandCamera.fieldOfView<<','<<frameTiming.sampledNs<<','<<frameTiming.targetPresentNs<<','<<(network?network->metrics().presentationLead*1000:0)<<','<<(network?network->metrics().localPresentationFraction*1000:0)<<','<<(network?network->metrics().remotePresentationTime:0)<<'\n';}
            if(scaleShips && frames%100==99){auto stats=renderer.stats();fprintf(stderr,"Scale frame %u: step %.3f ms, preparation %.3f ms, %zu draw items, %zu particles, %u draws, %u triangles, %llu misses\n",frames+1,scaleStepMs,scaleMilliseconds(scalePrepareStart,scaleRenderStart)-scaleStepMs,draws.size(),particles.size(),stats.draws,stats.triangles,(unsigned long long)stats.missed);}
            if(!requestedScreenshot.empty()){fprintf(stderr,"Screenshot saved to %s\n",requestedScreenshot.c_str());requestedScreenshot.clear();}
            ++frames;
        }
        if(networkSmoke && (!network || !network->ready() || !networkLaunched || game->state().docked || menuVisible))throw std::runtime_error("Multiplayer display smoke did not join and launch");
        if(gamesSmoke && gamesStage!=11)throw std::runtime_error("Classic game-entry diagnostic did not complete stage "+std::to_string(gamesStage));
        if(refitSmoke && refitStage!=15)throw std::runtime_error("Classic remote-refit diagnostic did not complete stage "+std::to_string(refitStage));
        if(network){auto& m=network->metrics();fprintf(stderr,"Network snapshots %llu RTT %.1f ms correction max %.3f m received %.1f KiB missing baselines %llu\n",(unsigned long long)m.snapshots,m.rtt,m.correctionMax,m.bytesReceived/1024.,(unsigned long long)m.missingBaselines);}
                if(cargoSmoke && cargoStage!=3)throw std::runtime_error("Cargo smoke did not complete");
        if(surfaceSmoke && surfaceMask!=15)throw std::runtime_error("Surface smoke did not exercise every effect: "+std::to_string(surfaceMask));
        if(dockSmoke && dockingStage!=4)throw std::runtime_error("Docking smoke did not finish its launch/dock/refit/relaunch cycle");
        if(trainingTerminalSmoke && trainingTerminalStage!=7)throw std::runtime_error("Training terminal smoke did not finish its debrief and next-lesson cycle");
        if(resultsUiSmoke && !resultsUiSmokePassed)throw std::runtime_error("Results UI smoke did not finish");
        if(lobbyChatSmoke && !lobbyChatSmokePassed)throw std::runtime_error("Lobby chat UI smoke did not finish");
        if(lobbySmoke && !lobbySmokePassed)throw std::runtime_error("Local lobby UI smoke did not finish");
        if(deviceSmoke && !deviceSmokePassed)throw std::runtime_error("Original device UI smoke did not finish");
        if(trainingEntrySmoke && !trainingEntrySmokePassed)throw std::runtime_error("Original training entry UI smoke did not finish");
        if(trainingSlidesMenuSmoke && !trainingSlidesMenuSmokePassed)throw std::runtime_error("Original slideshow menu smoke did not finish its UI-only checks");
        if(pingsSmoke && !pingsSmokePassed)throw std::runtime_error("Player Pings SDL smoke did not finish");
        if(customMapSmoke && !customMapSmokePassed)throw std::runtime_error("Original custom-map SDL smoke did not finish");
        if(crewUiSmoke && !crewUiSmokePassed)throw std::runtime_error("Crew UI smoke did not finish its docked purchase and camera checks");
        if(dockViewSmoke && !dockViewSmokePassed)throw std::runtime_error("Dock-view smoke did not finish its base-loss and observer transitions");
        if(findPlayerSmoke && !findPlayerSmokePassed)throw std::runtime_error("Find Player SDL smoke did not complete; it requires a connected LookupFixture pilot on a known server");
        if(lanSmoke && !lanSmokePassed)throw std::runtime_error("LAN SDL smoke did not discover and join the remote lobby");
        if(gameStateSmoke && !gameStateSmokePassed)throw std::runtime_error("Game-state smoke did not finish its hangar and flight checks");
        if(playerPaneSmoke && !playerPaneSmokePassed)throw std::runtime_error("Player-pane smoke did not finish its hangar and flight checks");
        if(classicKeyboardSmoke && !classicKeyboardSmokePassed)throw std::runtime_error("Classic keyboard smoke did not finish its UI-only checks");
        if(inputRoutingSmoke && !inputRoutingCheck.passed())throw std::runtime_error("Classic SDL input routing smoke did not finish");
        auto stats=renderer.stats();fprintf(stderr,"Rendered %u frames, display refresh %.3f Hz, last sample-to-present %.3f ms, %llu misses, %u triangles\n",frames,stats.refreshHz,stats.latencyMs,(unsigned long long)stats.missed,stats.triangles);
        return 0;
    } catch(const std::exception& e) {fprintf(stderr,"Allegiance: %s\n",e.what());return 1;}
}
