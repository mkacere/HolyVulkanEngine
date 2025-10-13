#include "pch.h"

#include <hvk/gfx/hvk_sampler_cache.h>
#include "hvk/gfx/hvk_device.h"

namespace hvk {

    // ---------- SamplerDesc presets ----------

    SamplerDesc SamplerDesc::linearRepeat() {
        SamplerDesc d;
        d.magFilter = d.minFilter = VK_FILTER_LINEAR;
        d.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        d.addressModeU = d.addressModeV = d.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        d.maxLod = 16.f;
        d.maxAnisotropy = 1.f;
        return d;
    }
    SamplerDesc SamplerDesc::linearClamp() {
        SamplerDesc d = linearRepeat();
        d.addressModeU = d.addressModeV = d.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        return d;
    }
    SamplerDesc SamplerDesc::nearestRepeat() {
        SamplerDesc d;
        d.magFilter = d.minFilter = VK_FILTER_NEAREST;
        d.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        d.addressModeU = d.addressModeV = d.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        d.maxLod = 0.f;
        return d;
    }
    SamplerDesc SamplerDesc::nearestClamp() {
        SamplerDesc d = nearestRepeat();
        d.addressModeU = d.addressModeV = d.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        return d;
    }
    SamplerDesc SamplerDesc::linearMipLinearRepeat() {
        SamplerDesc d = linearRepeat();
        d.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        d.minLod = 0.f; d.maxLod = 16.f;
        return d;
    }
    SamplerDesc SamplerDesc::shadowClampToBorderWhite() {
        SamplerDesc d{};
        d.magFilter = d.minFilter = VK_FILTER_LINEAR;
        d.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        d.addressModeU = d.addressModeV = d.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        d.compareEnable = VK_TRUE;
        d.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // typical for shadow maps
        d.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // avoid dark fringes on PCF
        d.maxLod = 16.f;
        return d;
    }
    SamplerDesc SamplerDesc::linearClampToBorder(VkBorderColor bc) {
        SamplerDesc d = linearClamp();
        d.addressModeU = d.addressModeV = d.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        d.borderColor = bc;
        return d;
    }
    SamplerDesc SamplerDesc::minReductionRepeat() {
        SamplerDesc d = linearRepeat();
        d.reduction = VK_SAMPLER_REDUCTION_MODE_MIN;
        return d;
    }

    // ---------- Hash / Eq ----------

    static inline void hash_combine(size_t& h, size_t v) {
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }

    size_t SamplerDescHash::operator()(const SamplerDesc& d) const noexcept {
        size_t h = 0;
        hash_combine(h, std::hash<uint32_t>{}(d.magFilter));
        hash_combine(h, std::hash<uint32_t>{}(d.minFilter));
        hash_combine(h, std::hash<uint32_t>{}(d.mipmapMode));
        hash_combine(h, std::hash<uint32_t>{}(d.addressModeU));
        hash_combine(h, std::hash<uint32_t>{}(d.addressModeV));
        hash_combine(h, std::hash<uint32_t>{}(d.addressModeW));
        hash_combine(h, std::hash<int>{}(static_cast<int>(d.compareEnable)));
        hash_combine(h, std::hash<uint32_t>{}(d.compareOp));
        hash_combine(h, std::hash<uint32_t>{}(d.borderColor));
        hash_combine(h, std::hash<int>{}(static_cast<int>(d.unnormalizedCoordinates)));
        hash_combine(h, std::hash<uint32_t>{}(d.reduction));
        hash_combine(h, std::hash<uint32_t>{}(d.flags));

        // floats: bit-hash
        auto hfloat = [](float f) -> size_t {
            uint32_t u; std::memcpy(&u, &f, sizeof(float)); return std::hash<uint32_t>{}(u);
            };
        hash_combine(h, hfloat(d.mipLodBias));
        hash_combine(h, hfloat(d.minLod));
        hash_combine(h, hfloat(d.maxLod));
        hash_combine(h, hfloat(d.maxAnisotropy));
        return h;
    }

    bool SamplerDescEq::operator()(const SamplerDesc& a, const SamplerDesc& b) const noexcept {
        return a.magFilter == b.magFilter &&
            a.minFilter == b.minFilter &&
            a.mipmapMode == b.mipmapMode &&
            a.addressModeU == b.addressModeU &&
            a.addressModeV == b.addressModeV &&
            a.addressModeW == b.addressModeW &&
            a.mipLodBias == b.mipLodBias &&
            a.minLod == b.minLod &&
            a.maxLod == b.maxLod &&
            a.maxAnisotropy == b.maxAnisotropy &&
            a.compareEnable == b.compareEnable &&
            a.compareOp == b.compareOp &&
            a.borderColor == b.borderColor &&
            a.unnormalizedCoordinates == b.unnormalizedCoordinates &&
            a.reduction == b.reduction &&
            a.flags == b.flags;
    }

