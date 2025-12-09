/**
 * @file hvk_debug_line_render_system.hpp
 * @brief Debug line rendering system for visualization
 * @author Holy Vulkan Engine
 * @date 2025
 * Renders colored lines for debugging purposes (collision shapes, gizmos, normals, etc.)
 */

#ifndef HVK_ECS_DEBUG_LINE_RENDER_SYSTEM_HPP
#define HVK_ECS_DEBUG_LINE_RENDER_SYSTEM_HPP

#include <hvk/ecs/hvk_system.hpp>
#include <hvk/gfx/hvk_gpu_resources.h>
#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>

namespace hvk {

// Forward declarations
class Device;
class Scene;
class CmdList;

/**
 * DebugLineRenderSystem - Renders debug visualization lines
 *
 * Features:
 * - Dynamic line addition/removal per frame (immediate mode)
 * - Per-line color support
 * - Depth testing with no depth writes (overlay rendering)
 * - Automatic vertex buffer management
 * - No culling (visible from both sides)
 *
 * Usage:
 * - Add to scene: scene.addSystem(std::make_unique<DebugLineRenderSystem>())
 * - Draw lines: Get system reference and call addLine(start, end, color)
 * - Lines are automatically cleared each frame
 *
 * Typical use cases:
 * - Physics collision shape visualization
 * - Transform gizmos and axes
 * - Bounding box visualization
 * - Normal/tangent visualization
 * - Path and trajectory visualization
 */
class DebugLineRenderSystem : public ISystem {
public:
    /**
     * Line vertex format (position + color)
     */
    struct LineVertex {
        glm::vec3 position;
        glm::vec3 color;

        static VkVertexInputBindingDescription getBindingDescription();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };

    DebugLineRenderSystem() = default;
    ~DebugLineRenderSystem() override = default;

    // Prevent copying
    DebugLineRenderSystem(const DebugLineRenderSystem&) = delete;
    DebugLineRenderSystem& operator=(const DebugLineRenderSystem&) = delete;

    void init(Scene& scene) override;
    void render(Scene& scene, CmdList& cmd) override;
    void cleanup();

    /**
     * Add a line to be rendered this frame
     *
     * @param start Start position (world space)
     * @param end End position (world space)
     * @param color RGB color (0-1 range)
     */
    void addLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

    /**
     * Add multiple lines efficiently
     *
     * @param lines Vector of line vertices (pairs of start/end with same color)
     */
    void addLines(const std::vector<LineVertex>& lines);

    /**
     * Clear all pending lines (called automatically at end of render())
     */
    void clearLines();

    /**
     * Get current line count
     */
    [[nodiscard]] size_t lineCount() const noexcept { return vertices_.size() / 2; }

private:
    // Vulkan resources
    const Device* device_ = nullptr;
    GraphicsPipelineCache* pipelineCache_ = nullptr;

    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkShaderModule vertexShader_ = VK_NULL_HANDLE;
    VkShaderModule fragmentShader_ = VK_NULL_HANDLE;

    // Vertex buffer (dynamic, host-visible)
    GpuBuffer vertexBuffer_;
    std::vector<LineVertex> vertices_;

    // Configuration
    static constexpr uint32_t MAX_LINES = 10000;
    static constexpr uint32_t MAX_VERTICES = MAX_LINES * 2;

    // Helper methods
    void uploadVertices();
};

} // namespace hvk

#endif // HVK_ECS_DEBUG_LINE_RENDER_SYSTEM_HPP
