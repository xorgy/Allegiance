#include "game.hpp"
#include "test_fixture.hpp"
#include "../Igc/shipIGC.h"
#include <unordered_map>
using namespace Native;
using NativeTest::require;
namespace {
struct World {
    Assets assets;
    std::unordered_map<std::string, std::shared_ptr<ModelData>> models;
    ModelLoader loader;
    SessionOptions options;
    std::unique_ptr<Game> game;
    explicit World(const std::filesystem::path &art,int limit=4): assets(art) {
        loader = [this](const std::string &name, const std::string &) {
            auto &m = models[name]; if (!m) m = assets.model(name);
            return ModelHandle{1,m->radius,m->frames,m->lights};
        };
        options.scenario = Scenario::Conquest; options.startInHangar = true;
        options.enemies = 0; options.rules.miners = 0;
        options.rules.maxDrones=limit;
        options.rules.showMap = options.rules.showHome = true; options.seed = 47;
        game = std::make_unique<Game>(art,loader,0,0,options,EffectLoader{},GameRole::Authority);
    }
};
IdroneTypeIGC *minerType(ImissionIGC *core,IstationIGC *home) {
    for(auto*l=core->GetDroneTypes()->first();l;l=l->next())
        if(l->data()->GetPilotType()==c_ptMiner && home->CanBuy(l->data()))return l->data();
    throw std::runtime_error("authored miner type");
}
IstationIGC *station(World &w,IstationTypeIGC *type,IclusterIGC *cluster,Vector position) {
    auto *core=w.game->core(); DataStationIGC d{};
    d.stationID=core->GenerateNewStationID();d.stationTypeID=type->GetObjectID();
    d.sideID=w.game->playerShip()->GetSide()->GetObjectID();d.clusterID=cluster->GetObjectID();
    d.position=position;d.forward=Vector(0,0,1);d.up=Vector(0,1,0);d.bpHull=d.bpShield=1;
    snprintf(d.name,sizeof(d.name),"Local %s",type->GetName());
    auto *s=static_cast<IstationIGC *>(core->CreateObject(core->GetLastUpdate(),OT_station,&d,sizeof(d)));
    require(s,"local production/unload station");s->Release();return s;
}
void commandCamera(const std::filesystem::path &art) {
    World w(art);w.game->launch();for(int i=0;i<100;++i)w.game->authorityStep({},.1);
    Game replica(art,w.loader,0,0,w.options,EffectLoader{},GameRole::Replica);
    auto publish=[&](uint64_t tick){replica.applyNetworkFrame(w.game->networkFrame(w.game->playerShip()->GetObjectID(),tick,0));};
    publish(600);replica.commandView(true);replica.presentPrediction(0,{},0);
    CameraInput input;input.panRight=.15f;
    const auto first=replica.camera(input);
    for(int i=1;i<=330;++i) {
        if(i%16==0)publish(600+uint64_t(i)*60/165);
        replica.presentPrediction(0,{},1./165);replica.camera(input);
    }
    auto last=replica.camera(input);
    require(glm::length(last.position-first.position)>1500,"network command camera pans continuously through unchanged sector publications");
    input.panRight=0;input.zoom=-.1f;
    auto before=last.position;
    for(int i=1;i<=165;++i){if(i%16==0)publish(720+uint64_t(i)*60/165);replica.presentPrediction(0,{},1./165);replica.camera(input);}
    require(glm::length(replica.camera(input).position-before)>500,"network command camera zoom survives publications");
    puts("Command view: continuous pilot-seat pan/zoom across snapshots passes.");
}
void mining(const std::filesystem::path &art,bool refinery) {
    World w(art);auto *core=w.game->core();auto *pilot=w.game->playerShip();
    auto *home=pilot->GetStation();auto *type=minerType(core,home);
    auto *side=pilot->GetSide();auto *cluster=core->GetClusters()->last()->data();
    // Use a neutral sector so the old home base is genuinely a remote option.
    for(auto*l=core->GetClusters()->first();l;l=l->next())if(!l->data()->GetHomeSector()){cluster=l->data();break;}
    IstationTypeIGC *baseType=home->GetBaseStationType();
    if(refinery) {
        baseType=nullptr;
        for(auto*l=core->GetStationTypes()->first();l;l=l->next())
            if(l->data()->HasCapability(c_sabmUnload) && l->data()->HasCapability(c_sabmTeleportUnload)){baseType=l->data();break;}
        require(baseType,"authored teleport-unload refinery");
    }
    auto *local=station(w,baseType,cluster,Vector(1500,0,0));
    IasteroidIGC *rock=nullptr;
    for(auto*l=cluster->GetAsteroids()->first();l;l=l->next())if(l->data()->HasCapability(c_aabmMineHe3)){rock=l->data();break;}
    require(rock,"neutral He3 asteroid");rock->SetPosition(Vector(0,0,0));rock->SetOre(5000);
    core->GetIgcSite()->DroneTypeCompleted(nullptr,home,type,core->GetLastUpdate());
    IshipIGC *miner=nullptr;
    for(auto*l=side->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()==c_ptMiner)miner=l->data();
    require(miner,"produced miner");miner->SetCluster(cluster);
    miner->SetPosition(Vector(rock->GetRadius()+miner->GetRadius()+5,0,0));
    miner->SetVelocity(Vector::GetZero());miner->SetCommand(c_cmdAccepted,rock,c_cidMine);
    // Off-sector strategic reports carry the roster and orders, not physical
    // resources. Inspect the mining sector through the same command-view path
    // used by a commander before comparing the full miner state.
    w.game->commandView(true);
    require(w.game->viewSector(cluster->GetObjectID()),"command view observes mining sector");
    Game replica(art,w.loader,0,0,w.options,EffectLoader{},GameRole::Replica);
    int deliveries=0,miningTicks=0,dockedTicks=0;float ore=0;
    for(int i=0;i<12000;++i) {
        w.game->authorityStep({},.1);
        miningTicks+=(miner->GetStateM()&miningMaskIGC)!=0;dockedTicks+=miner->GetStation()!=nullptr;
        if(ore>0 && miner->GetOre()==0)++deliveries;ore=miner->GetOre();
        if(miner->GetOre()>0 && miner->GetCommandID(c_cmdPlan)==c_cidGoto)
            require(miner->GetCommandTarget(c_cmdPlan)==local,"miner chooses local unload base over original garrison");
        if(i%10==0) {
            replica.applyNetworkFrame(w.game->networkFrame(pilot->GetObjectID(),uint64_t(i+1)*6,0));
            auto *copy=replica.core()->GetShip(miner->GetObjectID());
            require(copy && copy->GetOre()==miner->GetOre() && copy->GetStateM()==miner->GetStateM(),"miner ore/activity replicated exactly");
        }
    }
    printf("Mining at %s: %d deliveries, %d mining ticks, %d docked ticks, %d credits.\n",local->GetName(),deliveries,miningTicks,dockedTicks,w.game->state().minedCredits);
    require(deliveries>=5 && w.game->state().minedCredits>0,"miner keeps mining/unloading/relaunching for twenty simulation minutes");
    require(refinery?dockedTicks==0:dockedTicks>0,"refinery unloads without docking; garrison applies dock dwell");
}
void limits(const std::filesystem::path &art) {
    for(int limit:{4,10}) {
        World w(art,limit);auto *core=w.game->core();auto *home=w.game->playerShip()->GetStation();
        auto *type=minerType(core,home);IshipIGC *last=nullptr;
        for(int i=0;i<limit;++i) {
            core->GetIgcSite()->DroneTypeCompleted(nullptr,home,type,core->GetLastUpdate());
            for(auto*l=home->GetSide()->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()==c_ptMiner)last=l->data();
        }
        w.game->authorityStep({},.1);
        auto row=[&](const Game &game)->const ResearchState & {
            for(auto&r:game.state().research)if(r.type==OT_droneType && r.name==type->GetName())return r;
            throw std::runtime_error("miner production row");
        };
        require(row(*w.game).count==limit && !row(*w.game).available,"original per-type drone cap enforced");
        Game replica(art,w.loader,0,0,w.options,EffectLoader{},GameRole::Replica);
        replica.applyNetworkFrame(w.game->networkFrame(w.game->playerShip()->GetObjectID(),6,0));
        require(row(replica).count==limit && row(replica).limit==limit && !row(replica).available,"replica reports the same drone cap and count");
        last->Terminate();w.game->authorityStep({},.1);
        replica.applyNetworkFrame(w.game->networkFrame(w.game->playerShip()->GetObjectID(),12,0));
        require(row(replica).count==limit-1 && row(replica).available,"retired miner frees production capacity on both ends");
        printf("Production: %d-miner match limit, replica counts and replacement capacity pass.\n",limit);
    }
}
void depletion(const std::filesystem::path &art) {
    World w(art);auto *core=w.game->core();auto *home=w.game->playerShip()->GetStation();
    auto *side=home->GetSide();auto *type=minerType(core,home);
    auto regeneration=core->GetFloatConstant(c_fcidHe3Regeneration);core->SetFloatConstant(c_fcidHe3Regeneration,0);
    for(auto*l=core->GetAsteroids()->first();l;l=l->next())if(l->data()->HasCapability(c_aabmMineHe3))l->data()->SetOre(0);
    core->GetIgcSite()->DroneTypeCompleted(nullptr,home,type,core->GetLastUpdate());
    IshipIGC *miner=nullptr;
    for(auto*l=side->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()==c_ptMiner)miner=l->data();
    require(miner,"empty-world miner");
    for(int i=0;i<3000;++i)w.game->authorityStep({},.1);
    require(miner->GetStation()==home && miner->GetAutopilot() && miner->GetOre()==0,"depleted resource world causes original docked standby, not lost autopilot");
    auto message=[&](std::string_view text){for(auto&m:w.game->state().messages)if(m.text.find(text)!=m.text.npos)return true;return false;};
    require(message("He3 asteroid"),"miner reports missing mining work to commander");
    IasteroidIGC *rock=nullptr;
    for(auto*l=home->GetCluster()->GetAsteroids()->first();l;l=l->next())if(l->data()->HasCapability(c_aabmMineHe3)){rock=l->data();break;}
    require(rock,"recoverable home asteroid");rock->SetOre(1000);rock->SetPosition(home->GetPosition()+Vector(2500,0,0));rock->SetSideVisibility(side,true);
    for(int i=0;i<1500 && !(miner->GetStateM()&miningMaskIGC);++i)w.game->authorityStep({},.1);
    require(miner->GetStateM()&miningMaskIGC,"idle miner resumes when usable known ore becomes available");
    require(w.game->droneStandby(miner->GetObjectID(),true),"explicit return and stay docked");
    for(int i=0;i<2000 && !miner->GetStation();++i)w.game->authorityStep({},.1);
    require(miner->GetStation()==home && miner->GetStayDocked(),"return order docks at repair base");
    for(int i=0;i<300;++i)w.game->authorityStep({},.1);
    require(miner->GetStation()==home,"explicit stay prevents ten-second automatic relaunch");
    require(w.game->droneStandby(miner->GetObjectID(),false),"resume original automatic duties");
    for(int i=0;i<1500 && !(miner->GetStateM()&miningMaskIGC);++i)w.game->authorityStep({},.1);
    require((miner->GetStateM()&miningMaskIGC) && !miner->GetStayDocked(),"resumed miner launches and mines again");
    core->SetFloatConstant(c_fcidHe3Regeneration,regeneration);
    puts("Mining: depletion/notification/resource recovery and explicit docked standby/resume pass.");
}
void productionOrigin(const std::filesystem::path &art) {
    World w(art);auto *core=w.game->core();auto *pilot=w.game->playerShip();
    auto *home=pilot->GetStation();IclusterIGC *remote=nullptr;
    for(auto*l=core->GetClusters()->first();l;l=l->next())if(!l->data()->GetHomeSector()){remote=l->data();break;}
    require(remote,"remote production sector");
    auto *local=station(w,home->GetBaseStationType(),remote,Vector(1800,0,0));
    pilot->SetStation(local);
    for(ObjectType type:{OT_droneType,OT_stationType}) {
        IbucketIGC *bucket=nullptr;
        for(auto*l=pilot->GetSide()->GetBuckets()->first();l;l=l->next())
            if(l->data()->GetBucketType()==type && local->CanBuy(l->data()->GetBuyable())){bucket=l->data();break;}
        require(bucket,"production bucket compatible with both bases");
        auto *buyable=bucket->GetBuyable();IstationIGC *origin=nullptr;
        for(auto*l=pilot->GetSide()->GetStations()->first();l;l=l->next())if(l->data()->CanBuy(buyable)){origin=l->data();break;}
        require(origin==home,"original bucket selects first compatible base");
        bucket->ForceComplete(core->GetLastUpdate());
        IshipIGC *produced=nullptr;
        for(auto*l=pilot->GetSide()->GetShips()->first();l;l=l->next())if(l->data()->GetPilotType()<c_ptPlayer)produced=l->data();
        require(produced && produced->GetCluster()==home->GetCluster(),"production origin remains original bucket-selected base when pilot occupies another base");
        if(type==OT_stationType) {
            require(produced->GetBaseData()==buyable,"constructor preserves its ordered station blueprint");
            require(produced->GetCommandID(c_cmdAccepted)!=c_cidBuild && produced->GetCommandID(c_cmdPlan)!=c_cidBuild,"new constructor never automatically builds");
            require(std::string_view(produced->GetName()).starts_with(static_cast<IstationTypeIGC *>(buyable)->GetBuilderName()),"constructor has original blueprint-specific name");
        }
        produced->Terminate();
    }
    puts("Production: original compatible-base selection, constructor blueprint/name and manual build authorization pass.");
}
}
int main(int argc,char**argv) {
    try {auto art=argc>1?std::filesystem::path(argv[1]):Assets::steamArtwork();commandCamera(art);limits(art);productionOrigin(art);depletion(art);mining(art,false);mining(art,true);return 0;}
    catch(const std::exception&e){fprintf(stderr,"drones: %s\n",e.what());return 1;}
}