    // ---------- SamplerCache ----------

    SamplerCache::SamplerCache(const Device* device, std::string_view debugBaseName)
        : device_(device)
        , debugBase_(debugBaseName.empty() ? "samplers" : std::string(debugBaseName))
    {
        if (!device_) throw std::invalid_argument("SamplerCache: device is null");
    }

    SamplerCache::~SamplerCache() { destroy(); }

    SamplerCache::SamplerCache(SamplerCache&& o) noexcept {
        std::scoped_lock lk(o.mtx_);
        device_ = o.device_; o.device_ = nullptr;
        debugBase_ = std::move(o.debugBase_);
        map_ = std::move(o.map_);
        ordinal_ = o.ordinal_; o.ordinal_ = 0;
    }

    SamplerCache& SamplerCache::operator=(SamplerCache&& o) noexcept {
        if (this == &o) return *this;
        destroy();
        std::scoped_lock lk(mtx_, o.mtx_);
        device_ = o.device_; o.device_ = nullptr;
        debugBase_ = std::move(o.debugBase_);
        map_ = std::move(o.map_);
        ordinal_ = o.ordinal_; o.ordinal_ = 0;
        return *this;
    }

    void SamplerCache::destroy() {
        if (!device_) return;
        std::scoped_lock lk(mtx_);
        for (auto& kv : map_) {
            if (kv.second) vkDestroySampler(device_->device(), kv.second, nullptr);
        }
        map_.clear();
        device_ = nullptr;
    }

    size_t SamplerCache::size() const {
        std::scoped_lock lk(mtx_);
        return map_.size();
    }

    VkSampler SamplerCache::get(const SamplerDesc& inDesc) {
        // Canonicalize a copy (clamp to device features/limits)
        SamplerDesc d = inDesc;

        const auto& lim = device_->limits();

        // anisotropy
        const bool anisoSupported = device_->features10().samplerAnisotropy == VK_TRUE;
        if (!anisoSupported) {
            d.maxAnisotropy = 1.0f;
        }
        else {
            if (d.maxAnisotropy < 1.0f) d.maxAnisotropy = 1.0f;
            if (d.maxAnisotropy > lim.maxSamplerAnisotropy) d.maxAnisotropy = lim.maxSamplerAnisotropy;
        }

        // unnormalized coordinates restrictions (spec): force safe settings if requested
        if (d.unnormalizedCoordinates) {
            // Only valid with: no mipmapping, nearest filters, clamp to edge, no anisotropy, compare off
            d.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            d.minFilter = d.magFilter = VK_FILTER_NEAREST;
            d.addressModeU = d.addressModeV = d.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            d.maxAnisotropy = 1.0f;
            d.compareEnable = VK_FALSE;
            d.minLod = 0.0f; d.maxLod = 0.0f; d.mipLodBias = 0.0f;
        }

        // hash / lookup
        {
            std::scoped_lock lk(mtx_);
            auto it = map_.find(d);
            if (it != map_.end()) return it->second;
        }

        // create & insert
        VkSampler sampler = createSampler(d, ordinal_++);

        std::scoped_lock lk(mtx_);
        auto [it, inserted] = map_.emplace(d, sampler);
        if (!inserted) {
            // Rare race: another thread created the same sampler in the gap — destroy ours, return theirs
            vkDestroySampler(device_->device(), sampler, nullptr);
            return it->second;
        }
        return sampler;
    }

    VkSampler SamplerCache::createSampler(const SamplerDesc& d, uint32_t ordinal) {
        VkSamplerCreateInfo ci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        ci.flags = d.flags;
        ci.magFilter = d.magFilter;
        ci.minFilter = d.minFilter;
        ci.mipmapMode = d.mipmapMode;
        ci.addressModeU = d.addressModeU;
        ci.addressModeV = d.addressModeV;
        ci.addressModeW = d.addressModeW;
        ci.mipLodBias = d.mipLodBias;
        ci.anisotropyEnable = (d.maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;
        ci.maxAnisotropy = d.maxAnisotropy;
        ci.compareEnable = d.compareEnable;
        ci.compareOp = d.compareOp;
        ci.minLod = d.minLod;
        ci.maxLod = d.maxLod;
        ci.borderColor = d.borderColor;
        ci.unnormalizedCoordinates = d.unnormalizedCoordinates;

        // reduction mode pNext (if not weighted average)
        VkSamplerReductionModeCreateInfo reduction{ VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO };
        if (d.reduction != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE) {
            reduction.reductionMode = d.reduction;
            ci.pNext = &reduction;
        }

        VkSampler sampler = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSampler(device_->device(), &ci, nullptr, &sampler));

        if (!debugBase_.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_SAMPLER,
                reinterpret_cast<uint64_t>(sampler),
                debugBase_ + "/#" + std::to_string(ordinal));
        }
        return sampler;
    }

} // namespace hvk
