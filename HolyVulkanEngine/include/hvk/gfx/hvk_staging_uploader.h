#pragma once
#ifndef HVK_STAGING_UPLOADER_HPP
#define HVK_STAGING_UPLOADER_HPP

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <cassert>

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_command_pools.h>

#if !ENGINE_USE_VMA
#error "StagingUploader requires VMA (ENGINE_USE_VMA=1)."
#endif
#include "vk_mem_alloc.h"

namespace hvk {

    struct StagingUploaderCreateInfo {
        const Device* device = nullptr;          // required
        VkQueue       queue = VK_NULL_HANDLE;    // upload queue (transfer preferred; gfx ok)
        uint32_t      queueFamilyIndex = 0;      // that queue's family index
        uint32_t      framesInFlight = 2;        // 2–3 typical
        VkDeviceSize  bytesPerFrame = 32u * 1024u * 1024u; // 32 MB default
        std::string   debugBaseName = "staging";
    };

    // A slice within the current frame's staging buffer.
    struct StagingSlice {
        VkDeviceSize  offset = 0;
        VkDeviceSize  size = 0;
        void* ptr = nullptr;
    };

    // Single-threaded, frame-ring staging manager.
    // Usage:
    //   StagingUploader up(ci);
    //   up.beginFrame(frameIndex);
    //   auto s = up.write(data, size);
    //   up.copyBuffer(dst, 0, s);
    //   up.submit(); // or submit(signal)
    //   // Next frame...
    class StagingUploader {
    public:
        StagingUploader() = default;
        explicit StagingUploader(const StagingUploaderCreateInfo& ci) { init(ci); }
        ~StagingUploader() { destroy(); }

        StagingUploader(const StagingUploader&) = delete;
        StagingUploader& operator=(const StagingUploader&) = delete;

        StagingUploader(StagingUploader&& o) noexcept { move_from(std::move(o)); }
        StagingUploader& operator=(StagingUploader&& o) noexcept {
            if (this != &o) { destroy(); move_from(std::move(o)); } return *this;
        }

        void init(const StagingUploaderCreateInfo& ci);
        void destroy();

        // Frame lifecycle --------------------------------------------------------
        // Waits on previous use of this frame slot, resets pool, begins cmd, zeros bump ptr.
        void beginFrame(uint32_t frameIndex);

        // Bump allocations / writes ---------------------------------------------
        StagingSlice alloc(VkDeviceSize size, VkDeviceSize alignment = 16);
        StagingSlice write(const void* src, VkDeviceSize size, VkDeviceSize alignment = 16);

        // Copy recorders ---------------------------------------------------------
        void copyBuffer(VkBuffer dst, VkDeviceSize dstOffset, const StagingSlice& src);
        void copyBufferRegion(VkBuffer dst, VkDeviceSize dstOffset, VkBuffer src, VkDeviceSize srcOffset, VkDeviceSize size);
        void copyBufferToImageTightly(VkImage dstImage, const VkImageSubresourceLayers& sub,
            VkOffset3D offset, VkExtent3D extent,
            const StagingSlice& src,
            uint32_t rowLength = 0, uint32_t imageHeight = 0);
        void copyBufferToImageRegion(VkImage dstImage, const VkBufferImageCopy& region, VkBuffer srcBuffer);

        // Submit / wait ----------------------------------------------------------
        // Ends recording (if open), flushes written bytes if non-coherent, submits w/ fence.
        // If 'signal' is non-null, it will be signaled upon completion.
        void submit(bool endIfNeeded = true, VkSemaphore signal = VK_NULL_HANDLE) const;

        // Block CPU until current frame's fence is signaled.
        void waitCurrent() const;

        // Accessors (current frame)
        VkBuffer       stagingBuffer() const { return framesVec_[cur_].buffer; }
        VkDeviceSize   usedBytes()     const { return framesVec_[cur_].head; }
        VkDeviceSize   capacityBytes() const { return framesVec_[cur_].size; }
        VkCommandBuffer cmd()          const { return framesVec_[cur_].cmd; }

        // Upload queue info
        VkQueue   queue()      const { return queue_; }
        uint32_t  queueFamily()const { return qFamily_; }

    private:
        struct PerFrame {
            // Buffer
            VkBuffer       buffer = VK_NULL_HANDLE;
            VmaAllocation  alloc = VK_NULL_HANDLE;
            void* mapped = nullptr;
            VkDeviceSize   size = 0;
            VkDeviceSize   head = 0;   // bump pointer in bytes this frame

            // Recording
            CommandPool    pool;         // owns command buffer lifetime
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            VkFence        fence = VK_NULL_HANDLE; // signals on submit completion
            bool           recording = false;
        };

    private:
        void create_frame(uint32_t i, VkDeviceSize bytes);
        void destroy_frame(uint32_t i);
        void flush_written_range(PerFrame& pf) const;
        static VkDeviceSize round_up(VkDeviceSize v, VkDeviceSize a) { return a ? (v + a - 1) & ~(a - 1) : v; }

        void move_from(StagingUploader&& o) noexcept;

    private:
        const Device* device_ = nullptr;
        VkQueue       queue_ = VK_NULL_HANDLE;
        uint32_t      qFamily_ = 0;
        uint32_t      frames_ = 0;
        uint32_t      cur_ = 0;
        std::vector<PerFrame> framesVec_;
        std::string   baseName_;
        VkDeviceSize  nonCoherentAtom_ = 1; // from limits
    };

} // namespace hvk

#endif // HVK_STAGING_UPLOADER_HPP
