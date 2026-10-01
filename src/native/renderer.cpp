#include "renderer.hpp"
#include "../ClientCore/ModelLod.h"
#include "presentation.hpp"
#include "frustum.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#ifdef __APPLE__
#include <vulkan/vulkan_metal.h>
#endif
#include <algorithm>
#include <bit>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace Native {
void configureVulkanPlatform() {
#ifdef __APPLE__
    auto base=std::filesystem::path(SDL_GetBasePath());
    auto driver=base/"MoltenVK_icd.json";
    auto loader=base/"../Frameworks/libvulkan.1.dylib";
    if(std::filesystem::is_regular_file(driver) && std::filesystem::is_regular_file(loader)) {
        if(!getenv("VK_DRIVER_FILES") && !getenv("VK_ICD_FILENAMES"))setenv("VK_DRIVER_FILES",driver.c_str(),1);
        SDL_SetHint(SDL_HINT_VULKAN_LIBRARY,loader.c_str());
    }
#endif
}
namespace {
void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": Vulkan result "+std::to_string(result));
}
#define VK(call) check(call,#call)
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void*) {
    if(severity>=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) fprintf(stderr,"Vulkan validation: %s\n",data->pMessage);
    return VK_FALSE;
}
struct Buffer { VkBuffer handle{};VkDeviceMemory memory{};void* mapped{};VkDeviceSize size{}; };
struct Texture { VkImage image{};VkDeviceMemory memory{};VkImageView view{};uint32_t width{},height{},levels{};VkFormat format{};std::array<VkImageView,3> reducedViews{};bool reducible{}; };
struct Instance { glm::mat4 model;glm::vec4 color;glm::uvec4 params; };
static_assert(sizeof(Instance)==96);
struct TransparentModel {const DrawItem* item;float depth,screenRadius;uint32_t ordinal;};
struct TransparentParticle {float depth;uint32_t index;};
struct TransparentBatch {uint32_t first,count;bool particles;};
struct Mesh { uint32_t firstIndex,indexCount,vertexOffset,texture,bucket;float lodMin,lodMax; };
constexpr uint32_t maxTextures=2048,maxDraws=16384,maxHudQuads=16384,maxParticles=8192;
void barrier(VkCommandBuffer cmd,VkImage image,VkImageLayout oldLayout,VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage,VkAccessFlags2 srcAccess,VkPipelineStageFlags2 dstStage,VkAccessFlags2 dstAccess,
    VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT,uint32_t baseMip=0,uint32_t levels=1) {
    VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask=srcStage;b.srcAccessMask=srcAccess;b.dstStageMask=dstStage;b.dstAccessMask=dstAccess;
    b.oldLayout=oldLayout;b.newLayout=newLayout;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.image=image;b.subresourceRange={aspect,baseMip,levels,0,1};
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dep.imageMemoryBarrierCount=1;dep.pImageMemoryBarriers=&b;
    vkCmdPipelineBarrier2(cmd,&dep);
}
}
struct Renderer::Impl {
    SDL_Window* window;
    VkInstance instance{};VkDebugUtilsMessengerEXT debug{};VkSurfaceKHR surface{};
    VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t queueFamily{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};VkPhysicalDeviceProperties properties{};
    VkSwapchainKHR swapchain{};VkFormat format{};VkExtent2D extent{};
    std::vector<VkImage> images;std::vector<VkImageView> views;std::vector<VkSemaphore> rendered;
    Texture depth,multisampleColor;float gamma{1};VkPipeline gammaPipeline{};
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT},requestedSamples{VK_SAMPLE_COUNT_1_BIT};int textureLevel{3};
    VkCommandPool commandPool{};VkCommandBuffer uploadCommand{};bool uploadsOpen{};
    std::vector<Buffer> staging;
    VkDescriptorSetLayout textureLayout{},frameLayout{};VkDescriptorPool descriptorPool{};VkDescriptorSet textureSet{};
    VkPipelineLayout pipelineLayout{};VkPipeline transparentPipeline{};std::vector<TransparentModel> transparent;std::vector<TransparentParticle> particleOrder;std::vector<TransparentBatch> transparentBatches;VkPipeline backgroundPipeline{};VkPipeline scenePipeline{},hudPipeline{},particlePipeline{},solidParticlePipeline{};VkSampler sampler{};
    std::vector<Texture> textures;
    struct ImageBindings {uint32_t image;std::array<uint32_t,2> descriptors{UINT32_MAX,UINT32_MAX};std::array<uint32_t,4> limits{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};};
    std::unordered_map<const ImageData*,ImageBindings> imageIDs;
    std::vector<uint32_t> descriptorImages;
    std::vector<uint32_t> worldDescriptors;
    VkSampler nearestSampler{};
    std::vector<std::shared_ptr<ImageData>> imageOwners;
    struct ModelKey {const ModelData* model;const ImageData* skin;bool operator==(const ModelKey&)const=default;};
    struct ModelHash {size_t operator()(ModelKey key)const{return std::hash<const void*>{}(key.model)^(std::hash<const void*>{}(key.skin)<<1);}};
    std::unordered_map<ModelKey,uint32_t,ModelHash> modelIDs;
    struct Geometry {Mesh mesh;std::shared_ptr<MeshData> owner;};
    std::unordered_map<const MeshData*,Geometry> geometry;
    std::vector<std::shared_ptr<ModelData>> modelOwners;
    std::vector<std::vector<Mesh>> models;
    struct OpaqueInstance {Instance instance;uint32_t bucket;};
    struct OpaqueBucket {Mesh mesh{};uint32_t count{},cursor{};};
    std::vector<OpaqueInstance> opaqueInstances;std::vector<OpaqueBucket> opaqueBuckets;std::vector<uint32_t> opaqueGroups;
    Buffer vertices,indices;
    uint32_t vertexCount{},indexCount{};
    struct Frame { VkCommandBuffer command{};VkFence fence{};VkSemaphore acquired{};VkDescriptorSet set{};Buffer instances,indirect,hud,particles,video,gammaTable;uint32_t videoTexture{};bool videoPending{};Texture gammaImage;VkImageView gammaRawView{};uint32_t gammaDescriptor{};bool gammaInitialized{};float gammaValue{}; };
    std::array<Frame,2> frames;
    uint32_t frameIndex{},imageIndex{};bool recreate{},wait2{},timing{},googleTiming{};
#ifdef __APPLE__
    PFN_vkExportMetalObjectsEXT exportMetalObjects{};
    VkQueryPool gpuQueries{};
    PFN_vkGetCalibratedTimestampsKHR calibratedTimestamps{};
    uint64_t gpuClock{},gpuHost{},gpuDeviation{},gpuCalibration{};
    uint32_t gpuBits{};
    bool gpuClockValid{};
    std::array<uint64_t,2> gpuQueryIds{};
    void gpuFeedback();
