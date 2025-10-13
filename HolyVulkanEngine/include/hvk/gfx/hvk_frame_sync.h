#ifndef HVK_FRAME_SYNC_H
#define HVK_FRAME_SYNC_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <cassert>

namespace hvk {

    struct FrameSyncCreateInfo {
        VkDevice        device = VK_NULL_HANDLE;
        uint32_t        graphicsQueueFamilyIndex = 0;
        VkQueue         graphicsQueue = VK_NULL_HANDLE;
        VkQueue         presentQueue = VK_NULL_HANDLE; // can be same as graphics
        uint32_t        framesInFlight = 2;             // 2 or 3 recommended
        bool            preferTimelineSemaphore = true; // try timeline for CPU sync
        VkCommandPoolCreateFlags cmdPoolFlags =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;        // good defaults
        const VkAllocationCallbacks* alloc = nullptr;    // optional
    };

    class FrameSync {
    public:
        FrameSync() = default;
        explicit FrameSync(const FrameSyncCreateInfo& ci);
        ~FrameSync();

        FrameSync(const FrameSync&) = delete;
        FrameSync& operator=(const FrameSync&) = delete;

        FrameSync(FrameSync&& other) noexcept { move_from(std::move(other)); }
        FrameSync& operator=(FrameSync&& other) noexcept {
            if (this != &other) { destroy(); move_from(std::move(other)); }
            return *this;
        }

        // Per-frame lifecycle
        // 1) beginFrame() waits for this frame's GPU completion, resets pool, and begins the cmd buffer.
        // 2) acquireNextImage(...) gets a swapchain image and signals imageAvailable for this frame.
        // 3) record using cmd()
        // 4) submitAndPresent(...) ends, submits, and presents, signaling renderFinished.
        // 5) endFrame() advances the ring index.
        VkResult beginFrame(); // starts recording (ONE_TIME_SUBMIT), returns VK_SUCCESS or error
        VkResult acquireNextImage(VkSwapchainKHR swapchain,
            uint32_t& imageIndex,
            uint64_t timeout = UINT64_MAX);
        VkResult submitAndPresent(VkSwapchainKHR swapchain,
            uint32_t imageIndex,
            VkPipelineStageFlags waitDstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        void     endFrame();

        // Utilities
        void     waitIdle(); // waits for all in-flight work to finish
        uint32_t currentFrameIndex() const { return current_; }
        uint32_t frameCount() const { return static_cast<uint32_t>(frames_.size()); }
        bool     usingTimeline() const { return hasTimeline_; }

        // Accessors for the current frame
        VkCommandBuffer cmd() const { return frames_[current_].cmd; }
        VkCommandPool   cmdPool() const { return frames_[current_].cmdPool; }
        VkSemaphore     imageAvailable() const { return frames_[current_].imageAvailable; }
        VkSemaphore     renderFinished() const { return frames_[current_].renderFinished; }

        // For debug tools (optional)
        VkSemaphore     timelineSemaphore() const { return timeline_; }
        uint64_t        timelineValue() const { return timelineCounter_; }

        VkResult endRecordingIfOpen();
    private:
        struct PerFrame {
            VkSemaphore     imageAvailable = VK_NULL_HANDLE; // binary
            VkSemaphore     renderFinished = VK_NULL_HANDLE; // binary
            VkFence         fence = VK_NULL_HANDLE; // only if !timeline
            VkCommandPool   cmdPool = VK_NULL_HANDLE;
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            uint64_t        lastSignaledTimelineValue = 0;   // only if timeline
        };

        void destroy();
        void move_from(FrameSync&&);

        // creation helpers
        bool create_timeline_semaphore(); // returns true if created
        void create_per_frame_objects();
        void destroy_per_frame_objects();

    private:
        // immutable after construction
        VkDevice        device_ = VK_NULL_HANDLE;
        VkQueue         qGfx_ = VK_NULL_HANDLE;
        VkQueue         qPresent_ = VK_NULL_HANDLE;
        uint32_t        qGfxFamily_ = 0;
        uint32_t        frameCount_ = 0;
        bool            wantTimeline_ = true;

        // per-frame ring
        std::vector<PerFrame> frames_;
        uint32_t        current_ = 0;

        // timeline (optional)
        bool            hasTimeline_ = false;
        VkSemaphore     timeline_ = VK_NULL_HANDLE; // timeline semaphore if available
        uint64_t        timelineCounter_ = 0;       // monotonically increasing

        // config
        VkCommandPoolCreateFlags cmdPoolFlags_ = 0;
        const VkAllocationCallbacks* alloc_ = nullptr;
    };

} // namespace hvk

#endif // HVK_FRAME_SYNC_H
