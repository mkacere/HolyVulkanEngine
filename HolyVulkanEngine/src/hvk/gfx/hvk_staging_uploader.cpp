#include "pch.h"

#include <hvk/gfx/hvk_staging_uploader.h>

namespace hvk {

    void StagingUploader::init(const StagingUploaderCreateInfo& ci) {
        if (!ci.device) throw std::invalid_argument("StagingUploader: device is null");
        if (ci.queue == VK_NULL_HANDLE) throw std::invalid_argument("StagingUploader: queue is null");
        if (ci.framesInFlight == 0) throw std::invalid_argument("StagingUploader: framesInFlight == 0");
        if (ci.bytesPerFrame == 0) throw std::invalid_argument("StagingUploader: bytesPerFrame == 0");

        device_ = ci.device;
        queue_ = ci.queue;
        qFamily_ = ci.queueFamilyIndex;
        frames_ = ci.framesInFlight;
        baseName_ = ci.debugBaseName;
        nonCoherentAtom_ = std::max<VkDeviceSize>(1, device_->limits().nonCoherentAtomSize);

        framesVec_.resize(frames_);
        for (uint32_t i = 0; i < frames_; ++i) {
            create_frame(i, ci.bytesPerFrame);
        }
        cur_ = 0;
    }

    void StagingUploader::destroy() {
        if (!device_) return;
        // Wait the queue to be safe
        vkQueueWaitIdle(queue_);
        for (uint32_t i = 0; i < frames_; ++i) destroy_frame(i);
        framesVec_.clear();
        frames_ = 0;
        cur_ = 0;
        device_ = nullptr;
        queue_ = VK_NULL_HANDLE;
        qFamily_ = 0;
        baseName_.clear();
        nonCoherentAtom_ = 1;
    }

    // -----------------------------------------------------------------------------

    void StagingUploader::create_frame(uint32_t i, VkDeviceSize bytes) {
        auto& pf = framesVec_[i];

        // Buffer (host-visible, persistently mapped)
        VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bi.size = bytes;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo aci{};
        aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT |
            VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
        aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

        VmaAllocationInfo ainfo{};
        VK_CHECK(vmaCreateBuffer(device_->allocator(), &bi, &aci, &pf.buffer, &pf.alloc, &ainfo));
        pf.mapped = ainfo.pMappedData;
        pf.size = bytes;
        pf.head = 0;

        if (!baseName_.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_BUFFER, (uint64_t)pf.buffer,
                baseName_ + "/buf#" + std::to_string(i));
        }

        // Command pool + one primary command buffer
        CommandPoolCreateInfo pci{};
        pci.device = device_;
        pci.queueFamilyIndex = qFamily_;
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pci.debugName = baseName_.empty() ? std::string{} : baseName_ + "/pool#" + std::to_string(i);
        pf.pool = CommandPool{ pci };

