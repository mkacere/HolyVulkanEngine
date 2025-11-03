/**
 * @file hvk_sampler_cache.h
 * @brief Texture sampler caching and creation
 * @author Holy Vulkan Engine
 * @date 2025
 * Deduplicates and caches VkSampler objects based on filtering, addressing,
 * and mipmap settings with thread-safe access.
 */

#ifndef HVK_SAMPLER_CACHE_H
#define HVK_SAMPLER_CACHE_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace hvk {

    class Device; // fwd

    // Minimal description → canonicalized → hashed → cached sampler
    struct SamplerDesc {
        VkFilter             magFilter = VK_FILTER_LINEAR;
        VkFilter             minFilter = VK_FILTER_LINEAR;
        VkSamplerMipmapMode  mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        float                mipLodBias = 0.0f;
        float                minLod = 0.0f;
        float                maxLod = 16.0f;

        float                maxAnisotropy = 1.0f; // >1 enables anisotropy (if supported)

        VkBool32             compareEnable = VK_FALSE;
        VkCompareOp          compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkBorderColor        borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        VkBool32             unnormalizedCoordinates = VK_FALSE;

        // Optional: reduction mode (min/max filter, else weighted average)
        VkSamplerReductionMode reduction = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

        // Optional sampler creation flags (e.g., NON_SEAMLESS_CUBE_MAP_BIT_EXT)
        VkSamplerCreateFlags  flags = 0;

        // helpers
        static SamplerDesc linearRepeat();
        static SamplerDesc linearClamp();
        static SamplerDesc nearestRepeat();
        static SamplerDesc nearestClamp();
        static SamplerDesc linearMipLinearRepeat();
        static SamplerDesc shadowClampToBorderWhite(); // compare sampler for shadow maps
        static SamplerDesc linearClampToBorder(VkBorderColor bc);
        static SamplerDesc minReductionRepeat(); // min filter reduction sampler
    };

    // Hash & equality for map keys
    struct SamplerDescHash {
        size_t operator()(const SamplerDesc& d) const noexcept;
    };
    struct SamplerDescEq {
        bool operator()(const SamplerDesc& a, const SamplerDesc& b) const noexcept;
    };

    // Cache: get() returns a deduped VkSampler. Clear() destroys all samplers.
    class SamplerCache {
    public:
        SamplerCache() = default;
        explicit SamplerCache(const Device* device, std::string_view debugBaseName = {});
        ~SamplerCache();

        SamplerCache(const SamplerCache&) = delete;
        SamplerCache& operator=(const SamplerCache&) = delete;

        SamplerCache(SamplerCache&&) noexcept;
        SamplerCache& operator=(SamplerCache&&) noexcept;

        // Get or create a sampler matching 'desc'
        VkSampler get(const SamplerDesc& desc);

        // Destroy all cached samplers
        void clear();

        size_t size() const;

    private:
        void destroy();
        VkSampler createSampler(const SamplerDesc& desc, uint32_t ordinal);

    private:
        const Device* device_ = nullptr;
        std::string   debugBase_;
        std::unordered_map<SamplerDesc, VkSampler, SamplerDescHash, SamplerDescEq> map_;
        mutable std::mutex mtx_;
        uint32_t ordinal_ = 0; // for debug names
    };

} // namespace hvk

// Provide std::hash adapter if you prefer unordered_map with default hash
namespace std {
    template<> struct hash<hvk::SamplerDesc> {
        size_t operator()(const hvk::SamplerDesc& d) const noexcept {
            return hvk::SamplerDescHash{}(d);
        }
    };
}

#endif // HVK_SAMPLER_CACHE_H
