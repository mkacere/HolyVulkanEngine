#ifndef HVK_TEXTURE_H
#define HVK_TEXTURE_H

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_gpu_resources.h>
#include <hvk/gfx/hvk_sampler_cache.h>
#include <hvk/gfx/hvk_staging_uploader.h>
#include <hvk/gfx/hvk_cmd_list.hpp>

#include <vulkan/vulkan.h>
#include <string>
#include <string_view>
#include <cstdint>
#include <memory>
#include <vector>

namespace hvk {

// ============================================================================
// TextureDesc - Creation Parameters
// ============================================================================

struct TextureDesc {
    // Dimensions
    VkExtent3D          extent = {1, 1, 1};
    VkImageType         imageType = VK_IMAGE_TYPE_2D;
    VkFormat            format = VK_FORMAT_R8G8B8A8_SRGB;
    uint32_t            mipLevels = 1;      // 0 = auto-calculate all mips
    uint32_t            arrayLayers = 1;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    // Usage flags (auto-inferred if 0)
    VkImageUsageFlags   usage = 0; // default: SAMPLED | TRANSFER_DST | TRANSFER_SRC (for mips)

    // Memory
    VmaMemoryUsage      memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    // Initial data (nullptr = don't upload, user fills later)
    const void*         initialData = nullptr;
    size_t              dataSize = 0;

    // Sampler hints (resolved via SamplerCache)
    VkFilter            minFilter = VK_FILTER_LINEAR;
    VkFilter            magFilter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    float               maxAnisotropy = 16.0f; // 0 = disabled

    // Debug
    std::string         debugName;
};

// ============================================================================
// TextureLoadInfo - File Loading Parameters
// ============================================================================

struct TextureLoadInfo {
    std::string         filepath;

    // Format override (VK_FORMAT_UNDEFINED = auto-detect from channels)
    VkFormat            desiredFormat = VK_FORMAT_UNDEFINED;

    // Mipmap generation
    bool                generateMips = true;    // auto-generate mipmaps after upload

    // sRGB conversion
    bool                forceSRGB = false;      // force sRGB format (e.g., for albedo)
    bool                forceLinear = false;    // force linear format (e.g., for normal maps)

    // Sampler settings
    VkFilter            minFilter = VK_FILTER_LINEAR;
    VkFilter            magFilter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT; // U/V/W
    float               maxAnisotropy = 16.0f;

    // Flip on load (useful for some texture formats)
    bool                flipY = false;

    std::string         debugName; // defaults to filename if empty
};

// ============================================================================
// Texture - Main Class
// ============================================================================

class Texture {
public:
    Texture() = default;
    ~Texture() = default;

    // Move-only (RAII)
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) noexcept = default;
    Texture& operator=(Texture&&) noexcept = default;

    // ------------------------------------------------------------------------
    // Creation Methods
    // ------------------------------------------------------------------------

    // Create from descriptor (manual setup)
    explicit Texture(const Device& device, const TextureDesc& desc, SamplerCache& samplerCache);

    // Create from file (most common - stb_image)
    static Texture loadFromFile(
        const Device& device,
        StagingUploader& uploader,
        SamplerCache& samplerCache,
        const TextureLoadInfo& loadInfo
    );

    // Create 2D convenience
    static Texture create2D(
        const Device& device,
        SamplerCache& samplerCache,
        VkFormat format,
        VkExtent2D extent,
        uint32_t mipLevels = 1,
        const void* data = nullptr,
        size_t dataSize = 0,
        std::string_view debugName = {}
    );

    // Create from memory (pre-loaded pixel data)
    static Texture createFromMemory(
        const Device& device,
        StagingUploader& uploader,
        SamplerCache& samplerCache,
        const void* pixels,
        VkExtent2D extent,
        VkFormat format,
        bool generateMips,
        std::string_view debugName = {}
    );

    // ------------------------------------------------------------------------
    // Mipmap Generation (call after initial upload)
    // ------------------------------------------------------------------------

    // Generate mipmaps via vkCmdBlitImage (fast, GPU-side)
    void generateMipmaps(CmdList& cmd);

    // ------------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------------

    VkImage             image() const { return image_.handle(); }
    VkImageView         view() const { return view_.handle(); }
    VkSampler           sampler() const { return sampler_; }

    VkFormat            format() const { return format_; }
    VkExtent3D          extent() const { return extent_; }
    uint32_t            width() const { return extent_.width; }
    uint32_t            height() const { return extent_.height; }
    uint32_t            depth() const { return extent_.depth; }
    uint32_t            mipLevels() const { return mipLevels_; }
    uint32_t            arrayLayers() const { return arrayLayers_; }

    // Descriptor info for binding
    VkDescriptorImageInfo descriptorInfo(VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const {
        return { sampler_, view(), layout };
    }

    // Valid check
    bool valid() const { return image_.handle() != VK_NULL_HANDLE; }
    explicit operator bool() const { return valid(); }

    // ------------------------------------------------------------------------
    // Upload / Update (for dynamic textures)
    // ------------------------------------------------------------------------

    // Upload data to a specific mip level (requires TRANSFER_DST usage)
    void uploadMipLevel(
        StagingUploader& uploader,
        uint32_t mipLevel,
        const void* data,
        size_t dataSize,
        VkOffset3D offset = {0, 0, 0},
        VkExtent3D extent = {0, 0, 0} // 0 = full mip extent
    );

    // ------------------------------------------------------------------------
    // Utility
    // ------------------------------------------------------------------------

    // Calculate mip levels for given dimensions
    static uint32_t calculateMipLevels(uint32_t width, uint32_t height = 1);

    // Get mip extent at level
    static VkExtent3D mipExtent(VkExtent3D baseExtent, uint32_t level);

    // Format queries
    static bool isSRGBFormat(VkFormat format);
    static VkFormat toSRGB(VkFormat format);
    static VkFormat toLinear(VkFormat format);

private:
    // Internal helpers
    void createImage(const TextureDesc& desc);
    void createImageView();
    void createSampler(SamplerCache& samplerCache, const TextureDesc& desc);

    // Upload initial data (called internally by loadFromFile/createFromMemory)
    void uploadInitialData(StagingUploader& uploader, const void* data, size_t dataSize);

    // Transition to shader-read layout after upload+mipgen
    void transitionToShaderRead(CmdList& cmd);

private:
    const Device*   device_ = nullptr;

    GpuImage        image_;         // Owns VkImage + VmaAllocation
    ImageView       view_;          // Owns VkImageView
    VkSampler       sampler_ = VK_NULL_HANDLE; // From SamplerCache (not owned)

    VkFormat        format_ = VK_FORMAT_UNDEFINED;
    VkImageType     imageType_ = VK_IMAGE_TYPE_2D;
    VkExtent3D      extent_ = {0, 0, 0};
    uint32_t        mipLevels_ = 1;
    uint32_t        arrayLayers_ = 1;
};

// ============================================================================
// Texture2D - Convenience Alias
// ============================================================================

using Texture2D = Texture;

// ============================================================================
// TextureLoader - Static Utility for Batch Loading
// ============================================================================

class TextureLoader {
public:
    // Probe image file info without loading (width, height, channels)
    struct ImageInfo {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 0;
        bool valid = false;
    };
    static ImageInfo probeFile(const char* filepath);
};

} // namespace hvk

#endif // HVK_TEXTURE_H
