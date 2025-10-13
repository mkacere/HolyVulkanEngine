#ifndef HVK_DEFERRED_DELETION_H
#define HVK_DEFERRED_DELETION_H

#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <cstdint>
#include <string>
#include <algorithm>
#include <stdexcept>

#include "hvk_device.h"

#if ENGINE_USE_VMA
#include "vk_mem_alloc.h"
#endif

namespace hvk {

    // Monotonic retire markers: either "frame index" or "timeline semaphore value".
    using RetireEpoch = uint64_t;

    struct DeferredDeletionCreateInfo {
        const Device* device = nullptr;                 // required
        const VkAllocationCallbacks* alloc = nullptr;   // optional
        std::string debugName{};                        // optional
    };

    // A simple, fast, single-threaded deferred-deletion queue.
    // Typical usage:
    //   q.enqueueImageView(epoch, view);
    //   q.enqueueBufferVma(epoch, buf, alloc);
    //   q.collect(completedEpoch);  // call once/frame after you know what's retired
    //   q.flush();                  // on shutdown
    class DeferredDeletionQueue {
    public:
        DeferredDeletionQueue() = default;
        explicit DeferredDeletionQueue(const DeferredDeletionCreateInfo& ci) { init(ci); }
        ~DeferredDeletionQueue() { destroy_all(/*waitIdle*/false); }

        DeferredDeletionQueue(const DeferredDeletionQueue&) = delete;
        DeferredDeletionQueue& operator=(const DeferredDeletionQueue&) = delete;

        DeferredDeletionQueue(DeferredDeletionQueue&& o) noexcept { move_from(std::move(o)); }
        DeferredDeletionQueue& operator=(DeferredDeletionQueue&& o) noexcept {
            if (this != &o) { destroy_all(false); move_from(std::move(o)); }
            return *this;
        }

        void init(const DeferredDeletionCreateInfo& ci) {
            if (!ci.device) throw std::invalid_argument("DeferredDeletionQueue: device is null");
            device_ = ci.device;
            alloc_ = ci.alloc;
            name_ = ci.debugName;
            tasks_.clear();
            minIndex_ = 0;
            dirtyOrder_ = false;
        }

        // ---- Enqueue helpers for common Vulkan objects --------------------------

        // Generic lambda (captures allowed). Lambda must be noexcept-safe and not throw.
        // It will be invoked later on the render thread.
        void enqueueLambda(RetireEpoch epoch, std::function<void()> fn) {
            if (!device_) throw std::runtime_error("DeferredDeletionQueue not initialized");
            add_task(epoch, std::move(fn));
        }

        // Image views / sampler / framebuffers / etc. (destroy with vk*)
        void enqueueImageView(RetireEpoch e, VkImageView v) { add_task(e, [=] { if (v) vkDestroyImageView(device_->device(), v, alloc_); }); }
        void enqueueSampler(RetireEpoch e, VkSampler s) { add_task(e, [=] { if (s) vkDestroySampler(device_->device(), s, alloc_); }); }
        void enqueueFramebuffer(RetireEpoch e, VkFramebuffer f) { add_task(e, [=] { if (f) vkDestroyFramebuffer(device_->device(), f, alloc_); }); }
        void enqueueRenderPass(RetireEpoch e, VkRenderPass rp) { add_task(e, [=] { if (rp) vkDestroyRenderPass(device_->device(), rp, alloc_); }); }
        void enqueueShaderModule(RetireEpoch e, VkShaderModule m) { add_task(e, [=] { if (m) vkDestroyShaderModule(device_->device(), m, alloc_); }); }
        void enqueuePipeline(RetireEpoch e, VkPipeline p) { add_task(e, [=] { if (p) vkDestroyPipeline(device_->device(), p, alloc_); }); }
        void enqueuePipelineLayout(RetireEpoch e, VkPipelineLayout l) { add_task(e, [=] { if (l) vkDestroyPipelineLayout(device_->device(), l, alloc_); }); }
        void enqueueDescriptorSetLayout(RetireEpoch e, VkDescriptorSetLayout l) { add_task(e, [=] { if (l) vkDestroyDescriptorSetLayout(device_->device(), l, alloc_); }); }
        void enqueueDescriptorPool(RetireEpoch e, VkDescriptorPool p) { add_task(e, [=] { if (p) vkDestroyDescriptorPool(device_->device(), p, alloc_); }); }
        void enqueueCommandPool(RetireEpoch e, VkCommandPool p) { add_task(e, [=] { if (p) vkDestroyCommandPool(device_->device(), p, alloc_); }); }
        void enqueueSemaphore(RetireEpoch e, VkSemaphore s) { add_task(e, [=] { if (s) vkDestroySemaphore(device_->device(), s, alloc_); }); }
        void enqueueFence(RetireEpoch e, VkFence f) { add_task(e, [=] { if (f) vkDestroyFence(device_->device(), f, alloc_); }); }
        void enqueueSwapchain(RetireEpoch e, VkSwapchainKHR sw) { add_task(e, [=] { if (sw) vkDestroySwapchainKHR(device_->device(), sw, alloc_); }); }