#endif
    Presentation presentation;
    std::unique_ptr<FILE,decltype(&fclose)> timingTrace{nullptr,&fclose};
    RenderStats stats;

    explicit Impl(SDL_Window* w):window(w) {}
    ~Impl();
    void initialize(bool validation);
    void createSwapchain();
    void destroySwapchain();
    uint32_t memoryType(uint32_t bits,VkMemoryPropertyFlags required,VkMemoryPropertyFlags preferred=0);
    Buffer buffer(VkDeviceSize size,VkBufferUsageFlags usage,VkMemoryPropertyFlags flags);
    Texture texture(uint32_t w,uint32_t h,uint32_t levels,VkFormat fmt,VkImageUsageFlags usage,VkImageAspectFlags aspect,VkImageCreateFlags flags=0,VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT);
    void destroy(Buffer& b);
    void destroy(Texture& t);
    void createDescriptors();
    void createPipelines();
    void destroyPipelines();
    void createRenderTargets();
    VkShaderModule shader(const char* name);
    uint32_t addTexture(const std::shared_ptr<ImageData>& image,bool nearest=false);
    uint32_t bindTexture(ImageBindings& image,bool nearest);
    uint32_t addModel(const std::shared_ptr<ModelData>& model,const std::shared_ptr<ImageData>& skin);
    void flushUploads();
};
uint32_t Renderer::Impl::memoryType(uint32_t bits,VkMemoryPropertyFlags required,VkMemoryPropertyFlags preferred) {
    int best=-1;uint32_t result=0;
    for(uint32_t i=0;i<memoryProperties.memoryTypeCount;++i) {
        auto f=memoryProperties.memoryTypes[i].propertyFlags;
        if(!(bits&(1u<<i)) || (f&required)!=required)continue;
        int score=(f&preferred)==preferred ? 1 : 0;
        if(score>best){best=score;result=i;}
    }
    if(best<0)throw std::runtime_error("No suitable Vulkan memory type");return result;
}
Buffer Renderer::Impl::buffer(VkDeviceSize size,VkBufferUsageFlags usage,VkMemoryPropertyFlags flags) {
    Buffer b;b.size=size;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=size;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VK(vkCreateBuffer(device,&info,nullptr,&b.handle));VkMemoryRequirements req;vkGetBufferMemoryRequirements(device,b.handle,&req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memoryType(req.memoryTypeBits,flags,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK(vkAllocateMemory(device,&alloc,nullptr,&b.memory));VK(vkBindBufferMemory(device,b.handle,b.memory,0));
    if(flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)VK(vkMapMemory(device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped));
    return b;
}
Texture Renderer::Impl::texture(uint32_t w,uint32_t h,uint32_t levels,VkFormat fmt,VkImageUsageFlags usage,VkImageAspectFlags aspect,VkImageCreateFlags flags,VkSampleCountFlagBits samples) {
    Texture t;t.width=w;t.height=h;t.levels=levels;t.format=fmt;VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;info.format=fmt;info.extent={w,h,1};
    info.mipLevels=levels;info.arrayLayers=1;info.samples=samples;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage;info.flags=flags;
    VK(vkCreateImage(device,&info,nullptr,&t.image));VkMemoryRequirements req;vkGetImageMemoryRequirements(device,t.image,&req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,(usage&VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)?VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT:0);
    VK(vkAllocateMemory(device,&alloc,nullptr,&t.memory));VK(vkBindImageMemory(device,t.image,t.memory,0));
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=t.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=fmt;view.subresourceRange={aspect,0,levels,0,1};
    if(fmt==VK_FORMAT_R8_UNORM)view.components={VK_COMPONENT_SWIZZLE_ONE,VK_COMPONENT_SWIZZLE_ONE,VK_COMPONENT_SWIZZLE_ONE,VK_COMPONENT_SWIZZLE_R};
    VK(vkCreateImageView(device,&view,nullptr,&t.view));return t;
}
void Renderer::Impl::destroy(Buffer& b) { if(b.mapped)vkUnmapMemory(device,b.memory);if(b.handle)vkDestroyBuffer(device,b.handle,nullptr);if(b.memory)vkFreeMemory(device,b.memory,nullptr);b={}; }
void Renderer::Impl::destroy(Texture& t) {for(auto view:t.reducedViews)if(view)vkDestroyImageView(device,view,nullptr);if(t.view)vkDestroyImageView(device,t.view,nullptr);if(t.image)vkDestroyImage(device,t.image,nullptr);if(t.memory)vkFreeMemory(device,t.memory,nullptr);t={}; }
void Renderer::Impl::initialize(bool validation) {
    uint32_t count{};auto sdlExtensions=SDL_Vulkan_GetInstanceExtensions(&count);
    if(!sdlExtensions)throw std::runtime_error(SDL_GetError());
    std::vector<const char*> extensions(sdlExtensions,sdlExtensions+count);extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    if(validation)extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    uint32_t instanceExtensionCount{};VK(vkEnumerateInstanceExtensionProperties(nullptr,&instanceExtensionCount,nullptr));
    std::vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);VK(vkEnumerateInstanceExtensionProperties(nullptr,&instanceExtensionCount,instanceExtensions.data()));
    const bool portability=std::any_of(instanceExtensions.begin(),instanceExtensions.end(),[](auto& e){return !strcmp(e.extensionName,VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);});
    if(portability)extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    const char* layer="VK_LAYER_KHRONOS_validation";
    VkValidationFeatureEnableEXT validationFeature=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validationFeatures{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    validationFeatures.enabledValidationFeatureCount=1;validationFeatures.pEnabledValidationFeatures=&validationFeature;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Allegiance native";app.apiVersion=VK_API_VERSION_1_3;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pApplicationInfo=&app;info.enabledExtensionCount=extensions.size();info.ppEnabledExtensionNames=extensions.data();
    if(portability)info.flags|=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    if(validation){info.enabledLayerCount=1;info.ppEnabledLayerNames=&layer;info.pNext=&validationFeatures;}
    VK(vkCreateInstance(&info,nullptr,&instance));
    if(validation) {
        VkDebugUtilsMessengerCreateInfoEXT ci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};ci.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;ci.pfnUserCallback=debugCallback;
        auto create=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"));VK(create(instance,&ci,nullptr,&debug));
    }
    if(!SDL_Vulkan_CreateSurface(window,instance,nullptr,&surface))throw std::runtime_error(SDL_GetError());
    VK(vkEnumeratePhysicalDevices(instance,&count,nullptr));std::vector<VkPhysicalDevice> devices(count);VK(vkEnumeratePhysicalDevices(instance,&count,devices.data()));
    for(auto candidate:devices) {
        VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(candidate,&props);
        if(props.apiVersion<VK_API_VERSION_1_3 || props.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
        uint32_t n;vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,nullptr);std::vector<VkQueueFamilyProperties> qs(n);vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,qs.data());
        for(uint32_t q=0;q<n;++q) {
            VkBool32 present=VK_TRUE;
            VK(vkGetPhysicalDeviceSurfaceSupportKHR(candidate,q,surface,&present));
            if(present && (qs[q].queueFlags&VK_QUEUE_GRAPHICS_BIT)) {
                physical=candidate;queueFamily=q;
#ifdef __APPLE__
                gpuBits=qs[q].timestampValidBits;
#endif
                break;
            }
        }
        if(physical)break;
    }
    if(!physical)throw std::runtime_error("No hardware Vulkan 1.3 graphics/present queue");
    vkGetPhysicalDeviceProperties(physical,&properties);vkGetPhysicalDeviceMemoryProperties(physical,&memoryProperties);
    fprintf(stderr,"Vulkan device: %s\n",properties.deviceName);
    VK(vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,nullptr));std::vector<VkExtensionProperties> available(count);VK(vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,available.data()));
    auto has=[&](const char* name){return std::any_of(available.begin(),available.end(),[&](auto& e){return !strcmp(e.extensionName,name);});};
    wait2=has(VK_KHR_PRESENT_ID_2_EXTENSION_NAME)&&has(VK_KHR_PRESENT_WAIT_2_EXTENSION_NAME);
    timing=wait2&&has(VK_EXT_PRESENT_TIMING_EXTENSION_NAME)&&has(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
    googleTiming=has(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME);
    extensions={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    // Required for devices enumerated through Vulkan's portability subset.
    if(has("VK_KHR_portability_subset"))extensions.push_back("VK_KHR_portability_subset");
    if(wait2){extensions.push_back(VK_KHR_PRESENT_ID_2_EXTENSION_NAME);extensions.push_back(VK_KHR_PRESENT_WAIT_2_EXTENSION_NAME);}
    if(timing){extensions.push_back(VK_EXT_PRESENT_TIMING_EXTENSION_NAME);extensions.push_back(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);}
    if(googleTiming)extensions.push_back(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME);
#ifdef __APPLE__
    if(has(VK_EXT_METAL_OBJECTS_EXTENSION_NAME))extensions.push_back(VK_EXT_METAL_OBJECTS_EXTENSION_NAME);
    if(gpuBits && has(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME) && !timing)
        extensions.push_back(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
#endif
    VkPhysicalDevicePresentTimingFeaturesEXT tf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT};
    VkPhysicalDevicePresentWait2FeaturesKHR wf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_2_FEATURES_KHR};
    VkPhysicalDevicePresentId2FeaturesKHR idf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR};
    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&f12;f12.pNext=&f13;
    if(wait2){f13.pNext=&idf;idf.pNext=&wf;if(timing)wf.pNext=&tf;}
    vkGetPhysicalDeviceFeatures2(physical,&features);
    if(!f12.runtimeDescriptorArray || !f12.shaderSampledImageArrayNonUniformIndexing || !f12.descriptorBindingPartiallyBound ||
        !f12.descriptorBindingSampledImageUpdateAfterBind || !f13.shaderDemoteToHelperInvocation || !f13.dynamicRendering || !f13.synchronization2 || !features.features.multiDrawIndirect || !features.features.drawIndirectFirstInstance)
        throw std::runtime_error("Required bindless/dynamic-rendering features unavailable");
    if(wait2 && (!idf.presentId2 || !wf.presentWait2))throw std::runtime_error("Present wait2 feature unavailable");
    if(timing && (!tf.presentTiming || !tf.presentAtAbsoluteTime))throw std::runtime_error("Absolute present timing unavailable");
    // Enable only what this backend uses.
    f12={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};f12.pNext=&f13;
    f12.runtimeDescriptorArray=f12.shaderSampledImageArrayNonUniformIndexing=f12.descriptorBindingPartiallyBound=f12.descriptorBindingSampledImageUpdateAfterBind=VK_TRUE;
    f13={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};f13.shaderDemoteToHelperInvocation=f13.dynamicRendering=f13.synchronization2=VK_TRUE;
    if(wait2)f13.pNext=&idf;if(timing)tf.presentAtRelativeTime=VK_FALSE;
    VkPhysicalDeviceFeatures enabled{};enabled.multiDrawIndirect=enabled.drawIndirectFirstInstance=VK_TRUE;enabled.samplerAnisotropy=features.features.samplerAnisotropy;
    float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=queueFamily;qi.queueCount=1;qi.pQueuePriorities=&priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.pNext=&f12;di.pEnabledFeatures=&enabled;di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
    di.enabledExtensionCount=extensions.size();di.ppEnabledExtensionNames=extensions.data();VK(vkCreateDevice(physical,&di,nullptr,&device));vkGetDeviceQueue(device,queueFamily,0,&queue);
#ifdef __APPLE__
    if(has(VK_EXT_METAL_OBJECTS_EXTENSION_NAME))exportMetalObjects=reinterpret_cast<PFN_vkExportMetalObjectsEXT>(vkGetDeviceProcAddr(device,"vkExportMetalObjectsEXT"));
    if(gpuBits && has(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)) {
        calibratedTimestamps=reinterpret_cast<PFN_vkGetCalibratedTimestampsKHR>(vkGetDeviceProcAddr(device,"vkGetCalibratedTimestampsKHR"));
        VkQueryPoolCreateInfo queries{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};queries.queryType=VK_QUERY_TYPE_TIMESTAMP;queries.queryCount=frames.size();
        VK(vkCreateQueryPool(device,&queries,nullptr,&gpuQueries));
    }
