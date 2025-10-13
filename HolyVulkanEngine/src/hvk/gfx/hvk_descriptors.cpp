#include "pch.h"

#include <hvk/gfx/hvk_descriptors.h>
#include <hvk/gfx/hvk_device.h>

#ifndef VK_CHECK
#define VK_CHECK(x) do { VkResult _e = (x); if (_e != VK_SUCCESS) throw std::runtime_error("Vulkan error: " #x); } while(0)
#endif

namespace hvk {

    // ============================ DescriptorSetLayout ============================

    DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetLayoutCreateInfo& ci)
        : device_(ci.device)
        , bindings_(ci.bindings)
        , flags_(ci.bindingFlags)
        , updateAfterBindPool_(ci.updateAfterBindPool)
    {
        if (!device_) throw std::invalid_argument("DescriptorSetLayout: device is null");
        if (bindings_.empty()) throw std::invalid_argument("DescriptorSetLayout: no bindings");

        if (!flags_.empty() && flags_.size() != bindings_.size())
            throw std::invalid_argument("DescriptorSetLayout: bindingFlags size mismatch");

        // detect VARIABLE_DESCRIPTOR_COUNT usage
        hasVariableCount_ = false;
        for (size_t i = 0; i < flags_.size(); ++i) {
            if (flags_[i] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
                hasVariableCount_ = true;
        }

        // Build layout create info
        VkDescriptorSetLayoutCreateInfo lci{};
        lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        lci.bindingCount = static_cast<uint32_t>(bindings_.size());
        lci.pBindings = bindings_.data();
        lci.flags = updateAfterBindPool_
            ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT
            : 0;

        // If per-binding flags present, chain them in
        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
        if (!flags_.empty()) {
            flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            flagsInfo.bindingCount = static_cast<uint32_t>(flags_.size());
            flagsInfo.pBindingFlags = flags_.data();
            lci.pNext = &flagsInfo;
        }

        VK_CHECK(vkCreateDescriptorSetLayout(device_->device(), &lci, nullptr, &layout_));

        if (!ci.debugName.empty())
            setDebugName(ci.debugName);
    }

    DescriptorSetLayout::~DescriptorSetLayout() { destroy(); }

    void DescriptorSetLayout::destroy() {
        if (layout_) {
            vkDestroyDescriptorSetLayout(device_->device(), layout_, nullptr);
            layout_ = VK_NULL_HANDLE;
        }
        device_ = nullptr;
        bindings_.clear();
        flags_.clear();
        updateAfterBindPool_ = false;
        hasVariableCount_ = false;
    }

    void DescriptorSetLayout::move_from(DescriptorSetLayout&& o) noexcept {
        device_ = o.device_;    o.device_ = nullptr;
        layout_ = o.layout_;    o.layout_ = VK_NULL_HANDLE;
        bindings_ = std::move(o.bindings_);
        flags_ = std::move(o.flags_);
        updateAfterBindPool_ = o.updateAfterBindPool_; o.updateAfterBindPool_ = false;
        hasVariableCount_ = o.hasVariableCount_;       o.hasVariableCount_ = false;
    }

    void DescriptorSetLayout::setDebugName(std::string_view name) const {
        if (!device_ || !layout_) return;
        device_->setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
            reinterpret_cast<uint64_t>(layout_), name);
    }

    // ============================== DescriptorAllocator ==========================

    static void name_pool(const Device* dev, VkDescriptorPool pool, std::string_view name) {
        if (!dev || !pool || name.empty()) return;
        dev->setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_POOL,
            reinterpret_cast<uint64_t>(pool), name);
    }

    std::vector<VkDescriptorPoolSize> DescriptorAllocator::defaultPoolSizes() {
        // A balanced default block suitable for most engines; grows with more pools as needed.
        return {
            { VK_DESCRIPTOR_TYPE_SAMPLER,                256 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1024 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          256 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         512 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         512 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 256 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 128 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       64  },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   128 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   128 },
        };
    }

    DescriptorAllocator::DescriptorAllocator(const DescriptorAllocatorCreateInfo& ci)
        : device_(ci.device)
        , maxSetsPerPool_(ci.maxSetsPerPool ? ci.maxSetsPerPool : 1024)
        , basePoolSizes_(ci.poolSizes.empty() ? defaultPoolSizes() : ci.poolSizes)
        , baseName_(ci.debugName.empty() ? "desc" : std::string(ci.debugName))
    {
        if (!device_) throw std::invalid_argument("DescriptorAllocator: device is null");

        regular_.flags = 0;
        regular_.name = baseName_ + "/regular";

        if (ci.enableUpdateAfterBindBucket) {
            updateAfterBind_.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            updateAfterBind_.name = baseName_ + "/UAB";
        }
    }

    DescriptorAllocator::~DescriptorAllocator() { destroy(); }

    void DescriptorAllocator::destroy() {
        if (!device_) return;
        auto destroy_bucket = [&](PoolBucket& b) {
            for (auto p : b.pools) {
                vkDestroyDescriptorPool(device_->device(), p, nullptr);
            }
            b.pools.clear();
            b.nextPoolIndex = 0;
            };
        destroy_bucket(regular_);
        destroy_bucket(updateAfterBind_);
        device_ = nullptr;
    }

    DescriptorAllocator::PoolBucket& DescriptorAllocator::bucketForLayout(const DescriptorSetLayout& layout) {
        if (layout.usesUpdateAfterBindPool()) {
            // if disabled at allocator creation, still use regular to avoid hard fail
            return updateAfterBind_.pools.empty() && (updateAfterBind_.flags == 0)
                ? regular_
                : updateAfterBind_;
        }
        return regular_;
    }

    VkDescriptorPool DescriptorAllocator::createPool(PoolBucket& bucket) const {
        VkDescriptorPoolCreateInfo pci{};
        pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.flags = bucket.flags;
        pci.maxSets = maxSetsPerPool_;
        pci.poolSizeCount = static_cast<uint32_t>(basePoolSizes_.size());
        pci.pPoolSizes = basePoolSizes_.data();

        VkDescriptorPool pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(device_->device(), &pci, nullptr, &pool));
        name_pool(device_, pool, bucket.name);
        return pool;
    }

    VkDescriptorPool DescriptorAllocator::newPool(PoolBucket& bucket) {
        VkDescriptorPool p = createPool(bucket);
        bucket.pools.push_back(p);
        bucket.nextPoolIndex = static_cast<uint32_t>(bucket.pools.size() - 1);
        return p;
    }

    VkDescriptorPool DescriptorAllocator::currentPool(PoolBucket& bucket) {
        if (bucket.pools.empty()) return newPool(bucket);
        if (bucket.nextPoolIndex >= bucket.pools.size()) bucket.nextPoolIndex = 0;
        return bucket.pools[bucket.nextPoolIndex];
    }

    VkDescriptorSet DescriptorAllocator::allocate(const DescriptorSetLayout& layout, uint32_t variableCount) {
        if (!device_) throw std::runtime_error("DescriptorAllocator: not initialized");
        VkDescriptorSetLayout l = layout.handle();
        if (!l) throw std::invalid_argument("DescriptorAllocator::allocate: layout is null");

        PoolBucket& bucket = bucketForLayout(layout);
        VkDescriptorSet set = VK_NULL_HANDLE;

        // variable count chain (optional)
        VkDescriptorSetVariableDescriptorCountAllocateInfo varInfo{};
        varInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        varInfo.descriptorSetCount = 1;
        varInfo.pDescriptorCounts = &variableCount;

        // Try allocate, grow pools if needed
        for (int attempts = 0; attempts < 2; ++attempts) {
            VkDescriptorPool pool = currentPool(bucket);

            VkDescriptorSetAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ai.descriptorPool = pool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts = &l;

            if (layout.hasVariableCountBinding())
                ai.pNext = &varInfo;

            VkResult r = vkAllocateDescriptorSets(device_->device(), &ai, &set);
            if (r == VK_SUCCESS) return set;

            // Pool exhausted/fragmented: create a new pool and retry once
            if (r == VK_ERROR_OUT_OF_POOL_MEMORY || r == VK_ERROR_FRAGMENTED_POOL) {
                bucket.nextPoolIndex = static_cast<uint32_t>(bucket.pools.size()); // push at end
                newPool(bucket);
                continue;
            }
            // Other errors are fatal
            VK_CHECK(r);
        }
        // Should never reach here
        return set;
    }

    void DescriptorAllocator::resetEpoch() {
        auto reset_bucket = [&](PoolBucket& b) {
            for (auto p : b.pools) vkResetDescriptorPool(device_->device(), p, 0);
            b.nextPoolIndex = 0;
            };
        reset_bucket(regular_);
        reset_bucket(updateAfterBind_);
    }

    void DescriptorAllocator::trim() {

    }

    // ================================ DescriptorWrites ===========================

    DescriptorWrites& DescriptorWrites::writeBuffer(VkDescriptorSet set, uint32_t binding,
        VkDescriptorType type,
        VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range,
        uint32_t arrayElement) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = buffer; bi.offset = offset; bi.range = range;
        bufferInfos_.push_back(bi);

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = set;
        w.dstBinding = binding;
        w.dstArrayElement = arrayElement;
        w.descriptorType = type;
        w.descriptorCount = 1;
        w.pBufferInfo = &bufferInfos_.back();

        writes_.push_back(w);
        return *this;
    }

    DescriptorWrites& DescriptorWrites::writeImage(VkDescriptorSet set, uint32_t binding,
        VkDescriptorType type,
        VkImageView view, VkImageLayout layout,
        VkSampler sampler,
        uint32_t arrayElement) {
        VkDescriptorImageInfo ii{};
        ii.imageView = view; ii.imageLayout = layout; ii.sampler = sampler;
        imageInfos_.push_back(ii);

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = set;
        w.dstBinding = binding;
        w.dstArrayElement = arrayElement;
        w.descriptorType = type;
        w.descriptorCount = 1;
        w.pImageInfo = &imageInfos_.back();

        writes_.push_back(w);
        return *this;
    }

    DescriptorWrites& DescriptorWrites::writeTexel(VkDescriptorSet set, uint32_t binding,
        VkDescriptorType type,
        VkBufferView view,
        uint32_t arrayElement) {
        texelViews_.push_back(view);

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = set;
        w.dstBinding = binding;
        w.dstArrayElement = arrayElement;
        w.descriptorType = type;
        w.descriptorCount = 1;
        w.pTexelBufferView = &texelViews_.back();

        writes_.push_back(w);
        return *this;
    }

    void DescriptorWrites::commit(VkDevice device) {
        if (writes_.empty()) return;
        vkUpdateDescriptorSets(device,
            static_cast<uint32_t>(writes_.size()), writes_.data(),
            0, nullptr);
        // clear for reuse
        writes_.clear();
        imageInfos_.clear();
        bufferInfos_.clear();
        texelViews_.clear();
    }

    void DescriptorWrites::clear() {
        writes_.clear();
        imageInfos_.clear();
        bufferInfos_.clear();
        texelViews_.clear();
    }

    // ================================= BindlessSet ===============================

    BindlessSet::BindlessSet(const BindlessSetCreateInfo& ci)
        : device_(ci.device)
        , allocator_({ ci.device, 2048, {}, /*enableUAB*/ true, ci.debugNamePool })
        , capacity_(ci.maxTextures)
    {
        if (!device_) throw std::invalid_argument("BindlessSet: device is null");
        if (capacity_ == 0) throw std::invalid_argument("BindlessSet: capacity is 0");

        // binding 0: combined image sampler array
        VkDescriptorSetLayoutBinding b{};
        b.binding = 0;
        b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount = capacity_;
        b.stageFlags = ci.visibleStages;
        b.pImmutableSamplers = ci.immutableSampler ? ci.immutableSampler : nullptr;

        std::vector<VkDescriptorSetLayoutBinding> bindings = { b };

        // per-binding flags
        VkDescriptorBindingFlags f =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
            | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
            | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

        std::vector<VkDescriptorBindingFlags> flags = { f };

        DescriptorSetLayoutCreateInfo lci{};
        lci.device = device_;
        lci.bindings = std::move(bindings);
        lci.bindingFlags = std::move(flags);
        lci.updateAfterBindPool = true;
        lci.debugName = ci.debugNameLayout;

        layout_ = DescriptorSetLayout{ lci };

        // allocate one set with variable descriptor count = capacity_
        set_ = allocator_.allocate(layout_, capacity_);
        if (!set_) throw std::runtime_error("BindlessSet: failed to allocate set");

        if (!ci.debugNameSet.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET,
                reinterpret_cast<uint64_t>(set_), ci.debugNameSet);
        }
    }

    BindlessSet::~BindlessSet() { destroy(); }

    void BindlessSet::destroy() {
        // Descriptor sets are freed when pool resets/destroys; nothing to do.
        set_ = VK_NULL_HANDLE;
        capacity_ = 0;
        layout_ = DescriptorSetLayout{}; // RAII destroy
        allocator_ = DescriptorAllocator{}; // RAII destroy
        device_ = nullptr;
    }

    void BindlessSet::move_from(BindlessSet&& o) noexcept {
        device_ = o.device_;   o.device_ = nullptr;
        layout_ = std::move(o.layout_);
        allocator_ = std::move(o.allocator_);
        set_ = o.set_;      o.set_ = VK_NULL_HANDLE;
        capacity_ = o.capacity_; o.capacity_ = 0;
    }

    void BindlessSet::updateTexture(uint32_t index, VkImageView view, VkImageLayout layout, VkSampler sampler) {
        if (index >= capacity_) throw std::out_of_range("BindlessSet::updateTexture index out of range");
        DescriptorWrites w;
        w.writeImage(set_, /*binding*/0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            view, layout, sampler, /*arrayElement*/ index)
            .commit(device_->device());
    }

} // namespace hvk
