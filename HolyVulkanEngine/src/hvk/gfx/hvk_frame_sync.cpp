#include "pch.h"

#include <hvk/gfx/hvk_frame_sync.h>

#ifndef HVK_VK_CHECK
#define HVK_VK_CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { assert(false); return _r; } } while(0)
#endif

namespace hvk {

    FrameSync::FrameSync(const FrameSyncCreateInfo& ci)
        : device_(ci.device)
        , qGfx_(ci.graphicsQueue)
        , qPresent_(ci.presentQueue ? ci.presentQueue : ci.graphicsQueue)
        , qGfxFamily_(ci.graphicsQueueFamilyIndex)
        , frameCount_(ci.framesInFlight ? ci.framesInFlight : 2)
        , wantTimeline_(ci.preferTimelineSemaphore)
        , cmdPoolFlags_(ci.cmdPoolFlags)
        , alloc_(ci.alloc)
    {
        assert(device_ && qGfx_);
        frames_.resize(frameCount_);

        hasTimeline_ = wantTimeline_ && create_timeline_semaphore(); // try timeline, fall back gracefully
        create_per_frame_objects();
    }

    FrameSync::~FrameSync() { destroy(); }

    void FrameSync::destroy() {
        if (!device_) return;

        // Make sure GPU is done with our stuff
        waitIdle();

        destroy_per_frame_objects();

        if (hasTimeline_ && timeline_) {
            vkDestroySemaphore(device_, timeline_, alloc_);
            timeline_ = VK_NULL_HANDLE;
        }

        device_ = VK_NULL_HANDLE;
        qGfx_ = VK_NULL_HANDLE;
        qPresent_ = VK_NULL_HANDLE;
        frames_.clear();
        frameCount_ = 0;
        current_ = 0;
        hasTimeline_ = false;
        timelineCounter_ = 0;
    }

    void FrameSync::move_from(FrameSync&& o) {
        device_ = o.device_;         o.device_ = VK_NULL_HANDLE;
        qGfx_ = o.qGfx_;           o.qGfx_ = VK_NULL_HANDLE;
        qPresent_ = o.qPresent_;       o.qPresent_ = VK_NULL_HANDLE;
        qGfxFamily_ = o.qGfxFamily_;     o.qGfxFamily_ = 0;
        frameCount_ = o.frameCount_;     o.frameCount_ = 0;
        wantTimeline_ = o.wantTimeline_;
        frames_ = std::move(o.frames_);
        current_ = o.current_;        o.current_ = 0;
        hasTimeline_ = o.hasTimeline_;    o.hasTimeline_ = false;
        timeline_ = o.timeline_;       o.timeline_ = VK_NULL_HANDLE;
        timelineCounter_ = o.timelineCounter_; o.timelineCounter_ = 0;
        cmdPoolFlags_ = o.cmdPoolFlags_;
        alloc_ = o.alloc_;
    }

    bool FrameSync::create_timeline_semaphore() {
        // Try to create a timeline semaphore. If device lacks feature, driver returns error.
        VkSemaphoreTypeCreateInfo timelineInfo{};
        timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineInfo.initialValue = 0;

        VkSemaphoreCreateInfo semCI{};
        semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semCI.pNext = &timelineInfo;

        VkResult res = vkCreateSemaphore(device_, &semCI, alloc_, &timeline_);
        return (res == VK_SUCCESS);
    }

    void FrameSync::create_per_frame_objects() {
        // Common binary semaphore create info
        VkSemaphoreCreateInfo semCI{};
        semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // Fence create info
        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so the very first beginFrame does not stall

        // Command pool create info
        VkCommandPoolCreateInfo poolCI{};
        poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCI.queueFamilyIndex = qGfxFamily_;
        poolCI.flags = cmdPoolFlags_;

        for (uint32_t i = 0; i < frameCount_; ++i) {
            PerFrame& f = frames_[i];

            // Binary semaphores for swapchain usage
            vkCreateSemaphore(device_, &semCI, alloc_, &f.imageAvailable);
            vkCreateSemaphore(device_, &semCI, alloc_, &f.renderFinished);

            // If no timeline, create a per-frame fence for CPU sync
            if (!hasTimeline_) {
                vkCreateFence(device_, &fenceCI, alloc_, &f.fence);
            }

            // Command pool & one primary command buffer
            vkCreateCommandPool(device_, &poolCI, alloc_, &f.cmdPool);

            VkCommandBufferAllocateInfo cbAI{};
            cbAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cbAI.commandPool = f.cmdPool;
            cbAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbAI.commandBufferCount = 1;
            vkAllocateCommandBuffers(device_, &cbAI, &f.cmd);

            f.lastSignaledTimelineValue = 0;
        }
    }

    void FrameSync::destroy_per_frame_objects() {
        for (auto& f : frames_) {
            if (f.cmd) {
                vkFreeCommandBuffers(device_, f.cmdPool, 1, &f.cmd);
                f.cmd = VK_NULL_HANDLE;
            }
            if (f.cmdPool) {
                vkDestroyCommandPool(device_, f.cmdPool, alloc_);
                f.cmdPool = VK_NULL_HANDLE;
            }
            if (f.fence) {
                vkDestroyFence(device_, f.fence, alloc_);
                f.fence = VK_NULL_HANDLE;
            }
            if (f.imageAvailable) {
                vkDestroySemaphore(device_, f.imageAvailable, alloc_);
                f.imageAvailable = VK_NULL_HANDLE;
            }
            if (f.renderFinished) {
                vkDestroySemaphore(device_, f.renderFinished, alloc_);
                f.renderFinished = VK_NULL_HANDLE;
            }
        }
    }

