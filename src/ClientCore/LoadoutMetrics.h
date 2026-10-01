// Raw IGC quantities and the original loadout panel's authored calibration.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace Allegiance {
enum class LoadoutMetric {
    EquipmentType,Signature,Mass,WeaponDamage,WeaponRange,WeaponSpeed,WeaponRate,WeaponDamageClass,
    MagazineDamage,MagazineRange,MagazineLockTime,MagazineMaxLock,MagazineDamageClass,ChaffStrength,
    ProbeScanRange,ProbeVisibility,ProbeHitPts,ProbeLifespan,MineDamage,MineVisibility,MineRadius,MineLifespan,
    ShieldRegen,ShieldHitPts,ShieldDefenseClass,CloakCloaking,CloakDuration,CloakEnergy,CloakRecharge,
    AfterburnerMaxSpeed,AfterburnerBurnRate,AfterburnerHullSpeed,AfterburnerHullThrust,Count
};
struct LoadoutPartMetrics {
    std::array<float,size_t(LoadoutMetric::Count)> values{};
    uint64_t present{};
    void set(LoadoutMetric metric,float value){values[size_t(metric)]=value;present|=uint64_t(1)<<size_t(metric);}
    bool has(LoadoutMetric metric)const{return (present&(uint64_t(1)<<size_t(metric)))!=0;}
    float get(LoadoutMetric metric)const{return values[size_t(metric)];}
    bool operator==(const LoadoutPartMetrics&)const=default;
};
static_assert(size_t(LoadoutMetric::Count)<=64);
struct LoadoutMetricBinding {LoadoutMetric metric;const char* name;const char* maximum;bool percent{};};
// Null maxima are deliberately unscaled in WinTrek. In particular its
// maxMagazineMaxLock declaration is not used to normalize the lock gauge.
inline constexpr LoadoutMetricBinding LoadoutMetricBindings[]={
    {LoadoutMetric::Signature,"signature","maxPartSignature",true},{LoadoutMetric::Mass,"mass","maxPartMass"},
    {LoadoutMetric::WeaponDamage,"weaponDamage","maxWeaponPower"},{LoadoutMetric::WeaponRange,"weaponRange","maxWeaponRange"},
    {LoadoutMetric::WeaponSpeed,"weaponSpeed","maxWeaponSpeed"},{LoadoutMetric::WeaponRate,"weaponRate","maxWeaponRate"},
    {LoadoutMetric::MagazineDamage,"magazineDamage",nullptr},{LoadoutMetric::MagazineRange,"magazineRange","maxMagazineRange"},
    {LoadoutMetric::MagazineLockTime,"magazineLockTime","maxMagazineLockTime"},{LoadoutMetric::MagazineMaxLock,"magazineMaxLock",nullptr,true},
    {LoadoutMetric::ChaffStrength,"chaffStrength","maxChaffStrength",true},
    {LoadoutMetric::ProbeScanRange,"probeScanRange","maxProbeScanRange"},{LoadoutMetric::ProbeVisibility,"probeVisibility",nullptr,true},
    {LoadoutMetric::ProbeHitPts,"probeHitPts","maxProbeHitPts"},{LoadoutMetric::ProbeLifespan,"probeLifespan","maxProbeLifespan"},
    {LoadoutMetric::MineDamage,"mineDamage","maxMineDamage"},{LoadoutMetric::MineVisibility,"mineVisibility",nullptr,true},
    {LoadoutMetric::MineRadius,"mineRadius","maxMineRadius"},{LoadoutMetric::MineLifespan,"mineLifespan","maxMineLifespan"},
    {LoadoutMetric::ShieldRegen,"shieldRegen","maxShieldRegen"},{LoadoutMetric::ShieldHitPts,"shieldHitPts","maxShieldHitPts"},
    {LoadoutMetric::CloakCloaking,"cloakCloaking",nullptr,true},{LoadoutMetric::CloakDuration,"cloakDuration",nullptr},
    {LoadoutMetric::AfterburnerMaxSpeed,"afterburnerMaxSpeed",nullptr},{LoadoutMetric::AfterburnerBurnRate,"afterburnerBurnRate","maxAfterburnerBurnRate"}
};
inline constexpr const char* LoadoutHullMetrics[]={"MaxSpeed","Acceleration","TurnRate","HitPoints","MaxEnergy","ScanRange","Signature","Mass"};
inline constexpr const char* LoadoutDerivedMaxima[]={"maxMagazineHitDamage","maxMagazineAreaDamage","maxCloakEnergy","maxAfterburnerMaxThrust"};
template<class Maximum,class Emit>
void PresentLoadoutPart(const LoadoutPartMetrics& metrics,Maximum maximum,Emit emit){
    using enum LoadoutMetric;
    auto ratio=[](float value,float divisor){return divisor!=0?value/divisor:0.f;};
    for(const auto& binding:LoadoutMetricBindings)if(metrics.has(binding.metric)){
        float value=metrics.get(binding.metric),scale=binding.maximum?maximum(binding.maximum):1.f;
        if(binding.metric==MagazineDamage)scale=maximum("maxMagazineHitDamage")+maximum("maxMagazineAreaDamage");
        else if(binding.metric==CloakDuration){float recharge=metrics.get(CloakRecharge),limit=maximum("maxCloakEnergy");scale=limit<=recharge?-1:ratio(metrics.get(CloakEnergy),limit-recharge);}
        else if(binding.metric==AfterburnerMaxSpeed)scale=metrics.get(AfterburnerHullSpeed)*(1+ratio(maximum("maxAfterburnerMaxThrust"),metrics.get(AfterburnerHullThrust)));
        emit(binding.name,ratio(value,scale));emit(std::string(binding.name)+"Number",binding.percent?value*100:value);
    }
    for(auto [metric,name]:{std::pair{EquipmentType,"equipmentType"},std::pair{WeaponDamageClass,"weaponDamageClassNumber"},std::pair{MagazineDamageClass,"magazineDamageClassNumber"},std::pair{ShieldDefenseClass,"shieldDefenseClassNumber"}})
        if(metrics.has(metric))emit(name,metrics.get(metric));
}
}
