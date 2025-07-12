#ifndef HVK_GLOBAL_UBO_HPP
#define HVK_GLOBAL_UBO_HPP

// Ensure GLM is using Vulkan-style depth and radians
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace hvk {

    // Must match MAX_LIGHTS in your shaders
    constexpr int MAX_LIGHTS = 100;

    struct PointLight {
        glm::vec4 position;  // xyz = position, w unused
        glm::vec4 color;     // rgb = color, w = intensity
    };

    // Mirrors your GLSL std140 uniform block
    struct GlobalUbo {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 inverseView;
        glm::vec4 ambientLightColor;   // w = ambient intensity
        PointLight pointLights[MAX_LIGHTS];
        int        numLights;
        // pad to multiple of vec4 for std140 (if needed):
        // int padding[3];
    };

} // namespace hvk

#endif // HVK_GLOBAL_UBO_HPP
