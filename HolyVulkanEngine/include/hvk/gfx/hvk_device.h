/**
 * @file hvk_device.h
 * @brief Vulkan device abstraction with automatic feature enablement
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Provides RAII wrapper for Vulkan 1.4 device initialization including:
 * - Instance creation with validation layers
 * - Physical device selection with scoring
 * - Logical device with queue family management
 * - Vulkan Memory Allocator (VMA) integration
 * - Debug messenger with configurable verbosity
 */

#ifndef HVK_DEVICE
#define HVK_DEVICE

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

    /**
     * @enum DebugVerbosity
     * @brief Debug message verbosity levels for VK_EXT_debug_utils
     */
    enum class DebugVerbosity {
        None,       ///< No debug messages
        Error,      ///< Errors only
        Warn,       ///< Warnings and errors
        Info,       ///< Info, warnings, and errors
        Verbose     ///< All messages including verbose diagnostics
    };

    /**
     * @struct DeviceCreateInfo
     * @brief Configuration structure for Device initialization
     */
    struct DeviceCreateInfo {
        const char* appName = "Engine";            ///< Application name for Vulkan
        uint32_t    appVersion = VK_MAKE_VERSION(0, 1, 0); ///< Application version
        bool        enableValidation =
#ifdef NDEBUG
            false
#else
            true
#endif
            ; ///< Enable validation layers (default: true in Debug, false in Release)

        // Debug configuration
        DebugVerbosity debugVerbosity = DebugVerbosity::Info; ///< Debug message verbosity level
        VkDebugUtilsMessageTypeFlagsEXT debugMessageTypes =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; ///< Debug message type filter

        std::vector<const char*> extraInstanceLayers{};      ///< Additional instance layers
        std::vector<const char*> extraInstanceExtensions{};  ///< Additional instance extensions
        std::vector<const char*> extraDeviceExtensions{};    ///< Additional device extensions (VK_KHR_swapchain is auto-added)

        // Queue preferences
        bool requestSeparateTransferQueue = true;  ///< Request dedicated transfer queue if available
        bool requestSeparateComputeQueue = true;   ///< Request dedicated compute queue if available

        // Vulkan 1.2 / 1.3 features
        bool enableAnisotropy = true;              ///< Enable anisotropic filtering
        bool enableBufferDeviceAddress = true;     ///< Enable buffer device address (for GPU-driven rendering)
        bool enableDescriptorIndexing = true;      ///< Enable descriptor indexing (bindless)
        bool enableTimelineSemaphore = true;       ///< Enable timeline semaphores
        bool enableDynamicRendering = true;        ///< Enable dynamic rendering (1.3 core)
        bool enableSynchronization2 = true;        ///< Enable synchronization2 (1.3 core)

        // Vulkan 1.4 features
        bool enableIndexTypeUint8 = true;                  ///< Enable 8-bit index buffers
        bool enableDynamicRenderingLocalRead = true;       ///< Enable local reads in dynamic rendering
        bool enableMaintenance5 = true;                    ///< Enable maintenance5 features
        bool enableMaintenance6 = true;                    ///< Enable maintenance6 features
        bool enableHostImageCopy = true;                   ///< Enable host-side image copy
        bool enablePushDescriptor = false;                 ///< Enable push descriptors (opt-in)
        bool enablePipelineRobustnessFlag = true;          ///< Enable per-pipeline robustness flag
        bool enablePipelineProtectedAccess = false;        ///< Enable protected content paths

        // Shader features
        bool enableShaderSubgroupRotate = false;           ///< Enable subgroup rotate operations
        bool enableShaderSubgroupRotateClustered = false;  ///< Enable clustered subgroup rotate
        bool enableShaderFloatControls2 = true;            ///< Enable enhanced float controls
        bool enableShaderExpectAssume = true;              ///< Enable shader expect/assume

        // Line rasterization (opt-in)
        bool enableRectangularLines = false;               ///< Enable rectangular line rasterization
        bool enableBresenhamLines = false;                 ///< Enable Bresenham line rasterization
        bool enableSmoothLines = false;                    ///< Enable smooth line rasterization
        bool enableStippledRectangularLines = false;       ///< Enable stippled rectangular lines
        bool enableStippledBresenhamLines = false;         ///< Enable stippled Bresenham lines
        bool enableStippledSmoothLines = false;            ///< Enable stippled smooth lines

        // Vertex features
        bool enableVertexAttribDivisor = true;             ///< Enable vertex attribute divisor
        bool enableVertexAttribZeroDivisor = true;         ///< Enable zero divisor for vertex attributes
    };

    /**
     * @struct Queue
     * @brief Represents a Vulkan queue with its family index
     */
    struct Queue {
        VkQueue   handle = VK_NULL_HANDLE;  ///< Vulkan queue handle
        uint32_t  family = ~0u;             ///< Queue family index
    };

    /**
     * @class Device
     * @brief RAII wrapper for Vulkan device with automatic feature management
     *
     * Creates and manages a Vulkan 1.4 device with:
     * - Automatic physical device selection based on capabilities
     * - Feature enablement with fallback for unsupported features
     * - Queue family allocation (graphics, present, transfer, compute)
     * - VMA allocator for efficient memory management
     * - Debug utilities with runtime configuration
     *
     * @note This class is move-only (non-copyable)
     */
    class Device {
    public:
        /**
         * @brief Construct Device with window and configuration
         * @param window Window for surface creation
         * @param ci Device creation configuration
         * @throws std::runtime_error if device creation fails
         */
        explicit Device(const Window& window, const DeviceCreateInfo& ci = {});

        /**
         * @brief Destructor - cleans up all Vulkan resources
         */
        ~Device();

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&)                 noexcept;
        Device& operator=(Device&&)      noexcept;

        // Accessors

        /**
         * @brief Get Vulkan instance handle
         * @return VkInstance handle
         */
        VkInstance       instance()   const { return instance_; }

        /**
         * @brief Get physical device handle
         * @return VkPhysicalDevice handle
         */
        VkPhysicalDevice physical()   const { return physical_; }

        /**
         * @brief Get logical device handle
         * @return VkDevice handle
         */
        VkDevice         device()     const { return device_; }

        /**
         * @brief Get surface handle
         * @return VkSurfaceKHR handle
         */
        VkSurfaceKHR     surface()    const { return surface_; }

        /**
         * @brief Get graphics queue
         * @return Graphics queue handle and family
         */
        const Queue& graphics() const { return graphics_; }

        /**
         * @brief Get present queue
         * @return Present queue handle and family
         */
        const Queue& present()  const { return present_; }

        /**
         * @brief Get transfer queue
         * @return Transfer queue handle and family (may be same as graphics)
         */
        const Queue& transfer() const { return transfer_; }

        /**
         * @brief Get compute queue
         * @return Compute queue handle and family (may be same as graphics)
         */
        const Queue& compute()  const { return compute_; }

        /**
         * @brief Get physical device properties
         * @return Vulkan 1.0 properties
         */
        const VkPhysicalDeviceProperties& properties() const { return props10_; }

        /**
         * @brief Get memory properties
         * @return Physical device memory properties
         */
        const VkPhysicalDeviceMemoryProperties& memoryProperties() const { return memProps_; }

        /**
         * @brief Get device limits
         * @return Physical device limits
         */
        const VkPhysicalDeviceLimits& limits() const { return props10_.limits; }

        /**
         * @brief Get enabled Vulkan 1.0 features
         * @return Enabled features struct
         */
        const VkPhysicalDeviceFeatures& features10() const { return enabled10_; }

        /**
         * @brief Get enabled Vulkan 1.2 features
         * @return Enabled features struct
         */
        const VkPhysicalDeviceVulkan12Features& features12() const { return enabled12_; }

        /**
         * @brief Get enabled Vulkan 1.3 features
         * @return Enabled features struct
         */
        const VkPhysicalDeviceVulkan13Features& features13() const { return enabled13_; }

        /**
         * @brief Get enabled Vulkan 1.4 features
         * @return Enabled features struct
         */
        const VkPhysicalDeviceVulkan14Features& features14() const { return enabled14_; }

        /**
         * @brief Get Vulkan 1.4 properties
         * @return Physical device 1.4 properties
         */
        const VkPhysicalDeviceVulkan14Properties& properties14() const { return props14_; }

