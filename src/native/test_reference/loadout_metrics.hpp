#pragma once
#include "authored_hosts.hpp"
#include "../frontend.hpp"
#include "../../ClientCore/LoadoutPresentation.h"
#include <functional>
#include <regex>
#include <set>

namespace NativeTest {
// Independent numeric reference from WinTrek/load.cpp's Update*PartInfo.
// Keep the authored maxima here rather than using the production binding table.
inline Native::UiNumbers originalPartMetrics(ImissionIGC* core,const IhullTypeIGC* hull,const GlobalAttributeSet& ga,IpartTypeIGC* part,Native::OriginalUi& ui,bool base=false){
    Native::UiNumbers values;
    auto maximum=[&](const char* name){return float(ui.number("partinfo",name));};
    auto field=[&](const char* name,float value,float limit=1,bool percent=false){values[name]=limit!=0?value/limit:0;values[std::string(name)+"Number"]=percent?value*100:value;};
    field("signature",part->GetSignature()/ga.GetAttribute(c_gaSignature),maximum("maxPartSignature"),true);
    field("mass",part->GetMass(),maximum("maxPartMass"));values["equipmentType"]=part->GetEquipmentType();
    switch(part->GetEquipmentType()){
    case ET_Weapon:{
        auto* weapon=static_cast<const DataWeaponTypeIGC*>(part->GetData());auto* projectile=core->GetProjectileType(weapon->projectileTypeID);if(!projectile)break;
        float rate=1/weapon->dtimeBurst,speed=projectile->GetSpeed();if(weapon->cAmmoPerShot)speed*=ga.GetAttribute(c_gaSpeedAmmo);
        float range=speed*projectile->GetLifespan();if(!weapon->cAmmoPerShot)range*=ga.GetAttribute(c_gaLifespanEnergy);
        field("weaponDamage",(projectile->GetPower()+projectile->GetBlastPower())*rate*ga.GetAttribute(c_gaDamageGuns),maximum("maxWeaponPower"));
        field("weaponRange",range,maximum("maxWeaponRange"));field("weaponSpeed",speed,maximum("maxWeaponSpeed"));field("weaponRate",rate,maximum("maxWeaponRate"));values["weaponDamageClassNumber"]=projectile->GetDamageType();break;
    }
    case ET_Magazine:{
        auto* missile=static_cast<ImissileTypeIGC*>(static_cast<IlauncherTypeIGC*>(part)->GetExpendableType());float life=missile->GetLifespan();
        field("magazineDamage",(missile->GetPower()+missile->GetBlastPower())*ga.GetAttribute(c_gaDamageMissiles),maximum("maxMagazineHitDamage")+maximum("maxMagazineAreaDamage"));
        field("magazineRange",missile->GetInitialSpeed()*life+.5f*(missile->GetAcceleration()*life*life),maximum("maxMagazineRange"));
        field("magazineLockTime",missile->GetLockTime(),maximum("maxMagazineLockTime"));field("magazineMaxLock",missile->GetMaxLock(),1,true);values["magazineDamageClassNumber"]=missile->GetDamageType();break;
    }
    case ET_ChaffLauncher:{
        auto* chaff=static_cast<const DataChaffTypeIGC*>(static_cast<IlauncherTypeIGC*>(part)->GetExpendableType()->GetData());float strength=chaff->chaffStrength;
        field("chaffStrength",strength<=1?strength/2:1-1/(2*strength),maximum("maxChaffStrength"),true);break;
    }
    case ET_Dispenser:{
        auto* expendable=static_cast<IlauncherTypeIGC*>(part)->GetExpendableType();
        if(expendable->GetObjectType()==OT_probeType){
            auto* probe=static_cast<IprobeTypeIGC*>(expendable);auto* data=static_cast<const DataProbeTypeIGC*>(probe->GetData());auto* projectile=probe->GetProjectileType();
            float range=projectile?projectile->GetLifespan()*projectile->GetSpeed()/1.2f*(probe->GetAmmo()?ga.GetAttribute(c_gaSpeedAmmo):1):data->scannerRange*ga.GetAttribute(c_gaScanRange);
            field("probeScanRange",range,maximum("maxProbeScanRange"));field("probeVisibility",data->signature/ga.GetAttribute(c_gaSignature),1,true);
            field("probeHitPts",data->hitPoints,maximum("maxProbeHitPts"));field("probeLifespan",data->lifespan,maximum("maxProbeLifespan"));
        }else{
            auto* mine=static_cast<const DataMineTypeIGC*>(expendable->GetData());values["equipmentType"]=-4;
            field("mineDamage",mine->power,maximum("maxMineDamage"));field("mineVisibility",mine->signature/ga.GetAttribute(c_gaSignature),1,true);
            field("mineRadius",mine->radius,maximum("maxMineRadius"));field("mineLifespan",mine->lifespan,maximum("maxMineLifespan"));
        }break;
    }
    case ET_Shield:{
        auto* shield=static_cast<const DataShieldTypeIGC*>(part->GetData());float hp=shield->maxStrength*ga.GetAttribute(c_gaMaxShieldShip);
        field("shieldRegen",hp/(shield->rateRegen*ga.GetAttribute(c_gaShieldRegenerationShip)),maximum("maxShieldRegen"));field("shieldHitPts",hp,maximum("maxShieldHitPts"));values["shieldDefenseClassNumber"]=shield->defenseType;break;
    }
    case ET_Cloak:{
        auto* cloak=static_cast<const DataCloakTypeIGC*>(part->GetData());float recharge=hull->GetRechargeRate(),energy=hull->GetMaxEnergy()*(base?ga.GetAttribute(c_gaMaxEnergy):1);
        float duration=cloak->energyConsumption<=recharge?-1:energy/(cloak->energyConsumption-recharge);
        float limit=maximum("maxCloakEnergy"),maxDuration=limit<=recharge?-1:energy/(limit-recharge);
        field("cloakCloaking",cloak->maxCloaking,1,true);field("cloakDuration",duration,maxDuration);break;
    }
    case ET_Afterburner:{
        auto* afterburner=static_cast<const DataAfterburnerTypeIGC*>(part->GetData());float thrust=static_cast<const DataHullTypeIGC*>(hull->GetData())->thrust;
        float speed=hull->GetMaxSpeed()*(base?ga.GetAttribute(c_gaMaxSpeed):1);
        field("afterburnerMaxSpeed",speed*(1+afterburner->maxThrust/thrust),speed*(1+maximum("maxAfterburnerMaxThrust")/thrust));
        field("afterburnerBurnRate",hull->GetMaxFuel()/(afterburner->maxThrust*afterburner->fuelConsumption),maximum("maxAfterburnerBurnRate"));break;
    }
    default:break;
    }return values;
}
inline void alterLoadoutCalibration(const std::filesystem::path& root,std::string_view filename){
    auto path=root/filename;std::ifstream input(path,std::ios::binary);std::string source((std::istreambuf_iterator<char>(input)),{});input.close();
    std::regex declaration(R"(\b(max(?:Hull|Weapon|Shield|Cloak|Part|Magazine|Afterburner|Chaff|Pack|Mine|Probe)\w*)\s*=\s*([+.0-9eE-]+)\s*;)");
    std::string result;size_t position=0;int count=0;
    for(std::sregex_iterator i(source.begin(),source.end(),declaration),end;i!=end;++i){
        const auto& match=*i;result.append(source,position,match.position()-position);
        float factor=.55f+.37f*(count++%4);result+=match[1].str()+" = "+std::to_string(std::stof(match[2].str())*factor)+";";position=match.position()+match.length();
    }
    require(count>=8,"loadout calibration declarations missing");result.append(source,position,source.size()-position);
    std::filesystem::remove(path);std::ofstream output(path,std::ios::binary);output<<result;require(bool(output),"alternate loadout calibration write failed");
}
inline void checkLoadoutMetrics(Native::Assets& installed,const std::function<void(const Native::MenuView&,const Native::MenuView&,std::string_view)>& paint={}){
    using namespace Native;
    AuthoredAssets fixture(installed.root());alterLoadoutCalibration(fixture.root,"hullinfo.mdl");alterLoadoutCalibration(fixture.root,"partinfo.mdl");
    Assets alternate(fixture.root);OriginalUi stockUi(installed),alternateUi(alternate);
    SessionOptions options;options.scenario=Scenario::Conquest;options.startInHangar=true;options.enemies=0;options.rules.miners=0;
    Game game(installed.root(),[](auto&,auto&){return ModelHandle{1,1};},0,0,options);auto* core=game.core();auto* ship=game.playerShip();
    std::array<const IhullTypeIGC*,3> hulls{ship->GetHullType(),core->GetHullTypes()->first()->data(),core->GetHullTypes()->last()->data()};
    std::set<int> categories;int comparisons=0,fields=0;
    for(auto* source:{&stockUi,&alternateUi})for(size_t h=0;h<hulls.size();++h)for(int modifiers=0;modifiers<3;++modifiers){
        GlobalAttributeSet ga=ship->GetSide()->GetGlobalAttributeSet();
        for(auto attribute:{c_gaSignature,c_gaSpeedAmmo,c_gaDamageGuns,c_gaLifespanEnergy,c_gaDamageMissiles,c_gaScanRange,c_gaMaxShieldShip,c_gaShieldRegenerationShip,c_gaMaxEnergy,c_gaMaxSpeed})
            ga.SetAttribute(attribute,ga.GetAttribute(attribute)*(modifiers==0?1:modifiers==1?.73f:1.41f));
        for(auto* link=core->GetPartTypes()->first();link;link=link->next()){
            auto* part=link->data();UiNumbers actual;
            auto raw=Allegiance::DescribeLoadoutPart(core,hulls[h],ga,part,h!=0);
            Allegiance::PresentLoadoutPart(raw,[&](const char* name){return float(source->number("partinfo",name));},[&](const std::string& name,float value){actual[name]=value;});
            auto expected=originalPartMetrics(core,hulls[h],ga,part,*source,h!=0);require(actual.size()==expected.size(),"loadout metric field set differs from WinTrek");
            for(const auto& [name,value]:expected){
                auto found=actual.find(name);require(found!=actual.end(),"loadout metric missing original field");
                require(std::abs(found->second-value)<=1e-5*std::max(1.0,std::abs(value)),"loadout metric differs from original authored calibration");++fields;
            }
            categories.insert(int(actual.at("equipmentType")));++comparisons;
        }
    }
    require(categories.size()==9,"loadout matrix misses an equipment category");
    // Exercise the actual Frontend producer and its namespace-specific cache.
    // Only the authored UI root is compared/rendered, never a world/model scene.
    auto preview=game.previewLoadout();require(preview.valid,"loadout calibration preview missing");
    auto selected=std::find_if(preview.slots.begin(),preview.slots.end(),[](const auto& slot){return slot.id>=0 && slot.mount==0 && slot.kind==PartKind::Weapon;});require(selected!=preview.slots.end(),"loadout calibration hover slot missing");
    size_t slotIndex=selected-preview.slots.begin();auto mount=selected->mount;auto kind=selected->kind;
    constexpr const char* names[]={"MaxSpeed","Acceleration","TurnRate","HitPoints","MaxEnergy","ScanRange","Signature","Mass"};
    for(size_t i=0;i<8;++i){float max=float(stockUi.number("hullinfo",std::string("maxHull")+names[i]));preview.stats[i]=i==6?1/(max*.35f):max*.35f;preview.baseStats[i]=i==6?1/(max*.27f):max*.27f;}
    Settings settings;Frontend ui(settings);ui.setPlaying(true);
    ui.loadoutServices([&](const Loadout&,bool,int){return preview;},[](PartKind,int,int){return std::vector<PartChoice>{};});
    int panels=0;
    for(int variant:{0,1,0}){
        auto& assets=variant?alternate:installed;auto& source=variant?alternateUi:stockUi;
        ui.originalInterface(assets);ui.open(Screen::ShipLoadout,&game.state());std::set<int> seen;
        for(auto* link=core->GetPartTypes()->first();link;link=link->next()){
            auto* part=link->data();auto info=game.loadoutPartInfo(part->GetObjectID(),preview.loadout.hull);require(bool(info),"loadout calibration part metadata missing");
            int equipment=int(info->metrics.get(Allegiance::LoadoutMetric::EquipmentType));if(!seen.insert(equipment).second)continue;
            preview.slots[slotIndex]=*info;preview.slots[slotIndex].mount=mount;preview.slots[slotIndex].kind=kind;ui.stageLoadout(preview.loadout);
            auto hit=std::find_if(ui.view().items.begin(),ui.view().items.end(),[&](const auto& item){return item.action.op==MenuOp::PreviewSlot && item.action.value==int(slotIndex);});require(hit!=ui.view().items.end(),"loadout calibration hover control missing");ui.pointerHover(size_t(hit-ui.view().items.begin()));
            UiNumbers values=originalPartMetrics(core,core->GetHullType(preview.loadout.hull),ship->GetSide()->GetGlobalAttributeSet(),part,source,true);
            values.insert({{"partID",part->GetObjectID()},{"hullDefenseClassNumber",preview.defense},{"baseHullDefenseClassNumber",preview.defense},
                {"money",preview.money},{"oldShipCost",preview.oldValue},{"newShipCost",preview.value},{"loadoutShipState",preview.equivalent?0:preview.value<=0?1:preview.affordable?2:3},
                {"equipmentTypeWeapon",1},{"equipmentTypeShield",4},{"equipmentTypeMagazine",2},{"equipmentTypeDispenser",3},{"equipmentTypeCloak",5},{"equipmentTypeAfterburner",7},{"equipmentTypeChaff",0},{"equipmentTypeMine",-4}});
            for(size_t i=0;i<8;++i)for(bool base:{false,true}){
                float value=(base?preview.baseStats:preview.stats)[i];std::string name=std::string(base?"baseHull":"hull")+names[i];
                values[name]=(i==6?1/value:value)/float(source.number("hullinfo",std::string("maxHull")+names[i]));values[name+"Number"]=i==6?value*100:value;
            }
            std::string civ=preview.civilizationIcon,title=preview.name;std::transform(civ.begin(),civ.end(),civ.begin(),[](unsigned char c){return std::tolower(c);});std::transform(title.begin(),title.end(),title.begin(),[](unsigned char c){return std::toupper(c);});
            UiStrings strings{{"civImage",civ+"loadoutpanebmp"},{"civHullImage",civ+"hullinfobmp"},{"titleString",title},{"hullDescriptionString",preview.description},{"partImage","l"+info->model+"bmp"},{"partNameString",info->name},{"partDescriptionString",info->description}};
            MenuView expected=ui.view();expected.artwork=source.layout("loadoutpane","loadoutPane",values,strings);expected.items.clear();expected.text.clear();expected.shipPreview.reset();
            std::erase_if(expected.artwork,[&](const auto& control){return control.name==(preview.value==0?"buyButtonPane":"confirmButtonPane");});
            auto offset=2+source.loadoutOverlay(preview.loadout.hull).size();require(ui.view().artwork.size()>=offset+expected.artwork.size()-1,"loadout authored root truncated");
            MenuView actual=expected;actual.artwork={ui.view().artwork.front()};actual.artwork.insert(actual.artwork.end(),ui.view().artwork.begin()+offset,ui.view().artwork.begin()+offset+expected.artwork.size()-1);
            for(size_t i=0;i<expected.artwork.size();++i){
                const auto& a=actual.artwork[i];const auto& e=expected.artwork[i];
                require(a.kind==e.kind && a.name==e.name && a.text==e.text && a.font==e.font && a.bounds==e.bounds && a.color==e.color && a.uv==e.uv && a.clip==e.clip && a.image==e.image,"loadout authored panel differs from original metrics");
            }
            if(paint && panels<18)paint(actual,expected,"loadout-calibration-"+std::to_string(variant)+"-"+std::to_string(equipment));++panels;
        }
    }
    printf("Original loadout calibration: %d part/hull/attribute comparisons, %d numeric fields, %d stock/alternate/reloaded panels across all nine categories passed\n",comparisons,fields,panels);
}
}
