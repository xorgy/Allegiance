#include "server_host.hpp"
#include "crypto.hpp"

namespace Native::Net {
ServerHost::ServerHost(Server& entry,Digest content,uint16_t port,Factory factory,std::vector<std::string> cores)
    :entry_(entry),content_(content),ports_(gameListenerPorts(port)),capacity_(uint32_t(std::count_if(ports_.begin(),ports_.end(),[](auto p){return p!=0;}))),factory_(std::move(factory)),cores_(std::move(cores)){
    if(cores_.size()>Allegiance::CoreCatalogCapacity)throw std::runtime_error("Too many supported cores");
    for(size_t i=0;i<cores_.size();++i)if(cores_[i].empty() || !Allegiance::ValidMapFile(cores_[i]) || Allegiance::FindCore(std::span(cores_).first(i),cores_[i]))throw std::runtime_error("Invalid supported core catalog");
    entry_.hosting([this](auto player){return directory(player);},[this](const auto& query){return create(query);});
}
ServerHost::~ServerHost(){entry_.hosting({},{});}
std::vector<std::byte> ServerHost::directory(std::string_view player){
    DirectoryInfo info{};info.count=uint32_t(games());info.capacity=capacity_;info.cores=uint32_t(cores_.size());
    auto initial=entry_.listing(player);info.instance=initial.instance;text(info.server,initial.server);
    // No invented geographic or official-server designation.
    Header header{};header.kind=Kind::Directory;std::vector<std::byte> bytes;append(bytes,header);append(bytes,info);
    for(const auto& core:cores_){CoreInfo record{};text(record.file,core);append(bytes,record);}
    append(bytes,HostedGame{initial,ports_[0]});
    for(const auto& mission:missions_)append(bytes,HostedGame{mission.server->listing(player),mission.port});
    return bytes;
}
GameCreated ServerHost::create(const CreateGameRequest& query){
    const auto now=Clock::now();
    std::erase_if(creations_,[&](const auto& item){return now>=item.second.expires;});
    if(auto found=creations_.find(query.request);found!=creations_.end()){
        if(std::memcmp(&found->second.request,&query,sizeof(query)))throw std::runtime_error("Create-game request identity was reused");
        auto result=found->second.result;
        if(result.port && std::none_of(missions_.begin(),missions_.end(),[&](const auto& m){return m.port==result.port && m.created==found->second.created;})){
            result={};text(result.error,"The created game has expired.");
        }
        return result;
    }
    GameCreated result{};
    if(auto error=Allegiance::CreatedGameNameError(query.name);!error.empty())text(result.error,error);
    else if(!Allegiance::FindCore(cores_,query.core))text(result.error,"This server cannot run the selected core.");
    else if(games()>=capacity_)text(result.error,"The selected server is full.");
    else if(creations_.size()>=64)text(result.error,"Too many recent game creation requests; please try again later.");
    else try{
        SessionOptions options;options.scenario=Scenario::Conquest;options.coreFile=*Allegiance::FindCore(cores_,query.core);options.name=query.name;options.pilot="Server seat";options.startInHangar=true;
        Crypto::random({reinterpret_cast<uint8_t*>(&options.seed),sizeof(options.seed)});
        auto game=factory_(options);if(!game)throw std::runtime_error("Mission factory returned no game");
        std::unique_ptr<Server> server;uint16_t selected=0;std::string failure;
        // Another process may own a candidate port. Try other free slots without
        // publishing a mission until both IGC and its listener exist.
        for(auto candidate:std::span(ports_).subspan(1)){
            if(!candidate)break;
            if(std::any_of(missions_.begin(),missions_.end(),[&](const auto& m){return m.port==candidate;}))continue;
            try{server=std::make_unique<Server>(*game,options,content_,uint16_t(candidate),true,options.name,entry_.listing().server,false);selected=uint16_t(candidate);break;}
            catch(const std::exception& e){failure=e.what();}
        }
        if(!server)throw std::runtime_error(failure.empty()?"No game listener is available":failure);
        result.token=server->reserveCreator(query.pilot);result.port=selected;
        missions_.push_back({std::move(game),std::move(server),selected,now});
    }catch(const std::exception& e){result={};text(result.error,e.what());}
    if(creations_.size()<64)creations_.emplace(query.request,Creation{query,result,now,now+std::chrono::minutes(5)});
    return result;
}
void ServerHost::poll(Clock::time_point now){
    for(auto& mission:missions_){mission.server->pause(entry_.paused());mission.server->poll();mission.joined|=mission.server->occupants()>0;}
    std::erase_if(missions_,[&](const auto& m){return !m.server->occupants() && (m.joined || now-m.created>=std::chrono::seconds(30));});
}
void ServerHost::step(){for(auto& mission:missions_)if(mission.server->occupants())mission.server->step();}
}
