/**
 * @file hvk_material.h
 * @brief PBR material system
 * @author Holy Vulkan Engine
 * @date 2025
 * Defines physically-based rendering materials with texture support and parameters.
 */

#ifndef HVK_MATERIAL_H
#define HVK_MATERIAL_H

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_descriptors.h>
#include <hvk/resources/hvk_texture.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <string>
#include <memory>

namespace hvk {

/**
 * AlphaMode - How to interpret alpha channel
 */
enum class AlphaMode : uint32_t {
    Opaque = 0,      // Alpha is ignored, material is fully opaque
    Mask = 1,        // Alpha is used for binary transparency (alpha test)
    Blend = 2        // Alpha is used for blending
};

/**
 * MaterialParams - PBR material parameters (CPU-side)
 *
 * These are passed to shaders via push constants or UBO
 */
struct MaterialParams {
    // std140 layout (for push constants):
    // - vec4 = 16 bytes aligned to 16
    // - vec3 = 12 bytes but ALIGNED TO 16! (this is the key!)
    // - float/uint = 4 bytes aligned to 4

    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};  // 16 bytes, offset 0
    glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f};         // 12 bytes, offset 16
    float     _emissivePad = 0.0f;                       // 4 bytes padding (vec3 takes 16 bytes in std140!)

    float     metallicFactor = 1.0f;                     // 4 bytes, offset 32
    float     roughnessFactor = 1.0f;                    // 4 bytes, offset 36
    float     alphaCutoff = 0.5f;                        // 4 bytes, offset 40
    AlphaMode alphaMode = AlphaMode::Opaque;             // 4 bytes, offset 44

    // Texture presence flags (16 bytes aligned)
    uint32_t hasBaseColorTex = 0;                        // 4 bytes, offset 48
    uint32_t hasNormalTex = 0;                           // 4 bytes, offset 52
    uint32_t hasMetallicRoughnessTex = 0;                // 4 bytes, offset 56
    uint32_t hasEmissiveTex = 0;                         // 4 bytes, offset 60

    uint32_t hasOcclusionTex = 0;                        // 4 bytes, offset 64
    uint32_t _pad0 = 0;                                  // 4 bytes, offset 68
    uint32_t _pad1 = 0;                                  // 4 bytes, offset 72
    uint32_t _pad2 = 0;                                  // 4 bytes, offset 76

    // Total: 80 bytes (std140 aligned)

    // Default constructor
    MaterialParams() = default;
};

// Ensure proper alignment for GPU (std140/std430)
static_assert(sizeof(MaterialParams) % 16 == 0, "MaterialParams must be 16-byte aligned");

/**
 * Material - PBR material with textures and parameters
 *
 * Descriptor Set Layout (Set 1):
 *   Binding 0: Base Color / Albedo texture (RGBA)
 *   Binding 1: Normal map (RGB, tangent space)
 *   Binding 2: Metallic-Roughness texture (B=metallic, G=roughness)
 *   Binding 3: Emissive texture (RGB)
 *   Binding 4: Occlusion texture (R channel)
 *
 * Design notes:
 * - Textures are non-owning pointers (Model owns the actual Texture objects)
 * - Material owns its descriptor set
 * - Supports both textured and non-textured materials (uses white/default textures)
 */
class Material {
public:
    Material() = default;
    ~Material() = default;

    // Move-only
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    Material(Material&&) noexcept = default;
    Material& operator=(Material&&) noexcept = default;

    // --- Static Factory ---

    /**
     * Create descriptor set layout for materials (Set 1)
     *
     * Bindings:
     *   0: Base Color / Albedo (COMBINED_IMAGE_SAMPLER)
     *   1: Normal Map (COMBINED_IMAGE_SAMPLER)
     *   2: Metallic-Roughness (COMBINED_IMAGE_SAMPLER)
     *   3: Emissive (COMBINED_IMAGE_SAMPLER)
     *   4: Occlusion (COMBINED_IMAGE_SAMPLER)
     *
     * All bindings are available in fragment shader stage.
     *
     * @param device Device reference
     * @return DescriptorSetLayout for Set 1
     */
    static DescriptorSetLayout createDescriptorSetLayout(const Device& device);

    // --- Creation ---

