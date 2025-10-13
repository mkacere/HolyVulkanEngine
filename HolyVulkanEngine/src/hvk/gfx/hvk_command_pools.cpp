#include "pch.h"

#include <hvk/gfx/hvk_command_pools.h>
#include <hvk/gfx/hvk_device.h>

#ifndef VK_CHECK
#define VK_CHECK(x) do { VkResult _e = (x); if (_e != VK_SUCCESS) throw std::runtime_error("Vulkan error: " #x); } while(0)
#endif

namespace hvk {

    // ============================= CommandPool =============================

    CommandPool::CommandPool(const CommandPoolCreateInfo& ci)
        : device_(ci.device)
        , family_(ci.queueFamilyIndex)
        , flags_(ci.flags)
    {
        if (!device_) throw std::invalid_argument("CommandPool: device is null");

        VkCommandPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.queueFamilyIndex = family_;
        info.flags = flags_;
        VK_CHECK(vkCreateCommandPool(deviceVk(), &info, nullptr, &pool_));

        if (!ci.debugName.empty())
            setDebugName(ci.debugName);
    }

    CommandPool::~CommandPool() { destroy(); }

    void CommandPool::destroy() {
        if (pool_) {
            vkDestroyCommandPool(deviceVk(), pool_, nullptr);
            pool_ = VK_NULL_HANDLE;
        }
        device_ = nullptr;
        family_ = 0;
        flags_ = 0;
    }

    void CommandPool::move_from(CommandPool&& other) noexcept {
        device_ = other.device_;  other.device_ = nullptr;
        pool_ = other.pool_;    other.pool_ = VK_NULL_HANDLE;
        family_ = other.family_;  other.family_ = 0;
        flags_ = other.flags_;   other.flags_ = 0;
    }

    VkDevice CommandPool::deviceVk() const {
        // your Device exposes VkDevice via device()
        return device_ ? device_->device() : VK_NULL_HANDLE;
    }

    void CommandPool::reset(VkCommandPoolResetFlags flags) const {
        vkResetCommandPool(deviceVk(), pool_, flags);
    }

    void CommandPool::trim() const {
        // Core since Vulkan 1.1
        vkTrimCommandPool(deviceVk(), pool_, 0);
    }

    static void allocate_impl(VkDevice dev, VkCommandPool pool, VkCommandBufferLevel level,
        uint32_t count, std::vector<VkCommandBuffer>& out)
    {
        out.resize(count);
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = pool;
        ai.level = level;
        ai.commandBufferCount = count;
        VK_CHECK(vkAllocateCommandBuffers(dev, &ai, out.data()));
    }

    VkCommandBuffer CommandPool::allocatePrimary() const {
        std::vector<VkCommandBuffer> tmp;
        allocate_impl(deviceVk(), pool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, tmp);
        return tmp[0];
    }

    VkCommandBuffer CommandPool::allocateSecondary() const {
        std::vector<VkCommandBuffer> tmp;
        allocate_impl(deviceVk(), pool_, VK_COMMAND_BUFFER_LEVEL_SECONDARY, 1, tmp);
        return tmp[0];
    }

    void CommandPool::allocatePrimary(uint32_t count, std::vector<VkCommandBuffer>& out) const {
        allocate_impl(deviceVk(), pool_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, count, out);
    }

    void CommandPool::allocateSecondary(uint32_t count, std::vector<VkCommandBuffer>& out) const {
        allocate_impl(deviceVk(), pool_, VK_COMMAND_BUFFER_LEVEL_SECONDARY, count, out);
    }

    void CommandPool::free(const VkCommandBuffer* bufs, uint32_t count) const {
        if (!bufs || count == 0) return;
        vkFreeCommandBuffers(deviceVk(), pool_, count, bufs);
    }

    void CommandPool::setDebugName(std::string_view name) const {
        if (!device_ || !pool_) return;
        device_->setObjectName(VK_OBJECT_TYPE_COMMAND_POOL,
            reinterpret_cast<uint64_t>(pool_),
            name);
    }

    // ============================ CommandPoolRing ===========================

    CommandPoolRing::CommandPoolRing(const CommandPoolRingCreateInfo& ci) {
        if (!ci.device || ci.framesInFlight == 0)
            throw std::invalid_argument("CommandPoolRing: invalid args");

        pools_.reserve(ci.framesInFlight);
        for (uint32_t i = 0; i < ci.framesInFlight; ++i) {
            CommandPoolCreateInfo pci{};
            pci.device = ci.device;
            pci.queueFamilyIndex = ci.queueFamilyIndex;
            pci.flags = ci.flags;

            std::string name;
            if (!ci.baseDebugName.empty()) {
                name = std::string(ci.baseDebugName) + "#" + std::to_string(i);
                pci.debugName = name;
            }

            pools_.emplace_back(pci);
        }
        current_ = 0;
    }

    void CommandPoolRing::beginFrame(uint32_t frameIndex) {
        assert(!pools_.empty());
        current_ = frameIndex % static_cast<uint32_t>(pools_.size());
        pools_[current_].reset(0); // reset all buffers allocated from this pool
    }

    // ============================ ScopedCmdBuffer ===========================

    ScopedCmdBuffer::ScopedCmdBuffer(const CommandPool& pool)
        : pool_(&pool)
    {
        cmd_ = pool.allocatePrimary();

        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd_, &bi));
    }

    ScopedCmdBuffer::~ScopedCmdBuffer() {
        if (cmd_) {
            if (!ended_) {
                // Try to end; if it fails, we still attempt to free the buffer
                vkEndCommandBuffer(cmd_);
            }
            pool_->free(&cmd_, 1);
            cmd_ = VK_NULL_HANDLE;
        }
    }

    void ScopedCmdBuffer::end() {
        if (!ended_) {
            VK_CHECK(vkEndCommandBuffer(cmd_));
            ended_ = true;
        }
    }

    void ScopedCmdBuffer::submitAndWait(VkQueue queue) const {
        // Ensure ended; if not, we end implicitly
        if (!ended_) {
            const_cast<ScopedCmdBuffer*>(this)->end();
        }

        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd_;
        VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue));
    }

} // namespace hvk
