#include "pch.h"

#include <hvk/gfx/hvk_gpu_resources.h>

#ifndef VK_CHECK
#define VK_CHECK(x) do { VkResult _e = (x); if (_e != VK_SUCCESS) throw std::runtime_error("Vulkan error: " #x); } while(0)
#endif

namespace hvk {

    // ============================== GpuBuffer ===================================

    void GpuBuffer::create(const GpuBufferCreateInfo& ci) {
        if (!ci.device || ci.size == 0 || ci.usage == 0)
            throw std::invalid_argument("GpuBuffer::create: invalid args");

        destroy();

        device_ = ci.device;
        size_ = ci.size;
        usage_ = ci.usage;

        VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bi.size = size_;
        bi.usage = usage_;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo aci{};
        aci.flags = ci.allocFlags;
        aci.usage = ci.memUsage;

        // Hint host access if a client wants persistent mapping
        if (ci.persistentMap) {
            aci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VmaAllocationInfo ainfo{};
        VK_CHECK(vmaCreateBuffer(device_->allocator(), &bi, &aci, &buf_, &alloc_, &ainfo));
        mapped_ = ainfo.pMappedData; // non-null only if HOST_ACCESS + MAPPED were used

        if (!ci.debugName.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_BUFFER,
                reinterpret_cast<uint64_t>(buf_), ci.debugName);
        }
    }

    void GpuBuffer::destroy() {
        if (device_ && buf_) {
            vmaDestroyBuffer(device_->allocator(), buf_, alloc_);
        }
        buf_ = VK_NULL_HANDLE;
        alloc_ = VK_NULL_HANDLE;
        mapped_ = nullptr;
        device_ = nullptr;
        size_ = 0;
        usage_ = 0;
    }

    void* GpuBuffer::map() {
        if (mapped_) return mapped_;
        if (!device_ || !alloc_) throw std::runtime_error("GpuBuffer::map: not created");
        void* p = nullptr;
        VK_CHECK(vmaMapMemory(device_->allocator(), alloc_, &p));
        mapped_ = p;
        return mapped_;
    }

    void GpuBuffer::unmap() {
        if (!device_ || !alloc_ || !mapped_) return;
        vmaUnmapMemory(device_->allocator(), alloc_);
        mapped_ = nullptr;
    }

    void GpuBuffer::flush(VkDeviceSize offset, VkDeviceSize size) const {
        if (!device_ || !alloc_) return;
        // VMA treats flush as no-op for HOST_COHERENT memory; safe to call unconditionally.
        VK_CHECK(vmaFlushAllocation(device_->allocator(), alloc_, offset, size));
    }

    void GpuBuffer::invalidate(VkDeviceSize offset, VkDeviceSize size) const {
        if (!device_ || !alloc_) return;
        VK_CHECK(vmaInvalidateAllocation(device_->allocator(), alloc_, offset, size));
    }

    VkDeviceAddress GpuBuffer::deviceAddress() const {
        if (!device_ || !buf_) return 0;
        if ((usage_ & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == 0) return 0;
        VkBufferDeviceAddressInfo ai{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        ai.buffer = buf_;
        return vkGetBufferDeviceAddress(device_->device(), &ai);
    }

    void GpuBuffer::move_from(GpuBuffer&& o) noexcept {
        device_ = o.device_;   o.device_ = nullptr;
        buf_ = o.buf_;      o.buf_ = VK_NULL_HANDLE;
        alloc_ = o.alloc_;    o.alloc_ = VK_NULL_HANDLE;
        size_ = o.size_;     o.size_ = 0;
        usage_ = o.usage_;    o.usage_ = 0;
        mapped_ = o.mapped_;   o.mapped_ = nullptr;
    }

    // ============================== ImageView ===================================

    void ImageView::create(const ImageViewCreateInfo& ci) {
        if (!ci.device || !ci.image || ci.format == VK_FORMAT_UNDEFINED)
            throw std::invalid_argument("ImageView::create: invalid args");
        destroy();

        device_ = ci.device;

        VkImageViewCreateInfo vi{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vi.image = ci.image;
        vi.viewType = ci.viewType;
        vi.format = ci.format;
        vi.subresourceRange = ci.range;
        // Common swizzles (identity)
        vi.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };

        VK_CHECK(vkCreateImageView(device_->device(), &vi, nullptr, &view_));

        if (!ci.debugName.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_IMAGE_VIEW,
                reinterpret_cast<uint64_t>(view_), ci.debugName);
        }
    }

    void ImageView::destroy() {
        if (device_ && view_) {
            vkDestroyImageView(device_->device(), view_, nullptr);
        }
        view_ = VK_NULL_HANDLE;
        device_ = nullptr;
    }

    void ImageView::move_from(ImageView&& o) noexcept {
        device_ = o.device_; o.device_ = nullptr;
        view_ = o.view_;   o.view_ = VK_NULL_HANDLE;
    }

    // =============================== GpuImage ===================================

    void GpuImage::create(const GpuImageCreateInfo& ci) {
        if (!ci.device || ci.width == 0 || ci.height == 0)
            throw std::invalid_argument("GpuImage::create: invalid args");

        destroy();

        device_ = ci.device;
        format_ = ci.format;
        width_ = ci.width;
        height_ = ci.height;
        depth_ = ci.depth;
        mipLevels_ = std::max(1u, ci.mipLevels);
        arrayLayers_ = std::max(1u, ci.arrayLayers);
        samples_ = ci.samples;

        VkImageCreateInfo ii{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ii.flags = ci.flags;
        ii.imageType = ci.type;
        ii.format = ci.format;
        ii.extent = { ci.width, ci.height, ci.depth };
        ii.mipLevels = mipLevels_;
        ii.arrayLayers = arrayLayers_;
        ii.samples = ci.samples;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.usage = ci.usage;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo aci{};
        aci.usage = ci.memUsage;
        aci.flags = ci.allocFlags;

        VmaAllocationInfo ainfo{};
        VK_CHECK(vmaCreateImage(device_->allocator(), &ii, &aci, &img_, &alloc_, &ainfo));

        if (!ci.debugName.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_IMAGE,
                reinterpret_cast<uint64_t>(img_), ci.debugName);
        }
    }

    void GpuImage::destroy() {
        if (device_ && img_) {
            vmaDestroyImage(device_->allocator(), img_, alloc_);
        }
        img_ = VK_NULL_HANDLE;
        alloc_ = VK_NULL_HANDLE;
        device_ = nullptr;
        format_ = VK_FORMAT_UNDEFINED;
        width_ = height_ = 0; depth_ = 1;
        mipLevels_ = arrayLayers_ = 1;
        samples_ = VK_SAMPLE_COUNT_1_BIT;
    }

    void GpuImage::move_from(GpuImage&& o) noexcept {
        device_ = o.device_; o.device_ = nullptr;
        img_ = o.img_;    o.img_ = VK_NULL_HANDLE;
        alloc_ = o.alloc_;  o.alloc_ = VK_NULL_HANDLE;
        format_ = o.format_; o.format_ = VK_FORMAT_UNDEFINED;
        width_ = o.width_;  o.width_ = 0;
        height_ = o.height_; o.height_ = 0;
        depth_ = o.depth_;  o.depth_ = 1;
        mipLevels_ = o.mipLevels_;   o.mipLevels_ = 1;
        arrayLayers_ = o.arrayLayers_; o.arrayLayers_ = 1;
        samples_ = o.samples_;     o.samples_ = VK_SAMPLE_COUNT_1_BIT;
    }

} // namespace hvk
