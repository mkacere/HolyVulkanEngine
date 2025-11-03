/**
 * @file hvk_pipeline_layout_cache.h
 * @brief Pipeline layout caching and deduplication
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Caches VkPipelineLayout objects based on descriptor set layouts and push
 * constant ranges to avoid duplicate creation and improve performance.
 */

#ifndef HVK_PIPELINE_LAYOUT_CACHE_H
#define HVK_PIPELINE_LAYOUT_CACHE_H

#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <cstdint>

namespace hvk {

    class Device; // fwd

    /**
     * @struct PipelineLayoutDesc
     * @brief Describes a pipeline layout request for caching
     */
    struct PipelineLayoutDesc {
        std::vector<VkDescriptorSetLayout> setLayouts;       // order-sensitive
        std::vector<VkPushConstantRange>   pushConstants;    // optional
    };

    class PipelineLayoutCache {
    public:
        PipelineLayoutCache() = default;
        explicit PipelineLayoutCache(const Device* device, std::string_view debugBaseName = {});
        ~PipelineLayoutCache();

        PipelineLayoutCache(const PipelineLayoutCache&) = delete;
        PipelineLayoutCache& operator=(const PipelineLayoutCache&) = delete;

        PipelineLayoutCache(PipelineLayoutCache&&) noexcept;
        PipelineLayoutCache& operator=(PipelineLayoutCache&&) noexcept;

        // Get or create a VkPipelineLayout for the given description.
        VkPipelineLayout get(const PipelineLayoutDesc& desc);

        // Drop all cached layouts (call at shutdown or when device is idle).
        void clear();

        const Device* device() const { return device_; }

    private:
        struct Key {
            std::vector<uint64_t> setLayouts; // handles as u64
            std::vector<VkPushConstantRange> push;
            bool operator==(const Key& o) const;
        };

        struct KeyHasher {
            size_t operator()(const Key& k) const noexcept;
        };

        void destroy();

    private:
        const Device* device_ = nullptr;
        std::unordered_map<Key, VkPipelineLayout, KeyHasher> cache_;
        std::string debugBase_;
    };

} // namespace hvk

#endif // HVK_PIPELINE_LAYOUT_CACHE_H
