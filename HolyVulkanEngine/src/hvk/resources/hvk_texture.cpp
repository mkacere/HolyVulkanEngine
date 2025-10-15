#include <hvk/resources/hvk_texture.h>
#include <hvk/gfx/hvk_barriers.hpp>
#include <hvk/gfx/hvk_utils.hpp>

// Note: STB_IMAGE_IMPLEMENTATION is defined in hvk_gltf_loader.cpp
// to avoid multiple definition issues
#include <stb_image.h>

#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace hvk {

// ============================================================================
// Texture - Constructor
// ============================================================================

Texture::Texture(const Device& device, const TextureDesc& desc, SamplerCache& samplerCache)
    : device_(&device)
{
    createImage(desc);
    createImageView();
    createSampler(samplerCache, desc);
}

// ============================================================================
// Texture - Creation Helpers
// ============================================================================

void Texture::createImage(const TextureDesc& desc) {
    // Auto-calculate mip levels if 0
    uint32_t mipLevels = desc.mipLevels;
    if (mipLevels == 0) {
        mipLevels = calculateMipLevels(desc.extent.width, desc.extent.height);
    }

    // Auto-infer usage if not specified
    VkImageUsageFlags usage = desc.usage;
    if (usage == 0) {
        usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (mipLevels > 1) {
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // for mipmap generation
        }
    }

    // Create GpuImage
    GpuImageCreateInfo imgCI{};
    imgCI.device = device_;
    imgCI.type = desc.imageType;
    imgCI.format = desc.format;
    imgCI.width = desc.extent.width;
    imgCI.height = desc.extent.height;
    imgCI.depth = desc.extent.depth;
    imgCI.mipLevels = mipLevels;
    imgCI.arrayLayers = desc.arrayLayers;
    imgCI.samples = desc.samples;
    imgCI.usage = usage;
    imgCI.memUsage = desc.memUsage;
    imgCI.debugName = desc.debugName;

    image_ = GpuImage(imgCI);

    // Store metadata
    format_ = desc.format;
    imageType_ = desc.imageType;
    extent_ = desc.extent;
    mipLevels_ = mipLevels;
    arrayLayers_ = desc.arrayLayers;
}

void Texture::createImageView() {
    // Determine view type from image type
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    switch (imageType_) {
        case VK_IMAGE_TYPE_1D:
            viewType = arrayLayers_ > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
            break;
        case VK_IMAGE_TYPE_2D:
            viewType = arrayLayers_ > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            break;
        case VK_IMAGE_TYPE_3D:
            viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
    }

    // Determine aspect from format
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    // TODO: Add depth/stencil format detection if needed

    ImageViewCreateInfo viewCI{};
    viewCI.device = device_;
    viewCI.image = image_.handle();
    viewCI.viewType = viewType;
    viewCI.format = format_;
    viewCI.aspect = aspect;
    viewCI.range.aspectMask = aspect;
    viewCI.range.baseMipLevel = 0;
    viewCI.range.levelCount = mipLevels_;
    viewCI.range.baseArrayLayer = 0;
    viewCI.range.layerCount = arrayLayers_;

    view_ = ImageView(viewCI);
}

void Texture::createSampler(SamplerCache& samplerCache, const TextureDesc& desc) {
    SamplerDesc samplerDesc{};
    samplerDesc.minFilter = desc.minFilter;
    samplerDesc.magFilter = desc.magFilter;
    samplerDesc.mipmapMode = desc.mipmapMode;
    samplerDesc.addressModeU = desc.addressModeU;
    samplerDesc.addressModeV = desc.addressModeV;
    samplerDesc.addressModeW = desc.addressModeW;
    samplerDesc.maxAnisotropy = desc.maxAnisotropy;
    samplerDesc.minLod = 0.0f;
    samplerDesc.maxLod = static_cast<float>(mipLevels_);

    sampler_ = samplerCache.get(samplerDesc);
}

// ============================================================================
// Texture - loadFromFile
// ============================================================================

