#pragma once
#include "../frontend.hpp"
#include "../test_fixture.hpp"

namespace NativeTest {
// Independent HullMenu/GetNextHullType policy from WinTrek/load.cpp. The
// current argument is the temporary editor ship, not the owned trade-in.
inline std::vector<int> originalHullMenu(ImissionIGC* core,IstationIGC* station,IhullTypeIGC* current){
    std::vector<int> ids;
    for(auto* link=core->GetHullTypes()->first();link;link=link->next()){
        auto* hull=link->data();
        if(hull==current || (hull->GetGroupID()>=0 && station->CanBuy(hull) && !station->IsObsolete(hull)))ids.push_back(hull->GetObjectID());
    }
    return ids;
}
inline int originalNextHull(ImissionIGC* core,IstationIGC* station,IhullTypeIGC* current,bool forward){
    auto* link=core->GetHullTypes()->find(current);require(link,"original current editor hull is absent");
    for(;;){
        link=forward?link->next():link->txen();
        if(!link)link=forward?core->GetHullTypes()->first():core->GetHullTypes()->last();
        auto* hull=link->data();
        if(hull==current || (hull->GetGroupID()>=0 && station->CanBuy(hull) && !station->IsObsolete(hull)))return hull->GetObjectID();
    }
}
inline void checkLoadoutEditor(Native::Assets& installed){
    using namespace Native;
    auto cores=installed.coreCatalog();if(std::find(cores.begin(),cores.end(),"static_core")==cores.end())cores.insert(cores.begin(),"static_core");
    int menus=0,cycles=0,unavailable=0;
    for(const auto& core:cores){
        SessionOptions options;options.scenario=Scenario::Conquest;options.coreFile=core;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
        Game game(installed.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
        Settings settings;Frontend ui(settings);ui.originalInterface(installed);ui.setPlaying(true);
        int previews=0,remembers=0,cacheReads=0;
        ui.loadoutServices([&](const Loadout& fitting,bool defaults,int target){++previews;return game.previewLoadout(fitting,defaults,target);},
            [&](PartKind kind,int mount,int hull){return game.loadoutPartChoices(kind,mount,hull);},
            [&](int,int)->std::optional<Loadout>{++cacheReads;return {};});
        ui.loadoutMemory([&](const Loadout&){++remembers;});
        auto action=[&](MenuOp op,int value=0,int delta=1){
            auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.action.op==op && item.action.value==value && item.action.delta==delta;});
            require(i!=ui.view().items.end(),"loadout editor control missing");return size_t(i-ui.view().items.begin());
        };
        auto owned=game.currentLoadout();auto money=game.state().money;auto random=game.simulationRandomState();auto count=game.core()->GetShips()->n();
        auto* station=game.playerShip()->GetStation();ui.open(Screen::ShipLoadout,&game.state());
        for(auto* link=game.core()->GetHullTypes()->first();link;link=link->next()){
            auto* hull=link->data();if(hull->GetGroupID()<0 || hull->HasCapability(c_habmLifepod))continue;
            Loadout fitting{hull->GetObjectID(),{}};ui.stageLoadout(fitting);
            require(ui.stagedLoadout().valid && ui.stagedLoadout().loadout==fitting,"explicit editor fitting was refilled or rejected");
            std::vector<int> actual;for(const auto& row:ui.stagedLoadout().hulls)actual.push_back(row.id);
            require(actual==originalHullMenu(game.core(),station,hull),"hull menu differs from original staged-current/stock/order policy");++menus;
            if(!station->CanBuy(hull) || station->IsObsolete(hull)){
                ++unavailable;require(!ui.stagedLoadout().available,"unavailable hull fixture is purchasable");
                require(ui.view().items[action(MenuOp::PreviewDefault)].enabled,"unavailable editor hull disables its original Default button");
                for(const auto& item:ui.view().items)if(item.action.op==MenuOp::PreviewSlot)require(item.enabled,"unavailable editor hull disables original equipment editing");
            }
            auto reads=previews,saves=remembers,cached=cacheReads;ui.clearSounds();ui.pointerMove({400,350});ui.activate(action(MenuOp::LoadoutPopup,1));
            ui.activate(action(MenuOp::PreviewHull,hull->GetObjectID()));
            require(previews==reads && remembers==saves && cacheReads==cached && ui.stagedLoadout().loadout==fitting,"selecting current hull refills equipment or changes editor cache");
            require(std::none_of(ui.view().items.begin(),ui.view().items.end(),[](const auto& item){return item.popupLevel>=0;}) && std::find(ui.sounds().begin(),ui.sounds().end(),mountSound)==ui.sounds().end() && std::find(ui.sounds().begin(),ui.sounds().end(),changeLoadoutSound)==ui.sounds().end(),"current hull selection leaves its menu open or announces a hull change");
            for(bool forward:{false,true}){
                ui.stageLoadout(fitting);int expected=originalNextHull(game.core(),station,hull,forward);
                auto index=action(MenuOp::PreviewHull,-1,forward?1:-1);
                require(ui.view().items[index].enabled==(expected!=hull->GetObjectID()),"next/previous enabled state differs from original traversal");
                ui.clearSounds();ui.activate(index);require(ui.stagedLoadout().loadout.hull==expected,"next/previous skips the staged anchor or visits an unavailable owned hull");
                require(ui.sounds().size()==(expected!=hull->GetObjectID()?2u:0u) && (expected==hull->GetObjectID() || (ui.sounds()[0]==changeLoadoutSound && ui.sounds()[1]==mountSound)),"hull cycling loses or duplicates authored click/original BuyHull mount cues");++cycles;
            }
        }
        require(game.currentLoadout()==owned && game.state().money==money && game.simulationRandomState()==random && game.core()->GetShips()->n()==count && !game.core()->GetShip(-2),"editor browsing mutates owned ship, economy, random state or mission membership");
    }
    require(unavailable>0,"editor stock-loss matrix contains no unavailable staged hulls");
    printf("Original loadout editor: %d core menus, %d next/previous traversals, %d unavailable staged hulls and no-op current-hull selection passed\n",menus,cycles,unavailable);

