/**
 * @file hvk_dynamic_uniforms.hpp
 * @brief Dynamic uniform buffer management
 * @author Holy Vulkan Engine
 * @date 2025
 * Manages dynamic uniform buffer offsets for per-object data in rendering.
 */

#ifndef HVK_DYNAMIC_UNIFORMS_HPP
#define HVK_DYNAMIC_UNIFORMS_HPP

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <algorithm>

#include <hvk/gfx/hvk_device.h>

#if ENGINE_USE_VMA
#include "vk_mem_alloc.h"
#endif

namespace hvk {

    // --- small helpers -----------------------------------------------------------

    static inline VkDeviceSize hvk_round_up(VkDeviceSize v, VkDeviceSize a) {
        return (a ? (v + (a - 1)) & ~(a - 1) : v);
    }

    // A single per-draw slice from the dynamic UBO.
    struct DynSlice {
        uint32_t      index = 0;       // item index within the frame
        VkDeviceSize  offset = 0;      // byte offset = index * stride
        void* ptr = nullptr;   // CPU pointer to the start of the item
    };

    // Creation parameters for DynamicUniforms.
    struct DynamicUniformsCreateInfo {
        const Device* device = nullptr;     // required
        uint32_t      framesInFlight = 2;   // 23 recommended
        VkDeviceSize  itemSize = 0;         // size of one struct before alignment (required)
        uint32_t      itemsPerFrame = 1024; // initial capacity (grows on demand at beginFrame)
        bool          allowGrow = true;     // allow growth at beginFrame if expectedItems > capacity
        std::string   debugBaseName{};      // e.g. "dynubo"
    };

    // RAII, move-only dynamic uniform manager (per-frame host-visible UBO).
    class DynamicUniforms {
    public:
        DynamicUniforms() = default;
        explicit DynamicUniforms(const DynamicUniformsCreateInfo& ci) { init(ci); }
        ~DynamicUniforms() { destroy(); }

        DynamicUniforms(const DynamicUniforms&) = delete;
        DynamicUniforms& operator=(const DynamicUniforms&) = delete;

        DynamicUniforms(DynamicUniforms&& o) noexcept { move_from(std::move(o)); }
        DynamicUniforms& operator=(DynamicUniforms&& o) noexcept {
            if (this != &o) { destroy(); move_from(std::move(o)); } return *this;
        }

        // Initialize (call only once if default-constructed).
        void init(const DynamicUniformsCreateInfo& ci) {
            if (!ci.device) throw std::invalid_argument("DynamicUniforms: device is null");
            if (ci.itemSize == 0) throw std::invalid_argument("DynamicUniforms: itemSize is 0");

            device_ = ci.device;
            frames_ = ci.framesInFlight ? ci.framesInFlight : 2;

            VkDeviceSize align = device_->limits().minUniformBufferOffsetAlignment;
            stride_ = hvk_round_up(ci.itemSize, align);
            baseName_ = ci.debugBaseName.empty() ? std::string("dynubo") : ci.debugBaseName;

            perFrame_.resize(frames_);
            for (uint32_t i = 0; i < frames_; ++i) {
                create_frame_buffer(i, static_cast<VkDeviceSize>(ci.itemsPerFrame) * stride_);
            }

            allowGrow_ = ci.allowGrow;
        }

        // Begin a new frame slot: resets bump pointer; optionally ensures capacity for 'expectedItems'.
        void beginFrame(uint32_t frameIndex, uint32_t expectedItems = 0) {
            if (frames_ == 0) return;
            cur_ = frameIndex % frames_;

            PerFrame& pf = perFrame_[cur_];
            // Grow (only at frame begin) if caller expects more than we can hold
            if (expectedItems > 0) {
                VkDeviceSize needed = static_cast<VkDeviceSize>(expectedItems) * stride_;
                if (needed > pf.size) {
                    if (!allowGrow_) {
                        throw std::runtime_error("DynamicUniforms: capacity exceeded and allowGrow==false");
                    }
                    recreate_frame_buffer(cur_, (std::max)(needed, pf.size * 2)); // grow geometrically
                }
            }
            pf.headIndex = 0;
            // No need to memset mapped memory; user will write all used regions.
        }