    VkResult FrameSync::beginFrame() {
        PerFrame& f = frames_[current_];

        if (hasTimeline_) {
            if (f.lastSignaledTimelineValue > 0) {
                const VkSemaphore sems[1] = { timeline_ };
                const uint64_t    values[1] = { f.lastSignaledTimelineValue };
                VkSemaphoreWaitInfo waitInfo{};
                waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
                waitInfo.flags = 0;
                waitInfo.semaphoreCount = 1;
                waitInfo.pSemaphores = sems;
                waitInfo.pValues = values;
                VkResult wr = vkWaitSemaphores(device_, &waitInfo, UINT64_MAX);
                if (wr != VK_SUCCESS) return wr;
            }
        }
        else {
            // Wait for the frame's fence then reset it
            VkResult wr = vkWaitForFences(device_, 1, &f.fence, VK_TRUE, UINT64_MAX);
            if (wr != VK_SUCCESS) return wr;
            vkResetFences(device_, 1, &f.fence);
        }

        // Reset command pool and begin the primary cmd buffer
        vkResetCommandPool(device_, f.cmdPool, 0);

        VkCommandBufferBeginInfo cbBI{};
        cbBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        return vkBeginCommandBuffer(f.cmd, &cbBI);
    }

    VkResult FrameSync::acquireNextImage(VkSwapchainKHR swapchain,
        uint32_t& imageIndex,
        uint64_t timeout) {
        // WSI requires a binary semaphore for image acquisition.
        return vkAcquireNextImageKHR(device_, swapchain, timeout,
            frames_[current_].imageAvailable, // signal this when available
            VK_NULL_HANDLE,                    // no fence here
            &imageIndex);
    }

    VkResult FrameSync::submitAndPresent(VkSwapchainKHR swapchain,
        uint32_t imageIndex,
        VkPipelineStageFlags waitDstStageMask) {
        PerFrame& f = frames_[current_];

        // Finish recording
        VkResult endRes = vkEndCommandBuffer(f.cmd);
        if (endRes != VK_SUCCESS) return endRes;

        // Build submit info
        VkSemaphore          waitSems[1] = { f.imageAvailable }; // must wait for acquired image
        VkPipelineStageFlags waitStages[1] = { waitDstStageMask };

        // We always signal renderFinished; optionally also signal the timeline
        VkSemaphore          signalSems[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        uint32_t             signalCount = 0;

        signalSems[signalCount++] = f.renderFinished;

        // Timeline payloads (if used)
        VkTimelineSemaphoreSubmitInfo timelineSI{};
        timelineSI.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;

        uint64_t waitValues[1] = { 0 }; // 0 for binary
        uint64_t signalValues[2] = { 0, 0 };

        if (hasTimeline_) {
            // also signal timeline with the next value
            timelineCounter_ += 1;
            f.lastSignaledTimelineValue = timelineCounter_;
            signalSems[signalCount++] = timeline_;

            timelineSI.waitSemaphoreValueCount = 1;
            timelineSI.pWaitSemaphoreValues = waitValues;   // [0] ignored for binary
            timelineSI.signalSemaphoreValueCount = signalCount;  // includes binary+timeline
            signalValues[0] = 0;                                 // binary ignored
            signalValues[1] = f.lastSignaledTimelineValue;
            timelineSI.pSignalSemaphoreValues = signalValues;
        }

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.pNext = hasTimeline_ ? &timelineSI : nullptr;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = waitSems;
        submit.pWaitDstStageMask = waitStages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &f.cmd;
        submit.signalSemaphoreCount = signalCount;
        submit.pSignalSemaphores = signalSems;

        VkFence fence = hasTimeline_ ? VK_NULL_HANDLE : f.fence;

        HVK_VK_CHECK(vkQueueSubmit(qGfx_, 1, &submit, fence));

        // Present, waiting on renderFinished
        VkSwapchainKHR swapchains[1] = { swapchain };
        uint32_t       indices[1] = { imageIndex };

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &f.renderFinished;
        present.swapchainCount = 1;
        present.pSwapchains = swapchains;
        present.pImageIndices = indices;

        // Note: VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR should be handled by caller (recreate swapchain).
        return vkQueuePresentKHR(qPresent_, &present);
    }

    void FrameSync::endFrame() {
        current_ = (current_ + 1) % frameCount_;
    }

    void FrameSync::waitIdle() {
        if (!device_) return;

        if (hasTimeline_) {
            // Wait until the timeline has reached the max signaled value among frames
            uint64_t target = 0;
            for (const auto& f : frames_)
                if (f.lastSignaledTimelineValue > target) target = f.lastSignaledTimelineValue;

            if (target > 0) {
                VkSemaphore sem = timeline_;
                VkSemaphoreWaitInfo wi{};
                wi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
                wi.semaphoreCount = 1;
                wi.pSemaphores = &sem;
                wi.pValues = &target;
                vkWaitSemaphores(device_, &wi, UINT64_MAX);
            }
            // no fences to wait/reset
        }
        else {
            std::vector<VkFence> fences;
            fences.reserve(frames_.size());
            for (const auto& f : frames_) if (f.fence) fences.push_back(f.fence);
            if (!fences.empty()) {
                vkWaitForFences(device_, static_cast<uint32_t>(fences.size()),
                    fences.data(), VK_TRUE, UINT64_MAX);
            }
        }
    }

    VkResult FrameSync::endRecordingIfOpen() {
        // For now, just end—it’s harmless to fail if already ended.
        return vkEndCommandBuffer(frames_[current_].cmd);
    }

} // namespace hvk
