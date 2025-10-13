#ifndef HVK_BARRIERS_HPP
#define HVK_BARRIERS_HPP

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace hvk::barrier {

    // ----------------------------- small utils ----------------------------------

    inline VkDeviceSize round_up(VkDeviceSize v, VkDeviceSize a) { return a ? (v + a - 1) & ~(a - 1) : v; }

    inline bool is_depth_format(VkFormat f) {
        switch (f) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;
        default: return false;
        }
    }
    inline bool is_stencil_format(VkFormat f) {
        switch (f) {
        case VK_FORMAT_S8_UINT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;
        default: return false;
        }
    }

    inline VkImageAspectFlags aspect_from_format(VkFormat f) {
        VkImageAspectFlags a = 0;
        if (is_depth_format(f))   a |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (is_stencil_format(f)) a |= VK_IMAGE_ASPECT_STENCIL_BIT;
        if (!a)                   a |= VK_IMAGE_ASPECT_COLOR_BIT;
        return a;
    }

    inline VkImageSubresourceRange full_range(VkImageAspectFlags aspect,
        uint32_t baseMip = 0, uint32_t mipCount = VK_REMAINING_MIP_LEVELS,
        uint32_t baseLayer = 0, uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS) {
        VkImageSubresourceRange r{};
        r.aspectMask = aspect; r.baseMipLevel = baseMip; r.levelCount = mipCount;
        r.baseArrayLayer = baseLayer; r.layerCount = layerCount;
        return r;
    }

    // ----------------------------- usage presets --------------------------------

    enum class ImgUse {
        Undefined,
        TransferSrc,
        TransferDst,
        ColorAttachment,
        DepthStencilAttachment,
        DepthStencilReadOnly,
        ShaderRead,      // sampled/readonly image
        ShaderWrite,     // storage image writes (GENERAL)
        Present
    };

    struct ImgUseInfo {
        VkPipelineStageFlags2 stage = 0;
        VkAccessFlags2        access = 0;
        VkImageLayout         layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    // Defaults are conservative and work across graphics/compute
    inline ImgUseInfo info_for(ImgUse u) {
        switch (u) {
        case ImgUse::Undefined:
            return { VK_PIPELINE_STAGE_2_NONE, 0, VK_IMAGE_LAYOUT_UNDEFINED };
        case ImgUse::TransferSrc:
            return { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL };
        case ImgUse::TransferDst:
            return { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };
        case ImgUse::ColorAttachment:
            return { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                     VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL }; // Vulkan 1.3+ generalized attachment layout
        case ImgUse::DepthStencilAttachment:
            return { VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                     VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL };
        case ImgUse::DepthStencilReadOnly:
            return { VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                     VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL };
        case ImgUse::ShaderRead:
            return { VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, // sampled/read-only
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        case ImgUse::ShaderWrite:
            return { VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, // storage writes
                     VK_IMAGE_LAYOUT_GENERAL };
        case ImgUse::Present:
            // dst stage can be NONE for present; access must be 0
            return { VK_PIPELINE_STAGE_2_NONE, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
        }
        return {};
    }

    // --------------------------- barrier batch (sync2) ---------------------------

    struct Batch {
        std::vector<VkMemoryBarrier2>        mem;
        std::vector<VkBufferMemoryBarrier2>  bufs;
        std::vector<VkImageMemoryBarrier2>   imgs;

        void clear() { mem.clear(); bufs.clear(); imgs.clear(); }
        bool empty() const { return mem.empty() && bufs.empty() && imgs.empty(); }
    };

    inline void submit(VkCommandBuffer cmd, const Batch& b, VkDependencyFlags depFlags = 0) {
        if (b.empty()) return;
        VkDependencyInfo di{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        di.dependencyFlags = depFlags;
        di.memoryBarrierCount = static_cast<uint32_t>(b.mem.size());
        di.pMemoryBarriers = b.mem.data();
        di.bufferMemoryBarrierCount = static_cast<uint32_t>(b.bufs.size());
        di.pBufferMemoryBarriers = b.bufs.data();
        di.imageMemoryBarrierCount = static_cast<uint32_t>(b.imgs.size());
        di.pImageMemoryBarriers = b.imgs.data();
        vkCmdPipelineBarrier2(cmd, &di);
    }

    // ------------------------------- image barriers ------------------------------

    inline VkImageMemoryBarrier2 make_image_barrier(VkImage image,
        const VkImageSubresourceRange& range,
        ImgUse srcUse, ImgUse dstUse,
        uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        const ImgUseInfo s = info_for(srcUse);
        const ImgUseInfo d = info_for(dstUse);

        VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        b.srcStageMask = s.stage;
        b.srcAccessMask = s.access;
        b.dstStageMask = d.stage;
        b.dstAccessMask = d.access;
        b.oldLayout = s.layout;
        b.newLayout = d.layout;
        b.srcQueueFamilyIndex = srcQueueFamily;
        b.dstQueueFamilyIndex = dstQueueFamily;
        b.image = image;
        b.subresourceRange = range;
        return b;
    }

    // Convenience: if you know the format, we derive aspect for a full-range barrier
    inline VkImageMemoryBarrier2 make_image_barrier_full(VkImage image, VkFormat fmt,
        ImgUse srcUse, ImgUse dstUse,
        uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        return make_image_barrier(image, full_range(aspect_from_format(fmt)),
            srcUse, dstUse, srcQueueFamily, dstQueueFamily);
    }

    // Convenience helpers for common flows
    inline VkImageMemoryBarrier2 undefined_to_transfer_dst(VkImage image, VkFormat fmt) {
        return make_image_barrier_full(image, fmt, ImgUse::Undefined, ImgUse::TransferDst);
    }
    inline VkImageMemoryBarrier2 transfer_dst_to_shader_read(VkImage image, VkFormat fmt) {
        return make_image_barrier_full(image, fmt, ImgUse::TransferDst, ImgUse::ShaderRead);
    }
    inline VkImageMemoryBarrier2 shader_read_to_color_attachment(VkImage image, VkFormat fmt) {
        return make_image_barrier_full(image, fmt, ImgUse::ShaderRead, ImgUse::ColorAttachment);
    }
    inline VkImageMemoryBarrier2 color_to_present(VkImage image, VkFormat fmt) {
        // src: color attachment writes → dst: present
        return make_image_barrier_full(image, fmt, ImgUse::ColorAttachment, ImgUse::Present);
    }

    // Depth-only variants
    inline VkImageMemoryBarrier2 undefined_to_depth_attachment(VkImage image, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT) {
        return make_image_barrier(image, full_range(aspect), ImgUse::Undefined, ImgUse::DepthStencilAttachment);
    }
    inline VkImageMemoryBarrier2 depth_write_to_readonly(VkImage image, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT) {
        return make_image_barrier(image, full_range(aspect), ImgUse::DepthStencilAttachment, ImgUse::DepthStencilReadOnly);
    }

    // ------------------------------- buffer barriers -----------------------------

    inline VkBufferMemoryBarrier2 make_buffer_barrier(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size,
        VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
        VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
        uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED)
    {
        VkBufferMemoryBarrier2 b{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
        b.srcStageMask = srcStage;   b.srcAccessMask = srcAccess;
        b.dstStageMask = dstStage;   b.dstAccessMask = dstAccess;
        b.srcQueueFamilyIndex = srcQueueFamily;
        b.dstQueueFamilyIndex = dstQueueFamily;
        b.buffer = buffer; b.offset = offset; b.size = size;
        return b;
    }

    // Handy presets
    inline VkBufferMemoryBarrier2 transfer_write_to_vertex_read(VkBuffer buf, VkDeviceSize off = 0, VkDeviceSize sz = VK_WHOLE_SIZE) {
        return make_buffer_barrier(buf, off, sz,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT);
    }
    inline VkBufferMemoryBarrier2 transfer_write_to_index_read(VkBuffer buf, VkDeviceSize off = 0, VkDeviceSize sz = VK_WHOLE_SIZE) {
        return make_buffer_barrier(buf, off, sz,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT);
    }
    inline VkBufferMemoryBarrier2 transfer_write_to_shader_read(VkBuffer buf, VkDeviceSize off = 0, VkDeviceSize sz = VK_WHOLE_SIZE) {
        return make_buffer_barrier(buf, off, sz,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }
    inline VkBufferMemoryBarrier2 shader_write_to_shader_read(VkBuffer buf, VkDeviceSize off = 0, VkDeviceSize sz = VK_WHOLE_SIZE) {
        return make_buffer_barrier(buf, off, sz,
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }

    // ----------------------------- memory barrier (global) -----------------------

    inline VkMemoryBarrier2 make_memory_barrier(VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
        VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
    {
        VkMemoryBarrier2 m{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
        m.srcStageMask = srcStage;  m.srcAccessMask = srcAccess;
        m.dstStageMask = dstStage;  m.dstAccessMask = dstAccess;
        return m;
    }

    // Example: full GPU write → subsequent GPU read (very conservative)
    inline VkMemoryBarrier2 any_write_to_read() {
        return make_memory_barrier(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
    }

    // ------------------------------- examples -----------------------------------
    // Typical upload → sample transition for a texture:
    //
    //   hvk::barrier::Batch b;
    //   b.imgs.push_back(hvk::barrier::undefined_to_transfer_dst(tex.image, tex.format));
    //   hvk::barrier::submit(cmd, b); b.clear();
    //
    //   // Do vkCmdCopyBufferToImage...
    //
    //   b.imgs.push_back(hvk::barrier::transfer_dst_to_shader_read(tex.image, tex.format));
    //   hvk::barrier::submit(cmd, b);
    //
    // Typical swapchain image before rendering and before present:
    //
    //   b.imgs.push_back(hvk::barrier::make_image_barrier_full(scImage, scFormat,
    //                      hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::ColorAttachment));
    //   hvk::barrier::submit(cmd, b); b.clear();
    //
    //   // render...
    //
    //   b.imgs.push_back(hvk::barrier::color_to_present(scImage, scFormat));
    //   hvk::barrier::submit(cmd, b);

} // namespace hvk::barrier

#endif // HVK_BARRIERS_HPP
