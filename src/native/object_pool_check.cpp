#include "object_pool.hpp"
#include <array>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unordered_set>

#if defined(__SANITIZE_ADDRESS__)
#define ALLEGIANCE_POOL_CHECK_ASAN
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ALLEGIANCE_POOL_CHECK_ASAN
#endif
#endif

static void require(bool value) { if(!value)std::abort(); }
template<std::size_t Size,std::size_t Alignment> static void check() {
    using Pool=Native::ObjectPool<Size,Alignment>;
    constexpr int count=4097;
    std::array<void*,count> original{},replacement{};
    std::unordered_set<void*> live;
    for(auto& p:original) {
        p=Pool::allocate();require(std::uintptr_t(p)%Alignment==0);
        require(live.insert(p).second);std::memset(p,0x37,Size);
    }
    for(int i=0;i<count;i+=2) {
        auto* p=original[i];live.erase(p);Pool::release(p);
#ifdef ALLEGIANCE_POOL_CHECK_ASAN
        require(__asan_address_is_poisoned(p));
        require(__asan_address_is_poisoned(static_cast<char*>(p)+Size-1));
#endif
    }
    for(int i=0;i<count;i+=2) {
        auto* p=replacement[i]=Pool::allocate();require(live.insert(p).second);
        require(std::uintptr_t(p)%Alignment==0);std::memset(p,0x72,Size);
    }
    for(int i=0;i<count;++i) {
        auto* p=static_cast<unsigned char*>(i%2?original[i]:replacement[i]);
        for(std::size_t b=0;b<Size;++b)require(p[b]==(i%2?0x37:0x72));
        Pool::release(p);
    }
}
int main() {
    auto run=[]{check<64,16>();check<264,8>();check<416,8>();check<552,8>();};
    std::thread a(run),b(run);run();a.join();b.join();
    puts("Native pools: multi-block alignment, live uniqueness, reuse isolation, thread teardown and ASan retirement poisoning passed.");
}