#endif
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=queueFamily;pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;VK(vkCreateCommandPool(device,&pool,nullptr,&commandPool));
    VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=commandPool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;VK(vkAllocateCommandBuffers(device,&ca,&uploadCommand));
    for(auto& frame:frames) {
        VK(vkAllocateCommandBuffers(device,&ca,&frame.command));VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;VK(vkCreateFence(device,&fi,nullptr,&frame.fence));
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};VK(vkCreateSemaphore(device,&si,nullptr,&frame.acquired));
        constexpr auto host=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        frame.instances=buffer(maxDraws*sizeof(Instance),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,host);
        frame.indirect=buffer(maxDraws*sizeof(VkDrawIndexedIndirectCommand),VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,host);
        frame.hud=buffer(maxHudQuads*sizeof(HudQuad),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,host);
        frame.particles=buffer(maxParticles*sizeof(ParticleDraw),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,host);
    }
    constexpr auto host=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    vertices=buffer(16*1024*1024,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,host);indices=buffer(8*1024*1024,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,host);
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_LINEAR;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;si.maxLod=VK_LOD_CLAMP_NONE;
    si.anisotropyEnable=enabled.samplerAnisotropy;si.maxAnisotropy=std::min(4.0f,properties.limits.maxSamplerAnisotropy);VK(vkCreateSampler(device,&si,nullptr,&sampler));
    si.magFilter=si.minFilter=VK_FILTER_NEAREST;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.anisotropyEnable=VK_FALSE;si.maxLod=0;si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK(vkCreateSampler(device,&si,nullptr,&nearestSampler));
    createDescriptors();createSwapchain();createPipelines();
    auto white=std::make_shared<ImageData>();white->width=white->height=1;white->pixels={255,255,255,255};white->name="white";addTexture(white);
    // Reserve stable slots, but no images, for the optional final display ramp.
    for(auto& frame:frames){frame.gammaDescriptor=uint32_t(descriptorImages.size());descriptorImages.push_back(UINT32_MAX);worldDescriptors.push_back(frame.gammaDescriptor);}
}
void Renderer::Impl::createDescriptors() {
    VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,maxTextures,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
    VkDescriptorBindingFlags flags=VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT|VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo bf{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};bf.bindingCount=1;bf.pBindingFlags=&flags;
    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ci.pNext=&bf;ci.flags=VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;ci.bindingCount=1;ci.pBindings=&binding;
    VK(vkCreateDescriptorSetLayout(device,&ci,nullptr,&textureLayout));
    VkDescriptorSetLayoutBinding frameBindings[]={{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
    ci.pNext=nullptr;ci.flags=0;ci.bindingCount=2;ci.pBindings=frameBindings;VK(vkCreateDescriptorSetLayout(device,&ci,nullptr,&frameLayout));
    VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,maxTextures},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,uint32_t(frames.size()*2)}};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.flags=VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;pi.maxSets=3;pi.poolSizeCount=2;pi.pPoolSizes=sizes;VK(vkCreateDescriptorPool(device,&pi,nullptr,&descriptorPool));
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=descriptorPool;ai.descriptorSetCount=1;ai.pSetLayouts=&textureLayout;VK(vkAllocateDescriptorSets(device,&ai,&textureSet));
    ai.pSetLayouts=&frameLayout;
    for(auto& frame:frames) {
        VK(vkAllocateDescriptorSets(device,&ai,&frame.set));VkDescriptorBufferInfo bi{frame.instances.handle,0,frame.instances.size};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=frame.set;write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&bi;vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    }
    VkDescriptorSetLayout layouts[]={textureLayout,frameLayout};VkPushConstantRange ranges[]={{VK_SHADER_STAGE_VERTEX_BIT,0,112},{VK_SHADER_STAGE_FRAGMENT_BIT,112,16}};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.setLayoutCount=2;li.pSetLayouts=layouts;li.pushConstantRangeCount=2;li.pPushConstantRanges=ranges;VK(vkCreatePipelineLayout(device,&li,nullptr,&pipelineLayout));
}
void Renderer::Impl::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps));
    uint32_t count;VK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,nullptr));std::vector<VkSurfaceFormatKHR> formats(count);VK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,formats.data()));
    auto chosen=formats.front();for(auto f:formats)if(f.format==VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)chosen=f;
    format=chosen.format;extent=caps.currentExtent;
    if(extent.width==UINT32_MAX) {int w,h;SDL_GetWindowSizeInPixels(window,&w,&h);extent={std::clamp(uint32_t(w),caps.minImageExtent.width,caps.maxImageExtent.width),std::clamp(uint32_t(h),caps.minImageExtent.height,caps.maxImageExtent.height)};}
    count=std::max(3u,caps.minImageCount);
    if(caps.maxImageCount)count=std::min(count,caps.maxImageCount);
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};ci.surface=surface;ci.minImageCount=count;ci.imageFormat=format;ci.imageColorSpace=chosen.colorSpace;ci.imageExtent=extent;
    ci.imageArrayLayers=1;ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform=caps.currentTransform;ci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;ci.presentMode=VK_PRESENT_MODE_FIFO_KHR;ci.clipped=VK_TRUE;
    VkPresentTimingSurfaceCapabilitiesEXT tc{VK_STRUCTURE_TYPE_PRESENT_TIMING_SURFACE_CAPABILITIES_EXT};
    bool surfaceWait=false,surfaceTiming=false;
    if(wait2) {
        VkSurfaceCapabilitiesPresentId2KHR ic{VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_ID_2_KHR};
        VkSurfaceCapabilitiesPresentWait2KHR wc{VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_WAIT_2_KHR};ic.pNext=&wc;if(timing)wc.pNext=&tc;
        VkPhysicalDeviceSurfaceInfo2KHR si{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR};si.surface=surface;
        VkSurfaceCapabilities2KHR sc{VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR};sc.pNext=&ic;
        VK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(physical,&si,&sc));surfaceWait=ic.presentId2Supported&&wc.presentWait2Supported;
        if(surfaceWait)ci.flags|=VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR|VK_SWAPCHAIN_CREATE_PRESENT_WAIT_2_BIT_KHR;
        surfaceTiming=surfaceWait&&tc.presentTimingSupported&&tc.presentAtAbsoluteTimeSupported&&
            (tc.presentStageQueries&(VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT|VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT));
        if(surfaceTiming)ci.flags|=VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT;
    }
    VK(vkCreateSwapchainKHR(device,&ci,nullptr,&swapchain));VK(vkGetSwapchainImagesKHR(device,swapchain,&count,nullptr));images.resize(count);VK(vkGetSwapchainImagesKHR(device,swapchain,&count,images.data()));
    views.resize(count);rendered.resize(count);
    for(uint32_t i=0;i<count;++i) {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=images[i];vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
#ifdef __APPLE__
        VkExportMetalObjectCreateInfoEXT metal{VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECT_CREATE_INFO_EXT};metal.exportObjectType=VK_EXPORT_METAL_OBJECT_TYPE_METAL_TEXTURE_BIT_EXT;
        if(exportMetalObjects)vi.pNext=&metal;
#endif
        VK(vkCreateImageView(device,&vi,nullptr,&views[i]));
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};VK(vkCreateSemaphore(device,&si,nullptr,&rendered[i]));
    }
    createRenderTargets();
    auto mode=SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));presentation.reset(device,swapchain,surfaceWait,surfaceTiming,tc,mode ? mode->refresh_rate : 0,googleTiming);
#ifdef __APPLE__
    gpuQueryIds={};gpuClockValid=false;gpuCalibration=0;
