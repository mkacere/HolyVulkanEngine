#include <hvk/ecs/systems/hvk_fractal_render_system.hpp>
#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/ecs/hvk_components.hpp>
#include <hvk/gfx/hvk_global_descriptors.hpp>
#include <hvk/gfx/hvk_cmd_list.hpp>
#include <hvk/gfx/hvk_shader_builder.hpp>
#include <hvk/gfx/hvk_pipeline_builder.hpp>
#include <hvk/core/hvk_time.hpp>

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
#endif

namespace hvk {

void FractalRenderSystem::init(Scene& scene) {
    // Initialize base class (REQUIRED)
    initBase(scene);

    // Initialize default parameters
    initDefaultParams();

    // ========================================================================
    // Load Shaders (using ShaderBuilder - reduced from ~30 lines to 3!)
    // ========================================================================

    auto shaders = ShaderBuilder()
        .loadVertex(PROJECT_ROOT "/shaders/fractal.vert.spv")
        .loadFragment(PROJECT_ROOT "/shaders/fractal.frag.spv")
        .build(*device_);

    // Track shaders for automatic cleanup
    trackShaderModules(shaders);

    // ========================================================================
    // Create Pipeline Layout (using helper - reduced from ~15 lines to 1!)
    // ========================================================================

    pipelineLayout_ = createPipelineLayout(
        scene,
        shaders.stages,
        sizeof(FractalParams),
        VK_SHADER_STAGE_FRAGMENT_BIT
    );

    // ========================================================================
    // Create Pipeline (using PipelineBuilder - reduced from ~40 lines to 6!)
    // ========================================================================

    pipeline_ = PipelineBuilder()
        .setShaders(shaders)
        .setLayout(pipelineLayout_)
        .noVertexInput()               // Fullscreen triangle is procedural
        .makeFullscreen()              // Preset: no depth, no vertex input
        .build(*pipelineCache_, getRenderFormats(scene));

    // Pipeline is automatically cached, no manual cleanup needed
    trackPipeline(pipeline_);
}

void FractalRenderSystem::render(Scene& scene, CmdList& cmd) {
    // Bind pipeline
    cmd.bindGraphicsPipeline(pipeline_);

    // Get global descriptor set (Set 0: Camera + Scene + Lights)
    GlobalDescriptorSet* globalDescSet = scene.globalDescriptorSet();
    if (!globalDescSet) {
        return; // No global descriptors available
    }

    VkDescriptorSet descSet = globalDescSet->get(scene.frameIndex());
    cmd.bindGraphicsDescriptorSets(pipelineLayout_, 0, 1, &descSet);

    // Update time for animation
    if (params_.enableAnimation) {
        params_.time = Time::totalTime();
    }

    // Push constants (fractal parameters)
    cmd.pushConstants(pipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, params_);

    // Draw fullscreen triangle (3 vertices, no vertex buffer)
    cmd.draw(3);
}

// Cleanup is now automatic via RenderSystemBase destructor!

void FractalRenderSystem::initDefaultParams() {
    params_ = {};

    // Fractal type
    params_.fractalType = static_cast<uint32_t>(FractalType::Mandelbulb);
    params_.power = 8.0f;              // Classic Mandelbulb power
    params_.maxIterations = 20;        // Escape iterations
    params_.bailout = 2.0f;            // Escape radius

    // Julia set parameters (default)
    params_.juliaC = glm::vec4(0.18f, 0.88f, 0.24f, 0.16f);

    // Raymarching quality
    params_.maxSteps = 128;            // Good quality/performance balance
    params_.epsilon = 0.001f;          // Surface precision
    params_.maxDistance = 20.0f;       // Max render distance

    // Lighting
    params_.lightDir = glm::normalize(glm::vec3(-0.5f, 0.7f, -0.3f));
    params_.ambientStrength = 0.3f;
    params_.lightColor = glm::vec3(1.0f, 0.95f, 0.9f); // Warm white
    params_.aoStrength = 0.5f;

    // Coloring
    params_.color1 = glm::vec3(0.9f, 0.6f, 0.3f);  // Orange
    params_.color2 = glm::vec3(0.2f, 0.4f, 0.8f);  // Blue
    params_.colorMix = 0.5f;

    // Animation
    params_.time = 0.0f;
    params_.enableAnimation = 0;

    // Psychedelic effects
    params_.glowIntensity = 0.3f;
    params_.colorCycleSpeed = 0.5f;
    params_.depthColorShift = 2.0f;
    params_.iterationColorMix = 0.7f;
}

void FractalRenderSystem::resetParams() {
    initDefaultParams();
}

} // namespace hvk
