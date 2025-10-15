#include <hvk/resources/hvk_material.h>
#include <hvk/gfx/hvk_utils.hpp>

namespace hvk {

DescriptorSetLayout Material::createDescriptorSetLayout(const Device& device) {
    DescriptorSetLayoutCreateInfo layoutCI{};
    layoutCI.device = &device;

    // All 5 texture bindings available in fragment shader
    for (uint32_t i = 0; i < 5; ++i) {
        layoutCI.bindings.push_back({
            /*binding*/    i,
            /*type*/       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            /*count*/      1,
            /*stages*/     VK_SHADER_STAGE_FRAGMENT_BIT,
            /*samplers*/   nullptr
        });
    }

    layoutCI.debugName = "material_set1_layout";
    return DescriptorSetLayout(layoutCI);
}

void Material::init(
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
) {
    params_ = params;

    // Allocate descriptor set for this material
    descriptorSet_ = allocator.allocate(layout);

    // Use provided textures or fall back to defaults
    Texture* finalAlbedo = albedo ? albedo : defaultWhite;
    Texture* finalNormal = normal ? normal : defaultNormal;
    Texture* finalMR = metallicRoughness ? metallicRoughness : defaultMetallicRoughness;
    Texture* finalEmissive = emissive ? emissive : defaultWhite;
    Texture* finalOcclusion = occlusion ? occlusion : defaultWhite;

    // Update texture presence flags
    params_.hasBaseColorTex = albedo ? 1 : 0;
    params_.hasNormalTex = normal ? 1 : 0;
    params_.hasMetallicRoughnessTex = metallicRoughness ? 1 : 0;
    params_.hasEmissiveTex = emissive ? 1 : 0;
    params_.hasOcclusionTex = occlusion ? 1 : 0;

    // Write descriptor set
    DescriptorWrites writes;

    // Binding 0: Base Color
    auto albedoInfo = finalAlbedo->descriptorInfo();
    writes.writeImage(descriptorSet_, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      albedoInfo.imageView, albedoInfo.imageLayout, albedoInfo.sampler);

    // Binding 1: Normal
    auto normalInfo = finalNormal->descriptorInfo();
    writes.writeImage(descriptorSet_, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      normalInfo.imageView, normalInfo.imageLayout, normalInfo.sampler);

    // Binding 2: Metallic-Roughness
    auto mrInfo = finalMR->descriptorInfo();
    writes.writeImage(descriptorSet_, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      mrInfo.imageView, mrInfo.imageLayout, mrInfo.sampler);

    // Binding 3: Emissive
    auto emissiveInfo = finalEmissive->descriptorInfo();
    writes.writeImage(descriptorSet_, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      emissiveInfo.imageView, emissiveInfo.imageLayout, emissiveInfo.sampler);

    // Binding 4: Occlusion
    auto occlusionInfo = finalOcclusion->descriptorInfo();
    writes.writeImage(descriptorSet_, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      occlusionInfo.imageView, occlusionInfo.imageLayout, occlusionInfo.sampler);

    writes.commit(device.device());
}

Material MaterialBuilder::build(
    const Device& device,
    DescriptorAllocator& allocator,
    const DescriptorSetLayout& layout,
    Texture* defaultWhite,
    Texture* defaultNormal,
    Texture* defaultMetallicRoughness
) {
    Material mat;
    mat.init(device, allocator, layout, params_,
             albedo_, normal_, metallicRoughness_, emissive_, occlusion_,
             defaultWhite, defaultNormal, defaultMetallicRoughness);
    mat.setName(name_);
    return mat;
}

} // namespace hvk
