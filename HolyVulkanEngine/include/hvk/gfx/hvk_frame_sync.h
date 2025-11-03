/**
 * @file hvk_frame_sync.h
 * @brief Frame synchronization primitives for Vulkan rendering
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Provides automated frame-in-flight synchronization using semaphores and fences
 * (or optional timeline semaphores). Manages per-frame command buffers, pools, and
 * synchronization objects for double/triple-buffered rendering.
 */

#ifndef HVK_FRAME_SYNC_H
#define HVK_FRAME_SYNC_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <cassert>

namespace hvk {

    /**
     * @struct FrameSyncCreateInfo
     * @brief Configuration for creating a FrameSync instance
     */
    struct FrameSyncCreateInfo {
        VkDevice        device = VK_NULL_HANDLE;              ///< Logical device
        uint32_t        graphicsQueueFamilyIndex = 0;         ///< Queue family index for graphics
        VkQueue         graphicsQueue = VK_NULL_HANDLE;       ///< Graphics queue handle
        VkQueue         presentQueue = VK_NULL_HANDLE;        ///< Present queue (can be same as graphics)
        uint32_t        framesInFlight = 2;                   ///< Number of frames in flight (2-3 recommended)
        bool            preferTimelineSemaphore = true;       ///< Use timeline semaphore if available
        VkCommandPoolCreateFlags cmdPoolFlags =               ///< Command pool creation flags
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        const VkAllocationCallbacks* alloc = nullptr;         ///< Optional allocation callbacks
    };

    /**
     * @class FrameSync
     * @brief Manages frame-in-flight synchronization for Vulkan rendering
     *
     * FrameSync provides a complete solution for multi-buffered rendering, handling:
     * - Per-frame command buffers and pools
     * - Binary semaphores for image acquisition and presentation
     * - Fences or timeline semaphores for CPU-GPU synchronization
     * - Automatic frame index management
     *
     * Typical usage per frame:
     * 1. beginFrame() - Wait for GPU, reset command pool, begin recording
     * 2. acquireNextImage() - Get swapchain image
     * 3. Record commands via cmd()
     * 4. submitAndPresent() - Submit work and present
     * 5. endFrame() - Advance to next frame
     *
     * @note Supports both traditional fence-based and modern timeline semaphore synchronization
     */
    class FrameSync {
    public:
        FrameSync() = default;

        /**
         * @brief Constructs a FrameSync with the given configuration
         * @param ci Creation parameters including device, queues, and frame count
         */
        explicit FrameSync(const FrameSyncCreateInfo& ci);
        ~FrameSync();

        FrameSync(const FrameSync&) = delete;
        FrameSync& operator=(const FrameSync&) = delete;

        FrameSync(FrameSync&& other) noexcept { move_from(std::move(other)); }
        FrameSync& operator=(FrameSync&& other) noexcept {
            if (this != &other) { destroy(); move_from(std::move(other)); }
            return *this;
        }

        /**
         * @brief Begins a new frame by waiting for GPU completion and starting command buffer recording
         * @return VK_SUCCESS on success, or a Vulkan error code
         * @note Automatically waits for this frame's previous submission to complete
         */
        VkResult beginFrame();

        /**
         * @brief Acquires the next swapchain image for rendering
         * @param swapchain The swapchain to acquire from
         * @param imageIndex Output parameter receiving the acquired image index
         * @param timeout Timeout in nanoseconds (default: wait indefinitely)
         * @return VK_SUCCESS, VK_SUBOPTIMAL_KHR, VK_ERROR_OUT_OF_DATE_KHR, or other error code
         */
        VkResult acquireNextImage(VkSwapchainKHR swapchain,
            uint32_t& imageIndex,
            uint64_t timeout = UINT64_MAX);

