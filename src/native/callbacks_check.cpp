#include "test_fixture.hpp"
#include "game.hpp"
#include "../ClientCore/ShipSound.h"
#include "../ClientCore/ClusterWarnings.h"
#include "test_reference/cluster_warnings.hpp"
#include "test_reference/client_sound_events.hpp"
#include "../ClientCore/ClientSoundEvents.h"
#include <algorithm>

using NativeTest::require;
namespace {
void soundReference() {
    int cases=0;
    for(bool starting:{false,true}) {
        Allegiance::CountdownSounds sounds;
        for(int q=4000;q>=-16;--q) {
            double left=q*.25;int seconds=std::max(0,starting?int(left)+1:int(left+1));
            static_cast<void>(sounds.Observe(left,starting)); // repeated observations cannot repeat a cue
            require(sounds.Observe(left,starting)==NA,"countdown repeated speech at the same second");
            sounds.Reset();require(sounds.Observe(left,starting)==OriginalClientSounds::countdown(seconds),"original countdown threshold/truncation differs");++cases;
        }
    }
    for(int voice:std::array<int,2>{NA,voAffirmativeSound})for(int mask=0;mask<64;++mask) {
        bool command=mask&1,echo=mask&2,personal=mask&4,training=mask&8,connected=mask&16,leader=mask&32;
        require(Allegiance::ChatSound(voice,command,echo,personal,training,connected,leader)==OriginalClientSounds::chat(voice,command,echo,personal,training,connected,leader),"original voice/command/training/commander chat selection differs");++cases;
    }
    printf("%d frozen original countdown/chat sound comparisons passed.\n",cases);
}
void warningReference() {
    struct Trace {std::vector<int> events;std::string text;bool operator==(const Trace&)const=default;};
    unsigned cases=0;
    for(bool invulnerable:{false,true})for(bool inGame:{false,true})for(WingID wing:{0,2})
    for(int current=0;current<32768;++current) {
        AssetMask previous=AssetMask((current*109+101)&32767);
        auto run=[&](bool original) {
            Trace trace;
            auto threat=[&](bool critical,const char* text){trace.events.push_back(critical?1:2);trace.text=text;};
            auto secured=[&]{trace.events.push_back(3);};auto lost=[&]{trace.events.push_back(4);};
            auto sound=[&](SoundID sound){trace.events.push_back(1000+sound);};
            if(original)OriginalWarnings::changes(previous,AssetMask(current),inGame,invulnerable,wing,threat,secured,lost,sound);
            else Allegiance::ClusterWarningChanges(previous,AssetMask(current),inGame,invulnerable,wing,threat,secured,lost,sound);
            return trace;
        };
        require(run(false)==run(true),"original sector warning text, priority, sounds or order differs");++cases;
    }
    printf("%u frozen original sector warning mask/wing/mission comparisons passed.\n",cases);
}
void callbacks(const std::filesystem::path& art) {
    Native::Assets assets(art);Native::SessionOptions options;
    options.scenario=Native::Scenario::FreeFlight;options.map=3;options.enemies=0;
    Native::Game game(art,[&](auto& name,auto&){auto m=assets.model(name);return Native::ModelHandle{1,m->radius,m->frames,m->lights};},0,0,options,
        [](const std::string&){return Native::EffectTexture{19,64,64,1};});
    auto* core=game.core();auto* ship=game.playerShip();auto* first=ship->GetCluster();
    auto* next=core->GetClusters()->last()->data();require(next!=first,"sector transfer fixture");
    auto buoy=[&](IclusterIGC* cluster,BuoyType type) {
        DataBuoyIGC data{};data.clusterID=cluster->GetObjectID();data.buoyID=core->GenerateNewBuoyID();data.type=type;data.visible=true;
        auto* b=static_cast<IbuoyIGC*>(core->CreateObject(core->GetLastUpdate(),OT_buoy,&data,sizeof(data)));
        require(b,"sector buoy fixture");TRef<IbuoyIGC> hold=b;b->Release();return hold;
    };
    // Arrival clears all four slots, while a different sector and a waypoint
    // in the arrival sector retain their distinct meanings.
    auto arrival=buoy(next,c_buoyCluster),remote=buoy(first,c_buoyCluster),waypoint=buoy(next,c_buoyWaypoint);
    for(Command slot=0;slot<c_cmdMax;++slot)ship->SetCommand(slot,arrival,c_cidGoto);
    ship->SetStateBits(afterburnerButtonIGC,afterburnerButtonIGC);ship->SetCluster(next);
    for(Command slot=0;slot<c_cmdMax;++slot)require(!ship->GetCommandTarget(slot) && ship->GetCommandID(slot)==c_cidNone,"arrived sector order not cleared");
    require(!(ship->GetStateM()&afterburnerButtonIGC),"sector transition keeps afterburner active");
    ship->SetCommand(c_cmdQueued,remote,c_cidGoto);ship->SetCommand(c_cmdAccepted,waypoint,c_cidGoto);ship->SetCluster(next);
    require(ship->GetCommandTarget(c_cmdQueued)==remote && ship->GetCommandTarget(c_cmdAccepted)==waypoint,"arrival cleared an unrelated order");
    game.step({},.001);require(game.state().sectorId==next->GetObjectID() && game.state().target.empty(),"arrival left the old sector/target snapshot");
    // The authored probe readiness timer must reach the native adapter, not a
    // synthetic UI timer. Own and enemy destruction use different priority.
    IprobeTypeIGC* probeType=nullptr;IchaffTypeIGC* chaffType=nullptr;
    for(auto* l=core->GetExpendableTypes()->first();l;l=l->next()) {
        if(l->data()->GetObjectType()==OT_probeType) {
            auto* p=static_cast<IprobeTypeIGC*>(l->data());
            if(p->GetRipcordDelay()>=0)probeType=p;
        } else if(l->data()->GetObjectType()==OT_chaffType)chaffType=static_cast<IchaffTypeIGC*>(l->data());
    }
    require(probeType && chaffType,"authored ripcord probe and chaff definitions");
    for(int side:{0,1}) {
        game.clearMessages();game.clearSounds();DataProbeIGC data{};
        data.pprobetype=probeType;data.pside=core->GetSide(side);data.pcluster=next;
        data.p0=ship->GetPosition()+Vector(200,0,0);data.probeID=core->GenerateNewProbeID();
        data.time0=core->GetLastUpdate()-probeType->GetRipcordDelay()+.15f;
        auto* probe=static_cast<IprobeIGC*>(core->CreateObject(core->GetLastUpdate(),OT_probe,&data,sizeof(data)));
        require(probe && std::none_of(game.state().messages.begin(),game.state().messages.end(),[](auto& m){return m.text.find(" active in ")!=std::string::npos;}),"probe announced active before its readiness timer");
        require(std::any_of(game.state().messages.begin(),game.state().messages.end(),[&](auto& m){return m.warning && m.text.find(side==0?"deployed in":"spotted in")!=std::string::npos;}),"first probe discovery notification missing");
        auto sighting=std::find_if(game.state().messages.begin(),game.state().messages.end(),[](auto& m){return m.text.find("spotted in")!=std::string::npos;});
        if(side==1)require(sighting!=game.state().messages.end() && sighting->text.starts_with(std::string("\x81 ")+static_cast<const char*>(ConvertColorToString(data.pside->GetColor()))) && sighting->text.find("\x82  spotted in ")!=std::string::npos,"enemy probe sighting loses original faction color/reset");
        for(int i=0;i<3;++i)game.step({},.1);
        require(std::any_of(game.state().messages.begin(),game.state().messages.end(),[](auto& m){return m.warning && m.text.find(" active in ")!=std::string::npos;}),"probe activation notification missing");
        require(std::any_of(game.sounds().begin(),game.sounds().end(),[](auto& s){return s.id==newCriticalMsgSound;}),"probe activation lost original notification sound");
        game.clearSounds();probe->Terminate();probe->Release();
        require(game.state().messages.back().text.find("was destroyed")!=std::string::npos && game.state().messages.back().warning==(side==0),"probe destruction text/priority mismatch");
        require(!game.sounds().empty() && game.sounds().front().id==(side==0?newCriticalMsgSound:newNonCriticalMsgSound),"probe destruction notification sound mismatch");
    }
    // Original CchaffIGC calls ThingSite::Spin at 2.75 radians/second.
    DataChaffIGC data{};data.pchafftype=chaffType;data.pcluster=next;data.time0=core->GetLastUpdate();data.p0=ship->GetPosition()+Vector(0,0,200);data.v0=Vector::GetZero();
    auto* chaff=static_cast<IchaffIGC*>(core->CreateObject(core->GetLastUpdate(),OT_chaff,&data,sizeof(data)));require(chaff,"chaff fixture");
    game.step({},.1);game.camera({});auto draws=game.particles();
    auto pos=chaff->GetPosition();glm::vec3 center{pos.x,pos.y,pos.z};
    auto draw=std::find_if(draws.begin(),draws.end(),[&](auto& d){return glm::distance(glm::vec3(d.centerSize),center)<.001f;});
    if(draw==draws.end() || std::abs(draw->angle-.275f)>=.002f)fprintf(stderr,"chaff fixture: %zu sprites, found %d, angle %.6f, position %.3f %.3f %.3f\n",draws.size(),draw!=draws.end(),draw==draws.end()?0:draw->angle,center.x,center.y,center.z);
    require(draw!=draws.end() && std::abs(draw->angle-.275f)<.002f,"real IGC chaff rotation missing from the Vulkan sprite");
    chaff->Terminate();chaff->Release();
    arrival=nullptr;remote=nullptr;waypoint=nullptr;
    puts("Native sector arrival, unrelated order retention, probe timer/notifications and authored chaff rotation passed.");
}
}
int main(int argc,char** argv) {
    try{soundReference();warningReference();callbacks(argc>1?std::filesystem::path(argv[1]):Native::Assets::steamArtwork());return 0;}
    catch(const std::exception& e){fprintf(stderr,"callbacks: %s\n",e.what());return 1;}
}
