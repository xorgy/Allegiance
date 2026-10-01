#include "lan_discovery.hpp"
#include <chrono>
#include <thread>
#include <set>
#include <stdexcept>
#include <cstdio>
using Native::Net::LanDiscovery;
int main()try{
    // Keep synthetic discovery on loopback and off the game's discovery port.
    auto a=std::make_unique<LanDiscovery>(LanDiscovery::Role::Server,28761,28661,true);
    LanDiscovery b(LanDiscovery::Role::Server,28762,28661,true),browser(LanDiscovery::Role::Browser,0,28661,true);
    if(!a->available() || !b.available() || !browser.available())throw std::runtime_error("Discovery socket setup");
    std::set<uint64_t> instances;std::set<std::string> endpoints;
    auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(std::chrono::steady_clock::now()<until && endpoints.size()<2){
        a->poll();b.poll();for(auto& e:browser.poll()){instances.insert(e.instance);endpoints.insert(e.address);}
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(instances.size()!=2 || endpoints!=std::set<std::string>{"127.0.0.1:28761","127.0.0.1:28762"})throw std::runtime_error("Shared-port discovery loses a server or duplicates its interface replies");
    a.reset();std::set<uint64_t> remaining;until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(std::chrono::steady_clock::now()<until){b.poll();for(auto& e:browser.poll())remaining.insert(e.instance);std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    if(remaining.size()!=1 || !instances.contains(*remaining.begin()))throw std::runtime_error("Stopped server still responds, or live server changes identity on refresh");
    puts("LAN discovery: two shared-port servers, actual host-local multicast/broadcast/unicast, compatible endpoints, stable instance IDs, interface deduplication and responder teardown passed");return 0;
}catch(const std::exception& e){fprintf(stderr,"LAN discovery check: %s\n",e.what());return 1;}
