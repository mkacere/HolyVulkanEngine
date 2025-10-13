#ifndef HVK_DESCRIPTORS_H
#define HVK_DESCRIPTORS_H

#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>
#include <cstdint>
#include <cassert>
#include <optional>

namespace hvk {

    class Device; // fwd

    // --------------------------- DescriptorSetLayout -----------------------------

    struct DescriptorSetLayoutCreateInfo {
        const Device* device = nullptr; // required
        std::vector<VkDescriptorSetLayoutBinding> bindings; // required
        // Optional per-binding flags (same length as bindings if provided)
        std::vector<VkDescriptorBindingFlags> bindingFlags;
        // Layout-level flag: using UPDATE_AFTER_BIND pool?
        bool updateAfterBindPool = false;
        std::string_view debugName{};
    };

    class DescriptorSetLayout {
    public:
        DescriptorSetLayout() = default;
        explicit DescriptorSetLayout(const DescriptorSetLayoutCreateInfo& ci);
        ~DescriptorSetLayout();

        DescriptorSetLayout(const DescriptorSetLayout&) = delete;
        DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

        DescriptorSetLayout(DescriptorSetLayout&& o) noexcept { move_from(std::move(o)); }
        DescriptorSetLayout& operator=(DescriptorSetLayout&& o) noexcept {
            if (this != &o) { destroy(); move_from(std::move(o)); }
            return *this;
        }

        VkDescriptorSetLayout handle() const { return layout_; }
        const Device* device() const { return device_; }
        bool usesUpdateAfterBindPool() const { return updateAfterBindPool_; }

        // true if any binding has VARIABLE_DESCRIPTOR_COUNT_BIT set
        bool hasVariableCountBinding() const { return hasVariableCount_; }

        // binding meta
        const std::vector<VkDescriptorSetLayoutBinding>& bindings() const { return bindings_; }
        const std::vector<VkDescriptorBindingFlags>& bindingFlags() const { return flags_; }

        void setDebugName(std::string_view name) const;

        explicit operator bool() const { return layout_ != VK_NULL_HANDLE; }

    private:
        void destroy();
        void move_from(DescriptorSetLayout&& o) noexcept;

    private:
        const Device* device_ = nullptr;
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayoutBinding> bindings_;
        std::vector<VkDescriptorBindingFlags> flags_;
        bool updateAfterBindPool_ = false;
        bool hasVariableCount_ = false;
    };

    // ----------------------------- DescriptorAllocator ---------------------------

    struct DescriptorAllocatorCreateInfo {
        const Device* device = nullptr;      // required
        uint32_t maxSetsPerPool = 1024;      // grows when exhausted
        // default pool sizes per block; if empty, sensible defaults are used
        std::vector<VkDescriptorPoolSize> poolSizes;
        // create a separate bucket for UPDATE_AFTER_BIND layouts
        bool enableUpdateAfterBindBucket = true;
        std::string_view debugName{};
    };

    class DescriptorAllocator {
    public:
        DescriptorAllocator() = default;
        explicit DescriptorAllocator(const DescriptorAllocatorCreateInfo& ci);
        ~DescriptorAllocator();

        DescriptorAllocator(const DescriptorAllocator&) = delete;
        DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;

        DescriptorAllocator(DescriptorAllocator&& o) noexcept { move_from(std::move(o)); }
        DescriptorAllocator& operator=(DescriptorAllocator&& o) noexcept {
            if (this != &o) { destroy(); move_from(std::move(o)); }
            return *this;
        }

        // Allocate a set from the appropriate pool (regular or update-after-bind).
        // If the layout has a VARIABLE_DESCRIPTOR_COUNT binding, pass 'variableCount'
        // with the descriptor count for that binding; otherwise leave 0.
        VkDescriptorSet allocate(const DescriptorSetLayout& layout, uint32_t variableCount = 0);

        // Epoch reset: call when GPU is done with all sets allocated since previous reset.
        // (typical: once per frame for frame-scoped sets, or at level load, etc.)
        void resetEpoch();

        // Trim: optional memory hint to driver
        void trim();

        const Device* device() const { return device_; }

    private:
        struct PoolBucket {
            std::vector<VkDescriptorPool> pools;
            uint32_t nextPoolIndex = 0; // round-robin/growth
            VkDescriptorPoolCreateFlags flags = 0;
            std::string name;
        };

