/**
 * @file hvk_frame_context.hpp
 * @brief Per-frame rendering context
 * @author Holy Vulkan Engine
 * @date 2025
 * Lightweight frame-level data passed to render systems including timing and descriptors.
 */

#ifndef HVK_FRAME_CONTEXT_HPP
#define HVK_FRAME_CONTEXT_HPP

#include <vulkan/vulkan.h>
#include <cstdint>

namespace hvk {

/**
 * FrameContext - Lightweight rendering context (replaces old FrameInfo)
 *
 * Purpose:
 * - Pass essential frame-level information to rendering code
 * - Provide timing, viewport, and command buffer references
 * - Provide descriptor set bindings
 * - NO heavy data structures (no camera objects, no game object maps)
 *
 * Design philosophy:
 * - Render systems should query ECS directly for entity data
 * - FrameContext only provides "infrastructure" data
 * - Keeps rendering code decoupled from game logic
 *
 * Usage:
 *   FrameContext ctx;
 *   ctx.frameIndex = sync.currentFrame();
 *   ctx.cmd = sync.cmd();
 *   ctx.extent = swap.extent();
 *   // ... set other fields
 *
 *   renderSystem.render(ctx);
 */
struct FrameContext {
    // --- Frame Identification & Timing ---
    uint32_t frameIndex;        // Current frame-in-flight index (0..framesInFlight-1)
    uint64_t frameCount;        // Total frames rendered since start
    float    absoluteTime;      // Time in seconds since application start
    float    deltaTime;         // Time in seconds since last frame

    // --- Command Recording ---
    VkCommandBuffer cmd;        // Current command buffer for this frame

    // --- Viewport & Rendering Area ---
    VkExtent2D extent;          // Current render target extent (usually swapchain extent)
    VkViewport viewport;        // Current viewport (set via setViewportScissor)
    VkRect2D   scissor;         // Current scissor rect

    // --- Global Descriptor Sets (Set 0) ---
    VkDescriptorSet globalDescriptorSet;  // Set 0: SceneData + CameraData + LightBuffer

    // --- Optional: Material/Texture Descriptor Sets (Set 1+) ---
    // Note: For bindless rendering, materials are accessed via indices
    // For traditional binding, you might store a pointer to an array here
    VkDescriptorSet* materialDescriptorSets;  // Optional: array of per-material descriptor sets
    uint32_t         materialDescriptorCount; // Count of material descriptor sets

    // --- Constructor ---
    FrameContext()
        : frameIndex(0)
        , frameCount(0)
        , absoluteTime(0.0f)
        , deltaTime(0.0f)
        , cmd(VK_NULL_HANDLE)
        , extent{0, 0}
        , viewport{}
        , scissor{}
        , globalDescriptorSet(VK_NULL_HANDLE)
        , materialDescriptorSets(nullptr)
        , materialDescriptorCount(0)
    {}

    // --- Convenience Methods ---

    /**
     * Set viewport and scissor to cover the entire extent
     */
    void setFullViewport() {
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        scissor.offset = {0, 0};
        scissor.extent = extent;
    }

    /**
     * Get aspect ratio from extent
     */
    float getAspectRatio() const {
        return extent.height > 0 ? static_cast<float>(extent.width) / static_cast<float>(extent.height) : 1.0f;
    }

    /**
     * Check if command buffer is valid
     */
    bool isValid() const {
        return cmd != VK_NULL_HANDLE && extent.width > 0 && extent.height > 0;
    }
};

/**
 * FrameContextBuilder - Helper for constructing FrameContext
 *
 * Usage:
 *   auto ctx = FrameContextBuilder()
 *       .withFrameIndex(frameIdx)
 *       .withTiming(time, dt)
 *       .withCommandBuffer(cmd)
 *       .withExtent(extent)
 *       .withGlobalDescriptorSet(descSet)
 *       .build();
 */
class FrameContextBuilder {
public:
    FrameContextBuilder() = default;

    FrameContextBuilder& withFrameIndex(uint32_t index) {
        ctx_.frameIndex = index;
        return *this;
    }

    FrameContextBuilder& withFrameCount(uint64_t count) {
        ctx_.frameCount = count;
        return *this;
    }

    FrameContextBuilder& withTiming(float absTime, float dt) {
        ctx_.absoluteTime = absTime;
        ctx_.deltaTime = dt;
        return *this;
    }

    FrameContextBuilder& withCommandBuffer(VkCommandBuffer cmd) {
        ctx_.cmd = cmd;
        return *this;
    }

    FrameContextBuilder& withExtent(VkExtent2D extent) {
        ctx_.extent = extent;
        return *this;
    }

    FrameContextBuilder& withViewport(const VkViewport& vp) {
        ctx_.viewport = vp;
        return *this;
    }

    FrameContextBuilder& withScissor(const VkRect2D& sc) {
        ctx_.scissor = sc;
        return *this;
    }

    FrameContextBuilder& withGlobalDescriptorSet(VkDescriptorSet descSet) {
        ctx_.globalDescriptorSet = descSet;
        return *this;
    }

    FrameContextBuilder& withMaterialDescriptors(VkDescriptorSet* sets, uint32_t count) {
        ctx_.materialDescriptorSets = sets;
        ctx_.materialDescriptorCount = count;
        return *this;
    }

    // Set full viewport automatically from extent
    FrameContextBuilder& withFullViewport() {
        ctx_.setFullViewport();
        return *this;
    }

    FrameContext build() const {
        return ctx_;
    }

private:
    FrameContext ctx_;
};

} // namespace hvk

#endif // HVK_FRAME_CONTEXT_HPP
