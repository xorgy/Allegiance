#pragma once
#include "FramePacer.h"
#include "frame_timing.hpp"
#include <vulkan/vulkan.h>
#include <array>
#include <cstdio>

namespace Native {
void sleepUntil(uint64_t time);

// Presentation feedback is in the compositor/display clock, calibrated to
// CLOCK_MONOTONIC. CPU fence completion is never treated as display feedback.
class Presentation {
public:
    void reset(VkDevice device,VkSwapchainKHR swapchain,bool wait2,bool timing,
        const VkPresentTimingSurfaceCapabilitiesEXT& caps,double refreshHz,bool googleTiming=false);
    void begin();
    void gpuReady(uint64_t id,uint64_t time);
    void gpuTiming(bool reliable);
    VkResult present(VkQueue queue,VkPresentInfoKHR& info);
    uint64_t id() const { return id_; }
    const CFramePacer& pacer() const { return pacer_; }
    bool timed() const { return timing_ || google_; }
    double lastLatencyMs() const { return latencyMs_; }
    // Expected display time, not the earlier compositor request time (target).
    uint64_t targetPresentTime() const { return schedule_.deadline; }
    void trace(FILE* file) { trace_=file; }
private:
    bool chooseDomain();
    bool calibrate();
    void feedback();
    void googleFeedback();
    void displayed(uint64_t id,uint64_t ready,uint64_t shown);
    bool checkLate();
    void wait(uint64_t id,uint64_t until);
    VkResult waitOne(uint64_t id,uint64_t timeout);
    VkDevice device_{};
    VkSwapchainKHR swapchain_{};
    CFramePacer pacer_;
    CFramePacer::SSchedule schedule_{};
    uint64_t id_{},shown_{},waited_{},domainID_{},domainsCounter_{},propertiesCounter_{};
    VkTimeDomainKHR domain_{VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR};
    VkPresentStageFlagsEXT readyStage_{},shownStage_{};
    int64_t readyOffset_{},shownOffset_{},googleOffset_{};
    uint32_t pending_{},sinceCalibration_{};
    bool wait2_{},timing_{},google_{},gpuTiming_{};
    double latencyMs_{};
    std::array<uint64_t,64> starts_{};
    std::array<uint64_t,64> submitted_{},deadlines_{},scheduled_{};
    std::array<uint64_t,64> ready_{};
    FILE* trace_{};
    uint64_t segment_{};
    std::array<VkPastPresentationTimingEXT,32> timings_{};
    std::array<VkPresentStageTimeEXT,64> stages_{};
    std::array<VkPastPresentationTimingGOOGLE,32> googleTimings_{};
    PFN_vkGetPastPresentationTimingGOOGLE googlePast_{};
    PFN_vkGetRefreshCycleDurationGOOGLE googleRefresh_{};
    PFN_vkWaitForPresent2KHR waitForPresent_{};
    PFN_vkGetPastPresentationTimingEXT past_{};
    PFN_vkGetSwapchainTimeDomainPropertiesEXT domains_{};
    PFN_vkGetSwapchainTimingPropertiesEXT properties_{};
    PFN_vkSetSwapchainPresentTimingQueueSizeEXT queueSize_{};
    PFN_vkGetCalibratedTimestampsKHR timestamps_{};
};
}
