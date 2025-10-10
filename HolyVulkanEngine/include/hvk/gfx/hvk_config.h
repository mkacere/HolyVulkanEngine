#ifndef HVK_CONFIG
#define HVK_CONFIG

#include <vulkan/vulkan.h>
#include <vector>
#include <array>

namespace hvk {

    #ifdef NDEBUG
        constexpr bool enableValidationLayers = false;
    #else
        constexpr bool enableValidationLayers = true;
    #endif

    inline const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    inline const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
}

#endif // HVK_CONFIG 
