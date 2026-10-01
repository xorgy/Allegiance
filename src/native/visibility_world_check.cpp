#include "game.hpp"
#include "test_fixture.hpp"
#include <unordered_map>
using NativeTest::require;
namespace {
struct World {
    Native::Assets assets;std::unordered_map<std::string,uint32_t> ids;Native::Game game;
    explicit World(const std::filesystem::path& path):assets(path),game(path,[&](auto& name,auto& skin){
        auto model=assets.model(name);auto [id,added]=ids.try_emplace(name+"/"+skin,ids.size()+1);return Native::ModelHandle{id->second,model->radius,model->frames,model->lights};
    },0,0,options()){}
    static Native::SessionOptions options(){Native::SessionOptions o;o.scenario=Native::Scenario::Conquest;o.enemies=0;o.startInHangar=true;return o;}
    void move(IclusterIGC* cluster,const Vector& position,const Vector& direction) {
        auto* p=game.playerShip();if(p->GetStation()){game.launch();for(int i=0;i<32;++i)game.step({},.1);}p->SetAutopilot(false);p->SetCluster(cluster);p->SetPosition(position);p->SetVelocity(Vector::GetZero());p->SetOrientation(Orientation(direction));
        p->SetBB(game.core()->GetLastUpdate(),game.core()->GetLastUpdate(),0);tick();
    }
    void tick(){for(int i=0;i<10;++i)game.step({},.01);}
    const Native::Contact* contact(ObjectType type,int id) {
        for(auto& c:game.contacts())if(c.type==type && c.id==id)return &c;return nullptr;
    }
    uint32_t drawnAt(const Vector& pos) {
        for(auto& item:game.scene(true))if(!item.background && glm::distance(glm::vec3(item.transform[3]),glm::vec3(pos.x,pos.y,pos.z))<.1f)return item.model;return 0;
    }
};
void station(World& world) {
    auto& game=world.game;auto* core=game.core();auto* side=game.playerShip()->GetSide();
    auto* base=core->GetSide(1)->GetStations()->first()->data();auto* sector=base->GetCluster();auto pos=base->GetPosition();
    // Keep this visibility test independent of random map asteroid occlusion.
    while(auto* l=sector->GetAsteroids()->first())l->data()->Terminate();
    world.move(sector,pos+Vector(0,0,base->GetRadius()+300),Vector(0,0,-1));
    require(base->GetCurrentEye(side),"native scanner sees opposing station");game.selectTarget(OT_station,base->GetObjectID());world.tick();
    auto* contact=world.contact(OT_station,base->GetObjectID());require(contact,"observed station in native HUD contacts");
    std::string name(contact->name),type=game.state().targetType,icon(contact->icon);float radius=contact->radius,hull=contact->hull,shield=contact->shield;
    auto model=world.drawnAt(pos);require(model && game.state().targetHasModel && game.state().targetModel.id==model,"world and target preview use observed station model");
    world.move(sector,pos+Vector(0,0,50000),Vector(0,0,1));require(!base->GetCurrentEye(side) && base->SeenBySide(side),"base remembered after leaving scanner range");
    IstationTypeIGC* upgrade=nullptr;for(auto* l=core->GetStationTypes()->first();l;l=l->next())if(std::string(l->data()->GetModelName())!=base->GetBaseStationType()->GetModelName()){upgrade=l->data();break;}
    require(upgrade,"different authored station type");base->SetBaseStationType(upgrade);base->SetFraction(.42f);base->SetShieldFraction(.27f);world.tick();
    contact=world.contact(OT_station,base->GetObjectID());
    require(contact && contact->name==name && contact->icon==icon && contact->radius==radius && contact->hull==hull && contact->shield==shield,"unseen upgrade preserves native HUD observation");
    require(world.drawnAt(pos)==model && game.state().targetModel.id==model && game.state().targetType==type && !game.state().hasLead,"unseen upgrade preserves world and preview; no live lead");
    auto orders=game.orderTargets(game.state().shipId,c_cidGoto);
    auto order=std::find_if(orders.begin(),orders.end(),[&](auto& o){return o.type==OT_station && o.id==base->GetObjectID();});
    require(order!=orders.end() && order->name==name,"remote order list preserves observed station name");
    world.move(sector,pos+Vector(0,0,base->GetRadius()+300),Vector(0,0,-1));
    require(base->GetCurrentEye(side) && world.drawnAt(pos)!=model && game.state().targetType!=type,"reacquisition updates station model and type");
    world.move(sector,pos+Vector(0,0,50000),Vector(0,0,1));
    core->GetIgcSite()->CaptureStationEvent(game.playerShip(),base);world.tick();
    require(world.contact(OT_station,base->GetObjectID()) && !world.contact(OT_station,base->GetObjectID())->enemy,"public capture changes native ownership");
    int id=base->GetObjectID();core->GetIgcSite()->KillStationEvent(base,nullptr,0);world.tick();
    require(!world.contact(OT_station,id) && !world.drawnAt(pos) && game.state().target.empty(),"public destruction retires station HUD, target and draw");
}
void construction(World& world,bool oldSiteOnly) {
    auto& game=world.game;auto* core=game.core();auto* side=game.playerShip()->GetSide();
    IclusterIGC* sector=nullptr;for(auto* l=core->GetClusters()->first();l;l=l->next())if(!l->data()->GetHomeSector()){sector=l->data();break;}
    require(sector && sector->GetAsteroids()->first(),"neutral construction sector");
    TRef<IasteroidIGC> rock=sector->GetAsteroids()->first()->data();auto pos=rock->GetPosition();int rockId=rock->GetObjectID();float rockRadius=rock->GetRadius();
    // Isolate the retained rock from random neighboring occluders.
    for(auto* l=sector->GetAsteroids()->first();l;){auto* next=l->next();if(l->data()!=rock)l->data()->Terminate();l=next;}
    world.move(sector,pos+Vector(0,0,rock->GetRadius()+200),Vector(0,0,-1));
    require(rock->SeenBySide(side) && world.contact(OT_asteroid,rockId),"native discovers build asteroid");
    game.selectTarget(OT_asteroid,rockId);
    std::string name(world.contact(OT_asteroid,rockId)->name);auto model=world.drawnAt(pos);require(model,"native asteroid draw");
    world.move(sector,pos+Vector(0,0,50000),Vector(0,0,1));
    require(!rock->GetAsteroidCurrentEye(side),"asteroid out of current sight");
    bool seen[c_cSidesMax]{};auto* type=core->GetSide(1)->GetStations()->first()->data()->GetBaseStationType();
    core->GetIgcSite()->BuildStation(rock,core->GetSide(1),type,core->GetLastUpdate(),seen);world.tick();
    auto* base=sector->GetStations()->first()->data();require(base && !base->SeenBySide(side),"unseen construction station concealed");
    require(world.contact(OT_asteroid,rockId) && world.contact(OT_asteroid,rockId)->name==name && world.drawnAt(pos)==model,"native retains asteroid contact and draw after unseen construction");
    require(!core->GetAsteroid(rockId) && !world.contact(OT_station,base->GetObjectID()),"retained asteroid is presentation only, replacement not leaked");
    require(game.state().targetId==rockId && game.state().target==name,"unseen construction preserves existing selection");
    game.selectTarget(OT_asteroid,rockId);
    require(game.state().targetId==rockId && game.state().targetObjectType==OT_asteroid && game.state().target==name && game.state().targetModel.id==model && !game.state().hasLead,"retained rock supports HUD selection and preview without a live lead");
    game.toggleAutopilot();require(game.state().autopilot,"retained rock supports original waypoint navigation");
    // Scan the old asteroid's signature while the station is below detection.
    if(oldSiteOnly){base->SetSignature(0);base->SetRadius(1);}
    world.move(sector,pos+Vector(0,0,rockRadius+100),Vector(0,0,-1));
    require(base->SeenBySide(side)!=oldSiteOnly,"station discovery respects actual signature");
    require(!world.contact(OT_asteroid,rockId) && game.state().target.empty(),"discovery clears retained contact and selected target");
    require(oldSiteOnly?!world.drawnAt(pos):world.drawnAt(pos)!=model,"stale draw removed or replaced by discovered station");
}
void transient(World& world) {
    auto& game=world.game;auto* core=game.core();auto* player=game.playerShip();
    auto* sector=core->GetSide(0)->GetStations()->first()->data()->GetCluster();
    world.move(sector,Vector(0,0,50000),Vector(0,0,1));
    auto* type=core->GetProjectileTypes()->first()->data();
    DataProjectileIGC data{};data.projectileTypeID=type->GetObjectID();data.forward=Vector(0,0,1);data.lifespan=10;
    auto* projectile=static_cast<IprojectileIGC*>(core->CreateObject(core->GetLastUpdate(),OT_projectile,&data,sizeof(data)));
    require(projectile,"native transient projectile creation");
    projectile->SetLauncher(player);projectile->SetPosition(Vector(0,0,-50000));projectile->SetCluster(sector);
    require(!(projectile->GetAttributes()&c_mtSeenBySide) && !player->InScannerRange(projectile),"transient outside individual scanner range");
    for(auto* l=core->GetSides()->first();l;l=l->next())
        require(projectile->GetCurrentEye(l->data()) && projectile->SeenBySide(l->data()),"untracked transient retains original always-visible current eye");
    projectile->Terminate();projectile->Release();
}
}
int main(int argc,char** argv) {
    try {auto artwork=argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork();{World world(artwork);station(world);}{World world(artwork);construction(world,true);}{World world(artwork);construction(world,false);}{World world(artwork);transient(world);}puts("Native HUD, target preview, world draws and order lists preserve historical visibility; stale asteroid and transient lifecycle passed");return 0;}
    catch(const std::exception& e){fprintf(stderr,"world visibility: %s\n",e.what());return 1;}
}