    // Alter only an isolated asset cache. No renderer or bitmap decoder uses
    // these synthetic card dimensions: this checks Slot's fixed surface/input.
    Assets assets(installed.root());OriginalUi authored(assets);
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    LoadoutPreview preview;std::vector<OriginalLoadoutSlot> slots;
    for(auto* link=game.core()->GetHullTypes()->first();link;link=link->next()){
        auto* hull=link->data();if(hull->GetGroupID()<0 || hull->GetMaxWeapons()==hull->GetMaxFixedWeapons())continue;
        auto candidate=authored.loadoutSlots(hull->GetObjectID());
        if(std::none_of(candidate.begin(),candidate.end(),[](const auto& slot){return slot.type==-2;}))continue;
        preview=game.previewLoadout({hull->GetObjectID(),{}});slots=std::move(candidate);if(preview.valid)break;
    }
    require(preview.valid && !slots.empty(),"original turret slot fixture missing");
    for(auto& slot:slots)if(slot.empty){slot.empty->width=167;slot.empty->height=49;}
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);
    ui.loadoutServices([&](const Loadout&,bool,int){return preview;},[](PartKind,int,int){return std::vector<PartChoice>{{-1,"-Make Slot Empty-",0}};});
    ui.open(Screen::ShipLoadout,&game.state());int checked=0;
    for(const auto& slot:slots){
        int mount=slot.type==-1?slot.mount-5:slot.type==-2?slot.mount+preview.fixedWeapons:slot.mount;
        int type=slot.type==-2?1:slot.type;
        if((slot.type==1 && mount>=preview.fixedWeapons) || (slot.type==-2 && mount>=preview.weapons))continue;
        auto part=std::find_if(preview.slots.begin(),preview.slots.end(),[&](const auto& p){return p.mount==mount && (mount<0 || int(p.kind)==type);});if(part==preview.slots.end())continue;
        int index=int(part-preview.slots.begin());auto name="loadoutSlot"+std::to_string(index);glm::vec4 surface{slot.position,133,37};
        const auto& card=authoredControl(ui.view().artwork,name);
        require(card.bounds==glm::vec4(slot.position,167,49) && card.clip==surface,"alternate slot card was rescaled or escaped its original surface");
        auto hit=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.control==name;});
        require(hit!=ui.view().items.end() && hit->bounds==surface && hit->enabled,"slot hit area depends on card image size or hull stock");++checked;
        if(slot.type==-2)require(std::none_of(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Text && glm::vec2(c.bounds)==slot.position+glm::vec2(3,24);}),"empty turret paints a gunner/open-seat label");
    }
    auto turret=std::find_if(preview.slots.begin(),preview.slots.end(),[&](const auto& slot){return slot.mount>=preview.fixedWeapons && slot.kind==PartKind::Weapon;});require(turret!=preview.slots.end(),"mounted turret painter fixture missing");
    turret->id=game.playerShip()->GetMountedPart(ET_Weapon,0)->GetPartType()->GetObjectID();turret->model=game.playerShip()->GetMountedPart(ET_Weapon,0)->GetPartType()->GetModelName();
    for(const std::string gunner:{std::string{},std::string(80,'W')}){
        turret->gunner=gunner;ui.stageLoadout(preview.loadout);
        auto label=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[&](const auto& c){return c.kind==OriginalControl::Kind::Text && c.text==(gunner.empty()?"<open>":gunner);});
        require(label!=ui.view().artwork.end() && label->clip && label->clip->z==133 && label->clip->w==37 && label->font=="smallFont" && label->singleLine && !label->clipCharacters,"mounted turret loses original raw single-line label/font/surface clipping");
    }
    auto cargo=std::find_if(preview.slots.begin(),preview.slots.end(),[](const auto& slot){return slot.mount<0;});require(cargo!=preview.slots.end(),"cargo quantity painter fixture missing");
    cargo->id=game.core()->GetAmmoPack()->GetObjectID();cargo->kind=PartKind::Pack;cargo->model=game.core()->GetAmmoPack()->GetModelName();cargo->amount=12345;ui.stageLoadout(preview.loadout);
    auto countLabel=std::find_if(ui.view().artwork.begin(),ui.view().artwork.end(),[](const auto& c){return c.kind==OriginalControl::Kind::Text && c.text=="12345";});
    require(countLabel!=ui.view().artwork.end() && countLabel->justify==2 && countLabel->bounds.z==127 && countLabel->singleLine && !countLabel->clipCharacters && countLabel->clip && countLabel->clip->z==133 && countLabel->clip->w==37,"cargo quantity loses original right-aligned raw text or slot clipping");
    printf("Original loadout Slot: %d alternate-size cards, fixed 133x37 surfaces/hits, empty/mounted/open/long-name turret labels and right-aligned quantities passed\n",checked);
}
inline void checkLoadoutPresets(Native::Assets& assets){
    using namespace Native;
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(assets.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);
    Settings settings;Frontend ui(settings);ui.originalInterface(assets);ui.setPlaying(true);
    Loadouts presets;int remembered=0;
    ui.loadoutServices([&](const Loadout& fitting,bool defaults,int target){return game.previewLoadout(fitting,defaults,target);},
        [&](PartKind kind,int mount,int hull){return game.loadoutPartChoices(kind,mount,hull);},
        [&](int bank,int hull)->std::optional<Loadout>{if(auto* fitting=presets.findExact(bank,hull))return *fitting;return {};});
    ui.loadoutMemory([&](const Loadout&){++remembered;});ui.open(Screen::ShipLoadout,&game.state());
    auto item=[&](MenuOp op,int value=0){auto i=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& c){return c.action.op==op && c.action.value==value;});require(i!=ui.view().items.end(),"preset/feedback control missing");return size_t(i-ui.view().items.begin());};
    auto equal=[](Loadout a,Loadout b){auto sort=[](Loadout& l){std::sort(l.parts.begin(),l.parts.end(),[](auto& a,auto& b){return std::tie(a.mount,a.id,a.amount)<std::tie(b.mount,b.id,b.amount);});};sort(a);sort(b);return a==b;};
    auto live=game.currentLoadout();auto wallet=game.state().money;auto random=game.simulationRandomState();auto count=game.core()->GetShips()->n();
    auto* station=game.playerShip()->GetStation();auto* base=game.playerShip()->GetBaseHullType();
    std::vector<char> bytes(base->Export(nullptr));base->Export(bytes.data());auto* data=reinterpret_cast<DataHullTypeIGC*>(bytes.data());
    auto make=[&](HullID id,HullID successor,bool buyable){require(!game.core()->GetHullType(id),"preset fixture ID already exists");data->hullID=id;data->successorHullID=successor;if(buyable)data->ttbmRequired.ClearAll();else data->ttbmRequired.SetAll();auto* hull=static_cast<IhullTypeIGC*>(game.core()->CreateObject(game.core()->GetLastUpdate(),OT_hullType,bytes.data(),int(bytes.size())));require(hull,"preset stock/successor fixture creation");hull->Release();return hull;};
    int alternate=make(31063,NA,true)->GetObjectID(),cases=0;
    // GetLoadoutLink matches the chosen hull exactly. A foreign-only bank
    // falls through to BuyDefaultLoadout; a matching entry need not be first.
    for(int hull:{int(live.hull),alternate})for(int bank=0;bank<int(Loadouts::count);++bank)for(int mode=0;mode<4;++mode){
        presets={};int foreign=hull==live.hull?alternate:live.hull;
        for(int i=0;i<int(Loadouts::count);++i)if(i!=bank)presets.remember(i,{int16_t(hull),{}});
        if(mode>=2)presets.remember(bank,{int16_t(hull),{}});
        if(mode>=1)presets.remember(bank,{int16_t(foreign),{}});
        if(mode==3)presets.remember(bank,{int16_t(hull),{}});
        require(ui.stageLoadout({int16_t(hull),{}}),"preset anchor missing");ui.clearSounds();
        auto request=ui.key(SDL_Keycode(SDLK_0+bank),SDL_Scancode(SDL_SCANCODE_0),false,0);
        require(request.effect==FrontEffect::UseLoadout && request.value==bank && ui.stageLoadoutPreset(request.value),"original digit does not restore its selected bank");
        auto expected=mode>=2?Loadout{int16_t(hull),{}}:game.previewLoadout({int16_t(hull),{}},true).loadout;
        require(equal(ui.stagedLoadout().loadout,expected),"digit preset fits another hull's equipment or ignores a non-first exact match");
        require(ui.sounds().empty() && remembered==0 && !ui.hasMessageBox(),"preset staging buys/remembers equipment or duplicates main's original feedback");++cases;
    }
    auto before=ui.stagedLoadout().loadout;require(!ui.stageLoadoutPreset(-1) && !ui.stageLoadoutPreset(10) && ui.stagedLoadout().loadout==before,"invalid bank changes editor fitting");
    ui.clearSounds();ui.activate(item(MenuOp::PreviewDefault));require(ui.sounds().size()==1 && ui.sounds()[0]==changeLoadoutSound,"Default duplicates its authored change-loadout sound");
    ui.activate(item(MenuOp::LoadoutPopup,1));ui.clearSounds();ui.activate(item(MenuOp::PreviewHull,live.hull));
    require(ui.sounds().size()==1 && ui.sounds()[0]==mountSound,"hull menu omits original BuyHull mount sound or adds a cycling cue");

    auto* upgrade=make(31060,NA,true);auto* obsolete=make(31061,31060,true);auto* unavailable=make(31062,NA,false);
    require(station->CanBuy(obsolete) && station->GetSuccessor(obsolete)==upgrade && !station->CanBuy(unavailable),"preset fixture must exercise a successor and stock withdrawal");
    presets={};presets.remember(4,{obsolete->GetObjectID(),{}});ui.stageLoadout({obsolete->GetObjectID(),{}});
    require(ui.stageLoadoutPreset(4) && ui.stagedLoadout().loadout.hull==upgrade->GetObjectID(),"saved obsolete hull does not upgrade through original station policy");
    auto expectedUpgrade=game.previewLoadout({upgrade->GetObjectID(),{}},true).loadout;std::erase_if(expectedUpgrade.parts,[](const auto& p){return p.mount<0;});
    require(equal(ui.stagedLoadout().loadout,expectedUpgrade),"upgraded preset loses preferred weapons or invents default cargo");
    IhullTypeIGC* first=nullptr;for(auto* link=game.core()->GetHullTypes()->first();link;link=link->next()){auto* h=link->data();if(h->GetGroupID()>=0 && station->CanBuy(h) && h->GetPrice()<=INT_MAX/4){first=static_cast<IhullTypeIGC*>(station->GetSuccessor(h));break;}}
    require(first,"original first available default missing");presets={};presets.remember(4,{unavailable->GetObjectID(),{}});ui.stageLoadout({unavailable->GetObjectID(),{}});
    require(ui.stageLoadoutPreset(4) && equal(ui.stagedLoadout().loadout,game.previewLoadout({first->GetObjectID(),{}},true).loadout),"withdrawn saved hull fails original first-buyable/default-equipment fallback");
    presets={};ui.stageLoadout({unavailable->GetObjectID(),{}});
    require(ui.stageLoadoutPreset(4) && ui.stagedLoadout().loadout.hull==unavailable->GetObjectID(),"missing cache changes original Default's selected hull");
    require(game.currentLoadout()==live && game.state().money==wallet && game.simulationRandomState()==random && game.core()->GetShips()->n()==count && !game.core()->GetShip(-2),"preset/default/hull browsing mutates the live ship, credits, RNG or mission membership");
    printf("Original loadout presets: %d bank/hull/missing/foreign/exact-order cases, station successor/stock fallback, native Default and hull-menu cue counts passed\n",cases);
}
}
