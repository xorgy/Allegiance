#include "game.hpp"
#include "test_fixture.hpp"
#include "../ClientCore/Loadout.h"
#include "test_reference/loadout.hpp"
#include <fstream>
#include <tuple>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
using NativeTest::require;
namespace {
void presetFeedback(const std::filesystem::path& artwork) {
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.startInHangar=true;options.rules.miners=0;
    Native::Game game(artwork,[](auto&,auto&){return Native::ModelHandle{1,1};},0,0,options);
    auto fitting=game.currentLoadout();auto money=game.state().money;auto name=game.core()->GetHullType(fitting.hull)->GetName();
    for(bool saved:{true,false})for(int bank:{0,9}) {
        game.clearMessages();game.clearSounds();game.loadoutPresetFeedback(fitting,bank,saved);
        auto text=std::string(saved?"Saved ":"Loaded ")+name+" loadout "+std::to_string(bank);
        require(game.state().messages.size()==1 && game.state().messages.front().tip && !game.state().messages.front().warning && game.state().messages.front().text==text,"loadout preset loses original hull/bank console feedback");
        for(int sound:{newNonCriticalMsgSound,saved?positiveButtonClickSound:mountSound})require(std::count_if(game.sounds().begin(),game.sounds().end(),[&](const auto& s){return s.id==sound && s.relative;})==1,"loadout preset loses or duplicates its original sound");
        require(game.currentLoadout()==fitting && game.state().money==money && game.state().docked,"preset feedback mutates the owned fitting or balance");
    }
    game.clearMessages();game.clearSounds();game.loadoutPresetFeedback(fitting,-1,true);game.loadoutPresetFeedback({-1,{}},0,false);
    require(game.state().messages.empty() && game.sounds().empty(),"invalid preset announces a successful action");
    puts("Original saved/loaded preset hull names, banks, console tips and sounds passed");
}
std::vector<uint8_t> bytes(const std::filesystem::path& file) {
    std::ifstream in(file,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};
}
void write(const std::filesystem::path& file,const std::vector<uint8_t>& data) {
    std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());
}
void storage(const std::filesystem::path& file) {
    Native::Loadouts saved;
    saved.remember(0,{414,{{11,0},{12,0},{13,-1},{14,-5}}});
    saved.remember(0,{412,{{9,0}}});saved.remember(9,{310,{{7,-3}}});
    require(saved.find(0,414)->parts==std::vector<Native::LoadoutPart>{{14,-5},{13,-1},{12,0},{11,0}},"original insertion-before-equal mount order");
    require(saved.find(0,100)->hull==412 && saved.find(1,414)==nullptr,"bank current-hull selection and most recent fallback");
    require(saved.findExact(0,414)->hull==414 && saved.findExact(0,412)->hull==412 && !saved.findExact(0,100) && !saved.findExact(1,414) && !saved.findExact(10,414),"original editor lookup uses another hull, an empty bank or an invalid bank");
    saved.save(file);auto copy=Native::Loadouts::load(file);require(copy.banks==saved.banks,"loadout storage round trip");
    std::vector<uint8_t> expected{10,0,0,0,2,0,0,0,0x9c,1,1,0,0,0,9,0,0,0x9e,1,4,0,0,0,14,0,251,13,0,255,12,0,0,11,0,0};
    expected.insert(expected.end(),8*4,0);
    const uint8_t end[]={1,0,0,0,54,1,1,0,0,0,7,0,253};expected.insert(expected.end(),std::begin(end),std::end(end));
    require(bytes(file)==expected,"Windows int32/int16/int8 wire format and order");
    for(size_t n=0;n<expected.size();++n) {
        write(file,{expected.begin(),expected.begin()+n});bool rejected=false;
        try{Native::Loadouts::load(file);}catch(const std::exception&){rejected=true;}
        require(rejected,"truncated loadout accepted");
    }
    for(auto [offset,value]:std::vector<std::pair<size_t,uint8_t>>{{0,11},{7,127},{16,127},{19,250}}) {
        auto bad=expected;bad[offset]=value;write(file,bad);bool rejected=false;
        try{Native::Loadouts::load(file);}catch(const std::exception&){rejected=true;}require(rejected,"malformed loadout accepted");
    }
    saved.remember(0,{414,{{5,0}}});require(saved.banks[0].size()==2 && saved.banks[0].front().hull==414,"saving same hull replaces and moves it to front");
    saved.remember(1,{414,{{5,-5,2}}});require(saved.find(1,414)->parts.front().amount==32767,"preset storage retains original refill intent instead of current ammunition count");
    saved.save(file);require(Native::Loadouts::load(file).banks==saved.banks,"replace after malformed file");
    std::filesystem::remove(file);
}
auto inventory(IshipIGC* ship) {
    std::vector<std::tuple<int,int,int>> result;
    for(auto* l=ship->GetParts()->first();l;l=l->next())result.emplace_back(l->data()->GetPartType()->GetObjectID(),l->data()->GetMountID(),l->data()->GetAmount());
    return result; // Preserve IGC insertion order as well as amounts.
}
void reference(const std::filesystem::path& artwork) {
    NativeTest::Fixture f(artwork);int cases=0,parts=0;
    auto* station=f.mission->GetSide(0)->GetStations()->first()->data();
    for(HullID id:Native::hullIds) {
        auto* old=f.ship(0,id);auto* now=f.ship(0,id);auto* hull=old->GetBaseHullType();
        OriginalLoadout::CachedLoadout cache;cache.pht=hull;
        for(auto* l=hull->GetPreferredPartTypes()->first();l;l=l->next()) {
            auto* type=l->data();if(type->GetEquipmentType()==ET_Pack)continue;
            auto* item=new OriginalLoadout::CachedPartLink;item->data()={type,0};cache.cpl.last(item);
        }
        Mount cargo=-5;for(auto* l=hull->GetPreferredPartTypes()->first();l && cargo<0;l=l->next()) {
            auto* item=new OriginalLoadout::CachedPartLink;item->data()={l->data(),cargo++};cache.cpl.first(item);
        }
        for(bool upgraded:{false,true})for(Money budget:{0,1,10,100,500,1000,10000,100000}) {
            cache.pht=upgraded?f.mission->GetHullType(id==414?412:414):hull;
            while(auto* l=old->GetParts()->first())l->data()->Terminate();while(auto* l=now->GetParts()->first())l->data()->Terminate();
            Money a=OriginalLoadout::Reference::buy(old,station,&cache,budget),b=budget;
            Allegiance::BuyCachedLoadoutParts(now,station,cache.pht,&b,[&](auto buy){for(auto* l=cache.cpl.first();l;l=l->next())buy(l->data().ppt,l->data().mount);});
            require(a==b && inventory(old)==inventory(now),"original cached purchase order/amount/budget differs");++cases;parts+=now->GetParts()->n();
        }
    }
    // Installed cores can price every available part at zero. Use real IGC
    // definitions with explicit prices to cover exhaustion, upgrades and fallback.
    auto* old=f.ship();auto* now=f.ship();auto* gun=f.types(ET_Weapon,old).front();
    auto data=*static_cast<const DataWeaponTypeIGC*>(gun->GetData());
    auto makeGun=[&](int id,int successor,int price,bool available) {
        data.partID=id;data.successorPartID=successor;data.price=price;data.groupID=0;
        if(available)data.ttbmRequired.ClearAll();else data.ttbmRequired.SetAll();
        auto* p=static_cast<IpartTypeIGC*>(f.mission->CreateObject(f.mission->GetLastUpdate(),OT_partType,&data,sizeof(data)));require(p,"priced gun fixture");p->Release();return p;
    };
    auto* upgrade=makeGun(31000,NA,113,false);auto* predecessor=makeGun(31001,31000,51,true);
    auto* availableUpgrade=makeGun(31002,NA,113,true);auto* availableBase=makeGun(31003,31002,51,true);
    auto* launcher=static_cast<IlauncherTypeIGC*>(f.types(ET_Magazine,old).front());
    auto missile=*static_cast<const DataMissileTypeIGC*>(launcher->GetExpendableType()->GetData());
    missile.expendabletypeID=31000;missile.launcherDef.price=7;missile.launcherDef.groupID=0;missile.launcherDef.ttbmRequired.ClearAll();
    auto* expendable=f.mission->CreateObject(f.mission->GetLastUpdate(),OT_missileType,&missile,sizeof(missile));require(expendable,"priced missile fixture");expendable->Release();
    auto magazine=*static_cast<const DataLauncherTypeIGC*>(launcher->GetData());magazine.partID=31004;magazine.successorPartID=NA;magazine.expendabletypeID=31000;
    auto* rounds=static_cast<IpartTypeIGC*>(f.mission->CreateObject(f.mission->GetLastUpdate(),OT_partType,&magazine,sizeof(magazine)));require(rounds,"priced magazine fixture");rounds->Release();
    require(station->CanBuy(predecessor) && !station->CanBuy(upgrade) && station->GetSimilarPart(upgrade)==predecessor && station->GetSuccessor(availableBase)==availableUpgrade,"fixtures exercise real station successor and similar-part rules");
    int partial=0,spent=0;
    for(auto* requested:{upgrade,availableBase})for(Money budget:{0,1,6,7,8,50,51,52,112,113,114,200,500,10000}) {
        OriginalLoadout::CachedLoadout cache;cache.pht=old->GetBaseHullType();
        for(auto [part,mount]:std::vector<std::pair<IpartTypeIGC*,Mount>>{{rounds,-5},{requested,0},{rounds,0}}) {
            auto* l=new OriginalLoadout::CachedPartLink;l->data()={part,mount};cache.cpl.last(l);
        }
        while(auto* l=old->GetParts()->first())l->data()->Terminate();while(auto* l=now->GetParts()->first())l->data()->Terminate();
        Money a=OriginalLoadout::Reference::buy(old,station,&cache,budget),b=budget;
        Allegiance::BuyCachedLoadoutParts(now,station,cache.pht,&b,[&](auto buy){for(auto* l=cache.cpl.first();l;l=l->next())buy(l->data().ppt,l->data().mount);});
        require(a==b && inventory(old)==inventory(now),"priced successor/similar/partial reload parity");
        require(a>=0 && old->GetValue()+a==budget,"actual IGC part values conserve purchase credits");
        if(auto* p=now->GetMountedPart(NA,-5))partial+=p->GetAmount()>0 && p->GetAmount()<rounds->GetAmount(now);
        spent+=a<budget;++cases;
    }
    require(partial && spent,"paid matrix must exercise partial launcher purchases");
    require(parts>0,"reference purchase matrix bought no equipment");printf("%d frozen loadout purchase comparisons, %d equipped parts\n",cases,parts);
}
void restorationReference(const std::filesystem::path& artwork) {
    NativeTest::Fixture f(artwork);auto* station=f.mission->GetSide(0)->GetStations()->first()->data();
    int cases=0,reused=0,recovered=0;
    for(auto id:Native::hullIds)for(bool researched:{false,true}) {
        auto* source=f.ship(0,id);auto* old=f.ship(0,id);auto* now=f.ship(0,id);auto* hull=source->GetBaseHullType();
        auto tech=f.mission->GetCivilization(25)->GetBaseTechs();if(researched)tech.SetAll();source->GetSide()->SetDevelopmentTechs(tech);
        for(auto* l=hull->GetPreferredPartTypes()->first();l;l=l->next()) {
            auto* type=l->data();auto et=type->GetEquipmentType();if(et==ET_Pack)continue;
            for(Mount mount=0;mount<(et==ET_Weapon?hull->GetMaxWeapons():1);++mount)
                if(hull->CanMount(type,mount) && !source->GetMountedPart(et,mount))source->CreateAndAddPart(type,mount,32767);
        }
        std::vector<std::pair<IpartTypeIGC*,Mount>> cacheParts;
        for(auto* l=source->GetParts()->first();l;l=l->next())cacheParts.push_back({l->data()->GetPartType(),l->data()->GetMountID()});
        for(Mount mount=-c_maxCargo;mount<0;++mount) {
            auto* type=mount==-c_maxCargo?f.mission->GetAmmoPack():cacheParts.empty()?f.mission->GetFuelPack():cacheParts[size_t(mount+c_maxCargo)%cacheParts.size()].first;
            if(hull->CanMount(type,mount)){source->CreateAndAddPart(type,mount,32767);cacheParts.push_back({type,mount});}
        }
        // Original SaveLoadout inserts before equal mounts, including weapon,
        // missile and other module entries sharing mount zero.
        std::reverse(cacheParts.begin(),cacheParts.end());std::stable_sort(cacheParts.begin(),cacheParts.end(),[](auto& a,auto& b){return a.second<b.second;});
        std::vector<std::tuple<IpartTypeIGC*,Mount,short>> initial;
        for(auto* l=source->GetParts()->first();l;l=l->next())initial.push_back({l->data()->GetPartType(),l->data()->GetMountID(),l->data()->GetAmount()});
        for(int damage=0;damage<4;++damage)for(Money extra:{0,1,10,100,500,10000}) {
            for(auto* ship:{source,old,now})while(auto* l=ship->GetParts()->first())l->data()->Terminate();
            old->SetBaseHullType(hull);now->SetBaseHullType(hull);
            for(auto [type,mount,amount]:initial) {
                if((damage==1 && mount==-c_maxCargo) || (damage==2 && mount==0 && type->GetEquipmentType()==ET_Weapon))continue;
                if(damage==3 && IlauncherTypeIGC::IsLauncherType(type->GetEquipmentType()))amount=1;
                source->CreateAndAddPart(type,mount,amount);
            }
            OriginalLoadout::CachedLoadout cache;cache.pht=hull;
            struct Part {PartID id;Mount mount;bool bDuplicated{};};std::vector<Part> parts;
            for(auto [type,mount]:cacheParts){auto* l=new OriginalLoadout::CachedPartLink;l->data()={type,mount};cache.cpl.last(l);parts.push_back({type->GetObjectID(),mount});}
            Money budget=source->GetValue()+extra;
            auto a=OriginalLoadout::Reference::restore(source,old,station,&cache,budget);
            auto b=Allegiance::RestoreMatchingLoadout<Part>(source,now,station,hull,budget,
                [&](auto visit){for(auto& part:parts)visit(part);},
                [&](auto visit){for(auto it=parts.rbegin();it!=parts.rend();++it)if(!visit(*it))break;},
                [&](const Part& part){return f.mission->GetPartType(part.id);});
            require(a==b && old->GetBaseHullType()==now->GetBaseHullType() && inventory(old)==inventory(now),"matching-hull restoration differs from frozen WinTrek policy");
            auto* nativeLink=parts.data();for(auto* l=cache.cpl.first();l;l=l->next(),++nativeLink)require(l->data().bDuplicated==nativeLink->bDuplicated,"duplicate cache matching differs by mount or traversal order");
            for(auto* l=now->GetParts()->first();l;l=l->next())reused+=l->data()->GetMountID()>=0 && !station->CanBuy(l->data()->GetPartType());
            recovered+=damage>0 && now->GetParts()->n()>source->GetParts()->n();++cases;
        }
    }
    require(reused && recovered,"restoration matrix fails to exercise owned unavailable parts and missing equipment");
    printf("%d frozen matching-hull restoration comparisons, %d unavailable mounted parts retained, %d lost-slot recoveries\n",cases,reused,recovered);
}
void ownedRestoration(const std::filesystem::path& artwork) {
    Native::Assets assets(artwork);auto loader=[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};};
    Native::SessionOptions options;options.startInHangar=true;options.enemies=0;options.scenario=Native::Scenario::Conquest;options.rules.miners=0;
    Native::Game game(artwork,loader,0,0,options);auto* ship=game.playerShip();auto* station=ship->GetStation();auto* gun=ship->GetMountedPart(ET_Weapon,0);
    require(gun,"owned unavailable gun fixture");auto data=*static_cast<const DataWeaponTypeIGC*>(gun->GetPartType()->GetData());data.partID=31030;data.successorPartID=NA;data.price=0;data.groupID=0;data.ttbmRequired.SetAll();
    auto* type=static_cast<IpartTypeIGC*>(game.core()->CreateObject(game.core()->GetLastUpdate(),OT_partType,&data,sizeof(data)));require(type,"create owned unavailable gun");type->Release();
    gun->Terminate();ship->CreateAndAddPart(type,0,1);require(!station->CanBuy(type),"owned gun is not for sale here");
    auto remembered=game.currentLoadout();game.rememberEditorLoadout(remembered);
    auto* cargo=ship->GetMountedPart(NA,-5);require(cargo,"lost cargo fixture");int cargoType=cargo->GetPartType()->GetObjectID();cargo->Terminate();
    auto live=game.currentLoadout();auto rng=game.simulationRandomState();auto count=game.core()->GetShips()->n();auto money=game.state().money;
    auto preview=game.previewLoadout();require(preview.valid && std::any_of(preview.loadout.parts.begin(),preview.loadout.parts.end(),[](auto& p){return p.mount==0 && p.id==31030;}),"initial editor substitutes a different gun for owned unavailable equipment");
    require(std::any_of(preview.loadout.parts.begin(),preview.loadout.parts.end(),[&](auto& p){return p.mount==-5 && p.id==cargoType;}),"initial editor forgets lost cargo from remembered fitting");
    require(game.currentLoadout()==live && game.state().money==money && game.core()->GetShips()->n()==count && game.simulationRandomState()==rng,"restoration preview changes inventory, credits, mission membership or RNG");
    auto purchase=game.purchaseLoadout(preview.loadout);require(purchase.accepted && purchase.complete && !purchase.launched && ship->GetMountedPart(ET_Weapon,0)->GetPartType()==type,"restored purchase fails to reuse an unavailable owned gun");
    require(ship->GetMountedPart(NA,-5) && ship->GetMountedPart(NA,-5)->GetPartType()->GetObjectID()==cargoType,"restored purchase does not recover cached cargo");
    IhullTypeIGC* unavailable=nullptr;IhullTypeIGC* first=nullptr;
    for(auto* link=game.core()->GetHullTypes()->first();link;link=link->next()) {
        auto* hull=link->data();if(hull->GetGroupID()<0 || hull->HasCapability(c_habmLifepod))continue;
        if(!station->CanBuy(hull) && !unavailable)unavailable=hull;
        if(station->CanBuy(hull) && hull->GetPrice()<=game.state().money && !first)first=static_cast<IhullTypeIGC*>(station->GetSuccessor(hull));
    }
    require(unavailable && first,"cached unavailable hull fallback fixture");
    auto other=game.currentLoadout();while(auto* link=ship->GetParts()->first())link->data()->Terminate();ship->SetBaseHullType(unavailable);
    Native::Loadouts presets;presets.remember(0,game.currentLoadout());game.loadoutPresets(presets);game.rememberEditorLoadout(other);
    auto fallback=game.previewLoadout();require(fallback.valid && fallback.loadout.hull==first->GetObjectID() && ship->GetBaseHullType()==unavailable,"unavailable cached hull fails to use the original first affordable default without mutating the owned hull");
    puts("Initial editor preserves owned unavailable equipment, restores remembered cargo and remains read-only until purchase");
}
void replacement(const std::filesystem::path& artwork){
    Native::Assets assets(artwork);auto loader=[&](const std::string& name,const std::string&){auto model=assets.model(name);return Native::ModelHandle{1,model->radius,model->frames,model->lights};};
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.enemies=0;options.rules.miners=0;options.startInHangar=true;
    Native::Game game(artwork,loader,0,0,options);auto* ship=game.playerShip();auto before=game.currentLoadout();
    TechTreeBitMask all;all.SetAll();ship->GetSide()->SetDevelopmentTechs(all);
    auto choices=game.previewLoadout().hulls;auto other=std::find_if(choices.begin(),choices.end(),[&](auto& hull){return hull.id!=before.hull;});require(other!=choices.end(),"custom-zero replacement hull fixture");
    auto wanted=game.previewLoadout({int16_t(other->id),{}},true).loadout;
    std::erase_if(wanted.parts,[](const auto& part){return part.mount<0 && part.mount!=-5;});
    Native::Loadouts presets;presets.remember(0,wanted);game.loadoutPresets(presets);game.rememberEditorLoadout(before);
    auto rng=game.simulationRandomState();auto preview=game.previewLoadout({},false,-1,true);
    require(preview.valid && preview.loadout.hull==wanted.hull && game.currentLoadout()==before && game.simulationRandomState()==rng,"mission entry ignores custom zero or changes the live ship while previewing");
    auto result=game.restoreDockedLoadout(true);require(result.accepted && result.complete && !result.launched && game.currentLoadout().hull==wanted.hull,"mission entry fails to buy the most recent custom-zero hull");
    IhullTypeIGC* gunship=nullptr;for(auto* link=game.core()->GetHullTypes()->first();link;link=link->next()){
        auto* hull=link->data();if(hull->GetMaxWeapons()<=hull->GetMaxFixedWeapons())continue;
        for(auto* part=hull->GetPreferredPartTypes()->first();part;part=part->next())if(part->data()->GetEquipmentType()==ET_Weapon && hull->CanMount(part->data(),hull->GetMaxFixedWeapons())){gunship=hull;break;}
        if(gunship)break;
    }
    require(gunship,"disembark turret fixture");int parent=game.addLocalPilot("Transport",0,gunship->GetObjectID());
    require(parent>=0 && game.boardShip(parent),"board before custom-zero disembark");
    // A different session cache must not take precedence over custom zero.
    game.rememberEditorLoadout(before);int money=game.state().money;
    require(game.leaveShip() && !game.state().passenger && !game.state().pod && game.currentLoadout().hull==wanted.hull,"disembark fails to replace the pod from custom zero");
    require(game.state().money==money,"disembark replacement minted credits");
    for(const auto& part:wanted.parts){auto* type=game.core()->GetPartType(part.id);auto* actual=ship->GetMountedPart(part.mount<0?NA:type->GetEquipmentType(),part.mount);require(actual && actual->GetPartType()==type,"disembark forgets custom-zero equipment");}
    // Station membership changes outside DockWithStationEvent use the same
    // deferred original callback, without recursively mutating IGC.
    TRef<IstationIGC> station=ship->GetStation();game.launch();auto* lost=ship->GetMountedPart(NA,-5);require(lost,"direct station membership lost-slot fixture");lost->Terminate();ship->SetStation(station);
    require(!ship->GetMountedPart(NA,-5),"station callback recursively refits IGC");game.step({},1./60);
    require(ship->GetMountedPart(NA,-5),"direct station membership bypasses original restoration");
    puts("Mission-entry/custom-zero replacement, disembark precedence and deferred direct station restoration passed");
}
void dockingRestoration(const std::filesystem::path& artwork){
    Native::Assets assets(artwork);auto loader=[&](const std::string& name,const std::string&){auto model=assets.model(name);return Native::ModelHandle{1,model->radius,model->frames,model->lights};};
    Native::SessionOptions options;options.scenario=Native::Scenario::Conquest;options.enemies=0;options.rules.miners=0;options.startInHangar=true;
    Native::Game game(artwork,loader,0,0,options);auto* ship=game.playerShip();TRef<IstationIGC> station=ship->GetStation();
    TechTreeBitMask all;all.SetAll();ship->GetSide()->SetDevelopmentTechs(all);
    auto choices=game.previewLoadout().hulls;auto other=std::find_if(choices.begin(),choices.end(),[&](auto& hull){return hull.id!=ship->GetBaseHullType()->GetObjectID();});
    require(other!=choices.end(),"recovery needs a different available hull");
    auto wanted=game.previewLoadout({int16_t(other->id),{}},true).loadout;
    std::erase_if(wanted.parts,[](const auto& part){return part.mount<0 && part.mount!=-5;});
    auto bought=game.purchaseLoadout(wanted);require(bought.accepted && bought.complete,"purchase recovery fixture");
    int money=game.state().money;game.launch();
    auto* cargo=ship->GetMountedPart(NA,-5);require(cargo,"dock restoration cargo fixture");cargo->Terminate();
    auto* gun=ship->GetMountedPart(ET_Weapon,0);require(gun,"dock restoration gun fixture");gun->Terminate();
    require(game.core()->GetIgcSite()->DockWithStationEvent(ship,station),"original IGC docking callback");
    require(!ship->GetMountedPart(NA,-5) && !ship->GetMountedPart(ET_Weapon,0),"restoration reenters IGC docking callback");
    game.step({},1./60);
    for(const auto& part:wanted.parts){auto* type=game.core()->GetPartType(part.id);auto* actual=ship->GetMountedPart(part.mount<0?NA:type->GetEquipmentType(),part.mount);require(actual && actual->GetPartType()==type,"automatic docking forgets remembered equipment");}
    require(game.state().docked && game.state().money==money,"automatic docking changed free-core credits or launched");
    game.launch();auto position=ship->GetPosition();
    game.core()->GetIgcSite()->KillShipEvent(game.core()->GetLastUpdate(),ship,nullptr,-1,position,position);
    require(ship->GetStation() && ship->GetBaseHullType()->HasCapability(c_habmLifepod),"authority replaces a lost hull before the client restores it");
    game.step({},1./60);
    require(game.state().docked && !game.state().pod && game.currentLoadout().hull==wanted.hull,"recovery forgets the last selected hull");
    for(const auto& part:wanted.parts){auto* type=game.core()->GetPartType(part.id);auto* actual=ship->GetMountedPart(part.mount<0?NA:type->GetEquipmentType(),part.mount);require(actual && actual->GetPartType()==type,"recovery forgets the last selected equipment");}
    require(game.state().money==money,"recovery minted credits");
    puts("Offline IGC docking and fatal recovery restore the last chosen hull and missing equipment after callbacks, without minting credits");
}
void game(const std::filesystem::path& artwork,const std::filesystem::path& file) {
    Native::Assets assets(artwork);auto loader=[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};};
    Native::SessionOptions options;options.startInHangar=true;options.enemies=0;options.scenario=Native::Scenario::FreeFlight;
    Native::Loadout original;
    {
        Native::Game game(artwork,loader,0,0,options);auto guns=game.partChoices(Native::PartKind::Weapon,0);
        require(game.replacePart(Native::PartKind::Weapon,0,guns.back().id) && game.replacePart(Native::PartKind::Pack,-5,guns.front().id),"prepare customized loadout");
        original=game.currentLoadout();Native::Loadouts saved;saved.remember(3,original);saved.save(file);
        auto rng=game.simulationRandomState();auto ships=game.core()->GetShips()->n();auto revision=game.state().economyRevision;
        auto value=game.shipValue();auto money=game.state().money;auto life=game.playerShip()->GetLastUpdate();
        auto oldParts=inventory(game.playerShip());
        auto* currentHull=game.playerShip()->GetBaseHullType();auto* station=game.playerShip()->GetStation();
        auto* expectedHull=station->CanBuy(currentHull)?static_cast<IhullTypeIGC*>(station->GetSuccessor(currentHull)):currentHull;
        auto preview=game.previewLoadout();require(preview.valid && preview.affordable && preview.value>=0 && preview.loadout.hull==expectedHull->GetObjectID(),"current hull preview and affordable original successor restoration");
        require(preview.hulls.size()>1 && !preview.slots.empty(),"preview exposes available hulls and valid equipment slots");
        for(const auto& slot:preview.slots)if(slot.id>=0){
            auto info=game.loadoutPartInfo(slot.id,preview.loadout.hull);
            require(info && info->name==slot.name && info->description==slot.description && info->model==slot.model && info->metrics==slot.metrics,"read-only menu part info differs from original ship preview calculations");
        }
        require(!game.loadoutPartInfo(-1,preview.loadout.hull) && !game.loadoutPartInfo(preview.slots.front().id,-1),"invalid part or hull resolves menu metadata");

        for(auto& hull:preview.hulls){
            auto defaults=game.previewLoadout({int16_t(hull.id),{}},true);require(defaults.valid && defaults.loadout.hull==hull.id,"browse hull with original default equipment");
            for(float stat:defaults.stats)require(std::isfinite(stat),"preview hull statistics finite");
            for(const auto& slot:defaults.slots)if(slot.id>=0){auto info=game.loadoutPartInfo(slot.id,hull.id);require(info && info->metrics==slot.metrics,"read-only part info loses faction modifiers on a different staged hull");}

        }
        require(game.currentLoadout()==original && inventory(game.playerShip())==oldParts && game.shipValue()==value && game.state().money==money && game.simulationRandomState()==rng && game.core()->GetShips()->n()==ships && !game.core()->GetShip(-2) && game.playerShip()->GetLastUpdate()==life && game.state().economyRevision==revision,"browsing loadouts changed live ship, economy, RNG, mission membership or events");
        if(auto* magazine=game.playerShip()->GetMountedPart(ET_Magazine,0)){auto amount=magazine->GetAmount();if(amount>1){
            magazine->SetAmount(1);auto refill=game.previewLoadout();
            auto part=std::find_if(refill.loadout.parts.begin(),refill.loadout.parts.end(),[&](auto& p){return p.mount==0 && game.core()->GetPartType(p.id)->GetEquipmentType()==ET_Magazine;});
            require(part!=refill.loadout.parts.end(),"restoration loses the remembered launcher slot");
            auto* type=static_cast<IlauncherTypeIGC*>(game.core()->GetPartType(part->id));
            int capacity=game.core()->GetHullType(refill.loadout.hull)->GetCapacity(ET_Magazine);
            int full=std::max(1,capacity/type->GetExpendableType()->GetLauncherDef()->expendableSize);
            require(part->amount==(station->CanBuy(magazine->GetPartType())?full:1) && magazine->GetAmount()==1,"restoration loses launcher refill/owned-quantity policy or changes live ammo");
            magazine->SetAmount(amount);
        }}
        auto invalid=original;invalid.parts.push_back(invalid.parts.front());
        require(!game.applyLoadout(invalid) && game.currentLoadout()==original,"duplicate saved slot rejected without changing ship");
        game.launch();require(!game.applyLoadout(original) && game.currentLoadout().hull<0,"inflight save/refit rejected");
    }
    {
        Native::Game game(artwork,loader,0,0,options);auto saved=Native::Loadouts::load(file);game.selectHull(1);
        require(game.applyLoadout(*saved.find(3,original.hull)),"fit persisted ship in a new session");
        auto actual=game.currentLoadout();auto sort=[](auto& x){std::sort(x.parts.begin(),x.parts.end(),[](auto& a,auto& b){return std::tie(a.id,a.mount)<std::tie(b.id,b.mount);});};
        sort(actual);sort(original);require(actual==original && game.state().docked,"persisted hull and mounted/cargo IDs restored");
        for(auto& e:game.state().equipment)require(e.empty || e.mounted==1,"hangar refit arms equipment");
        game.launch();game.step({},.01);require(!game.state().dead && !game.state().docked,"restored ship launches");
    }
    options.scenario=Native::Scenario::Conquest;
    {
        Native::Game game(artwork,loader,0,0,options);Native::Loadouts saved;saved.remember(0,game.currentLoadout());
        auto wanted=*saved.find(0,-1);int before=game.state().money;
        require(game.applyLoadout(wanted) && game.state().money==before,"same economy loadout does not mint or lose credits");
        auto parts=game.partChoices(Native::PartKind::Pack,-1);
        auto chosen=std::find_if(parts.begin(),parts.end(),[](auto& p){return p.id>=0;});
        require(chosen!=parts.end() && game.replacePart(Native::PartKind::Pack,-1,chosen->id),"alter cargo before restoring economy loadout");
        require(game.applyLoadout(wanted) && game.state().money==before,"free-core refit preserves credits");
        for(int i=0;i<10;++i)require(game.applyLoadout(wanted) && game.state().money==before,"repeated restoration cannot mint credits");
    }
    std::filesystem::remove(file);
    puts("Persistent cross-session refit, legal state checks and original economy trade-in passed");
}
}
int main(int argc,char** argv) {
    try {auto artwork=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();auto file=std::filesystem::temp_directory_path()/("allegiance-loadouts-check-"+std::to_string(getpid()));storage(file);reference(artwork);restorationReference(artwork);ownedRestoration(artwork);replacement(artwork);dockingRestoration(artwork);game(artwork,file);presetFeedback(artwork);return 0;}
    catch(const std::exception& e){fprintf(stderr,"loadouts: %s\n",e.what());return 1;}
}
