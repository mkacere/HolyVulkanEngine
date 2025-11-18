/**
 * @file hvk_fractal_render_system.hpp
 * @brief 3D Raymarched Fractal Rendering System
 * @author Holy Vulkan Engine
 * @date 2025
 * Renders 3D fractals using raymarching in a fullscreen fragment shader.
 */

#ifndef HVK_ECS_FRACTAL_RENDER_SYSTEM_HPP
#define HVK_ECS_FRACTAL_RENDER_SYSTEM_HPP

#include <hvk/ecs/hvk_system.hpp>
#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <hvk/gfx/hvk_pipeline_layout_cache.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace hvk {

// Forward declarations
class GlobalDescriptorLayout;

/**
 * FractalRenderSystem - Renders 3D raymarched fractals
 *
 * Features:
 * - Multiple fractal types (Mandelbulb, Quaternion Julia, Menger Sponge)
 * - Real-time parameter adjustment via ImGui
 * - Phong lighting with ambient occlusion
 * - Distance-based fog and coloring
 * - No vertex buffers (procedural fullscreen triangle)
 *
 * Rendering:
 * - Fullscreen triangle with raymarching fragment shader
 * - Uses global descriptors (Set 0) for camera matrices
 * - Push constants for real-time fractal parameters
 *
 * Performance:
 * - Adjustable quality (max steps, epsilon)
 * - GPU-only rendering (all computation in fragment shader)
 *
 * Usage:
 * - Add to scene: scene.addSystem<FractalRenderSystem>()
 * - Control via ImGui in application's onImGui callback
 * - Navigate with camera controller (WASD + mouse)
 */
class FractalRenderSystem : public ISystem {
public:
    /**
     * Fractal type enumeration
     */
    enum class FractalType : uint32_t {
        Mandelbulb = 0,      // Classic 3D Mandelbrot
        QuaternionJulia = 1, // Quaternion Julia set
        MengerSponge = 2,    // Menger sponge
        Count
    };

    /**
     * Push constants for fractal parameters
     * Sent to GPU every frame for real-time control
     */
    struct FractalParams {
        // Fractal parameters
        uint32_t fractalType;     // FractalType enum value
        float power;              // Fractal power (e.g., 8.0 for Mandelbulb)
        uint32_t maxIterations;   // Max raymarching iterations
        float bailout;            // Escape radius for iterative fractals

        // Julia set parameters (used when fractalType == QuaternionJulia)
        glm::vec4 juliaC;         // Quaternion constant for Julia set

        // Raymarching quality
        uint32_t maxSteps;        // Max raymarch steps
        float epsilon;            // Surface hit threshold
        float maxDistance;        // Max ray distance
        float _pad0;

        // Lighting
        glm::vec3 lightDir;       // Directional light direction
        float ambientStrength;    // Ambient light strength
        glm::vec3 lightColor;     // Light color
        float aoStrength;         // Ambient occlusion strength

        // Coloring
        glm::vec3 color1;         // Primary color
        float colorMix;           // Color mixing factor
        glm::vec3 color2;         // Secondary color
        float _pad1;

        // Animation
        float time;               // Animated time parameter
        uint32_t enableAnimation; // 0 = off, 1 = on
        glm::vec2 _pad2;

        // Psychedelic effects
        float glowIntensity;      // Edge glow strength
        float colorCycleSpeed;    // Speed of color cycling
        float depthColorShift;    // Depth-based hue shift amount
        float iterationColorMix;  // How much iteration count affects color (0-1)
    };

    FractalRenderSystem() = default;
    ~FractalRenderSystem() override = default;

    void init(Scene& scene) override;
    void render(Scene& scene, CmdList& cmd) override;
    void cleanup();

    /**
     * Get/set fractal parameters for ImGui control
     */
    FractalParams& getParams() { return params_; }
    const FractalParams& getParams() const { return params_; }

    /**
     * Reset parameters to defaults
     */
    void resetParams();

private:
    // Pipeline resources
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;

    // Shader modules
    VkShaderModule vertexShader_ = VK_NULL_HANDLE;
    VkShaderModule fragmentShader_ = VK_NULL_HANDLE;

    // Cached pointers
    const Device* device_ = nullptr;
    GraphicsPipelineCache* pipelineCache_ = nullptr;

    // Fractal parameters (CPU-side)
    FractalParams params_;

    // Helper: Initialize default parameters
    void initDefaultParams();

    // Helper: Create pipeline
    void createPipeline(Scene& scene);
};

} // namespace hvk

#endif // HVK_ECS_FRACTAL_RENDER_SYSTEM_HPP
