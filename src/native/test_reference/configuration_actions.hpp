#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"
#include <cmath>
#include <fstream>
#include <limits>

namespace NativeTest {
inline void checkOriginalDeviceActions(Native::Assets& assets){
    using namespace Native;
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);int queries=0;
    ui.graphicsDeviceService([&]{++queries;return std::vector<std::string>{"Device: Test GPU","Vulkan 1.3","Resolution: 1600 x 1200","Rendering: borderless window","32 bits per pixel / sRGB / gamma 1.13"};});
    require(queries==0,"device query runs before its menu is opened");
    ui.open(Screen::Settings);ui.key(SDLK_D,SDL_SCANCODE_D);
    require(queries>0 && std::any_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.label=="Device: Test GPU";}),"original Graphics Device menu retains placeholder state");
    auto parent=ui.screen();float initial=settings.gamma;
    auto result=ui.key(SDLK_U,SDL_SCANCODE_U);
    require(result.effect==FrontEffect::ApplyOptions && result.value==16 && std::abs(settings.gamma-initial*1.01f)<1e-6 && ui.screen()==parent,"Brightness Up does not use original multiplier or applies unrelated settings");
    result=ui.key(SDLK_D,SDL_SCANCODE_D);require(result.value==16 && std::abs(settings.gamma-initial)<1e-6,"Brightness Down does not invert original multiplier");
    settings.gamma=2;ui.key(SDLK_U,SDL_SCANCODE_U);require(settings.gamma==2,"brightness exceeds original maximum");
    settings.gamma=1;ui.key(SDLK_D,SDL_SCANCODE_D);require(settings.gamma==1,"brightness falls below original minimum");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_G,SDL_SCANCODE_G);ui.key(SDLK_E,SDL_SCANCODE_E);
    result=ui.key(SDLK_C,SDL_SCANCODE_C);
    require(result.effect==FrontEffect::ApplyOptions && result.value==1 && settings.controller.autoCenter,"original Force Feedback Auto Center accelerator does not apply controller state");
    require(std::any_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.label=="Auto Center On";}),"Auto Center toggle fails to refresh its original label");
    settings.controller.feedback=false;ui.key(SDLK_C,SDL_SCANCODE_C);require(!settings.controller.autoCenter && !settings.controller.feedback,"Auto Center changes impact/fire feedback preference");
    ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);ui.key(SDLK_O,SDL_SCANCODE_O);ui.key(SDLK_A,SDL_SCANCODE_A);
    ui.graphicsSamples({0,4,8});
    for(int expected:{4,8,0}){result=ui.key(SDLK_A,SDL_SCANCODE_A);require(result.effect==FrontEffect::ApplyOptions && result.value==32 && settings.antialiasing==expected,"original AA accelerator ignores device-supported cycle or changes unrelated settings");}
    for(int level:{1,2,3,0}){result=ui.key(SDLK_X,SDL_SCANCODE_X);require(result.effect==FrontEffect::ApplyOptions && result.value==64 && settings.maxTextureSize==level,"original maximum-texture accelerator does not cycle 256/512/1024/2048");}
    require(settings.gamma==1 && !settings.controller.autoCenter && !settings.controller.feedback,"graphics quality modifies brightness or controller state");
    settings.gamma=std::numeric_limits<float>::quiet_NaN();settings.normalize();require(settings.gamma==1.13f,"invalid gamma does not restore original startup default");
    auto file=std::filesystem::temp_directory_path()/"allegiance-device-options-check.conf";
    settings.gamma=1.7f;settings.controller.autoCenter=true;settings.antialiasing=8;settings.maxTextureSize=2;settings.save(file);auto saved=Settings::load(file);std::filesystem::remove(file);
    require(saved.gamma==settings.gamma && saved.controller.autoCenter && !saved.controller.feedback && saved.antialiasing==8 && saved.maxTextureSize==2,"graphics/auto-centering do not persist independently");
    file=std::filesystem::temp_directory_path()/"allegiance-device-options-check.json";
    {std::ofstream out(file);out<<R"({"Gamma":"1.45","FFAutoCenter":1})";}
    require(saved.importOriginal(file)==2 && saved.gamma==1.45f && saved.controller.autoCenter,"original Gamma/FFAutoCenter preferences are not imported");std::filesystem::remove(file);
    {std::ofstream out(file);out<<R"({"UseAntialiasing":true,"MaxTextureSize":1,"Graphics.UseAntialiasing":8,"Graphics.MaxTextureSizeLevel":3})";}
    require(saved.importOriginal(file)==4 && saved.antialiasing==8 && saved.maxTextureSize==3,"original quality preferences do not import bool/numeric values with Graphics override precedence");std::filesystem::remove(file);
    saved.antialiasing=3;saved.maxTextureSize=8;saved.normalize();require(saved.antialiasing==0 && saved.maxTextureSize==3,"invalid quality preferences are not normalized");
    puts("Original device actions: brightness/clamps, actual device rows, Auto Center, device-supported AA/original texture cycles, independent persistence and original preference import passed");
}
}
