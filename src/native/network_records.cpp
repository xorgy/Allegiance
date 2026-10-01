#include "network_records.hpp"
#include "network_build.hpp"
#include "frame_timing.hpp"
#include "crypto.hpp"
#include "../ClientCore/Ballot.h"
#include <algorithm>
#include <bitset>
#include <fstream>
#include <memory>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
namespace Native::Net {
Session session(const SessionOptions &o) {
    Session s{};
    text(s.name,o.name);
    text(s.coreFile,sessionCoreFile(o));
    text(s.customMap,o.customMap);
    s.rules = o.rules;
    s.civilizations = o.teamCivilizations;
    for(size_t i=0;i<std::size(s.teamNames);++i)text(s.teamNames[i],o.teamNames[i]);
    s.alliances = o.alliances;
    s.teamPolicy=o.inactiveTeams|(o.requireTeamPlayers?0x40:0);
    if(std::any_of(s.alliances.begin(),s.alliances.begin()+std::clamp(o.rules.teams,0,6),[](char alliance){return alliance>=0;})){s.rules.allowDefections=true;s.rules.maxImbalance=32767;}
    s.seed = o.seed;
    s.scenario = int(o.scenario);
    s.map = o.map;
    s.hull = o.hull;
    s.enemies = o.enemies;
    s.allies = o.allies;
    s.difficulty = o.difficulty;
    s.killLimit = o.killLimit;
    s.civilization = o.civilization;
    return s;
}
SessionOptions options(const Session &s, int side, std::string_view name) {
    SessionOptions o;
    if(!terminated(s.name))throw std::runtime_error("Invalid game name");
    o.name=s.name;
    if(!terminated(s.coreFile) || !Allegiance::ValidMapFile(s.coreFile) || !s.coreFile[0])throw std::runtime_error("Invalid game core filename");o.coreFile=s.coreFile;
    if(!terminated(s.customMap) || !Allegiance::ValidMapFile(s.customMap))throw std::runtime_error("Invalid custom map filename");o.customMap=s.customMap;
    o.rules = s.rules;
    o.teamCivilizations = s.civilizations;
    for(size_t i=0;i<std::size(s.teamNames);++i){
        if(!terminated(s.teamNames[i]) || std::char_traits<char>::length(s.teamNames[i])>24)throw std::runtime_error("Invalid team name");
        o.teamNames[i]=s.teamNames[i];
    }
    o.alliances = s.alliances;
    if(s.teamPolicy&0x80)throw std::runtime_error("Invalid team activity policy");
    o.inactiveTeams=s.teamPolicy&0x3f;o.requireTeamPlayers=s.teamPolicy&0x40;
    o.seed = s.seed;
    o.scenario = Scenario(s.scenario);
    o.map = s.map;
    o.hull = s.hull;
    o.enemies = s.enemies;
    o.allies = s.allies;
    o.difficulty = s.difficulty;
    o.killLimit = s.killLimit;
    o.civilization = s.civilization;
    o.team = std::max(side,0); // The replica world needs a valid setup side; Welcome/frame retain actual No Team membership.
    o.pilot = name;
    o.startInHangar = true;
    return o;
}
std::vector<std::byte> packet(Frame &f) {
    f.materialize();
    f.status.entities = f.entities.size();
    f.status.parts = f.parts.size();
    f.status.pilots = f.pilots.size();
    f.status.members = f.members.size();
    f.status.shipIntel=f.shipIntel.size();
    f.status.teams = f.teams.size();
    f.status.buckets = f.buckets.size();
    f.status.notices = f.notices.size();
    f.status.events = f.events.size();
    f.status.highlights = f.highlights.size();
    f.status.resultPlayers=f.resultPlayers.size();f.status.resultTeams=f.resultTeams.size();
    f.status.payloadBytes = f.payload.size();
    std::vector<std::byte> out;
    out.reserve(sizeof(Header) + sizeof(Snapshot) + f.entities.size() * sizeof(Entity) + f.parts.size() * sizeof(Part) +
                f.members.size()*sizeof(Member)+f.pilots.size()*sizeof(Pilot)+f.teams.size()*sizeof(Team)+f.buckets.size()*sizeof(Bucket)+
                f.notices.size()*sizeof(Notice)+f.events.size()*sizeof(Event)+f.highlights.size()*sizeof(SectorHighlight)+
                f.resultPlayers.size()*sizeof(ResultPlayer)+f.resultTeams.size()*sizeof(ResultTeam)+f.shipIntel.size()*sizeof(ShipIntel)+f.payload.size());
    append(out, f.header);
    append(out, f.status);
    append(out, std::span(f.entities));
    append(out, std::span(f.parts));
    append(out, std::span(f.pilots));
    append(out, std::span(f.members));
    append(out, std::span(f.teams));
    append(out, std::span(f.buckets));
    append(out, std::span(f.notices));
    append(out, std::span(f.events));
    append(out, std::span(f.highlights));
    append(out,std::span(f.resultPlayers));append(out,std::span(f.resultTeams));
    append(out,std::span(f.shipIntel));
    append(out, std::span(f.payload));
    if (out.size() > maxFrame)
        throw std::runtime_error("Replication budget exceeded");
    return out;
}
Frame composeReport(const Frame& world,Frame personal) {
    auto result=world;
    result.header=personal.header;result.status=personal.status;result.omitted=personal.omitted;
    result.notices.clear();result.notices.reserve(world.notices.size()+personal.notices.size());
    std::merge(world.notices.begin(),world.notices.end(),personal.notices.begin(),personal.notices.end(),
        std::back_inserter(result.notices),[](const auto& a,const auto& b){return a.number<b.number;});
    result.events.clear();result.events.reserve(world.events.size()+personal.events.size());
    std::merge(world.events.begin(),world.events.end(),personal.events.begin(),personal.events.end(),
        std::back_inserter(result.events),[](const auto& a,const auto& b){return a.number<b.number;});
    return result;
}
Frame combineWorlds(std::span<const Frame* const> worlds) {
    Frame result;
    struct Selected {const Frame* frame;const Entity* entity;size_t payload,parts;};
    std::vector<Selected> selected;
    std::unordered_map<uint64_t,size_t> indices;
    for(const auto* world:worlds) {
        size_t payload=0,parts=0,plain=0,shot=0;
        for(size_t at=0;at<world->entities.size()+world->capturedProjectiles.size();++at) {
            if(shot<world->capturedProjectiles.size() && world->capturedProjectiles[shot].ordinal==at) {
                auto capture=world->capturedProjectiles[shot++];
                capture.ordinal=uint32_t(selected.size()+result.capturedProjectiles.size());
                result.capturedProjectiles.push_back(capture);continue;
            }
            if(plain>=world->entities.size())throw std::logic_error("Invalid world trajectory order");
            const auto& e=world->entities[plain++];
            if(e.payloadBytes>world->payload.size()-payload || e.partCount>world->parts.size()-parts)
                throw std::logic_error("World overlay record bounds");
            uint64_t key=e.identity.id>=0?
                (uint32_t(uint16_t(e.identity.type))<<16)|uint16_t(e.identity.id):uint64_t(e.identity.serial)<<32;
            auto [slot,inserted]=indices.try_emplace(key,selected.size());
            if(inserted)selected.push_back({world,&e,payload,parts});
            else if(!(e.flags&Strategic))
                selected[slot->second]={world,&e,payload,parts};
            payload+=e.payloadBytes;parts+=e.partCount;
        }
        result.members.insert(result.members.end(),world->members.begin(),world->members.end());
        result.pilots.insert(result.pilots.end(),world->pilots.begin(),world->pilots.end());
        result.teams.insert(result.teams.end(),world->teams.begin(),world->teams.end());
        result.buckets.insert(result.buckets.end(),world->buckets.begin(),world->buckets.end());
        result.events.insert(result.events.end(),world->events.begin(),world->events.end());
        result.notices.insert(result.notices.end(),world->notices.begin(),world->notices.end());
        result.highlights.insert(result.highlights.end(),world->highlights.begin(),world->highlights.end());
        result.resultPlayers.insert(result.resultPlayers.end(),world->resultPlayers.begin(),world->resultPlayers.end());
        result.resultTeams.insert(result.resultTeams.end(),world->resultTeams.begin(),world->resultTeams.end());
        result.shipIntel.insert(result.shipIntel.end(),world->shipIntel.begin(),world->shipIntel.end());
    }
    result.entities.reserve(selected.size());
    for(const auto& item:selected){const auto& e=*item.entity;result.entities.push_back(e);
        auto payload=std::span(item.frame->payload).subspan(item.payload,e.payloadBytes);
        result.payload.insert(result.payload.end(),payload.begin(),payload.end());
        auto parts=std::span(item.frame->parts).subspan(item.parts,e.partCount);
        result.parts.insert(result.parts.end(),parts.begin(),parts.end());}
    std::sort(result.events.begin(),result.events.end(),[](const auto& a,const auto& b){return a.number<b.number;});
    std::sort(result.notices.begin(),result.notices.end(),[](const auto& a,const auto& b){return a.number<b.number;});
    return result;
}

Frame frame(std::span<const std::byte> bytes) {
    Reader in(bytes);
    Frame f;
    f.header = in.take<Header>();
    f.status = in.take<Snapshot>();
    f.entities = in.array<Entity>(f.status.entities);
    f.parts = in.array<Part>(f.status.parts);
    f.pilots = in.array<Pilot>(f.status.pilots);
    f.members = in.array<Member>(f.status.members);
    f.teams = in.array<Team>(f.status.teams);
    f.buckets = in.array<Bucket>(f.status.buckets);
    f.notices = in.array<Notice>(f.status.notices);
    f.events = in.array<Event>(f.status.events);
    f.highlights = in.array<SectorHighlight>(f.status.highlights);
    f.resultPlayers=in.array<ResultPlayer>(f.status.resultPlayers);f.resultTeams=in.array<ResultTeam>(f.status.resultTeams);
    f.shipIntel=in.array<ShipIntel>(f.status.shipIntel);
    std::unordered_set<int> intelIds;
    for(const auto& s:f.shipIntel)if(!s.serial || s.id<0 || s.side<0 || s.side>=6 || s.pilotType<0 || s.pilotType>11 || !terminated(s.name) || s.reserved ||
        s.status.reserved || s.status.state<Allegiance::PlayerShipStatus::Dead || s.status.state>Allegiance::PlayerShipStatus::Turret ||
        (s.status.flags&~3u) || s.status.hull<-1 || s.status.sector<-1 || s.status.station<-1 || s.status.parent<-1 ||
        !intelIds.insert(s.id).second)throw std::runtime_error("Invalid ship status");
    if(f.status.winner<-1 || f.status.winner>=6 || (f.status.resultFlags&~3u))throw std::runtime_error("Invalid result policy");
    std::unordered_set<int> resultIds;
    for(const auto& p:f.resultPlayers)if(!terminated(p.name) || p.id<0 || p.side<0 || p.side>=6 || !std::isfinite(p.played) || p.played<0 || !std::isfinite(p.score) || (p.flags&~3u) || !resultIds.insert(p.id).second)throw std::runtime_error("Invalid result pilot");
    resultIds.clear();
    for(const auto& t:f.resultTeams){if(!terminated(t.name) || t.id<0 || t.id>=6 || !resultIds.insert(t.id).second)throw std::runtime_error("Invalid result team");for(float c:t.color)if(!std::isfinite(c) || c<0 || c>1)throw std::runtime_error("Invalid result color");}
    for(auto& v:f.highlights)if(v.sector<0 || v.sector>INT16_MAX)throw std::runtime_error("Invalid highlighted sector");
    if (in.rest().size() != f.status.payloadBytes)
        throw std::runtime_error("Replication payload length");
    f.payload.assign(in.rest().begin(), in.rest().end());
    if(f.status.groupFire>1 || f.status.selectedWeapon<0 || f.status.selectedWeapon>3)
        throw std::runtime_error("Invalid weapon selection");
    if (f.header.signature != magic || f.header.kind != Kind::Snapshot || !terminated(f.status.result))
        throw std::runtime_error("Invalid snapshot header");
    std::string context = "header";
    auto scalar = [&](float value) {
        if (!std::isfinite(value))
            throw std::runtime_error("Non-finite snapshot state: " + context);
    };
    auto vector = [&](Vec value) {
        scalar(value.x);
        scalar(value.y);
        scalar(value.z);
    };
    std::unordered_set<uint32_t> keys;
    std::unordered_map<uint32_t, const Entity *> identities;
    size_t payload = 0, parts = 0;
    for (auto &e : f.entities) {
        context = "entity " + std::to_string(e.identity.type) + "/" + std::to_string(e.identity.id);
        uint32_t key = (uint32_t(uint16_t(e.identity.type)) << 16) | uint16_t(e.identity.id);
        if ((e.identity.id >= 0 && !keys.insert(key).second) ||
            (e.identity.serial && !identities.emplace(e.identity.serial, &e).second))
            throw std::runtime_error("Duplicate entity identity");
        payload += e.payloadBytes;
        parts += e.partCount;
        context += " position";
        vector(e.motion.position);
        context = "entity " + std::to_string(e.identity.type) + "/" + std::to_string(e.identity.id) + " motion";
        vector(e.motion.velocity);
        vector(e.motion.forward);
        vector(e.motion.up);
        vector(e.engine);
        for (auto value : e.turn)
            scalar(value);
        for (auto value : e.controls)
            scalar(value);
        for (float value : {e.hull, e.shield, e.energy, e.fuel, e.ore, e.signature, e.experience, e.radius, e.power,
                            e.ripcord, e.lock, e.impact, e.cloaking})
            scalar(value);
        auto length = [](Vec v) { return double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z; };
        double dot = double(e.motion.forward.x) * e.motion.up.x + double(e.motion.forward.y) * e.motion.up.y +
                     double(e.motion.forward.z) * e.motion.up.z;
        if (e.radius < 0 || length(e.motion.forward) < .5 || length(e.motion.forward) > 1.5 ||
            length(e.motion.up) < .5 || length(e.motion.up) > 1.5 || std::abs(dot) > .25)
            throw std::runtime_error("Invalid entity geometry");
    }
    if (payload != f.payload.size() || parts != f.parts.size())
        throw std::runtime_error("Entity payload/part count");
    std::unordered_map<uint32_t, uint32_t> partCounts;
    for (auto &p : f.parts) {
        context = "part " + std::to_string(p.id) + " on " + std::to_string(p.ship.id);
        auto ship = identities.find(p.ship.serial);
        if (ship == identities.end() || ship->second->identity != p.ship || p.amount < 0 || p.amount > 32767)
            throw std::runtime_error("Invalid part owner or amount");
        ++partCounts[p.ship.serial];
        for (float value : {p.mounted, p.fraction, p.lock})
            scalar(value);
    }
    for (auto &e : f.entities)
        if (partCounts[e.identity.serial] != e.partCount)
            throw std::runtime_error("Part ownership count");
    std::bitset<shipIdentityCapacity> pilots;
    for (const auto &p : f.pilots) {
        if(p.id<0 || (p.side!=noTeam && (p.side<0 || p.side>=6)) || p.wing< -1 || p.wing>=10 ||
           !terminated(p.name) || pilots.test(p.id))throw std::runtime_error("Invalid faction pilot");
        pilots.set(p.id);
    }
    std::unordered_set<int> members;
    for(const auto& p:f.members)
        if(p.id<0 || (p.side!=noTeam && (p.side<0 || p.side>=6)) || p.requested<noTeamRequest || p.requested>=6 ||
           (p.flags&~7u) || !terminated(p.name) || !members.insert(p.id).second)throw std::runtime_error("Invalid mission member");
    for (auto &n : f.notices) {
        if(!terminated(n.text) || !std::isfinite(n.time))throw std::runtime_error("Invalid notice");
        if(n.flags&NoticeBallot){
            if((n.flags&~(NoticeBallot|NoticeHideBallot)) || n.sender<0 || n.color>=6 || n.time<0 ||
               n.voice<int(Allegiance::BallotKind::Resign) || n.voice>int(Allegiance::BallotKind::Mutiny) ||
               ((n.flags&NoticeHideBallot) && n.voice!=int(Allegiance::BallotKind::Mutiny)))throw std::runtime_error("Invalid ballot notice");
        }else if(n.flags&NoticeSharedTip){
            if(!(n.flags&NoticeTip) || (n.flags&~(NoticeSharedTip|NoticeTip|NoticeWarning)) || n.sender< -1 || n.sender>INT16_MAX || n.voice< -1 || n.voice>INT16_MAX)
                throw std::runtime_error("Invalid shared tip");
        }else if((n.flags&~(((NoticeModal<<1)-1u)|noticeChannelMask|NoticeTip)) || noticeChannel(n.flags)>14 || n.sender< -1 || n.sender>INT16_MAX || n.voice< -1)
            throw std::runtime_error("Invalid notice");
    }
    for (auto &e : f.events) {
        if (!std::isfinite(e.time))
            throw std::runtime_error("Invalid event time");
        vector(e.position);
        vector(e.velocity);
        vector(e.normal);
        vector(e.ellipse);
        scalar(e.radius);
    }
    for (auto &b : f.buckets)
        scalar(b.time);
    return f;
}
Digest buildDigest() {
    Digest source{},d{};
    constexpr std::string_view hex = ALLEGIANCE_NETWORK_BUILD;
    for (size_t i = 0; i < source.size(); ++i)
        source[i] = uint8_t(std::stoi(std::string(hex.substr(2 * i, 2)), nullptr, 16));
    Crypto::Sha256 hash;hash.update(source);
    // Fixed-width records use the host layout directly. Reject a different ABI
    // even when source identity matches, without excluding another platform.
    const uint32_t abi[]={tickRate,snapshotRate,
#define ABI(T) uint32_t(sizeof(T)),uint32_t(alignof(T))
        ABI(Header),ABI(MessagePrefix),ABI(InputHeader),ABI(Hello),ABI(Session),ABI(Welcome),ABI(BrowseQuery),ABI(PlayerQuery),ABI(ServerInfo),
        ABI(Vec),ABI(ShipDamageFrame),ABI(ShipDamageState),ABI(Identity),ABI(Motion),ABI(Input),
        ABI(OutfitPart),ABI(Action),ABI(Configuration),ABI(Receipt),ABI(MissionPolicy),ABI(Command),ABI(Entity),
        ABI(Part),ABI(Pilot),ABI(Member),ABI(Team),ABI(Bucket),ABI(Notice),ABI(Event),ABI(SectorHighlight),ABI(ResultPlayer),ABI(ResultTeam),ABI(Snapshot),ABI(ClockProbe),
        ABI(Delta),ABI(CopyRange),ABI(XorRange),ABI(Packed),ABI(Fragment),ABI(ArenaRecord),ABI(ArenaRef),ABI(ArenaManifest),ABI(PlayerConnectionInfo),ABI(ConnectionReport),ABI(Briefing)
#undef ABI
    };
    hash.update({reinterpret_cast<const uint8_t*>(abi),sizeof(abi)});hash.finish(d);
    return d;
}
Digest contentDigest(const std::filesystem::path &root) {
    std::vector<std::filesystem::path> files;
    for (auto &entry : std::filesystem::directory_iterator(root))
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension();
            auto name=entry.path().filename().string();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
            if (ext == ".igc" || ext == ".mdl" || ext == ".cvh" || name=="maps.txt")
                files.push_back(entry.path());
        }
    std::sort(files.begin(), files.end());
    Crypto::Sha256 hash;
    std::array<char, 65536> buffer;
    for (auto &file : files) {
        auto name = file.filename().string();
        hash.update({reinterpret_cast<const uint8_t*>(name.c_str()),name.size()+1});
        std::ifstream input(file, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot fingerprint " + file.string());
        while (input) {
            input.read(buffer.data(), buffer.size());
            if (input.gcount())
                hash.update({reinterpret_cast<const uint8_t*>(buffer.data()),size_t(input.gcount())});
        }
        if (!input.eof())
            throw std::runtime_error("Cannot read " + file.string());
    }
    Digest result;
    hash.finish(result);
    return result;
}
int64_t now() {
    return int64_t(monotonicNs() / 1000);
}
} // namespace Native::Net