        /**
         * @brief Ends recording, submits the command buffer, and presents the image
         * @param swapchain The swapchain to present to
         * @param imageIndex Index of the image to present
         * @param waitDstStageMask Pipeline stage to wait for image availability
         * @return VK_SUCCESS on success, or a Vulkan error code
         */
        VkResult submitAndPresent(VkSwapchainKHR swapchain,
            uint32_t imageIndex,
            VkPipelineStageFlags waitDstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        /**
         * @brief Advances to the next frame in the ring buffer
         */
        void     endFrame();

        /**
         * @brief Waits for all in-flight frames to complete on the GPU
         */
        void     waitIdle();

        /**
         * @brief Gets the current frame index in the ring buffer
         * @return Current frame index (0 to frameCount-1)
         */
        uint32_t currentFrameIndex() const { return current_; }

        /**
         * @brief Gets the total number of frames in flight
         * @return Number of buffered frames
         */
        uint32_t frameCount() const { return static_cast<uint32_t>(frames_.size()); }

        /**
         * @brief Checks if timeline semaphores are being used
         * @return True if using timeline semaphores, false if using fences
         */
        bool     usingTimeline() const { return hasTimeline_; }

        /**
         * @brief Gets the command buffer for the current frame
         * @return Command buffer handle
         */
        VkCommandBuffer cmd() const { return frames_[current_].cmd; }

        /**
         * @brief Gets the command pool for the current frame
         * @return Command pool handle
         */
        VkCommandPool   cmdPool() const { return frames_[current_].cmdPool; }

        /**
         * @brief Gets the image available semaphore for the current frame
         * @return Semaphore signaled when swapchain image is acquired
         */
        VkSemaphore     imageAvailable() const { return frames_[current_].imageAvailable; }

        /**
         * @brief Gets the render finished semaphore for the current frame
         * @return Semaphore signaled when rendering is complete
         */
        VkSemaphore     renderFinished() const { return frames_[current_].renderFinished; }

        /**
         * @brief Gets the timeline semaphore (if enabled)
         * @return Timeline semaphore handle, or VK_NULL_HANDLE if not using timeline semaphores
         */
        VkSemaphore     timelineSemaphore() const { return timeline_; }

        /**
         * @brief Gets the current timeline semaphore value
         * @return Current timeline counter value
         */
        uint64_t        timelineValue() const { return timelineCounter_; }

        /**
         * @brief Ends command buffer recording if currently recording
         * @return VK_SUCCESS on success, or a Vulkan error code
         */
        VkResult endRecordingIfOpen();
    private:
        /**
         * @struct PerFrame
         * @brief Per-frame synchronization and command recording resources
         */
        struct PerFrame {
            VkSemaphore     imageAvailable = VK_NULL_HANDLE; ///< Binary semaphore for image acquisition
            VkSemaphore     renderFinished = VK_NULL_HANDLE; ///< Binary semaphore for render completion
            VkFence         fence = VK_NULL_HANDLE;          ///< Fence for CPU wait (if not using timeline)
            VkCommandPool   cmdPool = VK_NULL_HANDLE;        ///< Command pool for this frame
            VkCommandBuffer cmd = VK_NULL_HANDLE;            ///< Primary command buffer for this frame
            uint64_t        lastSignaledTimelineValue = 0;   ///< Timeline value for this frame (if using timeline)
        };

        void destroy();
        void move_from(FrameSync&&);

        bool create_timeline_semaphore();
        void create_per_frame_objects();
        void destroy_per_frame_objects();

    private:
        VkDevice        device_ = VK_NULL_HANDLE;        ///< Logical device
        VkQueue         qGfx_ = VK_NULL_HANDLE;          ///< Graphics queue
        VkQueue         qPresent_ = VK_NULL_HANDLE;      ///< Present queue
        uint32_t        qGfxFamily_ = 0;                 ///< Graphics queue family index
        uint32_t        frameCount_ = 0;                 ///< Number of frames in flight
        bool            wantTimeline_ = true;            ///< Whether timeline semaphores were requested

        std::vector<PerFrame> frames_;                   ///< Per-frame resources ring buffer
        uint32_t        current_ = 0;                    ///< Current frame index

        bool            hasTimeline_ = false;            ///< Whether timeline semaphores are available
        VkSemaphore     timeline_ = VK_NULL_HANDLE;      ///< Timeline semaphore (if available)
        uint64_t        timelineCounter_ = 0;            ///< Monotonically increasing timeline value

        VkCommandPoolCreateFlags cmdPoolFlags_ = 0;      ///< Command pool creation flags
        const VkAllocationCallbacks* alloc_ = nullptr;   ///< Allocation callbacks
    };

} // namespace hvk

#endif // HVK_FRAME_SYNC_H
