// Copyright (c) 2026 Aaron. SPDX-License-Identifier: MIT
#include "thread_affinity.hpp"
#include <cstdio>
#include <stdexcept>
#ifdef __linux__
#include <pthread.h>
#endif
using namespace Native::Affinity;
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
void topologies() {
    std::vector<Cpu> cpus;
    for(int i=0;i<32;++i) {
        std::vector<int> cache;
        for(int j=0;j<32;++j)if((j%16)/8==(i%16)/8)cache.push_back(j);
        cpus.push_back({i,100,{i%16,i%16+16},std::move(cache)});
    }
    auto p=select(cpus);
    require(p.simulation==7 && p.publication==8 && p.render==15,"two LLC domains / SMT placement");
    require(p.workers.size()==12,"workers prefer render LLC and exclude all three physical cores");
    for(int id:{7,8,15,23,24,31})
        for(int worker:p.workers)require(worker!=id,"worker SMT exclusion");
    const int ranks[]{231,236,221,206,211,236,226,216};
    for(int i=0;i<8;++i){cpus[i].ranking=ranks[i];cpus[i+16].ranking=ranks[i];}
    p=select(cpus);
    require(p.simulation==2 && p.render==15,"simulation avoids most preferred desktop cores within fast band");
    for(int i=0;i<8;++i){cpus[i].ranking=100;cpus[i+16].ranking=100;}
    cpus[14].ranking=200;cpus[30].ranking=200;
    p=select(cpus);
    require(p.render==14 && p.simulation==7 && p.publication==8,"preferred core ranking");
    cpus.erase(cpus.begin(),cpus.begin()+16);
    p=select(cpus);
    require(p.render==30 && p.simulation==23 && p.publication==24,"sparse allowed hardware threads");
    cpus={{2,0,{2},{2,5,9,13}},{5,0,{5},{2,5,9,13}},
          {9,0,{9},{2,5,9,13}},{13,0,{13},{2,5,9,13}}};
    p=select(cpus);
    require(p.render==13 && p.simulation==9 && p.publication==2 && p.workers==std::vector<int>{5},"one cache / sparse cores");
    cpus.pop_back();require(select(cpus).render<0,"small machine leaves scheduling alone");
    // Sibling lists, unlike core_id, remain unambiguous across CPU packages.
    cpus={{1,0,{1,7},{1,7,3,9}},{7,0,{1,7},{1,7,3,9}},
          {3,0,{3,9},{1,7,3,9}},{9,0,{3,9},{1,7,3,9}},
          {20,0,{20,40},{20,40,22,42}},{40,0,{20,40},{20,40,22,42}},
          {22,0,{22,42},{20,40,22,42}},{42,0,{22,42},{20,40,22,42}}};
    p=select(cpus);
    require(p.render==22 && p.simulation==3 && p.publication==20,"multiple packages with repeated core IDs");
}
#ifdef __linux__
cpu_set_t mask() {
    cpu_set_t result{};
    require(pthread_getaffinity_np(pthread_self(),sizeof(result),&result)==0,"read affinity");
    return result;
}
void runtime() {
    const auto original=mask();initialize();pinCurrentThread(Lane::Render);
    const auto pinned=mask();cpu_set_t intersection{};
    CPU_AND(&intersection,&original,&pinned);
    require(CPU_EQUAL(&intersection,&pinned),"pin outside inherited mask");
    cpu_set_t child{};int status=-1;
    auto thread=createThread([&]{status=pthread_getaffinity_np(pthread_self(),sizeof(child),&child);});
    thread.join();require(status==0 && CPU_EQUAL(&child,&original),"child inherited temporary pin");
    auto after=mask();require(CPU_EQUAL(&after,&pinned),"thread creation did not restore pin");
    {
        ScopedUnpin background(true);const auto workers=mask();
        CPU_AND(&intersection,&original,&workers);
        require(CPU_EQUAL(&intersection,&workers) && CPU_COUNT(&workers)>0,"invalid worker mask");
        auto worker=std::thread([&]{status=pthread_getaffinity_np(pthread_self(),sizeof(child),&child);});
        worker.join();require(status==0 && CPU_EQUAL(&child,&workers),"library worker mask inheritance");
    }
    after=mask();require(CPU_EQUAL(&after,&pinned),"worker scope did not restore pin");
    restoreCurrentThread();after=mask();require(CPU_EQUAL(&after,&original),"original mask restore");
    int cpu=-1;
    for(int id=0;id<CPU_SETSIZE;++id)if(CPU_ISSET(id,&original) && !CPU_ISSET(id,&pinned)){cpu=id;break;}
    if(cpu>=0) {
        cpu_set_t restricted{};CPU_ZERO(&restricted);CPU_SET(cpu,&restricted);
        require(pthread_setaffinity_np(pthread_self(),sizeof(restricted),&restricted)==0,"restrict affinity");
        pinCurrentThread(Lane::Render);after=mask();require(CPU_EQUAL(&after,&restricted),"pin undid later restriction");
        {ScopedUnpin background(true);after=mask();require(CPU_EQUAL(&after,&restricted),"worker scope expanded later restriction");}
        require(pthread_setaffinity_np(pthread_self(),sizeof(original),&original)==0,"restore test mask");
    }
}
#endif
}
int main() {
    try {
        topologies();
#ifdef __linux__
        runtime();
#endif
        puts("Affinity: topology, SMT exclusion, restrictions, child inheritance and restoration passed");return 0;
    }catch(const std::exception& e){fprintf(stderr,"Affinity check: %s\n",e.what());return 1;}
}