Texture Texture::loadFromFile(
    const Device& device,
    StagingUploader& uploader,
    SamplerCache& samplerCache,
    const TextureLoadInfo& info
) {
    // Set flip if requested
    stbi_set_flip_vertically_on_load(info.flipY);

    // Load image data
    int width, height, channels;
    stbi_uc* pixels = stbi_load(info.filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture: " + info.filepath + " - " + stbi_failure_reason());
    }

    // Determine format
    VkFormat format = info.desiredFormat;
    if (format == VK_FORMAT_UNDEFINED) {
        // Auto-detect based on hints
        if (info.forceSRGB) {
            format = VK_FORMAT_R8G8B8A8_SRGB;
        } else if (info.forceLinear) {
            format = VK_FORMAT_R8G8B8A8_UNORM;
        } else {
            // Heuristic: check filename for common texture types
            std::string lower = info.filepath;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            if (lower.find("albedo") != std::string::npos ||
                lower.find("diffuse") != std::string::npos ||
                lower.find("color") != std::string::npos ||
                lower.find("basecolor") != std::string::npos) {
                format = VK_FORMAT_R8G8B8A8_SRGB; // color textures are sRGB
            } else {
                format = VK_FORMAT_R8G8B8A8_UNORM; // default linear for normal/roughness/etc
            }
        }
    }

    // Calculate mip levels
    uint32_t mipLevels = info.generateMips ? calculateMipLevels(width, height) : 1;

    // Create texture descriptor
    TextureDesc desc{};
    desc.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    desc.imageType = VK_IMAGE_TYPE_2D;
    desc.format = format;
    desc.mipLevels = mipLevels;
    desc.arrayLayers = 1;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (mipLevels > 1) {
        desc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // for blit-based mipmap generation
    }
    desc.memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    desc.minFilter = info.minFilter;
    desc.magFilter = info.magFilter;
    desc.mipmapMode = info.mipmapMode;
    desc.addressModeU = desc.addressModeV = desc.addressModeW = info.addressMode;
    desc.maxAnisotropy = info.maxAnisotropy;
    desc.debugName = info.debugName.empty() ? info.filepath : info.debugName;

    // Create texture
    Texture tex(device, desc, samplerCache);

    // Upload base mip (level 0)
    size_t dataSize = static_cast<size_t>(width) * height * 4; // RGBA8
    tex.uploadInitialData(uploader, pixels, dataSize);

    // Free CPU data
    stbi_image_free(pixels);

    return tex;
}

// ============================================================================
// Texture - create2D
// ============================================================================

Texture Texture::create2D(
    const Device& device,
    SamplerCache& samplerCache,
    VkFormat format,
    VkExtent2D extent,
    uint32_t mipLevels,
    const void* data,
    size_t dataSize,
    std::string_view debugName
) {
    TextureDesc desc{};
    desc.extent = {extent.width, extent.height, 1};
    desc.imageType = VK_IMAGE_TYPE_2D;
    desc.format = format;
    desc.mipLevels = mipLevels == 0 ? calculateMipLevels(extent.width, extent.height) : mipLevels;
    desc.arrayLayers = 1;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.debugName = std::string(debugName);

    Texture tex(device, desc, samplerCache);

    // Note: Data upload must be done separately via uploadInitialData if needed
    // This is intentional to allow deferred upload
    (void)data;
    (void)dataSize;

    return tex;
}

// ============================================================================
// Texture - createFromMemory
// ============================================================================

