#ifndef HVK_ECS_MESH_RENDER_SYSTEM_HPP
#define HVK_ECS_MESH_RENDER_SYSTEM_HPP

#include <hvk/ecs/hvk_system.hpp>
#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <hvk/gfx/hvk_pipeline_layout_cache.h>
#include <hvk/gfx/hvk_descriptors.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace hvk {

// Forward declarations
class GlobalDescriptorLayout;

/**
 * MeshRenderSystem - Renders all entities with MeshComponent
 *
 * Responsibilities:
 * - Render all visible meshes using Model::draw()
 * - Apply transform from TransformComponent
 *
 * Component requirements:
 * - MeshComponent (required)
 * - TransformComponent (required for world transform)
 *
 * Design notes:
 * - Uses existing Model::draw() infrastructure (pipelines, descriptors, etc.)
 * - Global descriptor set is passed via setGlobalDescriptorSet()
 */
class MeshRenderSystem : public ISystem {
public:
    MeshRenderSystem() = default;
    ~MeshRenderSystem() override = default;

    void init(Scene& scene) override;
    void render(Scene& scene, CmdList& cmd) override;
    void cleanup();

private:
    // Pipeline layout for model rendering
    // Layout: Set 0 (global) + Set 1 (material) + Push constants (model matrix + material params)
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    // Three pipelines for transparency rendering
    // 1. Opaque: depth write ON, blending OFF
    // 2. Masked: depth write ON, blending OFF, alpha-to-coverage ON (for hair/foliage)
    // 3. Blended: depth write OFF, blending ON (for true transparency)
    VkPipeline opaquePipeline_ = VK_NULL_HANDLE;
    VkPipeline maskedPipeline_ = VK_NULL_HANDLE;
    VkPipeline blendedPipeline_ = VK_NULL_HANDLE;

    // Shader modules (owned by this system)
    VkShaderModule vertexShader_ = VK_NULL_HANDLE;
    VkShaderModule fragmentShader_ = VK_NULL_HANDLE;

    // Cached pointers for initialization and cleanup
    const Device* device_ = nullptr;
    GraphicsPipelineCache* pipelineCache_ = nullptr;
};

} // namespace hvk

#endif // HVK_ECS_MESH_RENDER_SYSTEM_HPP
