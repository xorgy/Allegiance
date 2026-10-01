#include "presentation.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cinttypes>
#include <limits>
#include <time.h>
#include <vector>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#endif

namespace Native {
void sleepUntil(uint64_t time) {
#ifdef _WIN32
    struct Timer {
        HANDLE handle=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
        Timer(){if(!handle)handle=CreateWaitableTimerW(nullptr,FALSE,nullptr);}
        ~Timer(){if(handle)CloseHandle(handle);}
    };
    thread_local Timer timer;
    auto now=monotonicNs();if(time<=now)return;
    if(timer.handle) {
        LARGE_INTEGER due;due.QuadPart=-LONGLONG((time-now+99)/100);
        if(SetWaitableTimer(timer.handle,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer.handle,INFINITE);
    } else Sleep(DWORD((time-now+999'999)/1'000'000));
#elif defined(__APPLE__)
    // mach_wait_until applies normal timer coalescing even at interactive QoS
    // (5 ms in the measured 10 ms waits). A reusable strict one-shot dispatch
    // timer honors the frame deadline without spinning or raising priorities.
    struct Timer {
        dispatch_semaphore_t fired=dispatch_semaphore_create(0),drained=dispatch_semaphore_create(0);
        dispatch_source_t source{};
        Timer() {
            if(!fired || !drained)return;
            source=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,DISPATCH_TIMER_STRICT,dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE,0));
            if(!source)return;
            dispatch_set_context(source,this);
            dispatch_source_set_event_handler_f(source,[](void* p){dispatch_semaphore_signal(static_cast<Timer*>(p)->fired);});
            dispatch_source_set_cancel_handler_f(source,[](void* p){dispatch_semaphore_signal(static_cast<Timer*>(p)->drained);});
            dispatch_activate(source);
        }
        ~Timer() {
            if(source) {
                dispatch_source_cancel(source);dispatch_semaphore_wait(drained,DISPATCH_TIME_FOREVER);dispatch_release(source);
            }
            if(fired)dispatch_release(fired);
            if(drained)dispatch_release(drained);
        }
    };
    thread_local Timer timer;
    auto now=monotonicNs();if(time<=now)return;
    if(timer.source) {
        dispatch_source_set_timer(timer.source,dispatch_time(DISPATCH_TIME_NOW,int64_t(time-now)),DISPATCH_TIME_FOREVER,0);
        dispatch_semaphore_wait(timer.fired,DISPATCH_TIME_FOREVER);return;
    }
    // Allocation failure retains the kernel wait as a conservative fallback.
    static const auto base=[] {mach_timebase_info_data_t value;mach_timebase_info(&value);return value;}();
    do {
        auto remaining=time-now;
        auto ticks=remaining/base.numer*base.denom + remaining%base.numer*base.denom/base.numer;
        mach_wait_until(mach_absolute_time()+ticks);now=monotonicNs();
    } while(now<time);
#else
    timespec t{time_t(time/1'000'000'000),long(time%1'000'000'000)};
    while(clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&t,nullptr)==EINTR) {}
#endif
}
void Presentation::reset(VkDevice device,VkSwapchainKHR swapchain,bool wait2,bool timing,
    const VkPresentTimingSurfaceCapabilitiesEXT& caps,double refreshHz,bool googleTiming) {
    auto* trace=trace_;auto segment=segment_+1;
    *this=Presentation{};trace_=trace;segment_=segment;device_=device;swapchain_=swapchain;wait2_=wait2;timing_=timing;
#define LOAD(field, name) field=reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device,#name))
    LOAD(waitForPresent_,vkWaitForPresent2KHR);
    LOAD(past_,vkGetPastPresentationTimingEXT);
    LOAD(domains_,vkGetSwapchainTimeDomainPropertiesEXT);
    LOAD(properties_,vkGetSwapchainTimingPropertiesEXT);
    LOAD(queueSize_,vkSetSwapchainPresentTimingQueueSizeEXT);
    LOAD(timestamps_,vkGetCalibratedTimestampsKHR);
    LOAD(googlePast_,vkGetPastPresentationTimingGOOGLE);
    LOAD(googleRefresh_,vkGetRefreshCycleDurationGOOGLE);
#undef LOAD
    readyStage_=caps.presentStageQueries&VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT;
    shownStage_=(caps.presentStageQueries&VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT) ?
        VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT : VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT;
    timing_=timing_ && queueSize_ && queueSize_(device_,swapchain_,timings_.size())==VK_SUCCESS && chooseDomain();
    for(size_t i=0;i<timings_.size();++i) {timings_[i].sType=VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_EXT;timings_[i].pPresentStages=stages_.data()+i*2;}
    google_=!timing_ && googleTiming && googlePast_ && googleRefresh_;
    pacer_.SetEveryPresentationReported(timed());
    if(refreshHz>0) pacer_.SetRefreshPeriod(uint64_t(1e9/refreshHz),google_?CFramePacer::RefreshMode::FIXED:CFramePacer::RefreshMode::UNKNOWN);
    fprintf(stderr,"Presentation: %s; initial refresh %.3f Hz\n",timing_ ? "EXT present timing" : google_ ? "GOOGLE display timing" : wait2_ ? "present wait2" : "FIFO acquire",refreshHz);
}
bool Presentation::chooseDomain() {
    VkSwapchainTimeDomainPropertiesEXT p{VK_STRUCTURE_TYPE_SWAPCHAIN_TIME_DOMAIN_PROPERTIES_EXT};
    if(domains_(device_,swapchain_,&p,nullptr)!=VK_SUCCESS) return false;
    std::vector<VkTimeDomainKHR> domains(p.timeDomainCount);std::vector<uint64_t> ids(p.timeDomainCount);
    p.pTimeDomains=domains.data();p.pTimeDomainIds=ids.data();
    if(domains_(device_,swapchain_,&p,&domainsCounter_)!=VK_SUCCESS) return false;
    for(auto preferred:{
#ifdef _WIN32
        VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR,
#else
        VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR,
#endif
        VK_TIME_DOMAIN_SWAPCHAIN_LOCAL_EXT,VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT}) {
        auto it=std::find(domains.begin(),domains.begin()+p.timeDomainCount,preferred);
        if(it!=domains.begin()+p.timeDomainCount) {domain_=preferred;domainID_=ids[it-domains.begin()];return calibrate();}
    }
    return false;
}
void Presentation::gpuTiming(bool reliable) {
    if(gpuTiming_==reliable)return;
    gpuTiming_=reliable;
    fprintf(stderr,"GPU completion feedback: %s\n",reliable?"calibrated counters":"conservative fallback");
    // Real rendering-completion measurements cover GPU cost. The remaining
    // Mac latch uncertainty starts at the normal decay floor and still adapts
    // from display misses. Without reliable counters retain the safe estimate.
    pacer_.SetGpuCompletionMeasured(reliable);
}
void Presentation::gpuReady(uint64_t id,uint64_t time) {
    if(!time || id>id_ || id_-id>=ready_.size())return;
    // The GPU can finish while vkQueuePresent is still being encoded on the
    // CPU. That overlap has no additional GPU tail after submission.
    ready_[id%ready_.size()]=std::max(time,submitted_[id%ready_.size()]);
    pacer_.OnFrameReady(id,ready_[id%ready_.size()]);
}
bool Presentation::calibrate() {
    sinceCalibration_=0;
    if(domain_==VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR) {readyOffset_=shownOffset_=0;return true;}
#ifdef _WIN32
    // EXT presentation fields are nanoseconds even for the QPC domain;
    // KHR calibrated timestamps alone return raw QPC counter units.
    if(domain_==VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR) {readyOffset_=shownOffset_=0;return true;}
#endif
    auto offset=[&](VkPresentStageFlagsEXT stage,int64_t& out) {
        VkSwapchainCalibratedTimestampInfoEXT swap{VK_STRUCTURE_TYPE_SWAPCHAIN_CALIBRATED_TIMESTAMP_INFO_EXT};
        swap.swapchain=swapchain_;swap.timeDomainId=domainID_;swap.presentStage=domain_==VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT ? stage : 0;
        VkCalibratedTimestampInfoKHR infos[2]={{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR},{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR}};
        infos[0].pNext=&swap;infos[0].timeDomain=domain_;
#ifdef _WIN32
        infos[1].timeDomain=VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR;
#else
        infos[1].timeDomain=VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR;
#endif
        uint64_t times[2],deviation{};
        if(timestamps_(device_,2,infos,times,&deviation)!=VK_SUCCESS) return false;
#ifdef _WIN32
        times[1]=performanceNs(times[1]);
#endif
        out=int64_t(times[0])-int64_t(times[1]);return true;
    };
    return offset(shownStage_,shownOffset_) && (!readyStage_ || offset(readyStage_,readyOffset_));
}
void Presentation::feedback() {
    for(auto& t:timings_) t.presentStageCount=2;
    VkPastPresentationTimingInfoEXT info{VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_INFO_EXT};info.swapchain=swapchain_;
    VkPastPresentationTimingPropertiesEXT props{VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_PROPERTIES_EXT};
    props.presentationTimingCount=timings_.size();props.pPresentationTimings=timings_.data();
    auto result=past_(device_,&info,&props);if(result!=VK_SUCCESS && result!=VK_INCOMPLETE)return;
    if(props.timeDomainsCounter!=domainsCounter_) timing_=chooseDomain();
    else if(sinceCalibration_>=256) timing_=calibrate();
    pacer_.SetEveryPresentationReported(timing_);
    for(uint32_t i=0;i<props.presentationTimingCount;++i) {
        auto& t=timings_[i];if(!t.reportComplete)continue;
        pending_-=std::min(pending_,1u);++sinceCalibration_;uint64_t ready{},shown{};
        for(uint32_t s=0;s<t.presentStageCount && t.timeDomainId==domainID_;++s) {
            auto& time=t.pPresentStages[s];if(!time.time)continue;
            uint64_t stamp=time.time;
            if(time.stage==readyStage_)ready=stamp-readyOffset_;
            else if(time.stage==shownStage_)shown=stamp-shownOffset_;
        }
        displayed(t.presentId,ready,shown);
    }
    if(props.timingPropertiesCounter==propertiesCounter_ && pacer_.GetRefreshPeriod()!=0)return;
    VkSwapchainTimingPropertiesEXT timing{VK_STRUCTURE_TYPE_SWAPCHAIN_TIMING_PROPERTIES_EXT};uint64_t counter{};
    if(properties_(device_,swapchain_,&timing,&counter)!=VK_SUCCESS || !timing.refreshDuration)return;
    propertiesCounter_=counter;
    auto mode=timing.refreshInterval==UINT64_MAX ? CFramePacer::RefreshMode::VARIABLE :
        timing.refreshInterval==0 ? CFramePacer::RefreshMode::UNKNOWN : CFramePacer::RefreshMode::FIXED;
    pacer_.SetRefreshPeriod(timing.refreshDuration,mode);
}
void Presentation::displayed(uint64_t id,uint64_t ready,uint64_t shown) {
    if(ready)pacer_.OnFrameReady(id,ready);
    pacer_.OnFrameDisplayed(id,shown);shown_=std::max(shown_,id);
    if(shown && id<=id_ && id_-id<starts_.size() && shown>=starts_[id%starts_.size()]) {
        auto slot=id%starts_.size();latencyMs_=double(shown-starts_[slot])/1e6;
        if(trace_)fprintf(trace_,"%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",segment_,id,starts_[slot],submitted_[slot],ready,shown,deadlines_[slot],pacer_.GetMisses(),scheduled_[slot]);
    }
}
void Presentation::googleFeedback() {
#ifdef __APPLE__
    // Metal's drawable timestamps/presentAtTime use the mach absolute clock.
    // CLOCK_MONOTONIC has a different epoch on Darwin (and includes suspend).
    // Calibrate both directions before feedback/submission, including on wake.
    static const auto base=[] {mach_timebase_info_data_t value;mach_timebase_info(&value);return value;}();
    auto before=monotonicNs();auto ticks=mach_absolute_time();auto after=monotonicNs();
    auto mach=ticks/base.denom*base.numer + ticks%base.denom*base.numer/base.denom;
    googleOffset_=int64_t(mach)-int64_t(before+(after-before)/2);
#endif
    uint32_t count=googleTimings_.size();
    auto result=googlePast_(device_,swapchain_,&count,googleTimings_.data());
    if(result==VK_SUCCESS || result==VK_INCOMPLETE)for(uint32_t i=0;i<count;++i) {
        auto& t=googleTimings_[i];
        // GOOGLE IDs are 32-bit. Resolve them against the bounded recent-frame
        // arena rather than truncating the pacer's 64-bit identity after wrap.
        uint64_t age=uint32_t(uint32_t(id_)-t.presentID);
        if(age>=starts_.size() || age>id_ || !t.actualPresentTime)continue;
        auto shown=int64_t(t.actualPresentTime)-googleOffset_;
        if(shown>0)displayed(id_-age,ready_[(id_-age)%ready_.size()],uint64_t(shown));
        // MoltenVK's earliestPresentTime and presentMargin are estimates, not
        // GPU-ready times. Only the drawable's actual timestamp feeds pacing.
    }
    if((id_&63)==0) {
        VkRefreshCycleDurationGOOGLE duration{};
        if(googleRefresh_(device_,swapchain_,&duration)==VK_SUCCESS && duration.refreshDuration)
            pacer_.SetRefreshPeriod(duration.refreshDuration,CFramePacer::RefreshMode::FIXED);
#ifdef __APPLE__
        // WindowServer composition adds one refresh between a drawable being
        // latched and its reported output. Account for that pipeline interval
        // separately from CPU/GPU cost and the adaptive latch margin. Otherwise
        // a margin capped below one refresh can never converge on this path.
        pacer_.SetPresentationLead(pacer_.GetRefreshPeriod());
#endif
    }
}
VkResult Presentation::waitOne(uint64_t id,uint64_t timeout) {
    VkPresentWait2InfoKHR info{VK_STRUCTURE_TYPE_PRESENT_WAIT_2_INFO_KHR};info.presentId=id;info.timeout=timeout;
    return waitForPresent_(device_,swapchain_,&info);
}
bool Presentation::checkLate() {
    if(!wait2_)return false;auto now=monotonicNs();
    while(shown_<id_) {
        auto next=shown_+1,overdue=pacer_.GetOverdueTime(next);if(!overdue || now<=overdue)return false;
        auto result=waitOne(next,0);
        if(result==VK_TIMEOUT){pacer_.OnFrameNotShownBy(next,now);return true;}
        if(result!=VK_SUCCESS && result!=VK_SUBOPTIMAL_KHR)return false;
        shown_=next;waited_=std::max(waited_,next);
    }
    return false;
}
void Presentation::wait(uint64_t id,uint64_t until) {
    while(waited_<id_ && (!id || waited_<id)) {
        auto next=waited_+1;uint64_t shown=0;auto result=waitOne(next,0);
        if(result==VK_TIMEOUT) {
            auto now=monotonicNs();if(now>=until)return;
            result=waitOne(next,until-now);shown=monotonicNs();
            if(result==VK_TIMEOUT && !google_)pacer_.OnFrameNotShownBy(next,shown);
        }
        if(result!=VK_SUCCESS && result!=VK_SUBOPTIMAL_KHR)return;
        waited_=next;if(!timed()){shown_=next;pacer_.OnFrameDisplayed(next,shown);}
    }
}
void Presentation::begin() {
    if(timing_) {feedback();checkLate();} else if(wait2_) wait(0,0);
    if(google_)googleFeedback();
    auto available=monotonicNs();schedule_=pacer_.Schedule(available);
#ifdef __APPLE__
    // The drawable was obtained before this call. A schedule chosen in the
    // past is not work or wake-up jitter: input could not have been sampled
    // before acquisition returned. Keep it out of the replay's CPU budget.
    if(schedule_.deadline)schedule_.start=std::max(schedule_.start,available);
#endif
    if(schedule_.deadline) {
        if(!timed() && wait2_)wait(0,schedule_.start);
        uint64_t last=0;
        while(timing_ && shown_<id_) {
            auto overdue=pacer_.GetOverdueTime(shown_+1);
            if(!overdue || overdue==last || overdue>=schedule_.start)break;
            sleepUntil(overdue+1);last=overdue;if(checkLate())schedule_=pacer_.Schedule(monotonicNs());
        }
        sleepUntil(schedule_.start);
    } else if(wait2_ && id_>=2) wait(id_-1,monotonicNs()+100'000'000);
    auto now=monotonicNs();auto slot=(id_+1)%starts_.size();starts_[slot]=now;deadlines_[slot]=schedule_.deadline;scheduled_[slot]=schedule_.start;ready_[slot]=0;pacer_.OnFrameStarted(id_+1,now,schedule_);
}
VkResult Presentation::present(VkQueue queue,VkPresentInfoKHR& info) {
    ++id_;
    VkPresentId2KHR id{VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR};id.swapchainCount=1;id.pPresentIds=&id_;
    VkPresentTimingInfoEXT timing{VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT};
    VkPresentTimingsInfoEXT timings{VK_STRUCTURE_TYPE_PRESENT_TIMINGS_INFO_EXT};
    VkPresentTimeGOOGLE googleTime{uint32_t(id_),0};
    VkPresentTimesInfoGOOGLE googleTimes{VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE};
    if(wait2_) {id.pNext=info.pNext;info.pNext=&id;}
    if(timing_) {
        timing.timeDomainId=domainID_;
        if(schedule_.deadline) {
            timing.targetTime=schedule_.target+shownOffset_;
            if(schedule_.nearestRefresh)timing.flags=VK_PRESENT_TIMING_INFO_PRESENT_AT_NEAREST_REFRESH_CYCLE_BIT_EXT;
            if(domain_==VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT)timing.targetTimeDomainPresentStage=shownStage_;
        }
        if(pending_<timings_.size()) {timing.presentStageQueries=readyStage_|shownStage_;++pending_;}
        timings.swapchainCount=1;timings.pTimingInfos=&timing;timings.pNext=info.pNext;info.pNext=&timings;
    }
    if(google_) {
#ifndef __APPLE__
        if(schedule_.deadline) {
            // GOOGLE is a strict "no sooner than" request, unlike EXT's
            // nearest-refresh flag. Leave a quarter-refresh window before the
            // predicted vblank so nanosecond phase error cannot add a refresh.
            auto target=schedule_.deadline-pacer_.GetRefreshPeriod()/4;
            auto native=int64_t(target)+googleOffset_;
            if(native>0)googleTime.desiredPresentTime=uint64_t(native);
        }
#endif
        // On macOS, presentAtTime holds the drawable until its request time,
        // then WindowServer still needs its composition interval. Measured
        // output consistently misses by another refresh. Use actual drawable
        // feedback to schedule CPU work instead; never hold an already-ready
        // frame back at the Metal layer.
        googleTimes.swapchainCount=1;googleTimes.pTimes=&googleTime;googleTimes.pNext=info.pNext;info.pNext=&googleTimes;
    }
#ifdef __APPLE__
    // Include MoltenVK's CPU presentation encoding. Rendering timestamps alone
    // do not cover the command that requests the drawable's actual display.
    auto result=vkQueuePresentKHR(queue,&info);auto now=monotonicNs();submitted_[id_%starts_.size()]=now;
    pacer_.OnFramePresented(id_,now);return result;
#else
    auto now=monotonicNs();submitted_[id_%starts_.size()]=now;
    pacer_.OnFramePresented(id_,now);return vkQueuePresentKHR(queue,&info);
#endif
}
}