        // Allocate 'count' consecutive items; returns first slice.
        // (Use count>1 when you want a contiguous block; you can index i..i+count-1).
        DynSlice allocate(uint32_t count = 1) {
            assert(count > 0);
            PerFrame& pf = perFrame_[cur_];
            if (pf.headIndex + count > capacityItems(cur_)) {
                // Out of space in the current frame buffer (we only allow growth at beginFrame)
                throw std::runtime_error("DynamicUniforms: allocate() exceeds current frame capacity. "
                    "Call beginFrame(..., expectedItems) to grow.");
            }
            uint32_t idx = pf.headIndex;
            pf.headIndex += count;

            DynSlice s;
            s.index = idx;
            s.offset = static_cast<VkDeviceSize>(idx) * stride_;
            s.ptr = static_cast<uint8_t*>(pf.mapped) + s.offset;
            return s;
        }

        // Write helper: copies 'value' into a newly allocated item (T must fit itemSize).
        template<typename T>
        DynSlice push(const T& value) {
            static_assert(!std::is_pointer<T>::value, "push() expects a POD/struct, not a pointer");
            if (sizeof(T) > stride_) throw std::runtime_error("DynamicUniforms::push<T>: T larger than stride");
            DynSlice s = allocate(1);
            std::memcpy(s.ptr, &value, sizeof(T));
            return s;
        }

        // Flush the written region for the current frame (no-op on coherent memory).
        // Call once per frame after you finish writing, *if* you target non-coherent memory.
        void flushCurrentFrame() {
            PerFrame& pf = perFrame_[cur_];
            VkDeviceSize usedBytes = static_cast<VkDeviceSize>(pf.headIndex) * stride_;
            if (usedBytes == 0) return;
#if ENGINE_USE_VMA
            if (pf.alloc) {
                vmaFlushAllocation(device_->allocator(), pf.alloc, 0, usedBytes);
            }
#else
            VkMappedMemoryRange rng{};
            rng.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            rng.memory = pf.memory;
            rng.offset = 0;
            rng.size = usedBytes;
            vkFlushMappedMemoryRanges(device_->device(), 1, &rng);
#endif
        }

        // Accessors (current frame)
        VkBuffer     buffer()       const { return perFrame_[cur_].buffer; }
        void* mappedBase()   const { return perFrame_[cur_].mapped; }
        uint32_t     usedItems()    const { return perFrame_[cur_].headIndex; }
        uint32_t     capacityItems()const { return capacityItems(cur_); }
        VkDeviceSize stride()       const { return stride_; }

        // Descriptor info for binding this as UNIFORM_BUFFER(_DYNAMIC).
        // Use this in your descriptor writes; range = stride (one item).
        VkDescriptorBufferInfo descriptorInfo() const {
            VkDescriptorBufferInfo info{};
            info.buffer = buffer();
            info.offset = 0;            // dynamic offset supplies the per-draw offset
            info.range = stride_;      // size of one item
            return info;
        }

        // Compute dynamic offset for a previously allocated slice (or an index).
        VkDeviceSize dynamicOffset(uint32_t index) const { return static_cast<VkDeviceSize>(index) * stride_; }
        VkDeviceSize dynamicOffset(const DynSlice& s) const { return s.offset; }

        // Frame-meta
        uint32_t framesInFlight() const { return frames_; }
        uint32_t currentFrame()   const { return cur_; }

        // Destroy all buffers (call automatically on dtor).
        void destroy() {
            if (!device_) return;
            for (uint32_t i = 0; i < frames_; ++i) destroy_frame_buffer(i);
            perFrame_.clear();
            frames_ = 0;
            cur_ = 0;
            stride_ = 0;
            device_ = nullptr;
            baseName_.clear();
        }

    private:
        struct PerFrame {
            VkBuffer     buffer = VK_NULL_HANDLE;
#if ENGINE_USE_VMA
            VmaAllocation alloc = VK_NULL_HANDLE;
#else
            VkDeviceMemory memory = VK_NULL_HANDLE;
#endif
            void* mapped = nullptr;
            VkDeviceSize  size = 0;      // total bytes
            uint32_t      headIndex = 0;   // bump index (items used this frame)
        };

        uint32_t capacityItems(uint32_t frame) const {
            return static_cast<uint32_t>(perFrame_[frame].size / stride_);
        }

