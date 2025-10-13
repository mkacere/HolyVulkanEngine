#ifndef HVK_DEVICE
#define HVK_DEVICE

// Vulkan 1.4.313 device wrapper (RAII, OOP, game-engine ready).
// - Creates VkInstance (VK_API_VERSION_1_4) + debug messenger (Debug builds)
// - Creates VkSurfaceKHR from engine::Window
// - Picks VkPhysicalDevice, creates VkDevice + queues
// - Enables a curated set of 1.2/1.3/1.4 features when supported
// - Optional VMA allocator bound to 1.4
// - No swapchain (leave that to your Renderer)

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <optional>
#include <unordered_set>

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifndef ENGINE_USE_VMA
#define ENGINE_USE_VMA 1
#endif
#if ENGINE_USE_VMA
#include "vk_mem_alloc.h"
#endif

namespace hvk {

    class Window; // fwd

#ifndef VK_CHECK
#define VK_CHECK(x) do { VkResult _e = (x); if (_e != VK_SUCCESS) throw std::runtime_error("Vulkan error: " #x); } while(0)
#endif

    // -------------------------------------------------------------------------
    // Debug verbosity for VK_EXT_debug_utils
    // -------------------------------------------------------------------------
    enum class DebugVerbosity {
        None,       // no messages
        Error,      // errors only
        Warn,       // warnings + errors
        Info,       // info + warnings + errors
        Verbose     // verbose + info + warnings + errors
    };

    struct DeviceCreateInfo {
        const char* appName = "Engine";
        uint32_t    appVersion = VK_MAKE_VERSION(0, 1, 0);
        bool        enableValidation =
#ifdef NDEBUG
            false
#else
            true
#endif
            ;

        // NEW: fine-grained debug control (only takes effect when enableValidation=true and in !NDEBUG builds)
        DebugVerbosity debugVerbosity = DebugVerbosity::Info;
        VkDebugUtilsMessageTypeFlagsEXT debugMessageTypes =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        std::vector<const char*> extraInstanceLayers{};
        std::vector<const char*> extraInstanceExtensions{};
        std::vector<const char*> extraDeviceExtensions{}; // VK_KHR_swapchain is auto-added

        // Queue prefs
        bool requestSeparateTransferQueue = true;
        bool requestSeparateComputeQueue = true;

        // Vulkan 1.2 / 1.3 toggles (enabled if supported)
        bool enableAnisotropy = true;
        bool enableBufferDeviceAddress = true;
        bool enableDescriptorIndexing = true;
        bool enableTimelineSemaphore = true;
        bool enableDynamicRendering = true; // 1.3 core
        bool enableSynchronization2 = true; // 1.3 core

        // Vulkan 1.4 curated toggles (enabled if supported)
        bool enableIndexTypeUint8 = true;
        bool enableDynamicRenderingLocalRead = true;
        bool enableMaintenance5 = true;
        bool enableMaintenance6 = true;
        bool enableHostImageCopy = true;
        bool enablePushDescriptor = false; // opt-in; many engines still use regular sets/bindless
        bool enablePipelineRobustnessFlag = true;  // enables the *ability* to request per-pipeline robustness; not forced
        bool enablePipelineProtectedAccess = false; // off by default unless you need protected content paths
        // Shader niceties
        bool enableShaderSubgroupRotate = false;
        bool enableShaderSubgroupRotateClustered = false;
        bool enableShaderFloatControls2 = true;
        bool enableShaderExpectAssume = true;
        // Line rasterization variants (enable if you actually use lines)
        bool enableRectangularLines = false;
        bool enableBresenhamLines = false;
        bool enableSmoothLines = false;
        bool enableStippledRectangularLines = false;
        bool enableStippledBresenhamLines = false;
        bool enableStippledSmoothLines = false;

        // Vertex instancing divisors (handy for GPU-driven instancing)
        bool enableVertexAttribDivisor = true;
        bool enableVertexAttribZeroDivisor = true;
    };

    struct Queue {
        VkQueue   handle = VK_NULL_HANDLE;
        uint32_t  family = ~0u;
    };

    class Device {
    public:
        explicit Device(const Window& window, const DeviceCreateInfo& ci = {});
        ~Device();

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&)                 noexcept;
        Device& operator=(Device&&)      noexcept;

        // Handles
        VkInstance       instance()   const { return instance_; }
        VkPhysicalDevice physical()   const { return physical_; }
        VkDevice         device()     const { return device_; }
        VkSurfaceKHR     surface()    const { return surface_; }

