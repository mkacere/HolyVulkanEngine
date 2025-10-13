#include "pch.h"

#include <hvk/gfx/hvk_pipeline_layout_cache.h>
#include <hvk/gfx/hvk_device.h>

namespace hvk {

    static inline void hash_combine(size_t& h, size_t v) {
        // boost-like combine
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }

    static inline size_t hash_bytes(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        size_t h = 1469598103934665603ull; // FNV-1a 64
        for (size_t i = 0; i < n; ++i) {
            h ^= b[i];
            h *= 1099511628211ull;
        }
        return h;
    }

    PipelineLayoutCache::PipelineLayoutCache(const Device* device, std::string_view debugBaseName)
        : device_(device), debugBase_(debugBaseName.empty() ? "pl_cache" : std::string(debugBaseName)) {
        if (!device_) throw std::invalid_argument("PipelineLayoutCache: device is null");
    }

    PipelineLayoutCache::~PipelineLayoutCache() { destroy(); }

    PipelineLayoutCache::PipelineLayoutCache(PipelineLayoutCache&& o) noexcept {
        device_ = o.device_; o.device_ = nullptr;
        cache_ = std::move(o.cache_);
        debugBase_ = std::move(o.debugBase_);
    }

    PipelineLayoutCache& PipelineLayoutCache::operator=(PipelineLayoutCache&& o) noexcept {
        if (this != &o) {
            destroy();
            device_ = o.device_; o.device_ = nullptr;
            cache_ = std::move(o.cache_);
            debugBase_ = std::move(o.debugBase_);
        }
        return *this;
    }

    bool PipelineLayoutCache::Key::operator==(const Key& o) const {
        if (setLayouts.size() != o.setLayouts.size()) return false;
        if (push.size() != o.push.size()) return false;
        for (size_t i = 0; i < setLayouts.size(); ++i)
            if (setLayouts[i] != o.setLayouts[i]) return false;
        for (size_t i = 0; i < push.size(); ++i) {
            if (push[i].stageFlags != o.push[i].stageFlags ||
                push[i].offset != o.push[i].offset ||
                push[i].size != o.push[i].size) return false;
        }
        return true;
    }

    size_t PipelineLayoutCache::KeyHasher::operator()(const Key& k) const noexcept {
        size_t h = 0;
        hash_combine(h, std::hash<size_t>{}(k.setLayouts.size()));
        for (auto v : k.setLayouts) hash_combine(h, std::hash<uint64_t>{}(v));
        hash_combine(h, std::hash<size_t>{}(k.push.size()));
        for (const auto& r : k.push) {
            hash_combine(h, std::hash<uint32_t>{}(r.stageFlags));
            hash_combine(h, std::hash<uint32_t>{}(r.offset));
            hash_combine(h, std::hash<uint32_t>{}(r.size));
        }
        return h;
    }

    VkPipelineLayout PipelineLayoutCache::get(const PipelineLayoutDesc& desc) {
        // Build key
        Key key;
        key.setLayouts.reserve(desc.setLayouts.size());
        for (auto l : desc.setLayouts)
            key.setLayouts.push_back(reinterpret_cast<uint64_t>(l));
        key.push = desc.pushConstants;

        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;

        // Create layout
        VkPipelineLayoutCreateInfo ci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        ci.setLayoutCount = static_cast<uint32_t>(desc.setLayouts.size());
        ci.pSetLayouts = desc.setLayouts.data();
        ci.pushConstantRangeCount = static_cast<uint32_t>(desc.pushConstants.size());
        ci.pPushConstantRanges = desc.pushConstants.data();

        VkPipelineLayout layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreatePipelineLayout(device_->device(), &ci, nullptr, &layout));

        // Name it
        if (!debugBase_.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                reinterpret_cast<uint64_t>(layout), debugBase_ + "/layout#" + std::to_string(cache_.size()));
        }

        cache_.emplace(std::move(key), layout);
        return layout;
    }

    void PipelineLayoutCache::clear() {
        if (!device_) return;
        for (auto& kv : cache_) {
            vkDestroyPipelineLayout(device_->device(), kv.second, nullptr);
        }
        cache_.clear();
    }

    void PipelineLayoutCache::destroy() { clear(); }

} // namespace hvk