        void create_frame_buffer(uint32_t frame, VkDeviceSize bytes) {
            PerFrame& pf = perFrame_[frame];
            pf.size = bytes;

            // Create buffer
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = bytes;
            bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

#if ENGINE_USE_VMA
            VmaAllocationCreateInfo aci{};
            aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

            VmaAllocationInfo ainfo{};
            VK_CHECK(vmaCreateBuffer(device_->allocator(), &bi, &aci, &pf.buffer, &pf.alloc, &ainfo));
            pf.mapped = ainfo.pMappedData;
#else
            VK_CHECK(vkCreateBuffer(device_->device(), &bi, nullptr, &pf.buffer));

            VkMemoryRequirements req{};
            vkGetBufferMemoryRequirements(device_->device(), pf.buffer, &req);

            // Find a HOST_VISIBLE memory (prefer HOST_COHERENT if available)
            const auto& memProps = device_->memoryProperties();
            uint32_t chosen = UINT32_MAX, fallback = UINT32_MAX;
            for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
                if ((req.memoryTypeBits & (1u << i)) == 0) continue;
                auto flags = memProps.memoryTypes[i].propertyFlags;
                if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) continue;
                if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) { chosen = i; break; }
                fallback = i; // host-visible but non-coherent
            }
            if (chosen == UINT32_MAX) chosen = fallback;
            if (chosen == UINT32_MAX) throw std::runtime_error("DynamicUniforms: no HOST_VISIBLE memory type");

            VkMemoryAllocateInfo mai{};
            mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            mai.allocationSize = req.size;
            mai.memoryTypeIndex = chosen;
            VK_CHECK(vkAllocateMemory(device_->device(), &mai, nullptr, &pf.memory));
            VK_CHECK(vkBindBufferMemory(device_->device(), pf.buffer, pf.memory, 0));

            VK_CHECK(vkMapMemory(device_->device(), pf.memory, 0, pf.size, 0, &pf.mapped));
#endif

            // Debug names
            if (!baseName_.empty()) {
                device_->setObjectName(VK_OBJECT_TYPE_BUFFER,
                    reinterpret_cast<uint64_t>(pf.buffer),
                    baseName_ + "/frame#" + std::to_string(frame));
            }
        }

        void destroy_frame_buffer(uint32_t frame) {
            PerFrame& pf = perFrame_[frame];
            if (!device_) return;
#if ENGINE_USE_VMA
            if (pf.buffer) vmaDestroyBuffer(device_->allocator(), pf.buffer, pf.alloc);
            pf.alloc = VK_NULL_HANDLE;
#else
            if (pf.mapped) vkUnmapMemory(device_->device(), pf.memory);
            if (pf.buffer) vkDestroyBuffer(device_->device(), pf.buffer, nullptr);
            if (pf.memory) vkFreeMemory(device_->device(), pf.memory, nullptr);
#endif
            pf.buffer = VK_NULL_HANDLE;
            pf.mapped = nullptr;
            pf.size = 0;
            pf.headIndex = 0;
        }

        void recreate_frame_buffer(uint32_t frame, VkDeviceSize newSize) {
            destroy_frame_buffer(frame);
            create_frame_buffer(frame, newSize);
        }

        void move_from(DynamicUniforms&& o) noexcept {
            device_ = o.device_;   o.device_ = nullptr;
            frames_ = o.frames_;   o.frames_ = 0;
            cur_ = o.cur_;      o.cur_ = 0;
            stride_ = o.stride_;   o.stride_ = 0;
            perFrame_ = std::move(o.perFrame_);
            baseName_ = std::move(o.baseName_);
            allowGrow_ = o.allowGrow_;
        }

    private:
        const Device* device_ = nullptr;
        uint32_t      frames_ = 0;
        uint32_t      cur_ = 0;
        VkDeviceSize  stride_ = 0;
        std::vector<PerFrame> perFrame_;
        std::string   baseName_;
        bool          allowGrow_ = true;
    };

    // --------------------- usage helpers (optional, inline) ----------------------

    // Write 'count' items contiguously from an array; returns the first slice.
    template<typename T>
    inline DynSlice push_array(DynamicUniforms& du, const T* src, uint32_t count) {
        DynSlice first = du.allocate(count);
        uint8_t* dst = static_cast<uint8_t*>(first.ptr);
        for (uint32_t i = 0; i < count; ++i) {
            std::memcpy(dst + i * du.stride(), &src[i], sizeof(T));
        }
        return first;
    }

} // namespace hvk

#endif // HVK_DYNAMIC_UNIFORMS_HPP
