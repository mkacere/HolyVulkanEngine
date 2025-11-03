#ifndef HVK_ECS_BILLBOARD_RENDER_SYSTEM_HPP
#define HVK_ECS_BILLBOARD_RENDER_SYSTEM_HPP

#include <hvk/ecs/hvk_system.hpp>
#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_gpu_resources.h>
#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <hvk/gfx/hvk_pipeline_layout_cache.h>
#include <hvk/gfx/hvk_descriptors.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>

namespace hvk {

// Forward declarations
class GlobalDescriptorLayout;

/**
 * BillboardRenderSystem - Renders billboards with automatic batching
 *
 * Features:
 * - Auto-batching based on count (<10: individual, 10-100: static instancing, >100: dynamic)
 * - All 3 billboard modes (Spherical, Cylindrical, ScreenAligned)
 * - Textured and solid color billboards
 * - Alpha blending and additive blending
 *
 * Performance:
 * - Shared vertex buffer for all billboards (single quad)
 * - Instanced rendering for 10+ billboards
 * - Minimal CPU overhead (no matrix multiplications)
 *
 * Usage:
 * - Add BillboardComponent to entities with TransformComponent
 * - System automatically renders them
 */
class BillboardRenderSystem : public ISystem {
public:
    BillboardRenderSystem() = default;
    ~BillboardRenderSystem() override = default;

    void init(Scene& scene) override;
    void render(Scene& scene, CmdList& cmd) override;
    void cleanup();

private:
    // Vertex format for billboard quad
    struct BillboardVertex {
        glm::vec2 corner;  // (-1,-1), (1,-1), (-1,1), (1,1)
    };

    // Instance data (for instanced rendering)
    struct BillboardInstance {
        glm::vec3 position;      // World position
        float _pad0;
        glm::vec4 color;         // RGBA color
        glm::vec2 size;          // Width, height
        uint32_t mode;           // Billboard orientation mode
        uint32_t _pad1;
        glm::vec4 uvRect;        // UV coordinates (atlas support)
    };

    // Push constants for individual draws
    struct BillboardPushConstants {
        glm::vec3 position;
        uint32_t mode;           // Billboard mode
        glm::vec4 color;
        glm::vec2 size;
        glm::vec2 _pad;
        glm::vec4 uvRect;
    };

    // Shared quad vertex buffer (4 vertices for all billboards)
    GpuBuffer quadVertexBuffer_;
    GpuBuffer quadIndexBuffer_;

    // Instance buffer for batched rendering
    GpuBuffer instanceBuffer_;
    std::vector<BillboardInstance> instanceData_;  // CPU-side staging

    // Pipelines
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline alphaPipeline_ = VK_NULL_HANDLE;    // Alpha blending
    VkPipeline additivePipeline_ = VK_NULL_HANDLE; // Additive blending

    // Shader modules
    VkShaderModule vertexShader_ = VK_NULL_HANDLE;
    VkShaderModule fragmentShader_ = VK_NULL_HANDLE;

    // Cached pointers
    const Device* device_ = nullptr;
    GraphicsPipelineCache* pipelineCache_ = nullptr;

    // Batching thresholds
    static constexpr uint32_t INDIVIDUAL_DRAW_THRESHOLD = 10;
    static constexpr uint32_t STATIC_INSTANCING_THRESHOLD = 100;

    // Helper: Create shared quad geometry
    void createQuadGeometry();

    // Helper: Update instance buffer with billboard data
    void updateInstanceBuffer(const std::vector<BillboardInstance>& instances);
};

} // namespace hvk

#endif // HVK_ECS_BILLBOARD_RENDER_SYSTEM_HPP
