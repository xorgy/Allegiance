#include "test_fixture.hpp"
#include "../ClientCore/MissionRules.h"
using NativeTest::require;
int main(int argc,char** argv) {
    try {
        NativeTest::Fixture f(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());
        auto* mission=f.mission;
        auto* type=mission->GetStations()->first()->data()->GetBaseStationType();
        require(type->HasCapability(c_sabmFlag|c_sabmRestart),"fixture restart/flag base");
        auto* cluster=mission->GetClusters()->first()->data();
        while(auto* l=mission->GetStations()->first())l->data()->Terminate();
        auto station=[&](SideID side,IclusterIGC* sector) {
            DataStationIGC data{};data.stationID=mission->GenerateNewStationID();data.stationTypeID=type->GetObjectID();
            data.sideID=side;data.clusterID=sector->GetObjectID();data.bpHull=data.bpShield=1;
            data.forward=Vector(0,0,1);data.up=Vector(0,1,0);data.rotation.axis(data.forward);
            auto* st=static_cast<IstationIGC*>(mission->CreateObject(mission->GetLastUpdate(),OT_station,&data,sizeof(data)));
            require(st,"create victory fixture station");st->Release();return st;
        };
        for(auto* l=mission->GetSides()->first();l;l=l->next()){l->data()->SetActiveF(l->data()->GetObjectID()<2);l->data()->SetConquestPercent(0);}
        auto* first=mission->GetSide(0);auto* second=mission->GetSide(1);
        auto* a=station(0,cluster);station(0,cluster);auto* b=station(1,cluster);
        auto params=*mission->GetMissionParams();params.iGoalConquestPercentage=75;params.iGoalTerritoryPercentage=100;
        mission->SetMissionParams(&params);
        auto v=Allegiance::CheckStationVictory(mission,nullptr,nullptr);
        require(!v.winner && first->GetConquestPercent()==66 && second->GetConquestPercent()==33,"conquest uses truncated flag station shares");
        station(0,cluster);v=Allegiance::CheckStationVictory(mission,nullptr,nullptr);
        require(v.changed && v.winner==first && first->GetConquestPercent()==75,"new base reaches conquest threshold");
        require(!Allegiance::CheckStationVictory(mission,nullptr,nullptr).winner,"unchanged rankings do not reissue victory");
        first->SetConquestPercent(0);second->SetActiveF(false);
        require(!Allegiance::CheckStationVictory(mission,nullptr,nullptr).winner,"allow-empty-teams suppresses lone active conquest win");
        second->SetActiveF(true);params.iGoalConquestPercentage=0;mission->SetMissionParams(&params);
        v=Allegiance::CheckStationVictory(mission,b,second);
        require(v.eliminated==second && !v.winner,"last restart base excluded before termination eliminates side");
        require(!Allegiance::AlliedSurvivor(mission),"opposing active teams prevent survivor win");
        second->SetActiveF(false);require(Allegiance::AlliedSurvivor(mission)==first,"last active side wins elimination");
        second->SetActiveF(true);first->SetAllies(0);second->SetAllies(0);
        require(Allegiance::AlliedSurvivor(mission)==second,"surviving alliance wins with original representative order");
        first->SetAllies(NA);second->SetAllies(NA);
        params.iGoalTerritoryPercentage=99;mission->SetMissionParams(&params);
        v=Allegiance::CheckStationVictory(mission,nullptr,nullptr);
        require(!v.winner && first->GetTerritoryCount()==0,"contested sector counts for neither side");
        b->SetSide(first);
        for(auto* l=mission->GetClusters()->first();l;l=l->next())if(l->data()!=cluster)station(0,l->data());
        v=Allegiance::CheckStationVictory(mission,nullptr,nullptr);
        require(v.winner==first && first->GetTerritoryCount()==mission->GetClusters()->n(),"uncontested sector ownership meets territory goal");
        printf("Mission rules: conquest, territory, base elimination, empty teams and alliances passed.\n");
        return 0;
    }catch(const std::exception& e){fprintf(stderr,"mission rules: %s\n",e.what());return 1;}
}