        pf.cmd = pf.pool.allocatePrimary();
        if (!baseName_.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pf.cmd,
                baseName_ + "/cmd#" + std::to_string(i));
        }

        // Fence starts signaled so first beginFrame won't block
        VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(device_->device(), &fci, nullptr, &pf.fence));
        if (!baseName_.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_FENCE, (uint64_t)pf.fence,
                baseName_ + "/fence#" + std::to_string(i));
        }

        pf.recording = false;
    }

    void StagingUploader::destroy_frame(uint32_t i) {
        auto& pf = framesVec_[i];

        if (pf.fence) {
            vkDestroyFence(device_->device(), pf.fence, nullptr);
            pf.fence = VK_NULL_HANDLE;
        }
        // CommandPool dtor will free the command buffer and destroy the pool.
        pf.pool = CommandPool{}; // move-assign empty to force destruction before buffer (order)

        if (pf.buffer || pf.alloc) {
            vmaDestroyBuffer(device_->allocator(), pf.buffer, pf.alloc);
            pf.buffer = VK_NULL_HANDLE;
            pf.alloc = VK_NULL_HANDLE;
        }
        pf.mapped = nullptr;
        pf.size = 0;
        pf.head = 0;
        pf.cmd = VK_NULL_HANDLE;
        pf.recording = false;
    }

    // -----------------------------------------------------------------------------

    void StagingUploader::beginFrame(uint32_t frameIndex) {
        if (!device_) throw std::runtime_error("StagingUploader not initialized");
        cur_ = frameIndex % frames_;
        auto& pf = framesVec_[cur_];

        // Wait for previous submit that used this slot
        VK_CHECK(vkWaitForFences(device_->device(), 1, &pf.fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device_->device(), 1, &pf.fence));

        // Reset pool + cmd buffer state
        pf.pool.reset(); // keeps allocations; resets CB states (pool created with RESET flag)

        // Begin command buffer
        VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(pf.cmd, &bi));
        pf.recording = true;

        // Reset bump pointer
        pf.head = 0;
    }

    StagingSlice StagingUploader::alloc(VkDeviceSize size, VkDeviceSize alignment) {
        auto& pf = framesVec_[cur_];
        if (!pf.recording) throw std::runtime_error("StagingUploader::alloc: call beginFrame() first");
        if (alignment == 0) alignment = 1;
        VkDeviceSize off = round_up(pf.head, alignment);
        if (off + size > pf.size) {
            throw std::runtime_error("StagingUploader: out of staging memory for this frame");
        }
        pf.head = off + size;

        StagingSlice s{};
        s.offset = off;
        s.size = size;
        s.ptr = static_cast<uint8_t*>(pf.mapped) + off;
        return s;
    }

    StagingSlice StagingUploader::write(const void* src, VkDeviceSize size, VkDeviceSize alignment) {
        StagingSlice s = alloc(size, alignment);
        if (size) std::memcpy(s.ptr, src, static_cast<size_t>(size));
        return s;
    }

    // -----------------------------------------------------------------------------

    void StagingUploader::copyBuffer(VkBuffer dst, VkDeviceSize dstOffset, const StagingSlice& src) {
        auto& pf = framesVec_[cur_];
        if (!pf.recording) throw std::runtime_error("StagingUploader::copyBuffer: not recording");
        VkBufferCopy r{};
        r.srcOffset = src.offset;
        r.dstOffset = dstOffset;
        r.size = src.size;
        vkCmdCopyBuffer(pf.cmd, pf.buffer, dst, 1, &r);
    }

    void StagingUploader::copyBufferRegion(VkBuffer dst, VkDeviceSize dstOffset,
        VkBuffer src, VkDeviceSize srcOffset, VkDeviceSize size) {
        auto& pf = framesVec_[cur_];
        if (!pf.recording) throw std::runtime_error("StagingUploader::copyBufferRegion: not recording");
        VkBufferCopy r{};
        r.srcOffset = srcOffset;
        r.dstOffset = dstOffset;
        r.size = size;
        vkCmdCopyBuffer(pf.cmd, src, dst, 1, &r);
    }

    void StagingUploader::copyBufferToImageTightly(VkImage dstImage, const VkImageSubresourceLayers& sub,
        VkOffset3D offset, VkExtent3D extent,
        const StagingSlice& src,
        uint32_t rowLength, uint32_t imageHeight) {
        auto& pf = framesVec_[cur_];
        if (!pf.recording) throw std::runtime_error("StagingUploader::copyBufferToImageTightly: not recording");

        VkBufferImageCopy c{};
        c.bufferOffset = src.offset;
        c.bufferRowLength = rowLength;     // 0 = tightly packed per spec
        c.bufferImageHeight = imageHeight; // 0 = tightly packed
        c.imageSubresource = sub;
        c.imageOffset = offset;
        c.imageExtent = extent;

        vkCmdCopyBufferToImage(pf.cmd, pf.buffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
    }

    void StagingUploader::copyBufferToImageRegion(VkImage dstImage, const VkBufferImageCopy& region, VkBuffer srcBuffer) {
        auto& pf = framesVec_[cur_];
        if (!pf.recording) throw std::runtime_error("StagingUploader::copyBufferToImageRegion: not recording");
        VkBuffer buf = srcBuffer ? srcBuffer : pf.buffer;
        vkCmdCopyBufferToImage(pf.cmd, buf, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    // -----------------------------------------------------------------------------

    void StagingUploader::flush_written_range(PerFrame& pf) const {
        if (pf.head == 0) return;

        // Align to nonCoherentAtomSize for safety (VMA expects aligned ranges).
        VkDeviceSize flushSize = round_up(pf.head, nonCoherentAtom_);
        // Offset aligned to atom as well. We always flush from 0..head for simplicity.
        vmaFlushAllocation(device_->allocator(), pf.alloc, 0, flushSize);
    }

    void StagingUploader::submit(bool endIfNeeded, VkSemaphore signal) const {
        auto& pf = const_cast<PerFrame&>(framesVec_[cur_]);
        if (pf.recording && endIfNeeded) {
            VK_CHECK(vkEndCommandBuffer(pf.cmd));
            pf.recording = false;
        }

        // Flush non-coherent memory if needed before GPU reads
        flush_written_range(pf);

        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &pf.cmd;

        VkSemaphore signalSem = signal;
        if (signalSem != VK_NULL_HANDLE) {
            si.signalSemaphoreCount = 1;
            si.pSignalSemaphores = &signalSem;
        }

        VK_CHECK(vkQueueSubmit(queue_, 1, &si, pf.fence));
    }

    void StagingUploader::waitCurrent() const {
        auto& pf = framesVec_[cur_];
        VK_CHECK(vkWaitForFences(device_->device(), 1, &pf.fence, VK_TRUE, UINT64_MAX));
    }

    // -----------------------------------------------------------------------------

    void StagingUploader::move_from(StagingUploader&& o) noexcept {
        device_ = o.device_;   o.device_ = nullptr;
        queue_ = o.queue_;    o.queue_ = VK_NULL_HANDLE;
        qFamily_ = o.qFamily_;  o.qFamily_ = 0;
        frames_ = o.frames_;   o.frames_ = 0;
        cur_ = o.cur_;      o.cur_ = 0;
        framesVec_ = std::move(o.framesVec_);
        baseName_ = std::move(o.baseName_);
        nonCoherentAtom_ = o.nonCoherentAtom_;
    }

} // namespace hvk
