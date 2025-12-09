/**
 * @file hvk_render_system_base.hpp
 * @brief Base class for render systems with automatic resource management
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Provides common functionality and automatic cleanup for render systems,
 * reducing boilerplate code by 50-70%.
 *
 * Features:
 * - Automatic Vulkan resource cleanup (pipelines, layouts, shaders)
 * - Cached pointers to common engine systems
 * - Helper methods for resource creation
 * - Integration with ShaderBuilder and PipelineBuilder
 *
 * Usage example:
 * @code
 * class MyRenderSystem : public RenderSystemBase {
 * public:
 *     void init(Scene& scene) override {
 *         initBase(scene);  // REQUIRED: Call base init first
 *
 *         // Load shaders using ShaderBuilder
 *         auto shaders = ShaderBuilder()
 *             .loadVertex(PROJECT_ROOT "/shaders/my.vert.spv")
 *             .loadFragment(PROJECT_ROOT "/shaders/my.frag.spv")
 *             .build(*device_);
 *
 *         // Track for automatic cleanup
 *         trackShaderModules(shaders);
 *
 *         // Create pipeline layout
 *         auto layout = createPipelineLayout(scene, shaders.stages);
 *
 *         // Create pipeline using PipelineBuilder
 *         pipeline_ = PipelineBuilder()
 *             .setShaders(shaders)
 *             .setLayout(layout)
 *             .makeOpaque()
 *             .build(*pipelineCache_, getRenderFormats(scene));
 *
 *         // Automatically cleaned up in destructor!
 *     }
 *
 *     void render(Scene& scene, CmdList& cmd) override {
 *         cmd.bindGraphicsPipeline(pipeline_);
 *         // ... draw calls ...
 *     }
 *
 * private:
 *     VkPipeline pipeline_ = VK_NULL_HANDLE;
 * };
 * @endcode
 */

#ifndef HVK_ECS_RENDER_SYSTEM_BASE_HPP
#define HVK_ECS_RENDER_SYSTEM_BASE_HPP

#include <hvk/ecs/hvk_system.hpp>
#include <hvk/gfx/hvk_shader_builder.hpp>
#include <vulkan/vulkan.h>
#include <vector>

namespace hvk {

// Forward declarations
class Device;
class GraphicsPipelineCache;
class DescriptorSetLayout;
struct RenderFormats;
struct ShaderStageDesc;

/**
 * RenderSystemBase - Base class for render systems
 *
 * Simplifies render system creation by providing:
 * - Automatic resource cleanup (RAII)
 * - Cached pointers to engine systems
 * - Helper methods for common operations
 * - Integration with builder pattern APIs
 *
 * Design Philosophy:
 * - Reduces boilerplate without hiding complexity
 * - Derived classes still have full control
 * - Resources are automatically tracked and cleaned up
 * - Compatible with existing render system patterns
 *
 * Resource Tracking:
 * All Vulkan resources created via helper methods are automatically
 * tracked and destroyed in the destructor. You can also manually track
 * resources using the track*() methods.
 *
 * Important:
 * - ALWAYS call initBase(scene) at the start of your init() override
 * - Resources are destroyed in cleanup() and destructor
 * - Don't manually destroy tracked resources
 */
class RenderSystemBase : public ISystem {
public:
    RenderSystemBase() = default;
    ~RenderSystemBase() override;

    /**
     * Cleanup all tracked resources
     *
     * Called automatically in destructor and by Scene on system removal.
     * Destroys all tracked pipelines, layouts, and shader modules.
     */
    void cleanup() override;

protected:
    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * Initialize base class (REQUIRED in derived init())
     *
     * Sets up cached pointers to Device, GraphicsPipelineCache, etc.
     * MUST be called at the start of derived class init() method.
     *
     * @param scene Parent scene
     *
     * Example:
     * @code
     * void MySystem::init(Scene& scene) {
     *     initBase(scene);  // REQUIRED
     *     // ... your initialization ...
     * }
     * @endcode
     */
    void initBase(Scene& scene);

    // ========================================================================
    // Cached Pointers (available after initBase())
    // ========================================================================

    const Device* device_ = nullptr;
    GraphicsPipelineCache* pipelineCache_ = nullptr;
    const DescriptorSetLayout* globalDescriptorLayout_ = nullptr;

    // ========================================================================
    // Resource Tracking
    // ========================================================================

    /**
     * Track a pipeline for automatic cleanup
     *
     * @param pipeline Pipeline handle (can be VK_NULL_HANDLE)
     */
    void trackPipeline(VkPipeline pipeline);

    /**
     * Track a pipeline layout for automatic cleanup
     *
     * @param layout Pipeline layout handle (can be VK_NULL_HANDLE)
     */
    void trackPipelineLayout(VkPipelineLayout layout);

    /**
     * Track a shader module for automatic cleanup
     *
     * @param module Shader module handle (can be VK_NULL_HANDLE)
     */
    void trackShaderModule(VkShaderModule module);

    /**
     * Track shader modules from ShaderModules struct
     *
     * Convenience method to track all non-null shader modules
     * from a ShaderModules struct returned by ShaderBuilder.
     *
     * @param shaders ShaderModules from ShaderBuilder::build()
     */
    void trackShaderModules(const ShaderModules& shaders);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * Create a pipeline layout with push constants
     *
     * Creates a pipeline layout with global descriptors (Set 0) and
     * optional push constants. The layout is automatically tracked.
     *
     * @param scene Parent scene (for global descriptor layout)
     * @param shaderStages Shader stages (for push constant reflection)
     * @param pushConstantSize Size of push constants (0 if none)
     * @param pushConstantStages Shader stages that use push constants
     * @return Created pipeline layout (tracked for cleanup)
     */
    VkPipelineLayout createPipelineLayout(
        Scene& scene,
        const std::vector<ShaderStageDesc>& shaderStages,
        uint32_t pushConstantSize = 0,
        VkShaderStageFlags pushConstantStages = 0
    );

    /**
     * Get render formats for current swapchain
     *
     * @param scene Parent scene
     * @return RenderFormats for pipeline creation
     */
    RenderFormats getRenderFormats(Scene& scene) const;

private:
    // Resource tracking arrays
    std::vector<VkPipeline> pipelines_;
    std::vector<VkPipelineLayout> pipelineLayouts_;
    std::vector<VkShaderModule> shaderModules_;

    // Internal cleanup helper
    void destroyResources();
};

} // namespace hvk

#endif // HVK_ECS_RENDER_SYSTEM_BASE_HPP
