/**
 * @file hvk_gpu_resources.h
 * @brief GPU buffer and image resource management
 * @author Holy Vulkan Engine
 * @date 2025
 * RAII wrappers for VkBuffer and VkImage with VMA memory allocation,
 * helper utilities, and descriptor info generation.
 */

#ifndef HVK_GPU_RESOURCES_H
#define HVK_GPU_RESOURCES_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>
#include <cassert>
#include <algorithm>

#include "hvk_device.h"
#include "hvk_raii.hpp"

#if !ENGINE_USE_VMA
#error "GpuBuffer/GpuImage require VMA (ENGINE_USE_VMA=1)."
#endif
#include "vk_mem_alloc.h"

namespace hvk {

    // ----------------------------- helpers --------------------------------------

    constexpr uint32_t hvk_calc_mip_count(uint32_t w, uint32_t h, uint32_t d = 1) {
        const uint32_t m = (std::max)({ w, h, d });
        uint32_t levels = 0, t = m;
        while (t > 0) { t >>= 1; ++levels; }
        return (std::max)(1u, levels);
    }

    constexpr VkImageSubresourceRange hvk_full_range(VkImageAspectFlags aspect,
        uint32_t levels, uint32_t layers,
        uint32_t baseMip = 0, uint32_t baseLayer = 0) {
        VkImageSubresourceRange r{};
        r.aspectMask = aspect;
        r.baseMipLevel = baseMip;
        r.levelCount = levels - baseMip;
        r.baseArrayLayer = baseLayer;
        r.layerCount = layers - baseLayer;
        return r;
    }

    constexpr VkImageAspectFlags hvk_aspect_from_format(VkFormat fmt) {
        switch (fmt) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    // ------------------------------ GpuBuffer -----------------------------------

    struct GpuBufferCreateInfo {
        const Device* device = nullptr;   // required
        VkDeviceSize  size = 0;         // required
        VkBufferUsageFlags usage = 0;     // required (VERTEX/INDEX/UNIFORM/STORAGE/TRANSFER/BDA...)
        // VMA hints (good defaults below)
        VmaAllocationCreateFlags allocFlags =
            VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT; // +HOST_ACCESS bits if host-visible
        VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO; // AUTO prefers device-local
        bool persistentMap = false;                      // map at creation (if host-visible)
        std::string debugName{};
    };

    class GpuBuffer : public VulkanResource<GpuBuffer> {
        friend class VulkanResource<GpuBuffer>;
    public:
        GpuBuffer() = default;
        explicit GpuBuffer(const GpuBufferCreateInfo& ci) { create(ci); }

        // Move operations (explicit to properly handle CRTP)
        GpuBuffer(GpuBuffer&& o) noexcept { move_from(std::move(o)); }
        GpuBuffer& operator=(GpuBuffer&& o) noexcept {
            if (this != &o) { destroy(); move_from(std::move(o)); }
            return *this;
        }

        void create(const GpuBufferCreateInfo& ci);

        // info
        [[nodiscard]] constexpr VkBuffer      handle() const noexcept { return buf_; }
        [[nodiscard]] constexpr VmaAllocation allocation() const noexcept { return alloc_; }
        [[nodiscard]] constexpr VkDeviceSize  size() const noexcept { return size_; }
        [[nodiscard]] constexpr VkBufferUsageFlags usage() const noexcept { return usage_; }
        [[nodiscard]] constexpr const Device* device() const noexcept { return device_; }

        // map/unmap (persistent suggested)
        void* map();                 // returns persisted pointer if already mapped
        void  unmap();               // keeps valid even if called multiple times
        [[nodiscard]] constexpr void* mapped() const noexcept { return mapped_; }

        // flush/invalidate ranges (safe on coherent = no-op in VMA)
        void flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) const;
        void invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) const;

        // descriptor helper
        [[nodiscard]] inline constexpr VkDescriptorBufferInfo descriptor(VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE) const noexcept {
            return VkDescriptorBufferInfo{ buf_, offset, range };
        }

        // device address (0 if BDA not enabled/usage missing)
        VkDeviceAddress deviceAddress() const;

