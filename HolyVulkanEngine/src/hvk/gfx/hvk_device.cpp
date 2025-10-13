#include "pch.h"

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_window.h>

namespace hvk {

    // ----- local helpers -----

    static bool hasLayer(const char* name, const std::vector<VkLayerProperties>& avail) {
        for (auto& l : avail) if (std::strcmp(name, l.layerName) == 0) return true;
        return false;
    }
    static bool hasExtension(const char* name, const std::vector<VkExtensionProperties>& avail) {
        for (auto& e : avail) if (std::strcmp(name, e.extensionName) == 0) return true;
        return false;
    }

    // debug utils loaders
    static PFN_vkCreateDebugUtilsMessengerEXT  pfnCreateDebugUtilsMessengerEXT = nullptr;
    static PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyDebugUtilsMessengerEXT = nullptr;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT sev,
        VkDebugUtilsMessageTypeFlagsEXT        type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void*)
    {
        std::cerr << "[VK] "
            << ((sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR " :
                (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN  " :
                (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "INFO  " : "VERB  ")
            << data->pMessage << std::endl;
        return VK_FALSE;
    }

    VkDebugUtilsMessageSeverityFlagsEXT Device::severityMaskFor(DebugVerbosity v) {
        switch (v) {
        case DebugVerbosity::None:    return 0;
        case DebugVerbosity::Error:   return VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        case DebugVerbosity::Warn:    return VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        case DebugVerbosity::Info:    return VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        case DebugVerbosity::Verbose: return VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        }
        return 0;
    }


    // ----- ctor/dtor/move -----

    Device::Device(const Window& window, const DeviceCreateInfo& ci) {
        createInstance(window, ci);
        setupDebugMessenger();
        createSurface(window);
        pickPhysicalDevice(ci);
        buildFeatureChain(ci);
        createDeviceAndQueues(ci);
#if ENGINE_USE_VMA
        createAllocator();
#endif
    }

    Device::~Device() {
#if ENGINE_USE_VMA
        destroyAllocator();
#endif
        if (device_) { vkDeviceWaitIdle(device_); vkDestroyDevice(device_, nullptr); device_ = VK_NULL_HANDLE; }
        if (surface_) { vkDestroySurfaceKHR(instance_, surface_, nullptr); surface_ = VK_NULL_HANDLE; }
        destroyDebugMessenger();
        if (instance_) { vkDestroyInstance(instance_, nullptr); instance_ = VK_NULL_HANDLE; }
    }

    Device::Device(Device&& o) noexcept {
        *this = std::move(o);
    }
    Device& Device::operator=(Device&& o) noexcept {
        if (this == &o) return *this;

#if ENGINE_USE_VMA
        if (allocator_) { vmaDestroyAllocator(allocator_); allocator_ = VK_NULL_HANDLE; }
#endif
        if (device_) { vkDestroyDevice(device_, nullptr); device_ = VK_NULL_HANDLE; }
        if (surface_) { vkDestroySurfaceKHR(instance_, surface_, nullptr); surface_ = VK_NULL_HANDLE; }
        destroyDebugMessenger();
        if (instance_) { vkDestroyInstance(instance_, nullptr); instance_ = VK_NULL_HANDLE; }

        instance_ = o.instance_; o.instance_ = VK_NULL_HANDLE;
        debug_ = o.debug_;    o.debug_ = VK_NULL_HANDLE;
        fnSetName_ = o.fnSetName_; o.fnSetName_ = nullptr;

        surface_ = o.surface_;  o.surface_ = VK_NULL_HANDLE;
        physical_ = o.physical_; o.physical_ = VK_NULL_HANDLE;

        props10_ = o.props10_;
        memProps_ = o.memProps_;
        props14_ = o.props14_;

        enabled10_ = o.enabled10_;
        enabled12_ = o.enabled12_;
        enabled13_ = o.enabled13_;
        enabled14_ = o.enabled14_;

        device_ = o.device_;   o.device_ = VK_NULL_HANDLE;
        graphics_ = o.graphics_; o.graphics_ = {};
        present_ = o.present_;  o.present_ = {};
        transfer_ = o.transfer_; o.transfer_ = {};
        compute_ = o.compute_;  o.compute_ = {};

#if ENGINE_USE_VMA
        allocator_ = o.allocator_; o.allocator_ = VK_NULL_HANDLE;
#endif

        return *this;
    }

    // ----- public util -----

    void Device::setObjectName(VkObjectType type, uint64_t handle, std::string_view name) const {
        if (!fnSetName_ || name.empty()) return;
        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name.data();
        fnSetName_(device_, &info);
    }

    // ----- instance & debug -----

    void Device::createInstance(const Window& window, const DeviceCreateInfo& ci) {
        // enumerate layers/exts
        uint32_t lc = 0; vkEnumerateInstanceLayerProperties(&lc, nullptr);
        std::vector<VkLayerProperties> layers(lc); if (lc) vkEnumerateInstanceLayerProperties(&lc, layers.data());
        uint32_t ec = 0; vkEnumerateInstanceExtensionProperties(nullptr, &ec, nullptr);
        std::vector<VkExtensionProperties> exts(ec); if (ec) vkEnumerateInstanceExtensionProperties(nullptr, &ec, exts.data());

        std::vector<const char*> req = Window::requiredVulkanInstanceExtensions();
#ifndef NDEBUG
        if (ci.enableValidation && hasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, exts))
            req.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        for (auto* e : ci.extraInstanceExtensions) req.push_back(e);

        std::vector<const char*> layersEnabled;
#ifndef NDEBUG
        if (ci.enableValidation && hasLayer("VK_LAYER_KHRONOS_validation", layers))
            layersEnabled.push_back("VK_LAYER_KHRONOS_validation");
#endif
        for (auto* l : ci.extraInstanceLayers) layersEnabled.push_back(l);

        // cache debug policy (even if it ends up unused in Release)
        dbgVerbosity_ = ci.debugVerbosity;
        dbgTypes_ = ci.debugMessageTypes;

        VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        app.pApplicationName = ci.appName ? ci.appName : "Engine";
        app.applicationVersion = ci.appVersion;
        app.pEngineName = "Engine";
        app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        app.apiVersion = VK_API_VERSION_1_4;

        // pre-chain debug for early messages (only in Debug builds with validation)
        VkDebugUtilsMessengerCreateInfoEXT dbg{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
#ifndef NDEBUG
        if (ci.enableValidation) {
            dbg.messageSeverity = severityMaskFor(dbgVerbosity_);
            dbg.messageType = dbgTypes_;
            dbg.pfnUserCallback = debugCallback;
        }
#endif

        VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        ici.pApplicationInfo = &app;
        ici.enabledLayerCount = static_cast<uint32_t>(layersEnabled.size());
        ici.ppEnabledLayerNames = layersEnabled.data();
        ici.enabledExtensionCount = static_cast<uint32_t>(req.size());
        ici.ppEnabledExtensionNames = req.data();
#ifndef NDEBUG
        if (ci.enableValidation) ici.pNext = &dbg;
#endif

        VK_CHECK(vkCreateInstance(&ici, nullptr, &instance_));

        // load debug utils function pointers
        pfnCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        pfnDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        fnSetName_ = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetInstanceProcAddr(instance_, "vkSetDebugUtilsObjectNameEXT"));
    }

    void Device::setupDebugMessenger() {
#ifndef NDEBUG
        if (!pfnCreateDebugUtilsMessengerEXT) return;
        // If severity is None, skip creating the messenger (acts like off)
        VkDebugUtilsMessageSeverityFlagsEXT sev = severityMaskFor(dbgVerbosity_);
        if (sev == 0) return;

        VkDebugUtilsMessengerCreateInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        info.messageSeverity = sev;
        info.messageType = dbgTypes_;
        info.pfnUserCallback = debugCallback;
        VK_CHECK(pfnCreateDebugUtilsMessengerEXT(instance_, &info, nullptr, &debug_));
#endif
    }

    void Device::destroyDebugMessenger() {
#ifndef NDEBUG
        if (debug_ && pfnDestroyDebugUtilsMessengerEXT) {
            pfnDestroyDebugUtilsMessengerEXT(instance_, debug_, nullptr);
            debug_ = VK_NULL_HANDLE;
        }
#endif
    }

    void Device::setDebugVerbosity(DebugVerbosity v) {
        dbgVerbosity_ = v;
#ifndef NDEBUG
        // If validation isn’t enabled or extension missing, this was a no-op earlier anyway.
        // Recreate messenger to apply new severities.
        destroyDebugMessenger();
        setupDebugMessenger();
#endif
    }

    void Device::setDebugMessageTypes(VkDebugUtilsMessageTypeFlagsEXT types) {
        dbgTypes_ = types;
#ifndef NDEBUG
        destroyDebugMessenger();
        setupDebugMessenger();
#endif
    }

    // ----- surface & physical -----

    void Device::createSurface(const Window& window) {
        surface_ = window.createVulkanSurface(instance_);
    }

    bool Device::supportsPresentation(VkPhysicalDevice pd, uint32_t family, VkSurfaceKHR surface) {
        VkBool32 s = VK_FALSE; vkGetPhysicalDeviceSurfaceSupportKHR(pd, family, surface, &s);
        return s == VK_TRUE;
    }

    bool Device::deviceMeetsBasics(VkPhysicalDevice pd, VkSurfaceKHR surface, std::unordered_set<std::string>& missingExt) {
        missingExt.clear();
        uint32_t ec = 0; vkEnumerateDeviceExtensionProperties(pd, nullptr, &ec, nullptr);
        std::vector<VkExtensionProperties> devExt(ec); if (ec) vkEnumerateDeviceExtensionProperties(pd, nullptr, &ec, devExt.data());
        auto need = std::unordered_set<std::string>{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        for (auto& e : devExt) need.erase(e.extensionName);
        if (!need.empty()) { missingExt = std::move(need); return false; }

        // Require a graphics & a present-capable family
        uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn); if (qn) vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qf.data());
        bool hasG = false, hasP = false;
        for (uint32_t i = 0; i < qn; ++i) {
            if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) hasG = true;
            if (supportsPresentation(pd, i, surface))     hasP = true;
        }
        return hasG && hasP;
    }

    uint32_t Device::scorePhysicalDevice(VkPhysicalDevice pd, VkSurfaceKHR) {
        VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(pd, &props);
        uint32_t score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   score += 1000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 200;

        VkPhysicalDeviceMemoryProperties mem{}; vkGetPhysicalDeviceMemoryProperties(pd, &mem);
        for (uint32_t i = 0; i < mem.memoryHeapCount; ++i)
            if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                score += static_cast<uint32_t>(mem.memoryHeaps[i].size / (256ull * 1024ull * 1024ull));
        return score;
    }

    void Device::pickPhysicalDevice(const DeviceCreateInfo& ci) {
        uint32_t n = 0; vkEnumeratePhysicalDevices(instance_, &n, nullptr);
        if (!n) throw std::runtime_error("No Vulkan physical devices found.");
        std::vector<VkPhysicalDevice> pds(n); vkEnumeratePhysicalDevices(instance_, &n, pds.data());

        VkPhysicalDevice best = VK_NULL_HANDLE;
        uint32_t bestScore = 0;
        for (auto pd : pds) {
            std::unordered_set<std::string> missing;
            if (!deviceMeetsBasics(pd, surface_, missing)) continue;
            uint32_t s = scorePhysicalDevice(pd, surface_);
            if (s > bestScore) { best = pd; bestScore = s; }
        }
        if (!best) throw std::runtime_error("No suitable GPU supports required WSI features.");

        physical_ = best;
        vkGetPhysicalDeviceProperties(physical_, &props10_);
        vkGetPhysicalDeviceMemoryProperties(physical_, &memProps_);

        // Also fetch 1.4 properties (optional but useful)
        VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        props2.pNext = &props14_;
        vkGetPhysicalDeviceProperties2(physical_, &props2);
    }

    // ----- features chain & logical device -----

    void Device::buildFeatureChain(const DeviceCreateInfo& ci) {
        // Supported 1.0 features:
        VkPhysicalDeviceFeatures supp10{}; vkGetPhysicalDeviceFeatures(physical_, &supp10);

        // Query 1.2/1.3/1.4 supported features
        VkPhysicalDeviceVulkan12Features supp12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        VkPhysicalDeviceVulkan13Features supp13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        VkPhysicalDeviceVulkan14Features supp14{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };
        VkPhysicalDeviceFeatures2 feats2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        feats2.pNext = &supp12; supp12.pNext = &supp13; supp13.pNext = &supp14;
        vkGetPhysicalDeviceFeatures2(physical_, &feats2);

        // zero-init enabled
        enabled10_ = {};
        enabled12_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        enabled13_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        enabled14_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };

        // 1.0
        if (ci.enableAnisotropy && supp10.samplerAnisotropy) enabled10_.samplerAnisotropy = VK_TRUE;

        // 1.2
        if (ci.enableBufferDeviceAddress && supp12.bufferDeviceAddress) enabled12_.bufferDeviceAddress = VK_TRUE;
        if (ci.enableDescriptorIndexing && supp12.descriptorIndexing) {
            enabled12_.descriptorIndexing = VK_TRUE;
            if (supp12.runtimeDescriptorArray)                   enabled12_.runtimeDescriptorArray = VK_TRUE;
            if (supp12.descriptorBindingPartiallyBound)          enabled12_.descriptorBindingPartiallyBound = VK_TRUE;
            if (supp12.descriptorBindingVariableDescriptorCount) enabled12_.descriptorBindingVariableDescriptorCount = VK_TRUE;
            if (supp12.shaderSampledImageArrayNonUniformIndexing)enabled12_.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
            if (supp12.shaderStorageBufferArrayNonUniformIndexing)enabled12_.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
            if (supp12.shaderUniformBufferArrayNonUniformIndexing)enabled12_.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
        }
        if (ci.enableTimelineSemaphore && supp12.timelineSemaphore) enabled12_.timelineSemaphore = VK_TRUE;

        // 1.3
        if (ci.enableDynamicRendering && supp13.dynamicRendering) enabled13_.dynamicRendering = VK_TRUE;
        if (ci.enableSynchronization2 && supp13.synchronization2) enabled13_.synchronization2 = VK_TRUE;

        // 1.4 (pick a sensible default set for engines)
        if (ci.enableIndexTypeUint8 && supp14.indexTypeUint8)            enabled14_.indexTypeUint8 = VK_TRUE;
        if (ci.enableDynamicRenderingLocalRead && supp14.dynamicRenderingLocalRead) enabled14_.dynamicRenderingLocalRead = VK_TRUE;
        if (ci.enableMaintenance5 && supp14.maintenance5)              enabled14_.maintenance5 = VK_TRUE;
        if (ci.enableMaintenance6 && supp14.maintenance6)              enabled14_.maintenance6 = VK_TRUE;
        if (ci.enableHostImageCopy && supp14.hostImageCopy)             enabled14_.hostImageCopy = VK_TRUE;
        if (ci.enablePushDescriptor && supp14.pushDescriptor)            enabled14_.pushDescriptor = VK_TRUE;
        if (ci.enablePipelineRobustnessFlag && supp14.pipelineRobustness)        enabled14_.pipelineRobustness = VK_TRUE;
        if (ci.enablePipelineProtectedAccess && supp14.pipelineProtectedAccess)   enabled14_.pipelineProtectedAccess = VK_TRUE;

        if (ci.enableShaderSubgroupRotate && supp14.shaderSubgroupRotate)          enabled14_.shaderSubgroupRotate = VK_TRUE;
        if (ci.enableShaderSubgroupRotateClustered && supp14.shaderSubgroupRotateClustered) enabled14_.shaderSubgroupRotateClustered = VK_TRUE;
        if (ci.enableShaderFloatControls2 && supp14.shaderFloatControls2)          enabled14_.shaderFloatControls2 = VK_TRUE;
        if (ci.enableShaderExpectAssume && supp14.shaderExpectAssume)            enabled14_.shaderExpectAssume = VK_TRUE;

        if (ci.enableRectangularLines && supp14.rectangularLines)              enabled14_.rectangularLines = VK_TRUE;
        if (ci.enableBresenhamLines && supp14.bresenhamLines)                enabled14_.bresenhamLines = VK_TRUE;
        if (ci.enableSmoothLines && supp14.smoothLines)                   enabled14_.smoothLines = VK_TRUE;
        if (ci.enableStippledRectangularLines && supp14.stippledRectangularLines)    enabled14_.stippledRectangularLines = VK_TRUE;
        if (ci.enableStippledBresenhamLines && supp14.stippledBresenhamLines)        enabled14_.stippledBresenhamLines = VK_TRUE;
        if (ci.enableStippledSmoothLines && supp14.stippledSmoothLines)           enabled14_.stippledSmoothLines = VK_TRUE;

        if (ci.enableVertexAttribDivisor && supp14.vertexAttributeInstanceRateDivisor)      enabled14_.vertexAttributeInstanceRateDivisor = VK_TRUE;
        if (ci.enableVertexAttribZeroDivisor && supp14.vertexAttributeInstanceRateZeroDivisor)  enabled14_.vertexAttributeInstanceRateZeroDivisor = VK_TRUE;
    }

    void Device::createDeviceAndQueues(const DeviceCreateInfo& ci) {
        // queue family discovery
        uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(physical_, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn); vkGetPhysicalDeviceQueueFamilyProperties(physical_, &qn, qf.data());

        std::optional<uint32_t> gfx, present, trans, comp;

        for (uint32_t i = 0; i < qn; ++i) if ((qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && !gfx) gfx = i;
        for (uint32_t i = 0; i < qn; ++i) if (supportsPresentation(physical_, i, surface_)) { present = i; break; }

        if (ci.requestSeparateTransferQueue) {
            for (uint32_t i = 0; i < qn; ++i)
                if ((qf[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                    !(qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                    !(qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                    trans = i; break;
                }
        }
        if (!trans) for (uint32_t i = 0; i < qn; ++i) if (qf[i].queueFlags & VK_QUEUE_TRANSFER_BIT) { trans = i; break; }

        if (ci.requestSeparateComputeQueue) {
            for (uint32_t i = 0; i < qn; ++i)
                if ((qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && !(qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { comp = i; break; }
        }
        if (!comp) for (uint32_t i = 0; i < qn; ++i) if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { comp = i; break; }

        if (!gfx || !present) throw std::runtime_error("Required queue families (graphics/present) not found.");

        std::set<uint32_t> fams{ *gfx, *present, trans.value_or(*gfx), comp.value_or(*gfx) };
        float prio = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> qcis; qcis.reserve(fams.size());
        for (auto f : fams) {
            VkDeviceQueueCreateInfo qi{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
            qi.queueFamilyIndex = f; qi.queueCount = 1; qi.pQueuePriorities = &prio;
            qcis.push_back(qi);
        }

        // device extensions
        std::vector<const char*> devExts{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        for (auto* e : ci.extraDeviceExtensions) devExts.push_back(e);

        // build pNext chain for features
        VkPhysicalDeviceFeatures2 feats2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        feats2.features = enabled10_;
        feats2.pNext = &enabled12_;
        enabled12_.pNext = &enabled13_;
        enabled13_.pNext = &enabled14_;

        VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
        dci.pQueueCreateInfos = qcis.data();
        dci.enabledExtensionCount = static_cast<uint32_t>(devExts.size());
        dci.ppEnabledExtensionNames = devExts.data();
        dci.pNext = &feats2;

        VK_CHECK(vkCreateDevice(physical_, &dci, nullptr, &device_));

        // fetch queues
        for (auto f : fams) {
            VkQueue q = VK_NULL_HANDLE; vkGetDeviceQueue(device_, f, 0, &q);
            if (f == *gfx)     graphics_ = { q, f };
            if (f == *present) present_ = { q, f };
            if (trans && f == *trans) transfer_ = { q, f };
            if (comp && f == *comp) compute_ = { q, f };
        }
        if (!transfer_.handle) transfer_ = graphics_;
        if (!compute_.handle)  compute_ = graphics_;
    }

    // ----- VMA -----

#if ENGINE_USE_VMA
    void Device::createAllocator() {
        VmaAllocatorCreateInfo aci{};
        aci.instance = instance_;
        aci.device = device_;
        aci.physicalDevice = physical_;
        aci.vulkanApiVersion = VK_API_VERSION_1_4; // <<< tie allocator to 1.4
        if (enabled12_.bufferDeviceAddress)
            aci.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        VK_CHECK(vmaCreateAllocator(&aci, &allocator_));
    }
    void Device::destroyAllocator() {
        if (allocator_) { vmaDestroyAllocator(allocator_); allocator_ = VK_NULL_HANDLE; }
    }
#endif

} // namespace hvk