        // Buffers & Images:
#if ENGINE_USE_VMA
    // VMA-backed: destroy with vmaDestroy*
        void enqueueBufferVma(RetireEpoch e, VkBuffer buf, VmaAllocation alloc) {
            add_task(e, [=] { if (buf || alloc) vmaDestroyBuffer(device_->allocator(), buf, alloc); });
        }
        void enqueueImageVma(RetireEpoch e, VkImage img, VmaAllocation alloc) {
            add_task(e, [=] { if (img || alloc) vmaDestroyImage(device_->allocator(), img, alloc); });
        }
#endif
        // Raw: only destroys the handle (you must have freed/bound memory elsewhere).
        // Use only if you created/bound memory manually and are freeing memory in a separate task.
        void enqueueBufferRaw(RetireEpoch e, VkBuffer buf) {
            add_task(e, [=] { if (buf) vkDestroyBuffer(device_->device(), buf, alloc_); });
        }
        void enqueueImageRaw(RetireEpoch e, VkImage img) {
            add_task(e, [=] { if (img) vkDestroyImage(device_->device(), img, alloc_); });
        }
        void enqueueDeviceMemory(RetireEpoch e, VkDeviceMemory mem) {
            add_task(e, [=] { if (mem) vkFreeMemory(device_->device(), mem, alloc_); });
        }

        // ---- Maintenance --------------------------------------------------------

        // Destroy everything whose epoch <= completedEpoch.
        void collect(RetireEpoch completedEpoch) {
            if (tasks_.empty()) return;
            if (dirtyOrder_) {
                std::sort(tasks_.begin(), tasks_.end(), [](const Task& a, const Task& b) { return a.epoch < b.epoch; });
                dirtyOrder_ = false;
                minIndex_ = 0;
            }

            // Advance from minIndex_ while eligible
            size_t i = minIndex_;
            while (i < tasks_.size() && tasks_[i].epoch <= completedEpoch) {
                tasks_[i].fn(); // destroy
                ++i;
            }
            // erase consumed prefix
            if (i > minIndex_) {
                tasks_.erase(tasks_.begin() + static_cast<std::ptrdiff_t>(minIndex_),
                    tasks_.begin() + static_cast<std::ptrdiff_t>(i));
                // keep minIndex_ at 0 after erase
                minIndex_ = 0;
            }
        }

        // Wait for GPU idle then destroy everything immediately (use on shutdown).
        void flush() {
            if (!device_) return;
            device_->waitIdle();
            destroy_all(/*waitIdle*/false);
        }

        // Number of pending tasks
        size_t size() const { return tasks_.size(); }
        bool   empty() const { return tasks_.empty(); }

    private:
        struct Task {
            RetireEpoch epoch;
            std::function<void()> fn;
        };

        void add_task(RetireEpoch e, std::function<void()> fn) {
            // Keep tasks approximately sorted; if someone enqueues an out-of-order epoch, mark dirty.
            if (!tasks_.empty() && e < tasks_.back().epoch) dirtyOrder_ = true;
            tasks_.push_back(Task{ e, std::move(fn) });
        }

        void destroy_all(bool waitIdle) {
            if (!device_) return;
            if (waitIdle) device_->waitIdle();
            // Execute all tasks regardless of epoch.
            for (auto& t : tasks_) t.fn();
            tasks_.clear();
            minIndex_ = 0;
            dirtyOrder_ = false;
        }

        void move_from(DeferredDeletionQueue&& o) {
            device_ = o.device_;  o.device_ = nullptr;
            alloc_ = o.alloc_;   o.alloc_ = nullptr;
            name_ = std::move(o.name_);
            tasks_ = std::move(o.tasks_);
            minIndex_ = o.minIndex_;  o.minIndex_ = 0;
            dirtyOrder_ = o.dirtyOrder_; o.dirtyOrder_ = false;
        }

    private:
        const Device* device_ = nullptr;
        const VkAllocationCallbacks* alloc_ = nullptr;
        std::string name_;

        std::vector<Task> tasks_;
        size_t minIndex_ = 0;
        bool dirtyOrder_ = false;
    };

} // namespace hvk

#endif // HVK_DEFERRED_DELETION_H