        void destroy();
        void move_from(DescriptorAllocator&& o) noexcept;

        VkDescriptorPool createPool(PoolBucket& bucket) const;
        VkDescriptorPool currentPool(PoolBucket& bucket);
        VkDescriptorPool newPool(PoolBucket& bucket);

        PoolBucket& bucketForLayout(const DescriptorSetLayout& layout);

        static std::vector<VkDescriptorPoolSize> defaultPoolSizes();

    private:
        const Device* device_ = nullptr;
        uint32_t maxSetsPerPool_ = 1024;
        std::vector<VkDescriptorPoolSize> basePoolSizes_;

        PoolBucket regular_;
        PoolBucket updateAfterBind_;

        std::string baseName_;
    };

    // ------------------------------- DescriptorWrites ----------------------------

    // Small, RAII-safe builder that owns backing storage for infos until commit().
    class DescriptorWrites {
    public:
        DescriptorWrites() = default;

        // Buffer writes
        DescriptorWrites& writeBuffer(VkDescriptorSet set, uint32_t binding,
            VkDescriptorType type,
            VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range,
            uint32_t arrayElement = 0);

        // Image writes (combined sampler, sampled image, storage image)
        DescriptorWrites& writeImage(VkDescriptorSet set, uint32_t binding,
            VkDescriptorType type,
            VkImageView view, VkImageLayout layout,
            VkSampler sampler = VK_NULL_HANDLE,
            uint32_t arrayElement = 0);

        // Texel buffer view
        DescriptorWrites& writeTexel(VkDescriptorSet set, uint32_t binding,
            VkDescriptorType type,
            VkBufferView view,
            uint32_t arrayElement = 0);

        void commit(VkDevice device);

        // Clear accumulated writes (optional; auto-cleared on commit)
        void clear();

    private:
        std::vector<VkWriteDescriptorSet> writes_;
        std::vector<VkDescriptorImageInfo> imageInfos_;
        std::vector<VkDescriptorBufferInfo> bufferInfos_;
        std::vector<VkBufferView> texelViews_;
    };

    // ------------------------------- BindlessSet (optional) ----------------------
    // Minimal "bindless textures" set: one binding of COMBINED_IMAGE_SAMPLER[]
    // with VARIABLE_DESCRIPTOR_COUNT + PARTIALLY_BOUND + UPDATE_AFTER_BIND.
    struct BindlessSetCreateInfo {
        const Device* device = nullptr;              // required
        uint32_t maxTextures = 1024;                 // descriptorCount
        VkShaderStageFlags visibleStages = VK_SHADER_STAGE_FRAGMENT_BIT;
        // Sampler is per-entry; you can use immutable samplers by providing a single VkSampler*
        // for the binding; pass nullptr to use dynamic samplers per write.
        const VkSampler* immutableSampler = nullptr; // optional (1 sampler applied to all entries)
        std::string_view debugNameLayout{};
        std::string_view debugNamePool{};
        std::string_view debugNameSet{};
    };

    class BindlessSet {
    public:
        BindlessSet() = default;
        explicit BindlessSet(const BindlessSetCreateInfo& ci);
        ~BindlessSet();

        BindlessSet(const BindlessSet&) = delete;
        BindlessSet& operator=(const BindlessSet&) = delete;

        BindlessSet(BindlessSet&& o) noexcept { move_from(std::move(o)); }
        BindlessSet& operator=(BindlessSet&& o) noexcept {
            if (this != &o) { destroy(); move_from(std::move(o)); }
            return *this;
        }

        VkDescriptorSetLayout layout() const { return layout_.handle(); }
        VkDescriptorSet set() const { return set_; }
        uint32_t capacity() const { return capacity_; }

        // Update a single slot [0..capacity()-1]
        void updateTexture(uint32_t index, VkImageView view, VkImageLayout layout, VkSampler sampler);

    private:
        void destroy();
        void move_from(BindlessSet&& o) noexcept;

    private:
        const Device* device_ = nullptr;
        DescriptorSetLayout layout_;
        DescriptorAllocator allocator_;
        VkDescriptorSet set_ = VK_NULL_HANDLE;
        uint32_t capacity_ = 0;
    };

} // namespace hvk

#endif // HVK_DESCRIPTORS_H