#if ENGINE_USE_VMA
        /**
         * @brief Get VMA allocator handle
         * @return VmaAllocator handle
         */
        VmaAllocator allocator() const { return allocator_; }
#endif

        /**
         * @brief Wait for all device operations to complete
         */
        void waitIdle() const { vkDeviceWaitIdle(device_); }

        /**
         * @brief Set debug name for Vulkan object
         * @param type Object type
         * @param handle Object handle (cast to uint64_t)
         * @param name Debug name string
         */
        void setObjectName(VkObjectType type, uint64_t handle, std::string_view name) const;

        /**
         * @brief Set debug message verbosity at runtime
         * @param v New verbosity level
         * @note Only effective if validation is enabled
         */
        void setDebugVerbosity(DebugVerbosity v);

        /**
         * @brief Get current debug verbosity
         * @return Current verbosity level
         */
        DebugVerbosity debugVerbosity() const { return dbgVerbosity_; }

        /**
         * @brief Set debug message type filter at runtime
         * @param types Message type flags
         * @note Only effective if validation is enabled
         */
        void setDebugMessageTypes(VkDebugUtilsMessageTypeFlagsEXT types);

        /**
         * @brief Get current debug message types
         * @return Current message type flags
         */
        VkDebugUtilsMessageTypeFlagsEXT debugMessageTypes() const { return dbgTypes_; }

    private:
        // Initialization steps
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

        // Helper functions
        static bool supportsPresentation(VkPhysicalDevice pd, uint32_t family, VkSurfaceKHR surface);
        static bool deviceMeetsBasics(VkPhysicalDevice pd, VkSurfaceKHR surface, std::unordered_set<std::string>& missingExt);
        static uint32_t scorePhysicalDevice(VkPhysicalDevice pd, VkSurfaceKHR surface);
        static VkDebugUtilsMessageSeverityFlagsEXT severityMaskFor(DebugVerbosity v);

    private:
        // Instance & debug
        VkInstance                       instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT         debug_ = VK_NULL_HANDLE;
        PFN_vkSetDebugUtilsObjectNameEXT fnSetName_ = nullptr;

        DebugVerbosity                   dbgVerbosity_ = DebugVerbosity::Info;
        VkDebugUtilsMessageTypeFlagsEXT  dbgTypes_ =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        // Surface
        VkSurfaceKHR                     surface_ = VK_NULL_HANDLE;

        // Physical device
        VkPhysicalDevice                 physical_ = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties       props10_{};
        VkPhysicalDeviceMemoryProperties memProps_{};
        VkPhysicalDeviceVulkan14Properties props14_{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES };

        // Enabled features
        VkPhysicalDeviceFeatures         enabled10_{};
        VkPhysicalDeviceVulkan12Features enabled12_{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        VkPhysicalDeviceVulkan13Features enabled13_{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        VkPhysicalDeviceVulkan14Features enabled14_{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };

        // Logical device & queues
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
