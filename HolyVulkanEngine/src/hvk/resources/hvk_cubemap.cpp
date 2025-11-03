#include <hvk/resources/hvk_cubemap.h>
#include <hvk/gfx/hvk_barriers.hpp>

#include <stb_image.h>
#include <stdexcept>
#include <vector>
#include <cstring>

namespace hvk {

void Cubemap::transitionTo(
    VkCommandBuffer cmd,
    VkImage img,
    VkImageAspectFlags aspect,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage,
    VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 dstStage,
    VkAccessFlags2 dstAccess,
    uint32_t mipLevels,
    uint32_t layers)
{
    VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layers;

    VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dep);
}

void Cubemap::generateMipmaps(VkCommandBuffer cmd)
{
    if (mipLevels_ <= 1) return;

    VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.image = image_.handle();
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.subresourceRange.levelCount = 1; // each iteration handles 1 level

    int32_t mipWidth = static_cast<int32_t>(width_);
    int32_t mipHeight = static_cast<int32_t>(height_);

    for (uint32_t i = 1; i < mipLevels_; ++i) {
        // Transition previous mip level to TRANSFER_SRC
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

        VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &dep);

        // For all 6 faces
        for (uint32_t face = 0; face < 6; ++face) {
            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = face;
            blit.srcSubresource.layerCount = 1;

            int32_t nextWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
            int32_t nextHeight = (mipHeight > 1) ? mipHeight / 2 : 1;

            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { nextWidth, nextHeight, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = face;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmd,
                image_.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);
        }

        // Transition (i-1) to SHADER_READ_ONLY
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        vkCmdPipelineBarrier2(cmd, &dep);

        mipWidth = (mipWidth > 1) ? (mipWidth / 2) : 1;
        mipHeight = (mipHeight > 1) ? (mipHeight / 2) : 1;
    }

    // Transition last mip level to SHADER_READ_ONLY
    VkImageMemoryBarrier2 last{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    last.image = image_.handle();
    last.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    last.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    last.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    last.subresourceRange.baseMipLevel = mipLevels_ - 1;
    last.subresourceRange.levelCount = 1;
    last.subresourceRange.baseArrayLayer = 0;
    last.subresourceRange.layerCount = 6;
    last.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    last.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    last.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    last.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    last.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    last.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &last;
    vkCmdPipelineBarrier2(cmd, &dep);
}

Cubemap Cubemap::loadFromFiles(
    const Device& device,
    StagingUploader& uploader,
    SamplerCache& samplerCache,
    const std::array<std::string, 6>& filepaths,
    bool generateMips,
    bool forceSRGB,
    std::string debugName)
{
    Cubemap cube;
    cube.device_ = &device;

    // Load all faces
    int w = 0, h = 0, ch = 0;
    std::vector<std::vector<uint8_t>> facesData(6);

    stbi_set_flip_vertically_on_load(false);

    for (size_t i = 0; i < 6; ++i) {
        int tw, th, tc;
        stbi_uc* pixels = stbi_load(filepaths[i].c_str(), &tw, &th, &tc, STBI_rgb_alpha);
        if (!pixels) {
            // Fail-soft: return empty cubemap (caller can skip rendering)
            return cube;
        }
        if (i == 0) { w = tw; h = th; ch = 4; }
        if (tw != w || th != h) {
            stbi_image_free(pixels);
            return cube;
        }
        size_t bytes = static_cast<size_t>(w) * h * 4;
        facesData[i].resize(bytes);
        std::memcpy(facesData[i].data(), pixels, bytes);
        stbi_image_free(pixels);
    }

    cube.width_ = static_cast<uint32_t>(w);
    cube.height_ = static_cast<uint32_t>(h);
    cube.mipLevels_ = generateMips ? calcMipCount(cube.width_, cube.height_) : 1u;

    // Create image (cube compatible)
    GpuImageCreateInfo ci{};
    ci.device = &device;
    ci.format = forceSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    ci.width = cube.width_;
    ci.height = cube.height_;
    ci.depth = 1;
    ci.mipLevels = cube.mipLevels_;
    ci.arrayLayers = 6;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.type = VK_IMAGE_TYPE_2D;
    ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (cube.mipLevels_ > 1) ci.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ci.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ci.memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    ci.debugName = debugName;

    cube.image_ = GpuImage(ci);

    // Create cube image view
    ImageViewCreateInfo vci{};
    vci.device = &device;
    vci.image = cube.image_.handle();
    vci.format = ci.format;
    vci.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    vci.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.range.baseMipLevel = 0;
    vci.range.levelCount = cube.mipLevels_;
    vci.range.baseArrayLayer = 0;
    vci.range.layerCount = 6;
    vci.debugName = debugName + std::string("_view");
    cube.view_ = ImageView(vci);

    // Sampler (clamp to edge)
    SamplerDesc s{};
    s.minFilter = VK_FILTER_LINEAR;
    s.magFilter = VK_FILTER_LINEAR;
    s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    s.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    s.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    s.minLod = 0.0f;
    s.maxLod = static_cast<float>(cube.mipLevels_);
    s.maxAnisotropy = 0.0f; // not needed for skybox
    cube.sampler_ = samplerCache.get(s);

    // Begin upload
    uploader.beginFrame(0);

    // Transition to TRANSFER_DST
    transitionTo(
        uploader.cmd(), cube.image_.handle(), VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        cube.mipLevels_, 6);

    // Copy each face
    for (uint32_t face = 0; face < 6; ++face) {
        // Write to staging
        auto slice = uploader.write(facesData[face].data(), facesData[face].size());

        VkBufferImageCopy region{};
        region.bufferOffset = slice.offset;
        region.bufferRowLength = 0; // tightly packed
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = face;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { cube.width_, cube.height_, 1 };

        uploader.copyBufferToImageRegion(cube.image_.handle(), region, VK_NULL_HANDLE);
    }

    // Generate mips or transition to shader-read
    if (cube.mipLevels_ > 1) {
        cube.generateMipmaps(uploader.cmd());
    } else {
        transitionTo(
            uploader.cmd(), cube.image_.handle(), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            cube.mipLevels_, 6);
    }

    uploader.submit();
    uploader.waitCurrent();

    return cube;
}

} // namespace hvk
