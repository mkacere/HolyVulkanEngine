/**
 * @file hvk_command_pools.h
 * @brief Vulkan command pool management and allocation
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Provides RAII wrappers for command pools, frame-ring command pools, and
 * scoped command buffer allocation for one-time submissions.
 */

#ifndef HVK_COMMAND_POOLS_H
#define HVK_COMMAND_POOLS_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string_view>

namespace hvk {

    class Device; // fwd

    /**
     * @struct CommandPoolCreateInfo
     * @brief Configuration for creating a CommandPool
     */
    struct CommandPoolCreateInfo {
        const Device* device = nullptr;                 // required
        uint32_t      queueFamilyIndex = 0;             // required
        VkCommandPoolCreateFlags flags =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;        // good defaults for per-frame usage
        std::string_view debugName{};                   // optional (uses Device::setObjectName)
    };

    class CommandPool {
    public:
        CommandPool() = default;
        explicit CommandPool(const CommandPoolCreateInfo& ci);
        ~CommandPool();

        CommandPool(const CommandPool&) = delete;
        CommandPool& operator=(const CommandPool&) = delete;

        CommandPool(CommandPool&& other) noexcept { move_from(std::move(other)); }
        CommandPool& operator=(CommandPool&& other) noexcept {
            if (this != &other) { destroy(); move_from(std::move(other)); }
            return *this;
        }

        // access
        VkCommandPool handle()   const { return pool_; }
        VkDevice      deviceVk() const;
        uint32_t      queueFamilyIndex() const { return family_; }
        const Device* device()   const { return device_; }

        // lifetime ops
        void reset(VkCommandPoolResetFlags flags = 0) const;
        void trim() const; // vkTrimCommandPool

        // allocation
        VkCommandBuffer allocatePrimary() const;
        VkCommandBuffer allocateSecondary() const;
        void allocatePrimary(uint32_t count, std::vector<VkCommandBuffer>& out) const;
        void allocateSecondary(uint32_t count, std::vector<VkCommandBuffer>& out) const;
        void free(const VkCommandBuffer* bufs, uint32_t count) const;

        // naming
        void setDebugName(std::string_view name) const;

        explicit operator bool() const { return pool_ != VK_NULL_HANDLE; }

    private:
        void destroy();
        void move_from(CommandPool&& other) noexcept;

    private:
        const Device* device_ = nullptr;
        VkCommandPool  pool_ = VK_NULL_HANDLE;
        uint32_t       family_ = 0;
        VkCommandPoolCreateFlags flags_ = 0;
    };

    // --- CommandPoolRing (frames-in-flight helper) ----------------------------

    struct CommandPoolRingCreateInfo {
        const Device* device = nullptr;                 // required
        uint32_t      queueFamilyIndex = 0;             // required
        uint32_t      framesInFlight = 2;               // 2�3 recommended
        VkCommandPoolCreateFlags flags =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        std::string_view baseDebugName{};               // e.g., "gfx-frame-pool"
    };

    class CommandPoolRing {
    public:
        CommandPoolRing() = default;
        explicit CommandPoolRing(const CommandPoolRingCreateInfo& ci);

        // non-copyable, movable
        CommandPoolRing(const CommandPoolRing&) = delete;
        CommandPoolRing& operator=(const CommandPoolRing&) = delete;
        CommandPoolRing(CommandPoolRing&&) noexcept = default;
        CommandPoolRing& operator=(CommandPoolRing&&) noexcept = default;

        uint32_t size() const { return static_cast<uint32_t>(pools_.size()); }
        CommandPool& current() { return pools_[current_]; }
        const CommandPool& current() const { return pools_[current_]; }

        // Call once per frame (or when you swap to a new frame slot)
        void beginFrame(uint32_t frameIndex); // resets that pool

        // convenience passthroughs
        VkCommandBuffer allocPrimary()  const { return pools_[current_].allocatePrimary(); }
        VkCommandBuffer allocSecondary()const { return pools_[current_].allocateSecondary(); }

    private:
        std::vector<CommandPool> pools_;
        uint32_t current_ = 0;
    };

    // --- ScopedCmdBuffer (one-time primary recording helper) ------------------

    class ScopedCmdBuffer {
    public:
        // Begins recording with ONE_TIME_SUBMIT_BIT.
        ScopedCmdBuffer(const CommandPool& pool);
        ~ScopedCmdBuffer(); // frees the command buffer

        ScopedCmdBuffer(const ScopedCmdBuffer&) = delete;
        ScopedCmdBuffer& operator=(const ScopedCmdBuffer&) = delete;

        VkCommandBuffer cmd() const { return cmd_; }

        // Explicitly end recording (idempotent)
        void end();

        // Submit to a queue and wait until complete (CPU wait).
        // Useful for setup/transfer work.
        void submitAndWait(VkQueue queue) const;

    private:
        const CommandPool* pool_ = nullptr;
        VkCommandBuffer     cmd_ = VK_NULL_HANDLE;
        bool                ended_ = false;
    };

} // namespace hvk

#endif // HVK_COMMAND_POOLS_H