Texture Texture::createFromMemory(
    const Device& device,
    StagingUploader& uploader,
    SamplerCache& samplerCache,
    const void* pixels,
    VkExtent2D extent,
    VkFormat format,
    bool generateMips,
    std::string_view debugName
) {
    uint32_t mipLevels = generateMips ? calculateMipLevels(extent.width, extent.height) : 1;

    TextureDesc desc{};
    desc.extent = {extent.width, extent.height, 1};
    desc.imageType = VK_IMAGE_TYPE_2D;
    desc.format = format;
    desc.mipLevels = mipLevels;
    desc.arrayLayers = 1;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (mipLevels > 1) {
        desc.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    desc.debugName = std::string(debugName);

    Texture tex(device, desc, samplerCache);

    // Assume RGBA8 format (4 bytes per pixel)
    size_t dataSize = static_cast<size_t>(extent.width) * extent.height * 4;
    tex.uploadInitialData(uploader, pixels, dataSize);

    CmdList cmd{ uploader.cmd() };
    if (generateMips && tex.mipLevels() > 1) {
        tex.generateMipmaps(cmd);
    }
    else {
        tex.transitionToShaderRead(cmd);
    }

    return tex;
}

// ============================================================================
// Texture - Upload Helpers
// ============================================================================

void Texture::uploadInitialData(StagingUploader& uploader, const void* data, size_t dataSize) {
    if (!data || dataSize == 0) return;

    // Transition image from UNDEFINED to TRANSFER_DST for upload
    VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    barrier.srcAccessMask = VK_ACCESS_2_NONE;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image_.handle();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels_;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = arrayLayers_;

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(uploader.cmd(), &dep);

    // Write data to staging buffer
    auto stagingLoc = uploader.write(data, dataSize);

    // Copy staging -> image (mip 0, layer 0)
    VkImageSubresourceLayers subresource{};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = 0;
    subresource.baseArrayLayer = 0;
    subresource.layerCount = arrayLayers_;

    uploader.copyBufferToImageTightly(
        image_.handle(),
        subresource,
        {0, 0, 0}, // offset
        {extent_.width, extent_.height, 1}, // extent
        stagingLoc,
        0, // rowLength (tightly packed)
        0  // imageHeight (tightly packed)
    );
}

void Texture::uploadMipLevel(
    StagingUploader& uploader,
    uint32_t mipLevel,
    const void* data,
    size_t dataSize,
    VkOffset3D offset,
    VkExtent3D extent
) {
    if (!data || dataSize == 0) return;
    if (mipLevel >= mipLevels_) {
        throw std::invalid_argument("Mip level out of range");
    }

    // Calculate mip extent if not provided
    if (extent.width == 0 || extent.height == 0) {
        extent = mipExtent(extent_, mipLevel);
    }

    // Write to staging
    auto stagingLoc = uploader.write(data, dataSize);

    // Copy staging -> image
    VkImageSubresourceLayers subresource{};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = mipLevel;
    subresource.baseArrayLayer = 0;
    subresource.layerCount = arrayLayers_;

    uploader.copyBufferToImageTightly(
        image_.handle(),
        subresource,
        offset,
        extent,
        stagingLoc,
        0, // rowLength
        0  // imageHeight
    );
}

// ============================================================================
// Texture - Mipmap Generation
// ============================================================================

void Texture::generateMipmaps(CmdList& cmd) {
    if (mipLevels_ <= 1) return; // Nothing to do

    // Check format supports blit (optional validation)
    // Most common formats support it, but production code should query VkFormatProperties

    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.image = image_.handle();
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = arrayLayers_;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = static_cast<int32_t>(extent_.width);
    int32_t mipHeight = static_cast<int32_t>(extent_.height);

    for (uint32_t i = 1; i < mipLevels_; ++i) {
        // Transition previous mip level to TRANSFER_SRC
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

        VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd.handle(), &dep);

        // Blit from (i-1) to (i)
        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = arrayLayers_;

        int32_t nextWidth = mipWidth > 1 ? mipWidth / 2 : 1;
        int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {nextWidth, nextHeight, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = arrayLayers_;

        vkCmdBlitImage(cmd.handle(),
            image_.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image_.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        // Transition (i-1) to SHADER_READ_ONLY (we're done with it)
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        vkCmdPipelineBarrier2(cmd.handle(), &dep);

        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }

    // Transition last mip level to SHADER_READ_ONLY
    barrier.subresourceRange.baseMipLevel = mipLevels_ - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    VkDependencyInfo finalDep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    finalDep.imageMemoryBarrierCount = 1;
    finalDep.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd.handle(), &finalDep);
}

void Texture::transitionToShaderRead(CmdList& cmd) {
    // Transition entire image to shader read layout
    auto barrier = hvk::barrier::make_image_barrier_full(
        image_.handle(),
        format_,
        hvk::barrier::ImgUse::TransferDst,
        hvk::barrier::ImgUse::ShaderRead
    );

    hvk::barrier::Batch batch;
    batch.imgs.push_back(barrier);
    hvk::barrier::submit(cmd.handle(), batch);
}

// ============================================================================
// Texture - Utility Functions
// ============================================================================

uint32_t Texture::calculateMipLevels(uint32_t width, uint32_t height) {
    uint32_t maxDim = std::max(width, height);
    return static_cast<uint32_t>(std::floor(std::log2(maxDim))) + 1;
}

VkExtent3D Texture::mipExtent(VkExtent3D baseExtent, uint32_t level) {
    VkExtent3D extent = baseExtent;
    extent.width = std::max(1u, extent.width >> level);
    extent.height = std::max(1u, extent.height >> level);
    extent.depth = std::max(1u, extent.depth >> level);
    return extent;
}

bool Texture::isSRGBFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
            return true;
        default:
            return false;
    }
}

VkFormat Texture::toSRGB(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_R8G8B8_UNORM: return VK_FORMAT_R8G8B8_SRGB;
        case VK_FORMAT_B8G8R8_UNORM: return VK_FORMAT_B8G8R8_SRGB;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case VK_FORMAT_BC2_UNORM_BLOCK: return VK_FORMAT_BC2_SRGB_BLOCK;
        case VK_FORMAT_BC3_UNORM_BLOCK: return VK_FORMAT_BC3_SRGB_BLOCK;
        case VK_FORMAT_BC7_UNORM_BLOCK: return VK_FORMAT_BC7_SRGB_BLOCK;
        default: return format; // already sRGB or unsupported
    }
}

VkFormat Texture::toLinear(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_R8G8B8_SRGB: return VK_FORMAT_R8G8B8_UNORM;
        case VK_FORMAT_B8G8R8_SRGB: return VK_FORMAT_B8G8R8_UNORM;
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case VK_FORMAT_BC2_SRGB_BLOCK: return VK_FORMAT_BC2_UNORM_BLOCK;
        case VK_FORMAT_BC3_SRGB_BLOCK: return VK_FORMAT_BC3_UNORM_BLOCK;
        case VK_FORMAT_BC7_SRGB_BLOCK: return VK_FORMAT_BC7_UNORM_BLOCK;
        default: return format; // already linear or unsupported
    }
}

// ============================================================================
// TextureLoader - Utility
// ============================================================================

TextureLoader::ImageInfo TextureLoader::probeFile(const char* filepath) {
    ImageInfo info;
    int width, height, channels;

    if (stbi_info(filepath, &width, &height, &channels)) {
        info.width = static_cast<uint32_t>(width);
        info.height = static_cast<uint32_t>(height);
        info.channels = static_cast<uint32_t>(channels);
        info.valid = true;
    } else {
        info.valid = false;
    }

    return info;
}

} // namespace hvk