#endif
    fprintf(stderr,"Swapchain: %ux%u, %u images, FIFO\n",extent.width,extent.height,count);
}
void Renderer::Impl::destroySwapchain() {
    for(auto& frame:frames){if(frame.gammaRawView)vkDestroyImageView(device,frame.gammaRawView,nullptr);frame.gammaRawView={};destroy(frame.gammaImage);frame.gammaInitialized=false;frame.gammaValue=0;}
    destroy(depth);destroy(multisampleColor);for(auto view:views)vkDestroyImageView(device,view,nullptr);for(auto sem:rendered)vkDestroySemaphore(device,sem,nullptr);
    views.clear();rendered.clear();images.clear();if(swapchain)vkDestroySwapchainKHR(device,swapchain,nullptr);swapchain={};
}
#ifdef __APPLE__
void Renderer::Impl::gpuFeedback() {
    if(!gpuQueries || !calibratedTimestamps)return;
    auto now=monotonicNs();
    if(!gpuCalibration || now-gpuCalibration>=500'000'000) {
        // Device-only calibration brackets the same GPU counter used by the
        // queries with the application's host clock. This avoids MoltenVK's
        // differing mach/continuous epochs for its reported host domains.
        VkCalibratedTimestampInfoKHR info{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR};info.timeDomain=VK_TIME_DOMAIN_DEVICE_KHR;
        auto before=monotonicNs();uint64_t ticks{},deviation{};
        auto result=calibratedTimestamps(device,1,&info,&ticks,&deviation);auto after=monotonicNs();
        gpuClockValid=result==VK_SUCCESS && after-before<1'000'000 && deviation<1'000'000;
        if(gpuClockValid){gpuClock=ticks;gpuHost=before+(after-before)/2;gpuDeviation=deviation+(after-before)/2;}
        gpuCalibration=after;presentation.gpuTiming(gpuClockValid);
    }
    if(!gpuClockValid)return;
    struct Query {uint64_t ticks,available;};std::array<Query,2> results{};
    auto result=vkGetQueryPoolResults(device,gpuQueries,0,results.size(),sizeof(results),results.data(),sizeof(Query),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if(result!=VK_SUCCESS && result!=VK_NOT_READY){gpuClockValid=false;presentation.gpuTiming(false);return;}
    auto queriedAt=monotonicNs();
    for(size_t i=0;i<results.size();++i)if(results[i].available && gpuQueryIds[i]) {
        auto shift=64-gpuBits;
        auto delta=std::bit_cast<int64_t>((results[i].ticks-gpuClock)<<shift)>>shift;
        auto ready=int64_t(gpuHost)+int64_t(double(delta)*properties.limits.timestampPeriod)+int64_t(gpuDeviation);
        if(ready>0 && uint64_t(ready)<=queriedAt+gpuDeviation)presentation.gpuReady(gpuQueryIds[i],uint64_t(ready));
        else {gpuClockValid=false;presentation.gpuTiming(false);break;}
        gpuQueryIds[i]=0;
    }
}
#endif
VkShaderModule Renderer::Impl::shader(const char* name) {
    auto installed=std::filesystem::path(SDL_GetBasePath())/"../share/freeallegiance/shaders"/(std::string(name)+".spv");
#ifdef __APPLE__
    auto bundled=std::filesystem::path(SDL_GetBasePath())/"shaders"/(std::string(name)+".spv");
    if(std::filesystem::is_regular_file(bundled))installed=bundled;
#endif
    auto path=std::filesystem::is_regular_file(installed)?installed:std::filesystem::path(ALLEGIANCE_SHADER_DIR)/(std::string(name)+".spv");
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error(std::string("Cannot load shader ")+name);
    auto size=f.tellg();if(size<=0 || size%4)throw std::runtime_error("Invalid shader size");
    std::vector<uint32_t> data(size_t(size)/4);f.seekg(0);if(!f.read(reinterpret_cast<char*>(data.data()),size))throw std::runtime_error("Truncated shader");
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=size;ci.pCode=data.data();VkShaderModule module;VK(vkCreateShaderModule(device,&ci,nullptr,&module));return module;
}
void Renderer::Impl::createPipelines() {
    transparent.reserve(maxDraws);particleOrder.reserve(maxParticles);transparentBatches.reserve(maxDraws+maxParticles);
    for(int kind=0;kind<7;++kind) {
        bool hud=kind==1,particle=kind==2 || kind==4,transparent=kind==3,solidParticle=kind==4,background=kind==5,gamma=kind==6;
        VkShaderModule vs=shader(gamma?"gamma.vert":particle?"particle.vert":hud ? "hud.vert" : "scene.vert"),fs=shader(gamma?"gamma.frag":particle?"particle.frag":hud ? "hud.frag" : "scene.frag");
        VkPipelineShaderStageCreateInfo stages[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vs;stages[0].pName="main";stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fs;stages[1].pName="main";
        VkVertexInputBindingDescription binding{0,uint32_t(particle?sizeof(ParticleDraw):hud ? sizeof(HudQuad) : sizeof(Vertex)),(hud || particle) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attrs[6];uint32_t n;
        if(particle){n=6;for(int i=0;i<4;++i)attrs[i]={uint32_t(i),0,VK_FORMAT_R32G32B32A32_SFLOAT,uint32_t(i*16)};attrs[4]={4,0,VK_FORMAT_R32_UINT,64};attrs[5]={5,0,VK_FORMAT_R32_SFLOAT,68};}
        else if(hud) {n=4;attrs[0]={0,0,VK_FORMAT_R32G32B32A32_SFLOAT,0};attrs[1]={1,0,VK_FORMAT_R32G32B32A32_SFLOAT,16};attrs[2]={2,0,VK_FORMAT_R32G32B32A32_SFLOAT,32};attrs[3]={3,0,VK_FORMAT_R32_UINT,48};}
        else {n=3;attrs[0]={0,0,VK_FORMAT_R32G32B32_SFLOAT,0};attrs[1]={1,0,VK_FORMAT_R32G32B32_SFLOAT,12};attrs[2]={2,0,VK_FORMAT_R32G32_SFLOAT,24};}
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;vi.vertexAttributeDescriptionCount=n;vi.pVertexAttributeDescriptions=attrs;
        if(gamma)vi.vertexBindingDescriptionCount=vi.vertexAttributeDescriptionCount=0;
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=gamma?VK_SAMPLE_COUNT_1_BIT:samples;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=!hud && !background && !gamma;ds.depthWriteEnable=solidParticle || (!hud && !particle && !transparent && !background && !gamma);ds.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
        VkPipelineColorBlendAttachmentState ba{};ba.colorWriteMask=15;ba.blendEnable=hud || particle || transparent || background;ba.srcColorBlendFactor=VK_BLEND_FACTOR_ONE;ba.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.colorBlendOp=VK_BLEND_OP_ADD;ba.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;ba.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;ba.alphaBlendOp=VK_BLEND_OP_ADD;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&ba;
        VkDynamicState dynamic[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dyn.dynamicStateCount=2;dyn.pDynamicStates=dynamic;
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&format;rendering.depthAttachmentFormat=VK_FORMAT_D32_SFLOAT;
        if(gamma)rendering.depthAttachmentFormat=VK_FORMAT_UNDEFINED;
        VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pi.pNext=&rendering;pi.stageCount=2;pi.pStages=stages;pi.pVertexInputState=&vi;pi.pInputAssemblyState=&ia;pi.pViewportState=&viewport;pi.pRasterizationState=&raster;pi.pMultisampleState=&ms;pi.pDepthStencilState=&ds;pi.pColorBlendState=&blend;pi.pDynamicState=&dyn;pi.layout=pipelineLayout;
        auto result=vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,gamma?&gammaPipeline:background?&backgroundPipeline:solidParticle?&solidParticlePipeline:transparent?&transparentPipeline:particle?&particlePipeline:hud ? &hudPipeline : &scenePipeline);vkDestroyShaderModule(device,vs,nullptr);vkDestroyShaderModule(device,fs,nullptr);VK(result);
    }
}
void Renderer::Impl::createRenderTargets(){
    depth=texture(extent.width,extent.height,1,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT,0,samples);
    if(samples!=VK_SAMPLE_COUNT_1_BIT)multisampleColor=texture(extent.width,extent.height,1,format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,VK_IMAGE_ASPECT_COLOR_BIT,0,samples);
}
void Renderer::Impl::destroyPipelines(){
    for(auto* pipeline:{&gammaPipeline,&backgroundPipeline,&solidParticlePipeline,&transparentPipeline,&particlePipeline,&scenePipeline,&hudPipeline})if(*pipeline){vkDestroyPipeline(device,*pipeline,nullptr);*pipeline={};}
}
uint32_t Renderer::Impl::bindTexture(ImageBindings& image,bool nearest) {
    auto& id=image.descriptors[nearest];if(id!=UINT32_MAX)return id;
    if(descriptorImages.size()>=maxTextures)throw std::runtime_error("Bindless texture table exhausted");
    id=uint32_t(descriptorImages.size());VkDescriptorImageInfo ii{nearest?nearestSampler:sampler,textures[image.image].view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=textureSet;write.dstBinding=0;write.dstArrayElement=id;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&ii;vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    descriptorImages.push_back(image.image);worldDescriptors.push_back(id);
    auto& tex=textures[image.image];
    if(!nearest){
        for(int level=0;level<4;++level){
            uint32_t skip=0;if(tex.reducible)while((std::max(tex.width,tex.height)>>skip)>uint32_t(256<<level))++skip;
            if(!skip){image.limits[level]=id;continue;}
            if(descriptorImages.size()>=maxTextures)throw std::runtime_error("Bindless texture table exhausted");
            auto& view=tex.reducedViews[skip-1];
            if(!view){VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=tex.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=tex.format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,skip,tex.levels-skip,0,1};VK(vkCreateImageView(device,&vi,nullptr,&view));}
            uint32_t reduced=uint32_t(descriptorImages.size());ii.imageView=view;write.dstArrayElement=reduced;vkUpdateDescriptorSets(device,1,&write,0,nullptr);
            descriptorImages.push_back(image.image);worldDescriptors.push_back(reduced);image.limits[level]=reduced;
        }
        worldDescriptors[id]=image.limits[textureLevel];
    }
    return id;
}
uint32_t Renderer::Impl::addTexture(const std::shared_ptr<ImageData>& image,bool nearest) {
    if(!image)return 0;auto found=imageIDs.find(image.get());if(found!=imageIDs.end())return bindTexture(found->second,nearest);
    if(descriptorImages.size()>=maxTextures)throw std::runtime_error("Bindless texture table exhausted");
    if(image->pixels.size()!=image->byteSize())throw std::runtime_error("Invalid texture payload");
    if(!uploadsOpen) {VK(vkResetCommandBuffer(uploadCommand,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;VK(vkBeginCommandBuffer(uploadCommand,&begin));uploadsOpen=true;}
    auto levels=image->mipmaps ? uint32_t(std::floor(std::log2(std::max(image->width,image->height))))+1 : 1;
    auto tex=texture(image->width,image->height,levels,image->format==ImageFormat::Coverage?VK_FORMAT_R8_UNORM:VK_FORMAT_R8G8B8A8_SRGB,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
    auto eligible=[](uint32_t size){return size==512 || size==1024 || size==2048;};tex.reducible=image->mipmaps && image->format==ImageFormat::RgbaSrgb && eligible(image->width) && eligible(image->height);
    auto upload=buffer(image->pixels.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);std::memcpy(upload.mapped,image->pixels.data(),image->pixels.size());
    barrier(uploadCommand,tex.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,0,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_IMAGE_ASPECT_COLOR_BIT,0,levels);
    VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={image->width,image->height,1};vkCmdCopyBufferToImage(uploadCommand,upload.handle,tex.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    int32_t w=image->width,h=image->height;
    for(uint32_t level=1;level<levels;++level) {
        barrier(uploadCommand,tex.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT,VK_IMAGE_ASPECT_COLOR_BIT,level-1);
        VkImageBlit blit{};blit.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level-1,0,1};blit.srcOffsets[1]={w,h,1};blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,0,1};blit.dstOffsets[1]={std::max(1,w/2),std::max(1,h/2),1};
        vkCmdBlitImage(uploadCommand,tex.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,tex.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
        barrier(uploadCommand,tex.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,VK_IMAGE_ASPECT_COLOR_BIT,level-1);
        w=std::max(1,w/2);h=std::max(1,h/2);
    }
    barrier(uploadCommand,tex.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,VK_IMAGE_ASPECT_COLOR_BIT,levels-1);
    auto [binding,inserted]=imageIDs.emplace(image.get(),ImageBindings{uint32_t(textures.size())});
    textures.push_back(tex);staging.push_back(upload);imageOwners.push_back(image);return bindTexture(binding->second,nearest);
}
uint32_t Renderer::Impl::addModel(const std::shared_ptr<ModelData>& model,const std::shared_ptr<ImageData>& skin) {
    ModelKey key{model.get(),skin.get()};auto found=modelIDs.find(key);if(found!=modelIDs.end())return found->second;
    std::vector<Mesh> meshes;meshes.reserve(model->meshes.size());
    for(auto& mesh:model->meshes) {
        auto [entry,inserted]=geometry.try_emplace(mesh.get());
        if(inserted) {
            if((vertexCount+mesh->vertices.size())*sizeof(Vertex)>vertices.size || (indexCount+mesh->indices.size())*4>indices.size) {
                geometry.erase(entry);throw std::runtime_error("Geometry arena capacity exceeded");
            }
            std::memcpy(static_cast<Vertex*>(vertices.mapped)+vertexCount,mesh->vertices.data(),mesh->vertices.size()*sizeof(Vertex));
            std::memcpy(static_cast<uint32_t*>(indices.mapped)+indexCount,mesh->indices.data(),mesh->indices.size()*4);
            entry->second={{indexCount,uint32_t(mesh->indices.size()),vertexCount,0,uint32_t(opaqueBuckets.size()),mesh->lodMin,mesh->lodMax},mesh};
            opaqueBuckets.push_back({entry->second.mesh});
            vertexCount+=mesh->vertices.size();indexCount+=mesh->indices.size();
        }
        Mesh draw=entry->second.mesh;draw.texture=addTexture(skin?skin:mesh->image?mesh->image:model->defaultImage);meshes.push_back(draw);
    }
    uint32_t id=models.size();models.push_back(std::move(meshes));modelIDs.emplace(key,id);modelOwners.push_back(model);return id;
}

void Renderer::Impl::flushUploads() {
    if(!uploadsOpen)return;VK(vkEndCommandBuffer(uploadCommand));VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&uploadCommand;
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence;VK(vkCreateFence(device,&fi,nullptr,&fence));VK(vkQueueSubmit(queue,1,&submit,fence));VK(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX));vkDestroyFence(device,fence,nullptr);
    for(auto& b:staging)destroy(b);staging.clear();uploadsOpen=false;
}
Renderer::Impl::~Impl() {
    if(device) {
        vkDeviceWaitIdle(device);destroySwapchain();
#ifdef __APPLE__
        if(gpuQueries)vkDestroyQueryPool(device,gpuQueries,nullptr);
#endif
        for(auto& f:frames) {destroy(f.instances);destroy(f.indirect);destroy(f.hud);destroy(f.particles);destroy(f.video);destroy(f.gammaTable);if(f.fence)vkDestroyFence(device,f.fence,nullptr);if(f.acquired)vkDestroySemaphore(device,f.acquired,nullptr);}
        for(auto& t:textures)destroy(t);for(auto& b:staging)destroy(b);destroy(vertices);destroy(indices);
        destroyPipelines();
        if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);if(descriptorPool)vkDestroyDescriptorPool(device,descriptorPool,nullptr);
        if(textureLayout)vkDestroyDescriptorSetLayout(device,textureLayout,nullptr);if(frameLayout)vkDestroyDescriptorSetLayout(device,frameLayout,nullptr);
        if(nearestSampler)vkDestroySampler(device,nearestSampler,nullptr);
        if(sampler)vkDestroySampler(device,sampler,nullptr);if(commandPool)vkDestroyCommandPool(device,commandPool,nullptr);vkDestroyDevice(device,nullptr);
    }
    if(surface)vkDestroySurfaceKHR(instance,surface,nullptr);
    if(debug){auto destroy=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"));destroy(instance,debug,nullptr);}
    if(instance)vkDestroyInstance(instance,nullptr);
}
Renderer::Renderer(SDL_Window* window,bool validation):impl_(std::make_unique<Impl>(window)){impl_->initialize(validation);}
Renderer::~Renderer()=default;
uint32_t Renderer::addModel(const std::shared_ptr<ModelData>& m,const std::shared_ptr<ImageData>& skin){return impl_->addModel(m,skin);}
uint32_t Renderer::addTexture(const std::shared_ptr<ImageData>& i,bool nearest){return impl_->addTexture(i,nearest);}
void Renderer::flushUploads(){impl_->flushUploads();}
void Renderer::gamma(float level){impl_->gamma=std::isfinite(level)?std::clamp(level,1.f,2.f):1.13f;}
std::vector<int> Renderer::antialiasingOptions()const{
    const auto& r=*impl_;VkImageFormatProperties color{},depth{};
    if(vkGetPhysicalDeviceImageFormatProperties(r.physical,r.format,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,0,&color)!=VK_SUCCESS ||
       vkGetPhysicalDeviceImageFormatProperties(r.physical,VK_FORMAT_D32_SFLOAT,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,0,&depth)!=VK_SUCCESS)return {0};
    auto supported=r.properties.limits.framebufferColorSampleCounts&r.properties.limits.framebufferDepthSampleCounts&color.sampleCounts&depth.sampleCounts;
    std::vector<int> options{0};for(int count:{2,4,8,16})if(supported&count)options.push_back(count);return options;
}
int Renderer::antialiasing(int count){
    int selected=0;for(int supported:antialiasingOptions())if(supported<=count)selected=supported;
    impl_->requestedSamples=static_cast<VkSampleCountFlagBits>(selected?selected:1);return selected;
}
void Renderer::textureLimit(int level){
    auto& r=*impl_;r.textureLevel=std::clamp(level,0,3);
    for(const auto& [source,image]:r.imageIDs)if(image.descriptors[0]!=UINT32_MAX)r.worldDescriptors[image.descriptors[0]]=image.limits[r.textureLevel];
}
std::vector<std::string> Renderer::deviceInfo()const{
    const auto& r=*impl_;auto version=r.properties.apiVersion;
    char api[64],resolution[96],pixel[96];std::snprintf(api,sizeof(api),"Vulkan %u.%u.%u",VK_API_VERSION_MAJOR(version),VK_API_VERSION_MINOR(version),VK_API_VERSION_PATCH(version));
    std::snprintf(resolution,sizeof(resolution),"Resolution: %u x %u",r.extent.width,r.extent.height);
    bool srgb=r.format==VK_FORMAT_B8G8R8A8_SRGB || r.format==VK_FORMAT_R8G8B8A8_SRGB;
    std::snprintf(pixel,sizeof(pixel),"32 bits per pixel / %s / gamma %.2f",srgb?"sRGB":"linear",r.gamma);
    return {std::string("Device: ")+r.properties.deviceName,api,resolution,(SDL_GetWindowFlags(r.window)&SDL_WINDOW_FULLSCREEN)?"Rendering: borderless fullscreen":"Rendering: borderless window",pixel};
}
void Renderer::updateTexture(uint32_t id,const ImageData& image) {
    auto& r=*impl_;auto& f=r.frames[r.frameIndex];
    if(id>=r.descriptorImages.size())throw std::runtime_error("Invalid dynamic texture descriptor");
    auto imageId=r.descriptorImages[id];if(imageId>=r.textures.size())throw std::runtime_error("Dynamic texture is not an asset image");const auto& texture=r.textures[imageId];
    if(texture.levels!=1 || texture.width!=image.width || texture.height!=image.height || image.pixels.size()!=image.byteSize() || texture.format!=(image.format==ImageFormat::Coverage?VK_FORMAT_R8_UNORM:VK_FORMAT_R8G8B8A8_SRGB))
        throw std::runtime_error("Dynamic texture extent mismatch");
    if(f.video.size<image.pixels.size()) {r.destroy(f.video);f.video=r.buffer(image.pixels.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);}
    std::memcpy(f.video.mapped,image.pixels.data(),image.pixels.size());f.videoTexture=imageId;f.videoPending=true;
}
glm::ivec2 Renderer::size()const{return {impl_->extent.width,impl_->extent.height};}
RenderResources Renderer::resources()const {auto& r=*impl_;return {r.vertexCount,r.indexCount,uint32_t(r.models.size()),uint32_t(r.textures.size())};}
RenderStats Renderer::stats()const{return impl_->stats;}
uint64_t Renderer::targetPresentTime()const{return impl_->presentation.targetPresentTime();}
void Renderer::traceTiming(const std::string& file) {
    auto* trace=fopen(file.c_str(),"w");
    if(!trace)throw std::runtime_error("Cannot write timing trace: "+file);
    impl_->timingTrace.reset(trace);setvbuf(trace,nullptr,_IOFBF,65536);
    fputs("segment,id,sampled_ns,submitted_ns,ready_ns,displayed_ns,deadline_ns,misses,scheduled_ns\n",trace);
    impl_->presentation.trace(trace);
}
bool Renderer::beginFrame() {
    auto& r=*impl_;
    int w,h;SDL_GetWindowSizeInPixels(r.window,&w,&h);if(w<=0 || h<=0 || (SDL_GetWindowFlags(r.window)&SDL_WINDOW_MINIMIZED))return false;
    bool resize=r.recreate || uint32_t(w)!=r.extent.width || uint32_t(h)!=r.extent.height;
    bool quality=r.samples!=r.requestedSamples;
    if(resize || quality) {
        VK(vkDeviceWaitIdle(r.device));auto previousFormat=r.format;r.samples=r.requestedSamples;
        if(resize){r.destroySwapchain();r.createSwapchain();r.recreate=false;}
        else{r.destroy(r.depth);r.destroy(r.multisampleColor);r.createRenderTargets();}
        if(quality || previousFormat!=r.format){r.destroyPipelines();r.createPipelines();}
    }
    auto& f=r.frames[r.frameIndex];VK(vkWaitForFences(r.device,1,&f.fence,VK_TRUE,UINT64_MAX));
#ifdef __APPLE__
    r.gpuFeedback();
#endif
    auto result=vkAcquireNextImageKHR(r.device,r.swapchain,100'000'000,f.acquired,VK_NULL_HANDLE,&r.imageIndex);
    if(result==VK_ERROR_OUT_OF_DATE_KHR){r.recreate=true;return false;}
    if(result==VK_TIMEOUT || result==VK_NOT_READY)return false;
    if(result!=VK_SUCCESS && result!=VK_SUBOPTIMAL_KHR)VK(result);
    if(result==VK_SUBOPTIMAL_KHR)r.recreate=true;
#ifdef __APPLE__
    if(r.exportMetalObjects) {
        // Resolve the acquired view's backing drawable before pacing/input;
        // MoltenVK otherwise waits for nextDrawable during command encoding.
        VkExportMetalTextureInfoEXT texture{VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT};texture.imageView=r.views[r.imageIndex];texture.plane=VK_IMAGE_ASPECT_COLOR_BIT;
        VkExportMetalObjectsInfoEXT objects{VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT};objects.pNext=&texture;r.exportMetalObjects(r.device,&objects);
        if(!texture.mtlTexture)throw std::runtime_error("Acquired swapchain view has no Metal drawable texture");
    }
#endif
    r.presentation.begin();return true;
}
void Renderer::draw(const glm::mat4& viewProjection,std::span<const DrawItem> items,std::span<const HudQuad> hud,const std::string& screenshot,const ModelPreview* preview,std::span<const ParticleDraw> particles,const RenderLighting& lighting,HudBackground background) {
    auto& r=*impl_;auto& f=r.frames[r.frameIndex];
    if(r.uploadsOpen)throw std::runtime_error("flushUploads must precede rendering");
    bool ramp=r.gamma!=1;
    if(ramp && !f.gammaImage.image){
        // Only this reusable frame's idle descriptor/image is changed. Other
        // frames keep their own attachment; live brightness changes need no wait.
        // A compatible UNORM sampling view reads the completed display bytes
        // directly. The 256-entry table replaces per-pixel gamma/sRGB powers.
        bool srgb=r.format==VK_FORMAT_B8G8R8A8_SRGB || r.format==VK_FORMAT_R8G8B8A8_SRGB;
        f.gammaImage=r.texture(r.extent.width,r.extent.height,1,r.format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,srgb?VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT:0);
        if(srgb){VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=f.gammaImage.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=r.format==VK_FORMAT_B8G8R8A8_SRGB?VK_FORMAT_B8G8R8A8_UNORM:VK_FORMAT_R8G8B8A8_UNORM;view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VK(vkCreateImageView(r.device,&view,nullptr,&f.gammaRawView));}
        VkDescriptorImageInfo image{r.nearestSampler,f.gammaRawView?f.gammaRawView:f.gammaImage.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=r.textureSet;write.dstBinding=0;write.dstArrayElement=f.gammaDescriptor;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&image;vkUpdateDescriptorSets(r.device,1,&write,0,nullptr);
    }
    if(ramp && !f.gammaTable.handle){
        f.gammaTable=r.buffer(256*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkDescriptorBufferInfo table{f.gammaTable.handle,0,f.gammaTable.size};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=f.set;write.dstBinding=1;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&table;vkUpdateDescriptorSets(r.device,1,&write,0,nullptr);
    }
    if(ramp && f.gammaValue!=r.gamma){
        bool srgb=r.format==VK_FORMAT_B8G8R8A8_SRGB || r.format==VK_FORMAT_R8G8B8A8_SRGB;auto* table=static_cast<float*>(f.gammaTable.mapped);
        for(int i=0;i<256;++i){double value=std::pow(i/255.,1/double(r.gamma));table[i]=float(!srgb?value:value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4));}
        f.gammaValue=r.gamma;
    }
    // This frame's fence has been waited in beginFrame. Grow only its arenas;
    // other frames remain in flight and descriptor updates cannot race them.
    size_t instanceCapacity=0;for(const auto& item:items){if(item.model>=r.models.size())throw std::runtime_error("Invalid model ID");instanceCapacity+=r.models[item.model].size();}
    std::array<std::pair<uint32_t,uint32_t>,8> previewDraws{};size_t previewCount=0;
    for(auto* p=preview;p;p=p->next){if(++previewCount>previewDraws.size())throw std::runtime_error("Too many HUD previews");instanceCapacity+=r.models.at(p->item.model).size();}
    auto grow=[&](Buffer& buffer,size_t count,size_t stride,VkBufferUsageFlags usage) {
        if(count<=buffer.size/stride)return false;
        size_t capacity=std::max(count,size_t(buffer.size/stride)*2);VkDeviceSize bytes=capacity*stride;
        if((usage&VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) && bytes>r.properties.limits.maxStorageBufferRange)throw std::runtime_error("Instance storage exceeds the GPU buffer range");
        auto replacement=r.buffer(bytes,usage,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);r.destroy(buffer);buffer=replacement;return true;
    };
    if(grow(f.instances,instanceCapacity,sizeof(Instance),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
        VkDescriptorBufferInfo info{f.instances.handle,0,f.instances.size};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=f.set;write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&info;vkUpdateDescriptorSets(r.device,1,&write,0,nullptr);
    }
    grow(f.indirect,instanceCapacity,sizeof(VkDrawIndexedIndirectCommand),VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    grow(f.particles,particles.size(),sizeof(ParticleDraw),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    grow(f.hud,hud.size()+background.quads.size(),sizeof(HudQuad),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    auto* instances=static_cast<Instance*>(f.instances.mapped);auto* commands=static_cast<VkDrawIndexedIndirectCommand*>(f.indirect.mapped);uint32_t draws=0,instanceCount=0,triangles=0;
    r.opaqueInstances.clear();r.opaqueGroups.clear();
    auto rows=glm::transpose(viewProjection);
    float focal=glm::length(glm::vec3(rows[0]))*r.extent.width*.5f;
    std::array<glm::vec4,6> planes={rows[3]+rows[0],rows[3]-rows[0],rows[3]+rows[1],rows[3]-rows[1],rows[2],rows[3]-rows[2]};
    for(auto& plane:planes)plane/=glm::length(glm::vec3(plane));
#ifdef __aarch64__
    const Frustum frustum(planes);
#endif
    // Perspective W is camera depth. Orthographic reverse-Z uses the inverse
    // depth row instead, so diagnostic/world orthographic views retain order.
    const auto depthPlane=glm::dot(glm::vec3(rows[3]),glm::vec3(rows[3]))>0?rows[3]:-rows[2];
    r.transparent.clear();r.particleOrder.clear();r.transparentBatches.clear();bool orderedTransparency=false;
    auto append=[&](const DrawItem& item,float screenRadius,bool instanced=false) {
        for(auto& mesh:r.models[item.model]) {
            if(!Allegiance::ModelLodVisible(screenRadius,mesh.lodMin,mesh.lodMax))continue;
            uint32_t flags=(item.unlit?1u:0u)|(item.blend!=ModelBlend::Opaque?2u:0u)|(item.blend==ModelBlend::Additive?4u:0u)|
                (item.background?32u:0u)|(item.aleph?16u:(item.textureOffset!=0 || item.textureScale!=1)?8u:0u);
            Instance instance{item.transform,item.color,{r.worldDescriptors[mesh.texture],flags,std::bit_cast<uint32_t>(item.textureOffset),std::bit_cast<uint32_t>(item.textureScale)}};
            if(instanced){auto& bucket=r.opaqueBuckets[mesh.bucket];if(!bucket.count)r.opaqueGroups.push_back(mesh.bucket);++bucket.count;r.opaqueInstances.push_back({instance,mesh.bucket});}
            else{instances[instanceCount]=instance;commands[draws++]={mesh.indexCount,1,mesh.firstIndex,int32_t(mesh.vertexOffset),instanceCount++};}
            triangles+=mesh.indexCount/3;
        }
    };
    uint32_t environmentDraws=0;
    for(auto& item:items)if(item.background) {
        if(item.model>=r.models.size())throw std::runtime_error("Invalid backdrop model ID");
        if(item.background==1 && draws!=environmentDraws)throw std::runtime_error("Environment must precede posters");
        append(item,std::numeric_limits<float>::infinity());if(item.background==1)environmentDraws=draws;
    }
    uint32_t backgroundDraws=draws;
    for(auto& item:items) {
        if(item.background)continue;
        if(item.model>=r.models.size())throw std::runtime_error("Invalid model ID");
        float scale=std::max({glm::length(glm::vec3(item.transform[0])),glm::length(glm::vec3(item.transform[1])),glm::length(glm::vec3(item.transform[2]))});
        float radius=r.modelOwners[item.model]->radius*scale;
#ifdef __aarch64__
        if(frustum.outside(item.transform[3],radius))continue;
#else
        if(std::any_of(planes.begin(),planes.end(),[&](auto plane){return glm::dot(plane,item.transform[3]) < -radius;}))continue;
#endif
        float depth=glm::dot(rows[3],item.transform[3]);
        float screenRadius=depth>0?radius*focal/depth:radius;
        if(item.blend==ModelBlend::Opaque)append(item,screenRadius,true);else {
            r.transparent.push_back({&item,glm::dot(depthPlane,item.transform[3]),screenRadius,uint32_t(r.transparent.size())});
            orderedTransparency|=item.blend==ModelBlend::Alpha;
        }
    }
    // Histogram/scatter is linear in visible instances; bindless materials are
    // per instance, so even different skins share one geometry draw.
    for(uint32_t id:r.opaqueGroups){auto& b=r.opaqueBuckets[id];b.cursor=instanceCount;const auto& mesh=b.mesh;commands[draws++]={mesh.indexCount,b.count,mesh.firstIndex,int32_t(mesh.vertexOffset),instanceCount};instanceCount+=b.count;b.count=0;}
    for(const auto& entry:r.opaqueInstances)instances[r.opaqueBuckets[entry.bucket].cursor++]=entry.instance;
    uint32_t opaqueDraws=draws;
    uint32_t solidParticles=0;
    auto* particleInstances=static_cast<ParticleDraw*>(f.particles.mapped);
    for(uint32_t i=0;i<particles.size();++i) {
        const auto& p=particles[i];
        // Sprites are bounded by a sphere, including rotated billboards and
        // stretched projectiles. Keep ribbons, planes and screen-width lines
        // on their existing path; their payload has different geometry rules.
        if(!(p.texture&(particleRibbon|particleLine|particleOriented))) {
            float axisSquared=glm::dot(glm::vec3(p.axisWidth),glm::vec3(p.axisWidth));
            float radius=axisSquared>0.000001f?std::sqrt(axisSquared)+std::abs(p.centerSize.w*p.axisWidth.w):1.414214f*std::abs(p.centerSize.w);
            radius+=.001f+1e-5f*std::max({radius,std::abs(p.centerSize.x),std::abs(p.centerSize.y),std::abs(p.centerSize.z)});
#ifdef __aarch64__
            if(frustum.outside(glm::vec4(glm::vec3(p.centerSize),1),radius))continue;
#else
            if(std::any_of(planes.begin(),planes.end(),[&](const auto& plane){return glm::dot(plane,glm::vec4(glm::vec3(p.centerSize),1)) < -radius;}))continue;
#endif
        }
        if(p.texture&hudAdditive) {
            // Lines store endpoints instead of center/half-axis. Ribbons and
            // oriented shockwaves already store their center in centerSize.
            auto center=glm::vec3(p.centerSize);
            if(p.texture&particleLine)center=(center+glm::vec3(p.axisWidth))*.5f;
            r.particleOrder.push_back({orderedTransparency?glm::dot(depthPlane,glm::vec4(center,1)):0,i});
        } else particleInstances[solidParticles++]=p;
    }
    auto batch=[&](uint32_t first,uint32_t count,bool particles) {
        if(!count)return;
        if(!r.transparentBatches.empty()) {
            auto& previous=r.transparentBatches.back();
            if(previous.particles==particles && previous.first+previous.count==first){previous.count+=count;return;}
        }
        r.transparentBatches.push_back({first,count,particles});
    };
    // Pure additive effects commute. Keep that common case as two unsorted,
    // bindless batches; pay for ordering only when an alpha mesh is visible.
    if(orderedTransparency) {
        std::sort(r.transparent.begin(),r.transparent.end(),[](const auto& a,const auto& b){return a.depth!=b.depth?a.depth>b.depth:a.ordinal<b.ordinal;});
        std::sort(r.particleOrder.begin(),r.particleOrder.end(),[](const auto& a,const auto& b){return a.depth!=b.depth?a.depth>b.depth:a.index<b.index;});
    }
    uint32_t nextParticle=0;
    auto particle=[&] {
        uint32_t first=solidParticles+nextParticle;
        particleInstances[first]=particles[r.particleOrder[nextParticle++].index];batch(first,1,true);
    };
    for(const auto& entry:r.transparent) {
        if(orderedTransparency)while(nextParticle<r.particleOrder.size() && r.particleOrder[nextParticle].depth>entry.depth)particle();
        uint32_t first=draws;append(*entry.item,entry.screenRadius);batch(first,draws-first,false);
    }
    // Most battle effects are additive. Copy the remaining contiguous batch
    // directly rather than constructing/merging a batch for every particle.
    uint32_t remainingFirst=solidParticles+nextParticle;
    while(nextParticle<r.particleOrder.size()) {
        particleInstances[solidParticles+nextParticle]=particles[r.particleOrder[nextParticle].index];++nextParticle;
    }
    batch(remainingFirst,solidParticles+nextParticle-remainingFirst,true);
    size_t previewIndex=0;uint32_t previousOverlay=0;
    for(auto* p=preview;p;p=p->next) {
        if(p->item.model>=r.models.size() || p->overlayOffset>hud.size() || p->overlayOffset<previousOverlay)throw std::runtime_error("Invalid HUD preview");
        uint32_t first=draws;append(p->item,std::numeric_limits<float>::infinity());previewDraws[previewIndex++]={first,draws-first};previousOverlay=p->overlayOffset;
    }
    if(background.posterOffset>background.quads.size())throw std::runtime_error("Invalid HUD backdrop range");
    if(!background.quads.empty())std::memcpy(f.hud.mapped,background.quads.data(),background.quads.size_bytes());
    if(!hud.empty())std::memcpy(static_cast<HudQuad*>(f.hud.mapped)+background.quads.size(),hud.data(),hud.size_bytes());
    VK(vkResetFences(r.device,1,&f.fence));VK(vkResetCommandBuffer(f.command,0));
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;VK(vkBeginCommandBuffer(f.command,&begin));
#ifdef __APPLE__
    if(r.gpuQueries)vkCmdResetQueryPool(f.command,r.gpuQueries,r.frameIndex,1);
#endif
    if(f.videoPending) {
        auto& texture=r.textures[f.videoTexture];
        barrier(f.command,texture.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={texture.width,texture.height,1};
        vkCmdCopyBufferToImage(f.command,f.video.handle,texture.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        barrier(f.command,texture.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);f.videoPending=false;
    }
    // Chain the discard transition to the acquire semaphore's wait stage.
    // UNDEFINED discards pixels, but does not remove the presentation-engine dependency.
    barrier(f.command,r.images[r.imageIndex],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,0,VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    if(ramp)barrier(f.command,f.gammaImage.image,f.gammaInitialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        f.gammaInitialized?VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_2_NONE,f.gammaInitialized?VK_ACCESS_2_SHADER_SAMPLED_READ_BIT:0,VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    barrier(f.command,r.depth.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
    VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};color.imageView=r.views[r.imageIndex];color.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;color.clearValue.color={{0.002f,0.003f,0.009f,1}};
    if(ramp)color.imageView=f.gammaImage.view;
    if(r.samples!=VK_SAMPLE_COUNT_1_BIT){
        barrier(f.command,r.multisampleColor.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        color.resolveMode=VK_RESOLVE_MODE_AVERAGE_BIT;color.resolveImageView=color.imageView;color.resolveImageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.imageView=r.multisampleColor.view;color.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};depth.imageView=r.depth.view;depth.imageLayout=VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;depth.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;depth.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;depth.clearValue.depthStencil={0,0};
    VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};rendering.renderArea={{0,0},r.extent};rendering.layerCount=1;rendering.colorAttachmentCount=1;rendering.pColorAttachments=&color;rendering.pDepthAttachment=&depth;vkCmdBeginRendering(f.command,&rendering);
    VkViewport viewport{0,0,float(r.extent.width),float(r.extent.height),0,1};VkRect2D scissor{{0,0},r.extent};vkCmdSetViewport(f.command,0,1,&viewport);vkCmdSetScissor(f.command,0,1,&scissor);
    VkDescriptorSet sets[]={r.textureSet,f.set};vkCmdBindDescriptorSets(f.command,VK_PIPELINE_BIND_POINT_GRAPHICS,r.pipelineLayout,0,2,sets,0,nullptr);
    auto bindPipeline=[&](VkPipeline pipeline) {
        vkCmdBindPipeline(f.command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
#ifdef __APPLE__
        // Refresh MoltenVK's stage-specific argument-buffer assignments after
        // pipeline changes (HUD shaders omit the instance descriptor set).
        vkCmdBindDescriptorSets(f.command,VK_PIPELINE_BIND_POINT_GRAPHICS,r.pipelineLayout,0,2,sets,0,nullptr);
#endif
    };
    // All camera constants are consumed by vertex shaders. Binding them to an
    // unused fragment slot also aliases MoltenVK's bindless texture argument
    // buffer when switching between HUD and world pipelines.
    constexpr VkShaderStageFlags pushStages=VK_SHADER_STAGE_VERTEX_BIT;
    struct SceneCamera {glm::mat4 vp;RenderLighting light;};static_assert(sizeof(SceneCamera)==112);
    SceneCamera sceneCamera{viewProjection,lighting};
    sceneCamera.light.direction=glm::vec4(glm::normalize(glm::vec3(lighting.direction)),lighting.direction.w);
    vkCmdPushConstants(f.command,r.pipelineLayout,pushStages,0,sizeof(sceneCamera),&sceneCamera);
    VkDeviceSize offset=0;vkCmdBindVertexBuffers(f.command,0,1,&r.vertices.handle,&offset);vkCmdBindIndexBuffer(f.command,r.indices.handle,0,VK_INDEX_TYPE_UINT32);bindPipeline(r.scenePipeline);
    auto overlay=[&](uint32_t first,uint32_t count) {
        if(!count)return;
        bindPipeline(r.hudPipeline);
        vkCmdBindVertexBuffers(f.command,0,1,&f.hud.handle,&offset);vkCmdDraw(f.command,6,count,0,first);
    };
    auto backdrop=[&](uint32_t first,uint32_t count) {
        if(!count)return;
        bindPipeline(r.backgroundPipeline);
        vkCmdBindVertexBuffers(f.command,0,1,&r.vertices.handle,&offset);
        vkCmdDrawIndexedIndirect(f.command,f.indirect.handle,first*sizeof(VkDrawIndexedIndirectCommand),count,sizeof(VkDrawIndexedIndirectCommand));
    };
    backdrop(0,environmentDraws);overlay(0,background.posterOffset);
    backdrop(environmentDraws,backgroundDraws-environmentDraws);
    overlay(background.posterOffset,uint32_t(background.quads.size())-background.posterOffset);
    vkCmdBindVertexBuffers(f.command,0,1,&r.vertices.handle,&offset);bindPipeline(r.scenePipeline);
    if(opaqueDraws>backgroundDraws)vkCmdDrawIndexedIndirect(f.command,f.indirect.handle,backgroundDraws*sizeof(VkDrawIndexedIndirectCommand),opaqueDraws-backgroundDraws,sizeof(VkDrawIndexedIndirectCommand));
    struct ParticleCamera {glm::mat4 vp;glm::vec4 right,up;};
    ParticleCamera camera{viewProjection,glm::vec4(glm::normalize(glm::vec3(rows[0])),r.extent.width),glm::vec4(-glm::normalize(glm::vec3(rows[1])),r.extent.height)};
    auto drawParticles=[&](VkPipeline pipeline,uint32_t first,uint32_t count) {
        if(!count)return;
        vkCmdPushConstants(f.command,r.pipelineLayout,pushStages,0,sizeof(camera),&camera);
        bindPipeline(pipeline);
        vkCmdBindVertexBuffers(f.command,0,1,&f.particles.handle,&offset);vkCmdDraw(f.command,6,count,0,first);
    };
    drawParticles(r.solidParticlePipeline,0,solidParticles);
    for(const auto& batch:r.transparentBatches) {
        if(batch.particles)drawParticles(r.particlePipeline,batch.first,batch.count);
        else {
            vkCmdPushConstants(f.command,r.pipelineLayout,pushStages,0,sizeof(sceneCamera),&sceneCamera);
            vkCmdBindVertexBuffers(f.command,0,1,&r.vertices.handle,&offset);
            bindPipeline(r.transparentPipeline);
            vkCmdDrawIndexedIndirect(f.command,f.indirect.handle,batch.first*sizeof(VkDrawIndexedIndirectCommand),batch.count,sizeof(VkDrawIndexedIndirectCommand));
        }
    }
    uint32_t hudStart=uint32_t(background.quads.size());
    previousOverlay=0;previewIndex=0;
    for(auto* p=preview;p;p=p->next) {
        overlay(hudStart+previousOverlay,p->overlayOffset-previousOverlay);
        auto b=p->bounds;
        // Clamp the clear/scissor to the framebuffer, including on tiny windows.
        auto lo=glm::clamp(glm::ivec2(b),glm::ivec2(0),glm::ivec2(r.extent.width,r.extent.height));
        auto hi=glm::clamp(glm::ivec2(b)+glm::ivec2(b.z,b.w),lo,glm::ivec2(r.extent.width,r.extent.height));
        if(p->clip){auto c=*p->clip;lo=glm::max(lo,glm::clamp(glm::ivec2(c),glm::ivec2(0),glm::ivec2(r.extent.width,r.extent.height)));
            hi=glm::min(hi,glm::clamp(glm::ivec2(c)+glm::ivec2(c.z,c.w),glm::ivec2(0),glm::ivec2(r.extent.width,r.extent.height)));}
        if(hi.x>lo.x && hi.y>lo.y) {
            VkRect2D region{{lo.x,lo.y},{uint32_t(hi.x-lo.x),uint32_t(hi.y-lo.y)}};
            VkClearAttachment clear{};clear.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;clear.clearValue.depthStencil={0,0};
            VkClearRect clearRect{region,0,1};vkCmdClearAttachments(f.command,1,&clear,1,&clearRect);
            VkViewport pv{float(b.x),float(b.y),float(b.z),float(b.w),0,1};
            vkCmdSetViewport(f.command,0,1,&pv);vkCmdSetScissor(f.command,0,1,&region);
            bindPipeline(p->item.blend==ModelBlend::Opaque?r.scenePipeline:r.transparentPipeline);
            vkCmdBindVertexBuffers(f.command,0,1,&r.vertices.handle,&offset);
            SceneCamera previewCamera{p->viewProjection,sceneCamera.light};
            vkCmdPushConstants(f.command,r.pipelineLayout,pushStages,0,sizeof(previewCamera),&previewCamera);
            auto range=previewDraws[previewIndex];vkCmdDrawIndexedIndirect(f.command,f.indirect.handle,range.first*sizeof(VkDrawIndexedIndirectCommand),range.second,sizeof(VkDrawIndexedIndirectCommand));
            vkCmdSetViewport(f.command,0,1,&viewport);vkCmdSetScissor(f.command,0,1,&scissor);
        }
        previousOverlay=p->overlayOffset;++previewIndex;
    }
    overlay(hudStart+previousOverlay,uint32_t(hud.size())-previousOverlay);
    vkCmdEndRendering(f.command);
    if(ramp){
        barrier(f.command,f.gammaImage.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);f.gammaInitialized=true;
        color.imageView=r.views[r.imageIndex];color.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        color.resolveMode=VK_RESOLVE_MODE_NONE;color.resolveImageView=VK_NULL_HANDLE;color.resolveImageLayout=VK_IMAGE_LAYOUT_UNDEFINED;rendering.pDepthAttachment=nullptr;
        vkCmdBeginRendering(f.command,&rendering);bindPipeline(r.gammaPipeline);
        vkCmdPushConstants(f.command,r.pipelineLayout,VK_SHADER_STAGE_FRAGMENT_BIT,112,sizeof(f.gammaDescriptor),&f.gammaDescriptor);vkCmdDraw(f.command,3,1,0,0);vkCmdEndRendering(f.command);
    }
    Buffer capture;
    if(!screenshot.empty()) {
        capture=r.buffer(size_t(r.extent.width)*r.extent.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        barrier(f.command,r.images[r.imageIndex],VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={r.extent.width,r.extent.height,1};vkCmdCopyImageToBuffer(f.command,r.images[r.imageIndex],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,capture.handle,1,&copy);
    }
    barrier(f.command,r.images[r.imageIndex],screenshot.empty()?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        screenshot.empty()?VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT:VK_PIPELINE_STAGE_2_TRANSFER_BIT,screenshot.empty()?VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT:VK_ACCESS_2_TRANSFER_READ_BIT,0,0);
#ifdef __APPLE__
    if(r.gpuQueries)vkCmdWriteTimestamp2(f.command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,r.gpuQueries,r.frameIndex);
#endif
    VK(vkEndCommandBuffer(f.command));
    VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};wait.semaphore=f.acquired;wait.stageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};signal.stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signal.semaphore=r.rendered[r.imageIndex];
    VkCommandBufferSubmitInfo cmd{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};cmd.commandBuffer=f.command;
    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.waitSemaphoreInfoCount=1;submit.pWaitSemaphoreInfos=&wait;
    submit.commandBufferInfoCount=1;submit.pCommandBufferInfos=&cmd;submit.signalSemaphoreInfoCount=1;submit.pSignalSemaphoreInfos=&signal;VK(vkQueueSubmit2(r.queue,1,&submit,f.fence));
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&r.rendered[r.imageIndex];present.swapchainCount=1;present.pSwapchains=&r.swapchain;present.pImageIndices=&r.imageIndex;
    auto result=r.presentation.present(r.queue,present);if(result==VK_ERROR_OUT_OF_DATE_KHR || result==VK_SUBOPTIMAL_KHR)r.recreate=true;else VK(result);
#ifdef __APPLE__
    if(r.gpuQueries)r.gpuQueryIds[r.frameIndex]=r.presentation.id();
#endif
    if(capture.handle) {
        VK(vkWaitForFences(r.device,1,&f.fence,VK_TRUE,UINT64_MAX));
        std::ofstream out(screenshot,std::ios::binary);out<<"P6\n"<<r.extent.width<<" "<<r.extent.height<<"\n255\n";
        auto* pixels=static_cast<uint8_t*>(capture.mapped);bool bgra=r.format==VK_FORMAT_B8G8R8A8_SRGB || r.format==VK_FORMAT_B8G8R8A8_UNORM;
        for(size_t i=0;i<size_t(r.extent.width)*r.extent.height;++i){char rgb[]={char(pixels[i*4+(bgra?2:0)]),char(pixels[i*4+1]),char(pixels[i*4+(bgra?0:2)])};out.write(rgb,3);}
        r.destroy(capture);
    }
    uint32_t particleBatches=solidParticles>0;for(const auto& b:r.transparentBatches)particleBatches+=b.particles;
    r.stats.draws=draws+particleBatches;r.stats.triangles=triangles+(solidParticles+uint32_t(r.particleOrder.size()))*2;r.stats.latencyMs=r.presentation.lastLatencyMs();r.stats.missed=r.presentation.pacer().GetMisses();
    auto period=r.presentation.pacer().GetRefreshPeriod();r.stats.refreshHz=period?1e9/double(period):0;r.frameIndex=(r.frameIndex+1)%r.frames.size();
}
}
