#pragma once
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace Native {
// Keep a historical page anchored when new messages arrive or the bounded
// history evicts old entries. Original ChatListPane restores autoscroll after
// ten seconds, and PageDown restores it immediately at the newest page.
class MessagePager {
public:
    static constexpr size_t rows=12;
    void setRows(size_t count){count=std::max(size_t(1),count);if(count!=rows_){rows_=count;reset();}}
    size_t pageRows()const{return rows_;}
    size_t offset()const{return offset_;}
    bool following()const{return following_;}
    void reset(){offset_=0;latest_=anchor_=0;until_=0;following_=true;}
    template<class History> size_t update(const History& history,double now){
        if(!following_ && now>=until_)reset();
        if(history.empty()){offset_=0;latest_=anchor_=0;return 0;}
        auto serial=history.back().serial;
        if(serial<latest_)reset();
        else if(serial!=latest_ && !following_ && anchor_) {
            auto found=std::lower_bound(history.begin(),history.end(),anchor_,[](const auto& entry,uint64_t anchor){return entry.serial<anchor;});
            offset_=maximum(history.size())-std::min(maximum(history.size()),size_t(found-history.begin()));
        } else if(serial!=latest_)offset_=following_?0:maximum(history.size());
        latest_=serial;offset_=std::min(offset_,maximum(history.size()));remember(history);return offset_;
    }
    template<class History> void page(const History& history,double now,bool older){
        update(history,now);offset_=older?std::min(maximum(history.size()),offset_+rows_):offset_>rows_?offset_-rows_:0;
        following_=!older && offset_==0 && !history.empty();until_=following_?0:now+10;remember(history);
    }
private:
    size_t maximum(size_t count)const{return count>rows_?count-rows_:0;}
    template<class History> void remember(const History& history){anchor_=history.empty()?0:history[maximum(history.size())-offset_].serial;}
    size_t rows_{rows},offset_{};uint64_t latest_{},anchor_{};double until_{};bool following_{true};
};
}