    /**
     * Initialize material with textures and parameters
     *
     * @param device Device reference
     * @param allocator Descriptor allocator for Set 1
     * @param layout Descriptor set layout for materials (Set 1)
     * @param params Material parameters
     * @param albedo Base color texture (optional, can be nullptr)
     * @param normal Normal map texture (optional)
     * @param metallicRoughness Metallic-Roughness texture (optional)
     * @param emissive Emissive texture (optional)
     * @param occlusion Occlusion texture (optional)
     * @param defaultWhite Default white texture for missing albedo/emissive
     * @param defaultNormal Default normal map (flat, pointing up)
     * @param defaultMetallicRoughness Default MR texture (non-metallic, rough)
     */
    void init(
        const Device& device,
        DescriptorAllocator& allocator,
        const DescriptorSetLayout& layout,
        const MaterialParams& params,
        Texture* albedo,
        Texture* normal,
        Texture* metallicRoughness,
        Texture* emissive,
        Texture* occlusion,
        Texture* defaultWhite,
        Texture* defaultNormal,
        Texture* defaultMetallicRoughness
    );

    // --- Accessors ---

    VkDescriptorSet descriptorSet() const { return descriptorSet_; }
    const MaterialParams& params() const { return params_; }
    MaterialParams& params() { return params_; }

    // Check if material has specific textures
    bool hasAlbedoTexture() const { return params_.hasBaseColorTex != 0; }
    bool hasNormalTexture() const { return params_.hasNormalTex != 0; }
    bool hasMetallicRoughnessTexture() const { return params_.hasMetallicRoughnessTex != 0; }
    bool hasEmissiveTexture() const { return params_.hasEmissiveTex != 0; }
    bool hasOcclusionTexture() const { return params_.hasOcclusionTex != 0; }

    // Get alpha mode
    AlphaMode alphaMode() const { return params_.alphaMode; }
    bool isOpaque() const { return params_.alphaMode == AlphaMode::Opaque; }
    bool isBlended() const { return params_.alphaMode == AlphaMode::Blend; }
    bool isMasked() const { return params_.alphaMode == AlphaMode::Mask; }

    // Name (for debugging)
    void setName(const std::string& name) { name_ = name; }
    const std::string& name() const { return name_; }

private:
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    MaterialParams  params_;
    std::string     name_;
};

/**
 * MaterialBuilder - Fluent interface for constructing materials
 *
 * Usage:
 *   Material mat = MaterialBuilder()
 *       .withAlbedo(albedoTex)
 *       .withNormal(normalTex)
 *       .withBaseColorFactor({0.8f, 0.2f, 0.2f, 1.0f})
 *       .withMetallicFactor(0.0f)
 *       .withRoughnessFactor(0.5f)
 *       .build(device, allocator, layout, defaultTextures);
 */
class MaterialBuilder {
public:
    MaterialBuilder() = default;

    MaterialBuilder& withAlbedo(Texture* tex) {
        albedo_ = tex;
        return *this;
    }

    MaterialBuilder& withNormal(Texture* tex) {
        normal_ = tex;
        return *this;
    }

    MaterialBuilder& withMetallicRoughness(Texture* tex) {
        metallicRoughness_ = tex;
        return *this;
    }

    MaterialBuilder& withEmissive(Texture* tex) {
        emissive_ = tex;
        return *this;
    }

    MaterialBuilder& withOcclusion(Texture* tex) {
        occlusion_ = tex;
        return *this;
    }

    MaterialBuilder& withBaseColorFactor(const glm::vec4& color) {
        params_.baseColorFactor = color;
        return *this;
    }

    MaterialBuilder& withEmissiveFactor(const glm::vec3& color) {
        params_.emissiveFactor = color;
        return *this;
    }

    MaterialBuilder& withMetallicFactor(float metallic) {
        params_.metallicFactor = metallic;
        return *this;
    }

    MaterialBuilder& withRoughnessFactor(float roughness) {
        params_.roughnessFactor = roughness;
        return *this;
    }

    MaterialBuilder& withAlphaMode(AlphaMode mode) {
        params_.alphaMode = mode;
        return *this;
    }

    MaterialBuilder& withAlphaCutoff(float cutoff) {
        params_.alphaCutoff = cutoff;
        return *this;
    }

    MaterialBuilder& withName(const std::string& name) {
        name_ = name;
        return *this;
    }

    Material build(
        const Device& device,
        DescriptorAllocator& allocator,
        const DescriptorSetLayout& layout,
        Texture* defaultWhite,
        Texture* defaultNormal,
        Texture* defaultMetallicRoughness
    );

private:
    MaterialParams params_;
    Texture* albedo_ = nullptr;
    Texture* normal_ = nullptr;
    Texture* metallicRoughness_ = nullptr;
    Texture* emissive_ = nullptr;
    Texture* occlusion_ = nullptr;
    std::string name_;
};

} // namespace hvk

#endif // HVK_MATERIAL_H
