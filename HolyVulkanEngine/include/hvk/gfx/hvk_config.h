/**
 * @file hvk_config.h
 * @brief Global Vulkan configuration and extension settings
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Defines compilation-time configuration for validation layers and required
 * Vulkan device extensions used throughout the engine.
 */

#ifndef HVK_CONFIG
#define HVK_CONFIG

#include <vulkan/vulkan.h>
#include <vector>
#include <array>

namespace hvk {

    #ifdef NDEBUG
        constexpr bool enableValidationLayers = false; ///< Disable validation in release builds
    #else
        constexpr bool enableValidationLayers = true;  ///< Enable validation in debug builds
    #endif

    /// Required Vulkan validation layers (Khronos standard validation)
    inline const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    /// Required Vulkan device extensions (swapchain support)
    inline const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
}

#endif // HVK_CONFIG 
