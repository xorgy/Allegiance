#include "network_arena.hpp"
#include "network_transport.hpp"
#include <cstdio>
#include <stdexcept>
using namespace Native::Net;
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
Frame fixture(){Frame f;for(int i=0;i<256;++i){Entity e;e.identity={uint32_t(i+1),0,int16_t(i)};
    e.motion.forward={0,0,-1};e.motion.up={0,1,0};e.motion.position.x=float(i);e.radius=1;e.hull=1;f.entities.push_back(e);}return f;}
void receive(SharedBytes bytes,std::map<uint64_t,std::vector<std::byte>>& history){
    Reader reader(*bytes);auto header=reader.take<Header>();auto arena=reader.take<ArenaRecord>();
    require(header.kind==Kind::Arena && arena.match==7,"Arena envelope identity");
    auto full=expanded(reader.rest(),history);require(!full.empty(),"Shared chain missing basis");
    auto decoded=frame(full);require(decoded.header.tick==header.tick && decoded.status.pilot==-1,"World leaked personal state");
    history[decoded.header.sequence]=std::move(full);
}
}
int main(){try{
    {ArenaEncoder encoder;bool rejected=false;
        try{encoder.publish(0,0,7,fixture());}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"Revision zero aliases the no-baseline sentinel");}
    struct Received {std::array<std::byte,3> bytes;int* releases;};
    int releases=0;Message saved;
    {
        auto* storage=new Received{{std::byte{1},std::byte{2},std::byte{3}},&releases};
        Message borrowed{17,storage->bytes,storage,[](const void* p){auto* s=static_cast<const Received*>(p);++*s->releases;delete s;}};
        saved=borrowed;
        Message moved=std::move(borrowed);
        require(moved.data.data()==storage->bytes.data() && borrowed.data.empty() && !releases,"Receive ownership move copied or released storage");
        std::vector<Message> messages;messages.push_back(std::move(moved));
        messages.emplace_back(18,std::vector<std::byte>{std::byte{4}});
        require(messages.front().data[2]==std::byte{3} && saved.data.data()!=storage->bytes.data(),"Receive move/copy lost storage independence");
    }
    require(releases==1 && saved.peer==17 && saved.data[2]==std::byte{3},"Received storage release or startup copy lifetime");
    ArenaEncoder encoder;auto capture=fixture();std::vector<std::shared_ptr<const ArenaRevision>> first(16);
    for(int recipient=0;recipient<1024;++recipient){int domain=recipient%16;
        auto revision=encoder.publish(domain,6,7,capture);
        if(!first[domain])first[domain]=revision;else require(first[domain]==revision,"Recipient re-encoded shared world");}
    require(encoder.encodes()==16,"Encode count scales with recipients");
    require(encoder.publish(0,12,7,capture)==first[0],"Unchanged arena generated another revision");
    std::map<uint64_t,std::vector<std::byte>> fast,slow;
    receive(first[0]->checkpointBytes,fast);receive(first[0]->checkpointBytes,slow);
    auto latest=encoder.publish(0x800000,6,7,capture);
    fast.clear();slow.clear();receive(latest->checkpointBytes,fast);receive(latest->checkpointBytes,slow);
    for(uint64_t tick=18;tick<=240;tick+=6){capture.entities[0].motion.position.y=float(tick);
        latest=encoder.publish(0x800000,tick,7,capture);receive(latest->delta,fast);
        // Only one cached encoding exists despite different receiver baselines.
        require(encoder.publish(0x800000,tick,7,capture)==latest,"Different ACK duplicated encoding");
        if(tick==48){for(const auto& block:latest->chain)receive(block.bytes,slow);
            require(fast.rbegin()->second==slow.rbegin()->second,"Lost reports changed reconstructed state");}
    }
    require(latest->checkpoint>=186 && latest->chainBase<latest->checkpoint,"Checkpoint forced healthy recipients to resynchronize");
    slow.clear();receive(latest->checkpointBytes,slow);
    for(const auto& block:latest->chain)if(block.revision>latest->checkpoint)receive(block.bytes,slow);
    require(fast.rbegin()->second==slow.rbegin()->second,"Checkpoint recovery diverged");
    auto state=encoder.publish(0,246,7,capture);std::map<uint64_t,std::vector<std::byte>> checkpointOnly;
    receive(state->checkpointBytes,checkpointOnly);
    for(uint64_t tick=252;tick<=300;tick+=6){capture.entities[0].motion.position.x=float(tick);state=encoder.publish(0,tick,7,capture);
        if(tick==264){auto jump=encoder.stateUpdate(*state,246);Reader r(*jump);r.take<Header>();r.take<ArenaRecord>();r.take<Header>();
            require(r.take<Delta>().baseline==246,"Repair retransmitted a full world with an available shared basis");receive(jump,checkpointOnly);}}
    require(state->chain.empty(),"Flight state retained a replay chain");
    auto repaired=encoder.stateUpdate(*state,checkpointOnly.rbegin()->first);
    require(encoder.stateUpdate(*state,checkpointOnly.rbegin()->first)==repaired,"Same baseline cohort encoded twice");
    receive(repaired,checkpointOnly);
    require(checkpointOnly.rbegin()->second==*state->raw,"Skipped flight states need intermediate reports");
    // Public trajectories are partitioned by stable identity, once per sector.
    // Removing one projectile must not re-encode any unaffected bucket.
    ArenaEncoder shots;Frame trajectories;
    for(uint32_t serial=1;serial<=96;++serial){Frame::ProjectileCapture shot;
        shot.ordinal=serial-1;shot.identity={serial,1,int16_t(serial)};
        shot.motion.position.x=float(serial);shot.sector=3;shot.radius=1;
        trajectories.capturedProjectiles.push_back(shot);}
    auto initial=shots.publishWorld(0x800003,6,7,trajectories);
    std::vector<std::shared_ptr<const ArenaRevision>> buckets(initial.begin(),initial.end());
    require(buckets.size()==projectileArenaBuckets && shots.encodes()==projectileArenaBuckets,"Projectile partition count");
    for(uint32_t i=0;i<projectileArenaBuckets;++i){
        require(buckets[i]->domain==(0x800300|i),"Projectile sector/bucket identity");
        auto entities=frame(*buckets[i]->raw).entities;require(entities.size()==96/projectileArenaBuckets,"Partition lost trajectories");
        for(const auto& e:entities)require(e.identity.serial%projectileArenaBuckets==i,"Trajectory entered wrong bucket");}
    trajectories.capturedProjectiles.erase(trajectories.capturedProjectiles.begin());
    auto changed=shots.publishWorld(0x800003,12,7,trajectories);
    require(shots.encodes()==projectileArenaBuckets+1,"Removal re-encoded unaffected trajectory buckets");
    for(uint32_t i=0;i<projectileArenaBuckets;++i)
        require((changed[i]==buckets[i])==(i!=1),"Partition revision changed outside affected bucket");
    for(uint32_t i=0;i<trajectories.capturedProjectiles.size();++i)trajectories.capturedProjectiles[i].ordinal=i;
    Frame prefix;Entity model;model.identity.serial=999;prefix.entities.push_back(model);
    std::array<const Frame*,2> components{&prefix,&trajectories};
    auto deferred=combineWorlds(components);require(deferred.capturedProjectiles.size()==95 && deferred.entities.size()==1,"Composition materialized trajectories prematurely");
    deferred.header.kind=Kind::Snapshot;auto materialized=frame(packet(deferred));require(materialized.entities.size()==96 && materialized.entities[1].identity.serial==2,"Deferred trajectory order lost");
    Frame strategic,physical;Entity oldModel,newModel;oldModel.identity={900,0,7};oldModel.flags=Strategic;oldModel.payloadBytes=1;oldModel.partCount=1;
    newModel=oldModel;newModel.identity.serial=901;newModel.flags=0;newModel.motion.velocity.x=37;
    strategic.entities={oldModel};strategic.payload={std::byte{1}};strategic.parts.resize(1);strategic.parts[0].ship=oldModel.identity;
    physical.entities={newModel};physical.payload={std::byte{2}};physical.parts.resize(1);physical.parts[0].ship=newModel.identity;
    std::array<const Frame*,2> overlay{&strategic,&physical};auto merged=combineWorlds(overlay);
    require(merged.entities.size()==1 && merged.entities[0].identity.serial==901 && merged.entities[0].motion.velocity.x==37 &&
            merged.payload==physical.payload && merged.parts[0].ship.serial==901,"New physical lifetime did not replace cached strategic identity and equipment");
    Frame common,personal;Event a,b,c;a.number=1;b.number=2;c.number=3;
    common.events={a,c};personal.events={b};personal.status.pilot=123;personal.header.kind=Kind::Snapshot;personal.header.sequence=99;personal.header.tick=240;
    Notice firstTip,secondTip,thirdTip;firstTip.number=1;secondTip.number=2;thirdTip.number=3;firstTip.flags=thirdTip.flags=NoticeTip|NoticeSharedTip;
    common.notices={firstTip,thirdTip};personal.notices={secondTip};
    auto composed=composeReport(common,personal);require(composed.events.size()==3 && composed.events[1].number==2,"Effect merge lost chronological order");
    require(composed.notices.size()==3 && composed.notices[0].number==1 && composed.notices[1].number==2 && composed.notices[2].number==3,"Common/private tips lose chronological order or are overwritten");
    Frame later;Notice fourth;fourth.number=4;later.notices={fourth};std::array<const Frame*,2> tips{&later,&common};
    auto combined=combineWorlds(tips);require(combined.notices.size()==3 && combined.notices[0].number==1 && combined.notices[2].number==4,"Shared tip arena composition loses records/order");
    Frame tipsOnly;tipsOnly.notices=common.notices;ArenaEncoder feedback;auto tipArena=feedback.publishWorld(0x300000,6,7,tipsOnly).front();auto encoded=feedback.encodes();
    for(int pilot=0;pilot<1024;++pilot)require(feedback.publishWorld(0x300000,6,7,tipsOnly).front()==tipArena,"Identical tip recipients do not share their encoded arena");
    require(feedback.encodes()==encoded,"Identical tip recipients re-encode a common report");
    auto malformed=composed;malformed.notices[0].flags|=NoticeChat;bool rejected=false;
    try{frame(packet(malformed));}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"Shared-tip metadata admits a private chat record");
    std::array<ArenaRef,2> references{{{0,0,240},{1,0,6}}};auto bytes=reportPacket(personal,references);
    Reader reader(bytes);require(reader.take<Header>().kind==Kind::Report,"Private envelope kind");
    require(reader.take<ArenaManifest>().count==2,"Manifest count");reader.array<ArenaRef>(2);
    auto decoded=frame(expanded(reader.rest(),{}));require(decoded.status.pilot==123 && decoded.entities.empty(),"Private envelope contains world copy");
    // A flying commander can view another sector while prior-round results
    // and current feedback remain available. All six nonprojectile domains
    // must coexist with the viewed sector's eight trajectory partitions.
    std::vector<ArenaRef> complete{{0,0,6},{0x400300,0,6},{0x400400,stateOnlyArena,6},
        {0x100000,0,6},{0x100001,0,6},{0x300000,0,6}};
    for(const auto& bucket:buckets)complete.push_back({bucket->domain,presentationArena,bucket->revision});
    auto fullReport=reportPacket(personal,complete);Reader fullReader(fullReport);fullReader.take<Header>();
    auto manifest=fullReader.take<ArenaManifest>();
    require(manifest.count==complete.size() && manifest.count<=maxReportArenas,"Complete remote-command report exceeds the client arena limit");
    fullReader.array<ArenaRef>(manifest.count);
    require(frame(expanded(fullReader.rest(),{})).status.pilot==123,"Maximum-arena report loses its private envelope");
    printf("Shared arenas: 1024 recipients, unchanged state, divergent receipts, chain repair, checkpoints and private effects passed\n");return 0;
}catch(const std::exception& e){fprintf(stderr,"Arena check: %s\n",e.what());return 1;}}
