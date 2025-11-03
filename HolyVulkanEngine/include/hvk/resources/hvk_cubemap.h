/**
 * @file hvk_cubemap.h
 * @brief Cubemap texture support
 * @author Holy Vulkan Engine
 * @date 2025
 * Handles cubemap creation and loading for skyboxes and environment mapping.
 */

#ifndef HVK_CUBEMAP_H
#define HVK_CUBEMAP_H

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_gpu_resources.h>
#include <hvk/gfx/hvk_sampler_cache.h>
#include <hvk/gfx/hvk_staging_uploader.h>

#include <array>
#include <string>

namespace hvk {

// Simple cubemap texture (6 faces) with optional mip generation.
class Cubemap {
public:
    Cubemap() = default;
    ~Cubemap() = default;

    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;
    Cubemap(Cubemap&&) noexcept = default;
    Cubemap& operator=(Cubemap&&) noexcept = default;

    // Load a cubemap from 6 images: order = +X, -X, +Y, -Y, +Z, -Z
    // Supported by stb_image (png/jpg/etc). Images must share width/height/channels.
    static Cubemap loadFromFiles(
        const Device& device,
        StagingUploader& uploader,
        SamplerCache& samplerCache,
        const std::array<std::string, 6>& filepaths,
        bool generateMips = true,
        bool forceSRGB = true,
        std::string debugName = "skybox"
    );

    // GPU handles
    VkImage image() const { return image_.handle(); }
    VkImageView view() const { return view_.handle(); }
    VkSampler sampler() const { return sampler_; }

    // Descriptor helper
    VkDescriptorImageInfo descriptorInfo(VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const {
        return { sampler_, view(), layout };
    }

    // Info
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint32_t mipLevels() const { return mipLevels_; }

    explicit operator bool() const { return image_.handle() != VK_NULL_HANDLE; }

private:
    // Internal helpers
    static uint32_t calcMipCount(uint32_t w, uint32_t h) {
        uint32_t m = (std::max)(w, h);
        uint32_t levels = 0; while (m) { m >>= 1; ++levels; }
        return (std::max)(1u, levels);
    }

    static void transitionTo(
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
        uint32_t layers);

    void generateMipmaps(VkCommandBuffer cmd);

private:
    const Device* device_ = nullptr;
    GpuImage   image_{};
    ImageView  view_{};        // VK_IMAGE_VIEW_TYPE_CUBE
    VkSampler  sampler_ = VK_NULL_HANDLE;
    uint32_t   width_ = 0;
    uint32_t   height_ = 0;
    uint32_t   mipLevels_ = 1;
};

} // namespace hvk

#endif // HVK_CUBEMAP_H

