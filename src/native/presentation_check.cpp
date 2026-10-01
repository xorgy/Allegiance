#include "presentation.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

namespace {
constexpr uint64_t period=10'000'000;
struct Event {uint32_t id{};uint64_t time{};};
std::array<Event,128> events{};
size_t written{},read{};
uint64_t origin{},nextDisplay{},waits{};
int64_t clockOffset{};
bool report{};
void require(bool value,const char* text) {if(!value)throw std::runtime_error(text);}
VKAPI_ATTR VkResult VKAPI_CALL past(VkDevice,VkSwapchainKHR,uint32_t* count,VkPastPresentationTimingGOOGLE* data) {
    uint32_t copied=0;auto now=Native::monotonicNs();
    while(report && read<written && events[read].time<=now && copied<*count) {
        auto event=events[read++];data[copied++]={event.id,0,uint64_t(int64_t(event.time)+clockOffset),event.time,0};
    }
    *count=copied;return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL refresh(VkDevice,VkSwapchainKHR,VkRefreshCycleDurationGOOGLE* duration) {
    duration->refreshDuration=period;return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL gpuComplete(VkDevice,VkSwapchainKHR,const VkPresentWait2InfoKHR*) {
    ++waits;return VK_SUCCESS; // GPU completion is deliberately earlier than display.
}
}
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice,const char* name) {
    if(!std::strcmp(name,"vkGetPastPresentationTimingGOOGLE"))return reinterpret_cast<PFN_vkVoidFunction>(past);
    if(!std::strcmp(name,"vkGetRefreshCycleDurationGOOGLE"))return reinterpret_cast<PFN_vkVoidFunction>(refresh);
    if(!std::strcmp(name,"vkWaitForPresent2KHR"))return reinterpret_cast<PFN_vkVoidFunction>(gpuComplete);
    return nullptr;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue,const VkPresentInfoKHR* info) {
    const VkPresentTimeGOOGLE* time=nullptr;uint64_t id{};
    for(auto* entry=static_cast<const VkBaseInStructure*>(info->pNext);entry;entry=entry->pNext) {
        if(entry->sType==VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE)time=reinterpret_cast<const VkPresentTimesInfoGOOGLE*>(entry)->pTimes;
        if(entry->sType==VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR)id=*reinterpret_cast<const VkPresentId2KHR*>(entry)->pPresentIds;
    }
    require(time && id && time->presentID==uint32_t(id),"display reports and present-wait must identify the same frame");
#ifdef __APPLE__
    require(time->desiredPresentTime==0,"Mac ready drawables must not be held behind timed Metal presentation");
#endif
    uint64_t earliest=Native::monotonicNs();
    if(time->desiredPresentTime)earliest=std::max(earliest,uint64_t(int64_t(time->desiredPresentTime)-clockOffset));
    auto candidate=origin+((earliest-origin)/period+1)*period;
#ifdef __APPLE__
    candidate+=period; // Native compositor output follows the drawable latch.
#endif
    nextDisplay=std::max(nextDisplay+period,candidate); // FIFO: distinct refreshes.
    require(written<events.size(),"synthetic presentation arena overflow");
    events[written++]={time->presentID,nextDisplay};return VK_SUCCESS;
}
int main() {
    try {
#ifdef __APPLE__
        mach_timebase_info_data_t base{};mach_timebase_info(&base);auto ticks=mach_absolute_time();
        auto mach=ticks/base.denom*base.numer+ticks%base.denom*base.numer/base.denom;
        clockOffset=int64_t(mach)-int64_t(Native::monotonicNs());
#endif
        origin=Native::monotonicNs();Native::Presentation presentation;
        VkPresentTimingSurfaceCapabilitiesEXT caps{VK_STRUCTURE_TYPE_PRESENT_TIMING_SURFACE_CAPABILITIES_EXT};
        presentation.reset({}, {},true,false,caps,100,true);
        auto trace=std::unique_ptr<FILE,decltype(&fclose)>(tmpfile(),fclose);
        require(bool(trace),"cannot create feedback regression trace");presentation.trace(trace.get());
        require(presentation.timed(),"GOOGLE display feedback must be enabled independently of EXT timing");
        auto submit=[&] {
            Native::sleepUntil(Native::monotonicNs()+500'000);
            VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            require(presentation.present({},info)==VK_SUCCESS,"synthetic present failed");
            // Rendering may finish before vkQueuePresent's CPU encoding ends.
            // Explicit GPU counters must preserve that overlap without turning
            // early present-wait completion into a fictitious display event.
            presentation.gpuReady(presentation.id(),Native::monotonicNs()-1'000'000);
        };
        presentation.begin();submit();presentation.begin();
        require(waits && presentation.lastLatencyMs()==0 && presentation.targetPresentTime()==0,
                "early GPU present-wait completion must not seed display timing or latency");
        submit();report=true;uint64_t targets=0;auto tick=Native::monotonicNs();
        for(int frame=0;frame<118;++frame) {
            tick+=period;Native::sleepUntil(tick);presentation.begin();
            auto sampled=Native::monotonicNs(),target=presentation.targetPresentTime();
            if(target>sampled) {
                ++targets;Native::FrameTiming timing{sampled,target};
                require(timing.evaluatedNs()==target,"interpolation must evaluate the scheduled display deadline");
            }
            submit();
        }
        require(presentation.pacer().GetRefreshPeriod()==period && targets>=10,
                "actual display reports must establish refresh pacing and future interpolation targets");
        require(presentation.lastLatencyMs()>0 && presentation.lastLatencyMs()<100,
                "display feedback must be calibrated into the application's monotonic clock");
        rewind(trace.get());unsigned long long segment{},frame{},sampled{},submitted{},ready{},shown{},deadline{},misses{};
        require(fscanf(trace.get(),"%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",&segment,&frame,&sampled,&submitted,&ready,&shown,&deadline,&misses)==8 &&
                ready==submitted && shown>ready && sampled<=submitted,
                "GPU completion overlapping CPU present encoding must clamp to submission, while display feedback stays distinct");
        presentation.reset({}, {},true,false,caps,100,true);
        require(!presentation.id() && !presentation.targetPresentTime() && !presentation.lastLatencyMs(),
                "swapchain recreation must discard old presentation identities and deadlines");
        Native::CFramePacer calibrated;calibrated.SetRefreshPeriod(period,Native::CFramePacer::RefreshMode::FIXED);
        calibrated.SetPresentationLead(period);calibrated.SetGpuCompletionMeasured(true);
        uint64_t clock=1'000'000'000;
        for(uint64_t id=1;id<=180;++id) {
            auto schedule=calibrated.Schedule(clock);auto start=std::max(clock,schedule.start);
            calibrated.OnFrameStarted(id,start,schedule);calibrated.OnFramePresented(id,start+500'000);calibrated.OnFrameReady(id,start+1'500'000);
            auto display=((start+1'500'000)/period+1)*period+period;calibrated.OnFrameDisplayed(id,display);clock=start+period;
        }
        // SDL and display timing can report periods that differ by a nanosecond.
        // A refresh update must retain the measured-GPU margin policy, rather
        // than silently restoring a blind quarter-refresh GPU allowance.
        calibrated.SetRefreshPeriod(period+1,Native::CFramePacer::RefreshMode::FIXED);calibrated.SetPresentationLead(period+1);
        auto measured=calibrated.Schedule(clock);
        require(measured.deadline && measured.deadline-measured.start<period+2'500'000,
                "refresh updates must retain the latency benefit of calibrated GPU completion");
        calibrated.SetGpuCompletionMeasured(false);auto fallback=calibrated.Schedule(clock);
        require(fallback.deadline-fallback.start>=period+period/4,
                "loss of reliable GPU counters must restore the conservative pacing margin");
        puts("Presentation: early GPU completion, actual display feedback, calibrated clocks, convergent deadlines, interpolation targets and swapchain reset passed");
        return 0;
    } catch(const std::exception& error) {fprintf(stderr,"Presentation check: %s\n",error.what());return 1;}
}