        // Queues
        const Queue& graphics() const { return graphics_; }
        const Queue& present()  const { return present_; }
        const Queue& transfer() const { return transfer_; }
        const Queue& compute()  const { return compute_; }

        // Properties / limits
        const VkPhysicalDeviceProperties& properties()        const { return props10_; }
        const VkPhysicalDeviceMemoryProperties& memoryProperties()  const { return memProps_; }
        const VkPhysicalDeviceLimits& limits()            const { return props10_.limits; }

        // Enabled features (actual)
        const VkPhysicalDeviceFeatures& features10() const { return enabled10_; }
        const VkPhysicalDeviceVulkan12Features& features12() const { return enabled12_; }
        const VkPhysicalDeviceVulkan13Features& features13() const { return enabled13_; }
        const VkPhysicalDeviceVulkan14Features& features14() const { return enabled14_; }

        // 1.4 properties queried (optional but handy)
        const VkPhysicalDeviceVulkan14Properties& properties14() const { return props14_; }

#if ENGINE_USE_VMA
        VmaAllocator allocator() const { return allocator_; }
#endif

        void waitIdle() const { vkDeviceWaitIdle(device_); }
        void setObjectName(VkObjectType type, uint64_t handle, std::string_view name) const;

        // ---------------------------------------------------------------------
        // Debug controls (runtime)
        //   - No-ops if validation is disabled or VK_EXT_debug_utils unavailable.
        // ---------------------------------------------------------------------
        void setDebugVerbosity(DebugVerbosity v); // recreate messenger with new severity
        DebugVerbosity debugVerbosity() const { return dbgVerbosity_; }

        void setDebugMessageTypes(VkDebugUtilsMessageTypeFlagsEXT types); // recreate messenger with new type mask
        VkDebugUtilsMessageTypeFlagsEXT debugMessageTypes() const { return dbgTypes_; }

    private:
        // Steps
        void createInstance(const Window& window, const DeviceCreateInfo& ci);
        void setupDebugMessenger();
        void destroyDebugMessenger();
        void createSurface(const Window& window);
        void pickPhysicalDevice(const DeviceCreateInfo& ci);
        void buildFeatureChain(const DeviceCreateInfo& ci);
        void createDeviceAndQueues(const DeviceCreateInfo& ci);

#if ENGINE_USE_VMA
        void createAllocator();
        void destroyAllocator();
#endif

        // Helpers
        static bool supportsPresentation(VkPhysicalDevice pd, uint32_t family, VkSurfaceKHR surface);
        static bool deviceMeetsBasics(VkPhysicalDevice pd, VkSurfaceKHR surface, std::unordered_set<std::string>& missingExt);
        static uint32_t scorePhysicalDevice(VkPhysicalDevice pd, VkSurfaceKHR surface);

        // map verbosity→severity flags
        static VkDebugUtilsMessageSeverityFlagsEXT severityMaskFor(DebugVerbosity v);

    private:
        // Instance & debug
        VkInstance                       instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT         debug_ = VK_NULL_HANDLE;
        PFN_vkSetDebugUtilsObjectNameEXT fnSetName_ = nullptr;

        // NEW: cached debug policy
        DebugVerbosity                   dbgVerbosity_ = DebugVerbosity::Info;
        VkDebugUtilsMessageTypeFlagsEXT  dbgTypes_ =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        // Surface
        VkSurfaceKHR                     surface_ = VK_NULL_HANDLE;

        // Physical
        VkPhysicalDevice                 physical_ = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties       props10_{};
        VkPhysicalDeviceMemoryProperties memProps_{};
        VkPhysicalDeviceVulkan14Properties props14_{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES };

        // Enabled features
        VkPhysicalDeviceFeatures         enabled10_{};
        VkPhysicalDeviceVulkan12Features enabled12_{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        VkPhysicalDeviceVulkan13Features enabled13_{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        VkPhysicalDeviceVulkan14Features enabled14_{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };

        // Logical & queues
        VkDevice                         device_ = VK_NULL_HANDLE;
        Queue                            graphics_{};
        Queue                            present_{};
        Queue                            transfer_{};
        Queue                            compute_{};

#if ENGINE_USE_VMA
        VmaAllocator                     allocator_ = VK_NULL_HANDLE;
#endif
    };

} // namespace hvk

#endif // HVK_DEVICE
