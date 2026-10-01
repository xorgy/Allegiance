#pragma once
#include <array>
#include <string>
#include <vector>

namespace Allegiance {
// NewGameScreen's read-only Game details report. Take a value snapshot on
// opening the popup; no IGC pointers or live settings-editor state are retained.
struct MissionDetails {
    std::string name,server,core,customMap;
    int lives{32767},map{},connectivity{},imbalance{};
    float helium{};
    std::array<double,4> resources{};
    bool scores{},pods{},friendlyFire{},defections{},joiners{},invulnerable{},developments{},
        shipyard{},tactical{},expansion{},supremacy{},emptyTeams{},alliedRipcord{},experimental{};
};
template<class Params> MissionDetails DescribeMission(const Params& p) {
    return {p.strGameName,{},p.szIGCStaticFile,p.szCustomMapFile,p.iLives,p.mmMapType,p.iRandomEncounters,p.iMaxImbalance,p.fHe3Density,
        {double(p.nNeutralSectorMineableAsteroids),double(p.nPlayerSectorMineableAsteroids),double(p.nNeutralSectorSpecialAsteroids),double(p.nPlayerSectorSpecialAsteroids)},
        bool(p.bScoresCount),bool(p.bEjectPods),bool(p.bAllowFriendlyFire),bool(p.bAllowDefections),bool(p.bAllowJoiners),bool(p.bInvulnerableStations),bool(p.bAllowDevelopments),
        bool(p.bAllowShipyardPath),bool(p.bAllowTacticalPath),bool(p.bAllowExpansionPath),bool(p.bAllowSupremacyPath),bool(p.bAllowEmptyTeams),bool(p.bAllowAlliedRip),bool(p.bExperimental)};
}
struct MissionDetailLabels {std::string map,connectivity,resources,totalMoney,imbalance;};
inline std::vector<std::string> MissionDetailsParagraphs(const MissionDetails& d,const MissionDetailLabels& labels) {
    auto yes=[](bool value){return std::string(value?"yes":"no");};
    // Empty entries represent the original consecutive <p> tags. Preserve the
    // original field order and wording, including lives as allowed deaths.
    return {"Name: "+d.name,"","Server: "+d.server,"Core: "+d.core,"",
        "Lives: "+(d.lives==32767?std::string("Unlimited"):std::to_string(d.lives)),
        "Map Type: "+labels.map,"Map Connectivity: "+labels.connectivity,"Resources: "+labels.resources,
        "Total money: "+labels.totalMoney,"Scores Count: "+yes(d.scores),"Eject Pods: "+yes(d.pods),
        "Allow Friendly Fire: "+yes(d.friendlyFire),"Max Team Imbalance: "+labels.imbalance,
        "Allow Defections: "+yes(d.defections),"Allow Joiners: "+yes(d.joiners),
        "Invulnerable Stations: "+yes(d.invulnerable),"Developments: "+yes(d.developments),
        "Allow Shipyards: "+yes(d.shipyard),"Allow Tactical: "+yes(d.tactical),
        "Allow Expansion: "+yes(d.expansion),"Allow Supremacy: "+yes(d.supremacy),
        "Allow Empty Teams: "+yes(d.emptyTeams),"Allow Allied Ripcord: "+yes(d.alliedRipcord),
        "Experimental: "+yes(d.experimental),""};
}
}
