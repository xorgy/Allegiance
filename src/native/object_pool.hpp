// Native IGC objects remain confined to their owning simulation thread. Pools
// retain peak storage until that thread exits; no live object may outlast it.
#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <new>
#include <vector>
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ALLEGIANCE_POOL_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) || defined(ALLEGIANCE_POOL_ASAN)
#include <sanitizer/asan_interface.h>
#endif

namespace Native {
template<std::size_t Size, std::size_t Alignment> class ObjectPool {
    struct Header { ObjectPool* owner; Header* next; };
    static constexpr auto alignment = std::max(Alignment, alignof(Header));
    static constexpr auto prefix = (sizeof(Header)+alignment-1)&~(alignment-1);
    static constexpr auto stride = (prefix+Size+alignment-1)&~(alignment-1);
    static constexpr std::size_t blockCount = 256;
    ObjectPool()=default;
    ObjectPool(const ObjectPool&)=delete;
    ObjectPool& operator=(const ObjectPool&)=delete;
    Header* free_{};
    std::size_t live_{};
    std::vector<void*> blocks_;
#if defined(__SANITIZE_ADDRESS__) || defined(ALLEGIANCE_POOL_ASAN)
    // Preserve an ASan quarantine, rather than immediately unpoisoning a
    // recycled address and hiding short-lived stale references.
    std::array<Header*,64> quarantine_{};
    std::size_t quarantineCount_{},quarantineCursor_{};
#endif
    static void poison(void* p) {
#if defined(__SANITIZE_ADDRESS__) || defined(ALLEGIANCE_POOL_ASAN)
        __asan_poison_memory_region(p, Size);
#endif
    }
    static void unpoison(void* p, std::size_t bytes) {
#if defined(__SANITIZE_ADDRESS__) || defined(ALLEGIANCE_POOL_ASAN)
        __asan_unpoison_memory_region(p, bytes);
#endif
    }
    void grow() {
        void* block = ::operator new(stride*blockCount, std::align_val_t{alignment});
        try { blocks_.push_back(block); }
        catch (...) { ::operator delete(block, std::align_val_t{alignment}); throw; }
        for(std::size_t i=0;i<blockCount;++i) {
            auto* storage = static_cast<std::byte*>(block)+i*stride;
            free_ = ::new(storage) Header{this, free_};
            poison(storage+prefix);
        }
    }
    static ObjectPool& local() { static thread_local ObjectPool pool; return pool; }
public:
    ~ObjectPool() {
        assert(live_==0);
        // Do not invalidate an outstanding payload if a caller violates the
        // ownership contract. LeakSanitizer can then report that allocation.
        if(live_)return;
        for(void* block:blocks_) {
            unpoison(block,stride*blockCount);
            ::operator delete(block,std::align_val_t{alignment});
        }
    }
    static void* allocate() {
        auto& pool=local();if(!pool.free_)pool.grow();
        auto* header=pool.free_;pool.free_=header->next;++pool.live_;
        auto* payload=reinterpret_cast<std::byte*>(header)+prefix;
        unpoison(payload,Size);return payload;
    }
    static void release(void* payload) noexcept {
        if(!payload)return;
        auto* header=reinterpret_cast<Header*>(static_cast<std::byte*>(payload)-prefix);
        auto& pool=*header->owner;
        assert(&pool==&local() && pool.live_);
        poison(payload);--pool.live_;
#if defined(__SANITIZE_ADDRESS__) || defined(ALLEGIANCE_POOL_ASAN)
        if(pool.quarantineCount_==pool.quarantine_.size()) {
            auto* oldest=pool.quarantine_[pool.quarantineCursor_];
            oldest->next=pool.free_;pool.free_=oldest;
        }else ++pool.quarantineCount_;
        pool.quarantine_[pool.quarantineCursor_]=header;
        pool.quarantineCursor_=(pool.quarantineCursor_+1)%pool.quarantine_.size();
#else
        header->next=pool.free_;pool.free_=header;
#endif
    }
};
}
#undef ALLEGIANCE_POOL_ASAN
