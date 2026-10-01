#include "server_browser.hpp"
#include <chrono>
#include <bit>
#include "crypto.hpp"
#include <unordered_map>

namespace Native::Net {
namespace {
struct InstanceHash {
    size_t operator()(const Instance& id)const noexcept{
        uint64_t a=(uint64_t(id[0])<<32)|id[1],b=(uint64_t(id[2])<<32)|id[3];
        auto mix=[](uint64_t value){value=(value^(value>>30))*0xbf58476d1ce4e5b9ull;value=(value^(value>>27))*0x94d049bb133111ebull;return value^(value>>31);};
        return size_t(mix(a)^std::rotl(mix(b),1));
    }
};
template<class Row> std::vector<Row> coalesce(std::vector<Row> rows){
    std::unordered_map<Instance,size_t,InstanceHash> identities;std::unordered_map<std::string,size_t> anonymous;
    // A listener can be reused after a mission/server restarts. A newer reply
    // at that listener retires stale identities retained by another seed.
    std::unordered_map<std::string,std::pair<int64_t,Instance>> listeners;listeners.reserve(rows.size());
    for(const auto& row:rows){auto [entry,inserted]=listeners.try_emplace(row.address,row.observedAt,row.info.instance);
        if(!inserted && row.observedAt>entry->second.first)entry->second={row.observedAt,row.info.instance};}
    identities.reserve(rows.size());size_t kept=0;
    for(size_t i=0;i<rows.size();++i){auto& row=rows[i];
        if(listeners.at(row.address).second!=row.info.instance)continue;
        size_t slot;bool inserted;
        if(validInstance(row.info.instance)){auto result=identities.try_emplace(row.info.instance,kept);slot=result.first->second;inserted=result.second;}
        else{auto result=anonymous.try_emplace(row.address,kept);slot=result.first->second;inserted=result.second;}
        // Store indices only: compacting/replacing rows must not invalidate
        // pointers into the vector or keys used to identify later aliases.
        if(inserted){if(kept!=i)rows[kept]=std::move(row);++kept;}
        else if(row.observedAt>rows[slot].observedAt)rows[slot]=std::move(row);
    }
    rows.resize(kept);return rows;
}
std::string gameAddress(std::string_view address,uint32_t port){
    if(!port || port>65535)throw std::runtime_error("Invalid game listener");
    std::string host(address);
    if(host.starts_with("[")){auto end=host.find(']');if(end==host.npos)throw std::runtime_error("Invalid authority address");host.resize(end+1);}
    else if(std::count(host.begin(),host.end(),':')==1)host.resize(host.find(':'));
    else if(std::count(host.begin(),host.end(),':')>1)host="["+host+"]";
    return host+":"+std::to_string(port);
}
void validate(const ServerInfo& info){
    if(!validInstance(info.instance) || !terminated(info.name) || !terminated(info.server) || !terminated(info.core) || !info.core[0] || !Allegiance::ValidMapFile(info.core) || info.players>shipIdentityCapacity ||
        info.noTeam>info.players || info.capacity!=shipIdentityCapacity || (info.flags&~63u) || info.session.scenario<0 ||
        info.session.scenario>=int(scenarios.size()) || info.session.rules.teams<2 || info.session.rules.teams>6)
        throw std::runtime_error("Invalid server listing");
    auto session=options(info.session,0,"");if(!Allegiance::CoreFileEqual(info.core,sessionCoreFile(session)))throw std::runtime_error("Listing core differs from session");
}
int64_t clockUs(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}}
bool ServerBrowser::add(std::string_view address){
    if(address.empty() || address.size()>128)return false;
    auto known=std::find_if(endpoints_.begin(),endpoints_.end(),[&](const auto& e){return e.address==address;});
    if(known!=endpoints_.end()){known->explicitAddress=true;return false;}
    if(endpoints_.size()>=registryCapacity+64)return false;
    endpoints_.push_back({std::string(address)});endpoints_.back().explicitAddress=true;++revision_;return true;
}
void ServerBrowser::finish(Endpoint& e,std::string error){
    if(e.connection)transport_->close(e.connection,"Browse complete");
    e.connection=0;e.next=clockUs()+5000000;
    if(!error.empty()){e.error=std::move(error);e.games.clear();e.authority.reset();}
    ++revision_;
}
void ServerBrowser::stop(){discovery_.reset();transport_.reset();activeSearch_=0;searchResult_.reset();creation_.reset();creationResult_.reset();for(auto& e:endpoints_){e.connection=0;e.next=0;}}
void ServerBrowser::finishCreation(GameCreationResult result){
    if(creation_ && creation_->connection)transport_->close(creation_->connection,"Creation complete");
    creation_.reset();creationResult_=std::move(result);
    for(auto& e:endpoints_)e.next=0;
}
bool ServerBrowser::createGame(std::string_view address,std::string_view name,std::string_view pilot,std::string_view core){
    if(creation_ || address.empty() || address.size()>128 || !Allegiance::CreatedGameNameError(name).empty() ||
        pilot.empty() || pilot.size()>=sizeof(CreateGameRequest::pilot) || core.empty() || core.size()>=sizeof(CreateGameRequest::core) ||
        name.find('\0')!=name.npos || pilot.find('\0')!=pilot.npos || core.find('\0')!=core.npos)return false;
    // Selecting a discovered authority for creation must not silently pin it
    // as a saved endpoint after its public registration is withdrawn.
    if(std::none_of(endpoints_.begin(),endpoints_.end(),[&](const auto& e){return e.address==address;}))add(address);
    Creation request;request.address=address;request.request.identity={buildDigest(),content_};
    Crypto::random({reinterpret_cast<uint8_t*>(request.request.request.data()),sizeof(request.request.request)});
    text(request.request.name,name);text(request.request.pilot,pilot);text(request.request.core,core);
    creationResult_.reset();creation_=std::move(request);return true;
}
void ServerBrowser::findPlayer(std::string_view name){
    activeSearch_=++searchSerial_;playerName_=name;searchResult_.reset();
    for(auto& e:endpoints_){if(e.connection)finish(e);e.next=0;}
    if(name.empty() || name.size()>=sizeof(PlayerQuery::player)){activeSearch_=0;searchResult_=PlayerSearchResult{};}
}
void ServerBrowser::poll(bool discoverLan){
    const auto now=clockUs();
    auto retireExpired=[&]{std::erase_if(endpoints_,[&](auto& e){if(e.explicitAddress || (e.discovery && now<e.expires) || now<e.directoryExpires)return false;if(e.connection)transport_->close(e.connection,"Discovery expired");++revision_;return true;});};
    retireExpired();
    if(discoverLan){
        if(!discovery_)discovery_=std::make_unique<LanDiscovery>(LanDiscovery::Role::Browser);
        for(auto& found:discovery_->poll()){
            auto known=std::find_if(endpoints_.begin(),endpoints_.end(),[&](const auto& e){return e.discovery==found.instance || e.address==found.address;});
            if(known!=endpoints_.end()){known->discovery=found.instance;known->expires=now+5000000;}
            else if(endpoints_.size()<registryCapacity+64){endpoints_.push_back({std::move(found.address)});auto& e=endpoints_.back();e.discovery=found.instance;e.expires=now+5000000;++revision_;}
        }
    }else discovery_.reset();
    if(endpoints_.empty() && !creation_){if(activeSearch_){activeSearch_=0;searchResult_=PlayerSearchResult{};}return;}
    if(!transport_)transport_=clientTransport();
    auto incoming=transport_->receive();
    for(const auto& event:transport_->events()){
        if(creation_ && creation_->connection==event.peer){
            if(!event.connected)finishCreation({{},event.reason.empty()?"Server unavailable":event.reason});
            else {Header h{};h.kind=Kind::CreateGame;std::vector<std::byte> bytes;append(bytes,h);append(bytes,creation_->request);
                if(!transport_->send(event.peer,bytes,Lane::Control))finishCreation({{},"Game creation could not be sent"});}
            continue;
        }
        auto e=std::find_if(endpoints_.begin(),endpoints_.end(),[&](const auto& e){return e.connection==event.peer;});
        if(e==endpoints_.end())continue;
        if(!event.connected){finish(*e,event.reason.empty()?"Server unavailable":event.reason);continue;}
        BrowseQuery identity{buildDigest(),content_};bool search=activeSearch_ && e->search==activeSearch_;
        Header h{};h.kind=search?Kind::FindPlayer:Kind::Browse;std::vector<std::byte> bytes;append(bytes,h);
        if(search){PlayerQuery query{identity};text(query.player,playerName_);append(bytes,query);}else append(bytes,identity);
        if(!transport_->send(event.peer,bytes,Lane::Control))finish(*e,"Server query could not be sent");
    }
    std::vector<std::pair<std::string,std::vector<std::string>>> directoryReplies;
    for(const auto& message:incoming){
        if(creation_ && creation_->connection==message.peer){
            try{Reader r(message.data);auto h=r.take<Header>();auto result=r.take<GameCreated>();
                if(h.signature!=magic || h.kind!=Kind::GameCreated || !r.rest().empty() || !terminated(result.error) || result.reserved ||
                    (result.error[0]?(result.port || result.token[0] || result.token[1]):(!result.port || result.port>65535 || (!result.token[0] && !result.token[1]))))
                    throw std::runtime_error("Invalid game creation reply");
                if(result.error[0])finishCreation({{},result.error});
                else {auto source=transport_->remoteAddress(creation_->connection);if(source.empty())throw std::runtime_error("Server address is unknown");finishCreation({gameAddress(source,result.port),{},result.token});}
            }catch(const std::exception& e){finishCreation({{},e.what()});}
            continue;
        }
        auto e=std::find_if(endpoints_.begin(),endpoints_.end(),[&](const auto& e){return e.connection==message.peer;});
        if(e==endpoints_.end())continue;
        try{
            Reader r(message.data);auto h=r.take<Header>();
            if(h.signature!=magic)throw std::runtime_error("Foreign server listing");
            if(h.kind==Kind::Registry){
                auto info=r.take<RegistryInfo>();
                if(info.reserved || info.count>registryCapacity || r.rest().size()!=size_t(info.count)*sizeof(RegistryEndpoint))throw std::runtime_error("Invalid public directory");
                std::vector<std::string> addresses;addresses.reserve(info.count);
                for(uint32_t i=0;i<info.count;++i){auto endpoint=r.take<RegistryEndpoint>();
                    if(!terminated(endpoint.address) || !endpoint.address[0])throw std::runtime_error("Invalid public authority endpoint");
                    std::string address(endpoint.address);
                    if(address.find(':')==address.npos || address.find_first_not_of("0123456789abcdefABCDEF.[]:")!=address.npos || std::find(addresses.begin(),addresses.end(),address)!=addresses.end())throw std::runtime_error("Invalid public authority address");
                    addresses.push_back(std::move(address));
                }
                directoryReplies.emplace_back(e->address,std::move(addresses));
                e->games.clear();e->authority.reset();e->error.clear();finish(*e);
                continue;
            }
            auto source=transport_->remoteAddress(e->connection);if(source.empty())throw std::runtime_error("Server address is unknown");
            std::vector<ServerListing> games;std::optional<AuthorityListing> authority;
            if(h.kind==Kind::ServerInfo){auto info=r.take<ServerInfo>();validate(info);games.push_back({source,info,now});}
            else if(h.kind==Kind::Directory){
                auto info=r.take<DirectoryInfo>();
                if(!info.count || info.count>maxHostedGames || info.count>info.capacity || info.capacity>maxHostedGames ||
                    !validInstance(info.instance) || !terminated(info.server) || !terminated(info.location) || info.reserved || info.cores>Allegiance::CoreCatalogCapacity)throw std::runtime_error("Invalid server directory");
                authority=AuthorityListing{source,info,transport_->quality(e->connection).ping,{},now};
                for(uint32_t i=0;i<info.cores;++i){auto core=r.take<CoreInfo>();
                    if(!terminated(core.file) || !core.file[0] || !Allegiance::ValidMapFile(core.file) || Allegiance::FindCore(authority->cores,core.file))throw std::runtime_error("Invalid authority core catalog");
                    authority->cores.emplace_back(core.file);
                }
                for(uint32_t i=0;i<info.count;++i){auto game=r.take<HostedGame>();validate(game.info);
                    if(game.reserved)throw std::runtime_error("Invalid hosted game");
                    auto address=gameAddress(source,game.port);
                    if((i==0 && info.instance!=game.info.instance) || std::any_of(games.begin(),games.end(),[&](const auto& g){return g.address==address || g.info.instance==game.info.instance;}))throw std::runtime_error("Duplicate or mismatched hosted game identity");
                    games.push_back({std::move(address),game.info,now});
                }
            }else throw std::runtime_error("Invalid server listing kind");
            if(!r.rest().empty())throw std::runtime_error("Trailing server listing bytes");
            for(auto& game:games){bool found=game.info.flags&ServerPlayerFound;game.info.flags&=~ServerPlayerFound;
                if(found && activeSearch_ && e->search==activeSearch_){searchResult_=PlayerSearchResult{game};activeSearch_=0;}}
            e->games=std::move(games);e->authority=std::move(authority);e->error.clear();finish(*e);
        }catch(const std::exception& error){finish(*e,error.what());}
    }
    // Appending may reallocate endpoints_. Retire all message-local iterators
    // before merging directory membership, including during Find Player.
    bool registryChanged=!directoryReplies.empty();
    for(auto& [source,addresses]:directoryReplies)registries_[source]={now+registryLeaseSeconds*1000000ll,std::move(addresses)};
    registryChanged|=std::erase_if(registries_,[&](const auto& entry){return now>=entry.second.expires;})!=0;
    if(registryChanged){
        for(auto& e:endpoints_)e.directoryExpires=0;
        for(const auto& [source,catalog]:registries_)for(const auto& address:catalog.addresses){
            auto known=std::find_if(endpoints_.begin(),endpoints_.end(),[&](const auto& e){return e.address==address;});
            if(known!=endpoints_.end())known->directoryExpires=std::max(known->directoryExpires,catalog.expires);
            else if(endpoints_.size()<registryCapacity+64){endpoints_.push_back({address});endpoints_.back().directoryExpires=catalog.expires;++revision_;}
        }
        retireExpired();
    }
    if(creation_){
        if(!creation_->connection){try{creation_->connection=transport_->open(creation_->address);creation_->started=now;}catch(const std::exception& e){finishCreation({{},e.what()});}}
        else if(now-creation_->started>10000000)finishCreation({{},"Game creation timed out"});
    }
    size_t active=0;
    for(auto& e:endpoints_)if(e.connection){if(now-e.started>5000000)finish(e,"Server query timed out");else ++active;}
    for(auto& e:endpoints_)if(active<4 && !e.connection && (activeSearch_?e.search!=activeSearch_:now>=e.next)){
        e.search=activeSearch_;
        try{e.connection=transport_->open(e.address);e.started=now;++active;}
        catch(const std::exception& error){finish(e,error.what());}
    }
    if(activeSearch_ && std::all_of(endpoints_.begin(),endpoints_.end(),[&](const auto& e){return e.search==activeSearch_ && !e.connection;})){
        activeSearch_=0;searchResult_=PlayerSearchResult{};
    }
}
std::vector<ServerListing> ServerBrowser::listings()const{
    std::vector<ServerListing> result;
    for(const auto& e:endpoints_)result.insert(result.end(),e.games.begin(),e.games.end());
    return coalesceGames(std::move(result));
}
std::vector<AuthorityListing> ServerBrowser::authorities()const{
    std::vector<AuthorityListing> result;
    for(const auto& e:endpoints_)if(e.authority)result.push_back(*e.authority);
    return coalesceAuthorities(std::move(result));
}
std::vector<ServerListing> coalesceGames(std::vector<ServerListing> rows){return coalesce(std::move(rows));}
std::vector<AuthorityListing> coalesceAuthorities(std::vector<AuthorityListing> rows){return coalesce(std::move(rows));}
}
