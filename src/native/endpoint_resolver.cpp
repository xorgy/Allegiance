#include "endpoint_resolver.hpp"
#include "thread_affinity.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif
#include <algorithm>
#include <charconv>
#include <condition_variable>
#include <deque>
#include <stdexcept>
#include <thread>
#include <utility>

namespace Native::Net {
std::string Endpoint::address()const{return (host.find(':')==host.npos?host:"["+host+"]")+":"+std::to_string(port);}
Endpoint parseEndpoint(std::string_view input){
    if(input.empty() || input.size()>261 || input.find('\0')!=input.npos)throw std::runtime_error("Invalid server address");
    Endpoint result;std::string_view host=input,port;
    if(input.front()=='['){
        auto end=input.find(']');if(end==input.npos || end==1)throw std::runtime_error("Invalid IPv6 server address");
        host=input.substr(1,end-1);auto tail=input.substr(end+1);
        if(!tail.empty()){if(tail.front()!=':')throw std::runtime_error("Invalid IPv6 server address");port=tail.substr(1);if(port.empty())throw std::runtime_error("Invalid server port");}
        if(host.find(':')==host.npos)throw std::runtime_error("Bracketed server addresses must be IPv6");
    }else if(auto colon=input.find(':');colon!=input.npos && input.find(':',colon+1)==input.npos){
        host=input.substr(0,colon);port=input.substr(colon+1);if(port.empty())throw std::runtime_error("Invalid server port");
    }
    if(!port.empty()){unsigned value{};auto parsed=std::from_chars(port.data(),port.data()+port.size(),value);
        if(parsed.ec!=std::errc{} || parsed.ptr!=port.data()+port.size() || !value || value>65535)throw std::runtime_error("Invalid server port");result.port=uint16_t(value);}
    if(host.empty() || host.size()>253)throw std::runtime_error("Invalid server host");
    if(host.find(':')!=host.npos){
        if(host.find_first_not_of("0123456789abcdefABCDEF:.")!=host.npos)throw std::runtime_error("Invalid IPv6 server address");result.numeric=true;
    }else{
        auto labels=host;if(labels.back()=='.')labels.remove_suffix(1);
        while(!labels.empty()){auto dot=labels.find('.');auto label=labels.substr(0,dot);
            if(label.empty() || label.size()>63 || label.front()=='-' || label.back()=='-' || label.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")!=label.npos)throw std::runtime_error("Invalid server host");
            if(dot==labels.npos)break;labels.remove_prefix(dot+1);if(labels.empty())throw std::runtime_error("Invalid server host");
        }
        if(labels.empty())throw std::runtime_error("Invalid server host");
        result.numeric=host.find_first_not_of("0123456789.")==host.npos;
    }
    result.host=host;return result;
}
void EndpointResolution::complete(ResolvedEndpoints result){
    if(cancelled())return;std::lock_guard lock(mutex_);if(!result_)result_=std::move(result);
}
std::optional<ResolvedEndpoints> EndpointResolution::take(){std::lock_guard lock(mutex_);return std::exchange(result_,{});}
namespace {
ResolvedEndpoints lookup(const Endpoint& endpoint){
#ifdef _WIN32
    struct Winsock {bool ready{};Winsock(){WSADATA data{};ready=WSAStartup(MAKEWORD(2,2),&data)==0;}~Winsock(){if(ready)WSACleanup();}} winsock;
    if(!winsock.ready)return {{},"Cannot initialize name resolution",true};
#endif
    addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_DGRAM;hints.ai_protocol=IPPROTO_UDP;hints.ai_flags=AI_NUMERICSERV;
    addrinfo* list{};auto service=std::to_string(endpoint.port);int status=getaddrinfo(endpoint.host.c_str(),service.c_str(),&hints,&list);
    if(status){
#ifdef _WIN32
        auto error="DNS error "+std::to_string(status);
#else
        std::string error=gai_strerror(status);
#endif
        return {{},"Cannot resolve '"+endpoint.host+"': "+error,status==EAI_AGAIN || status==EAI_MEMORY};
    }
    std::unique_ptr<addrinfo,decltype(&freeaddrinfo)> owned(list,freeaddrinfo);ResolvedEndpoints result;
    for(auto* address=list;address && result.addresses.size()<8;address=address->ai_next){
        if(address->ai_family!=AF_INET && address->ai_family!=AF_INET6)continue;
        char host[NI_MAXHOST]{};if(getnameinfo(address->ai_addr,address->ai_addrlen,host,sizeof(host),nullptr,0,NI_NUMERICHOST)!=0 || std::string_view(host).find('%')!=std::string_view::npos)continue;
        auto numeric=Endpoint{host,endpoint.port,true}.address();if(std::find(result.addresses.begin(),result.addresses.end(),numeric)==result.addresses.end())result.addresses.push_back(std::move(numeric));
    }
    if(result.addresses.empty())result.error="No supported addresses for '"+endpoint.host+"'";
    return result;
}
struct ResolverQueue {
    struct Job {Endpoint endpoint;std::weak_ptr<EndpointResolution> result;};
    std::mutex mutex;std::condition_variable wake;std::deque<Job> jobs;bool stopping{};
};
class ResolverService {
    std::shared_ptr<ResolverQueue> queue_=std::make_shared<ResolverQueue>();
    static void worker(std::shared_ptr<ResolverQueue> queue){
        for(;;){ResolverQueue::Job job;
            {std::unique_lock lock(queue->mutex);queue->wake.wait(lock,[&]{return queue->stopping || !queue->jobs.empty();});if(queue->stopping)return;job=std::move(queue->jobs.front());queue->jobs.pop_front();}
            auto result=job.result.lock();if(!result || result->cancelled())continue;
            try{result->complete(lookup(job.endpoint));}catch(const std::exception& error){result->complete({{},error.what(),true});}
        }
    }
  public:
    ResolverService(){
        // Exactly two process-wide workers. They retain their private queue
        // until OS lookups return; UI teardown and process shutdown never join
        // them. No worker touches GNS or any object owned by a caller.
        const Affinity::ScopedUnpin workerMask(true);
        try{for(int i=0;i<2;++i)std::thread(worker,queue_).detach();}
        catch(...){stop();throw;}
    }
    ~ResolverService(){stop();}
    void stop(){{std::lock_guard lock(queue_->mutex);queue_->stopping=true;queue_->jobs.clear();}queue_->wake.notify_all();}
    std::shared_ptr<EndpointResolution> resolve(const Endpoint& endpoint){
        auto result=std::make_shared<EndpointResolution>();
        {std::lock_guard lock(queue_->mutex);std::erase_if(queue_->jobs,[](const auto& job){auto r=job.result.lock();return !r || r->cancelled();});
            if(queue_->jobs.size()>=64)result->complete({{},"Name resolution queue is full",true});
            else queue_->jobs.push_back({endpoint,result});}
        queue_->wake.notify_one();return result;
    }
};
}
std::shared_ptr<EndpointResolution> resolveEndpoint(const Endpoint& endpoint){static ResolverService service;return service.resolve(endpoint);}
}
