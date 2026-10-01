// Copyright (c) 2026 Aaron. SPDX-License-Identifier: MIT
// Adapted from the author's 0 A.D. thread affinity code with permission.
#include "thread_affinity.hpp"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <string>
#ifdef __linux__
#include <pthread.h>
#endif

namespace Native::Affinity {
namespace {
bool contains(const std::vector<int>& cpus,int id) {
    return std::find(cpus.begin(),cpus.end(),id)!=cpus.end();
}
bool sameCore(const Cpu& a,const Cpu& b) {
    return a.id==b.id || contains(a.siblings,b.id) || contains(b.siblings,a.id);
}
}
Selection select(const std::vector<Cpu>& allowed) {
    std::vector<const Cpu*> cores;
    for(const auto& cpu:allowed) {
        auto it=std::find_if(cores.begin(),cores.end(),[&](auto* c){return sameCore(*c,cpu);});
        if(it==cores.end())cores.push_back(&cpu);
        else if(cpu.id<(*it)->id)*it=&cpu;
    }
    // Keep enough independent cores for the OS, transport and driver workers.
    if(cores.size()<4)return {};
    std::sort(cores.begin(),cores.end(),[](auto* a,auto* b){return a->id<b->id;});
    auto prefer=[](const Cpu* a,const Cpu* b){
        return !a || b->ranking>a->ranking || (b->ranking==a->ranking && b->id>a->id);
    };
    const Cpu* render=nullptr;
    for(auto* c:cores)
        if((cores.back()->cache.empty() || contains(cores.back()->cache,c->id)) && prefer(render,c))render=c;
    const Cpu* simulation=nullptr;
    // Separate the server and client cache domains on machines that have them.
    for(auto* c:cores)
        if(c!=render && !contains(render->cache,c->id) && prefer(simulation,c))simulation=c;
    if(!simulation)
        for(auto* c:cores)if(c!=render && prefer(simulation,c))simulation=c;
    // The most preferred cores also attract bursty desktop work. Keep the
    // authority on a fast core without selecting the OS's first preference.
    // Ranking-free and small domains retain the last-core tie break above.
    std::vector<const Cpu*> fast;
    for(auto* c:cores)
        if(c!=render && (simulation->cache.empty() || contains(simulation->cache,c->id)) &&
           c->ranking*100LL>=simulation->ranking*85LL)fast.push_back(c);
    if(fast.size()>=4 && simulation->ranking>0) {
        std::sort(fast.begin(),fast.end(),[](auto* a,auto* b){return a->ranking>b->ranking || (a->ranking==b->ranking && a->id>b->id);});
        if(fast.front()->ranking!=fast.back()->ranking)simulation=fast[fast.size()/2];
    }
    const Cpu* publication=nullptr;
    for(auto* c:cores)
        if(c!=render && c!=simulation && contains(render->cache,c->id) &&
           (!publication || c->ranking>publication->ranking))publication=c;
    if(!publication)
        for(auto* c:cores)if(c!=render && c!=simulation && !publication)publication=c;
    Selection result{simulation->id,publication->id,render->id,{}};
    std::vector<int> near;
    size_t nearCores=0;
    for(auto* c:cores)
        if(c!=render && c!=simulation && c!=publication && contains(render->cache,c->id))++nearCores;
    for(const auto& c:allowed)
        if(!sameCore(c,*simulation) && !sameCore(c,*publication) && !sameCore(c,*render)) {
            result.workers.push_back(c.id);
            if(contains(render->cache,c.id))near.push_back(c.id);
        }
    // Driver workers share data with rendering. Where there is room, keep them
    // in that LLC and leave the authority's cache domain undisturbed.
    if(nearCores>=3)result.workers=std::move(near);
    return result;
}

#ifdef __linux__
namespace {
Selection placement;
bool initialized=false;
thread_local bool pinned=false;
thread_local cpu_set_t original{}, selected{};

std::vector<int> readList(const std::string& path) {
    std::ifstream file(path);std::string list;
    if(!(file>>list))return {};
    std::vector<int> result;
    size_t pos=0;
    while(pos<list.size()) {
        int first=-1,last=-1;
        auto begin=list.data()+pos,end=list.data()+list.size();
        auto [next,error]=std::from_chars(begin,end,first);
        if(error!=std::errc{} || first<0 || first>=CPU_SETSIZE)return {};
        last=first;
        if(next!=end && *next=='-') {
            auto range=std::from_chars(next+1,end,last);next=range.ptr;
            if(range.ec!=std::errc{} || last<first || last>=CPU_SETSIZE)return {};
        }
        for(int cpu=first;cpu<=last;++cpu)result.push_back(cpu);
        if(next==end)break;
        if(*next!=',')return {};
        pos=size_t(next-list.data())+1;
    }
    return result;
}
int ranking(const std::string& directory) {
    for(auto* name:{"/cpufreq/amd_pstate_prefcore_ranking","/acpi_cppc/highest_perf"}) {
        int value=0;std::ifstream file(directory+name);if(file>>value)return value;
    }
    return 0;
}
bool ownPin(const cpu_set_t& current) {return pinned && CPU_EQUAL(&current,&selected);}
}
void initialize() {
    // Initialize once on the process main thread, before any worker uses it.
    if(initialized)return;
    initialized=true;
    cpu_set_t allowed{};
    if(pthread_getaffinity_np(pthread_self(),sizeof(allowed),&allowed)!=0)return;
    std::vector<Cpu> cpus;
    for(int id=0;id<CPU_SETSIZE;++id) {
        if(!CPU_ISSET(id,&allowed))continue;
        const auto dir="/sys/devices/system/cpu/cpu"+std::to_string(id);
        Cpu cpu{id,ranking(dir),readList(dir+"/topology/thread_siblings_list"),{}};
        if(cpu.siblings.empty())return;
        int highest=0;
        // LLC need not be index3 or even level3 (e.g. ARM and Intel E cores).
        for(int index=0;index<16;++index) {
            const auto cache=dir+"/cache/index"+std::to_string(index);
            int level=0;std::string type;std::ifstream levels(cache+"/level"),types(cache+"/type");
            if((levels>>level) && (types>>type) && type!="Instruction" && level>highest) {
                auto shared=readList(cache+"/shared_cpu_list");
                if(!shared.empty()){highest=level;cpu.cache=std::move(shared);}
            }
        }
        cpus.push_back(std::move(cpu));
    }
    placement=select(cpus);
}
bool pinCurrentThread(Lane lane) {
    int cpu=lane==Lane::Simulation?placement.simulation:lane==Lane::Publication?placement.publication:placement.render;
    if(cpu<0)return false;
    cpu_set_t current{},target{};
    if(pthread_getaffinity_np(pthread_self(),sizeof(current),&current)!=0)return false;
    // Never undo an outside affinity restriction installed since initialization.
    const auto& allowed=ownPin(current)?original:current;
    if(!CPU_ISSET(cpu,&allowed))return false;
    CPU_ZERO(&target);CPU_SET(cpu,&target);
    if(pthread_setaffinity_np(pthread_self(),sizeof(target),&target)!=0)return false;
    original=allowed;selected=target;pinned=true;
    fprintf(stderr,"Affinity: %s thread on CPU %d\n",lane==Lane::Simulation?"simulation":lane==Lane::Publication?"publication":"render",cpu);
    return true;
}
bool restoreCurrentThread() {
    cpu_set_t current{};
    if(pthread_getaffinity_np(pthread_self(),sizeof(current),&current)!=0 || !ownPin(current))return false;
    if(pthread_setaffinity_np(pthread_self(),sizeof(original),&original)!=0)return false;
    pinned=false;return true;
}
ScopedUnpin::ScopedUnpin(bool workers) {
    if(pthread_getaffinity_np(pthread_self(),sizeof(previous_),&previous_)!=0)return;
    cpu_set_t mask=ownPin(previous_)?original:previous_;
    if(workers && !placement.workers.empty()) {
        cpu_set_t background{};CPU_ZERO(&background);
        for(int cpu:placement.workers)if(CPU_ISSET(cpu,&mask))CPU_SET(cpu,&background);
        if(CPU_COUNT(&background)>0)mask=background;
    }
    if(!CPU_EQUAL(&previous_,&mask))
        changed_=pthread_setaffinity_np(pthread_self(),sizeof(mask),&mask)==0;
}
void ScopedUnpin::restore() {
    if(changed_ && pthread_setaffinity_np(pthread_self(),sizeof(previous_),&previous_)==0)changed_=false;
}
#else
void initialize() {}
bool pinCurrentThread(Lane) {return false;}
bool restoreCurrentThread() {return false;}
ScopedUnpin::ScopedUnpin(bool) {}
void ScopedUnpin::restore() {}
#endif
ScopedUnpin::~ScopedUnpin() {restore();}
} // namespace Native::Affinity