    private:
        // RAII implementation (called by VulkanResource<GpuBuffer>)
        void destroy();
        void move_from(GpuBuffer&& o) noexcept;
        const Device* device_ = nullptr;
        VkBuffer       buf_ = VK_NULL_HANDLE;
        VmaAllocation  alloc_ = VK_NULL_HANDLE;
        VkDeviceSize   size_ = 0;
        VkBufferUsageFlags usage_ = 0;
        void* mapped_ = nullptr;
    };

    // ------------------------------ ImageView -----------------------------------

    struct ImageViewCreateInfo {
        const Device* device = nullptr;   // required
        VkImage       image = VK_NULL_HANDLE; // required
        VkFormat      format = VK_FORMAT_UNDEFINED; // required
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageSubresourceRange range{ aspect, 0, 1, 0, 1 };
        std::string debugName{};
    };

    class ImageView : public VulkanResource<ImageView> {
        friend class VulkanResource<ImageView>;
    public:
        ImageView() = default;
        explicit ImageView(const ImageViewCreateInfo& ci) { create(ci); }

        // Move operations (explicit to properly handle CRTP)
        ImageView(ImageView&& o) noexcept { move_from(std::move(o)); }
        ImageView& operator=(ImageView&& o) noexcept {
            if (this != &o) { destroy(); move_from(std::move(o)); }
            return *this;
        }

        void create(const ImageViewCreateInfo& ci);

        VkImageView handle() const { return view_; }
        const Device* device() const { return device_; }

    private:
        // RAII implementation (called by VulkanResource<ImageView>)
        void destroy();
        void move_from(ImageView&& o) noexcept;
        const Device* device_ = nullptr;
        VkImageView   view_ = VK_NULL_HANDLE;
    };

    // ------------------------------ GpuImage ------------------------------------

    struct GpuImageCreateInfo {
        const Device* device = nullptr; // required
        VkFormat   format = VK_FORMAT_R8G8B8A8_UNORM;   // required
        uint32_t   width = 1;
        uint32_t   height = 1;
        uint32_t   depth = 1;                          // for 3D
        uint32_t   mipLevels = 1;                       // use hvk_calc_mip_count(...) for full chain
        uint32_t   arrayLayers = 1;                     // 6 for cube, 6*N for cube arrays
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageType  type = VK_IMAGE_TYPE_2D;
        VkImageUsageFlags usage =
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;            // good defaults (sampling + upload + blit)
        VkImageCreateFlags flags = 0;                   // e.g., CUBE_COMPATIBLE
        // VMA hints
        VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VmaAllocationCreateFlags allocFlags =
            VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
        std::string debugName{};
    };

    class GpuImage : public VulkanResource<GpuImage> {
        friend class VulkanResource<GpuImage>;
    public:
        GpuImage() = default;
        explicit GpuImage(const GpuImageCreateInfo& ci) { create(ci); }

        // Move operations (explicit to properly handle CRTP)
        GpuImage(GpuImage&& o) noexcept { move_from(std::move(o)); }
        GpuImage& operator=(GpuImage&& o) noexcept {
            if (this != &o) { destroy(); move_from(std::move(o)); }
            return *this;
        }

        void create(const GpuImageCreateInfo& ci);

        // info
        [[nodiscard]] constexpr VkImage     handle() const noexcept { return img_; }
        [[nodiscard]] constexpr VmaAllocation allocation() const noexcept { return alloc_; }
        [[nodiscard]] constexpr VkFormat    format() const noexcept { return format_; }
        [[nodiscard]] constexpr VkExtent3D  extent() const noexcept { return { width_, height_, depth_ }; }
        [[nodiscard]] constexpr uint32_t    width() const noexcept { return width_; }
        [[nodiscard]] constexpr uint32_t    height() const noexcept { return height_; }
        [[nodiscard]] constexpr uint32_t    depth() const noexcept { return depth_; }
        [[nodiscard]] constexpr uint32_t    mipLevels() const noexcept { return mipLevels_; }
        [[nodiscard]] constexpr uint32_t    arrayLayers() const noexcept { return arrayLayers_; }
        [[nodiscard]] constexpr VkSampleCountFlagBits samples() const noexcept { return samples_; }
        [[nodiscard]] constexpr const Device* device() const noexcept { return device_; }

        // helpers
        [[nodiscard]] inline constexpr VkImageSubresourceRange fullRange() const noexcept {
            return hvk_full_range(hvk_aspect_from_format(format_), mipLevels_, arrayLayers_);
        }

        // quick view maker (returns RAII ImageView)
        ImageView makeView(VkImageViewType type,
            VkImageAspectFlags aspect,
            VkImageSubresourceRange range,
            std::string debugName = {}) const
        {
            ImageViewCreateInfo vci{};
            vci.device = device_;
            vci.image = img_;
            vci.format = format_;
            vci.viewType = type;
            vci.aspect = aspect;
            vci.range = range;
            vci.debugName = std::move(debugName);
            return ImageView{ vci };
        }

    private:
        // RAII implementation (called by VulkanResource<GpuImage>)
        void destroy();
        void move_from(GpuImage&& o) noexcept;
        const Device* device_ = nullptr;
        VkImage       img_ = VK_NULL_HANDLE;
        VmaAllocation alloc_ = VK_NULL_HANDLE;
        VkFormat      format_ = VK_FORMAT_UNDEFINED;
        uint32_t      width_ = 0, height_ = 0, depth_ = 1;
        uint32_t      mipLevels_ = 1, arrayLayers_ = 1;
        VkSampleCountFlagBits samples_ = VK_SAMPLE_COUNT_1_BIT;
    };

} // namespace hvk

#endif // HVK_GPU_RESOURCES_H
