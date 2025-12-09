#include <hvk/ecs/systems/hvk_render_system_base.hpp>
#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/ecs/hvk_resource_manager.hpp>
#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <hvk/gfx/hvk_global_descriptors.hpp>
#include <stdexcept>

namespace hvk {

RenderSystemBase::~RenderSystemBase() {
    cleanup();
}

void RenderSystemBase::cleanup() {
    destroyResources();
}

// ========================================================================
// Initialization
// ========================================================================

void RenderSystemBase::initBase(Scene& scene) {
    device_ = &scene.resources().device();
    pipelineCache_ = scene.pipelineCache();
    globalDescriptorLayout_ = scene.globalDescriptorLayout();

    if (!device_) {
        throw std::runtime_error("RenderSystemBase::initBase() - Device is null");
    }
    if (!pipelineCache_) {
        throw std::runtime_error("RenderSystemBase::initBase() - Pipeline cache is null");
    }
}

// ========================================================================
// Resource Tracking
// ========================================================================

void RenderSystemBase::trackPipeline(VkPipeline pipeline) {
    if (pipeline != VK_NULL_HANDLE) {
        pipelines_.push_back(pipeline);
    }
}

void RenderSystemBase::trackPipelineLayout(VkPipelineLayout layout) {
    if (layout != VK_NULL_HANDLE) {
        pipelineLayouts_.push_back(layout);
    }
}

void RenderSystemBase::trackShaderModule(VkShaderModule module) {
    if (module != VK_NULL_HANDLE) {
        shaderModules_.push_back(module);
    }
}

void RenderSystemBase::trackShaderModules(const ShaderModules& shaders) {
    trackShaderModule(shaders.vertex);
    trackShaderModule(shaders.fragment);
    trackShaderModule(shaders.compute);
    trackShaderModule(shaders.geometry);
    trackShaderModule(shaders.tessControl);
    trackShaderModule(shaders.tessEval);
}

// ========================================================================
// Helper Methods
// ========================================================================

VkPipelineLayout RenderSystemBase::createPipelineLayout(
    Scene& scene,
    const std::vector<ShaderStageDesc>& /*shaderStages*/,
    uint32_t pushConstantSize,
    VkShaderStageFlags pushConstantStages
) {
    const DescriptorSetLayout* globalLayout = scene.globalDescriptorLayout();
    if (!globalLayout) {
        throw std::runtime_error("RenderSystemBase::createPipelineLayout() - Missing global descriptor layout");
    }

    // Set layouts (currently only global descriptors)
    std::vector<VkDescriptorSetLayout> setLayouts = {
        globalLayout->handle()
    };

    // Push constants (if any)
    VkPushConstantRange pushRange{};
    if (pushConstantSize > 0) {
        pushRange.stageFlags = pushConstantStages;
        pushRange.offset = 0;
        pushRange.size = pushConstantSize;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutCI.pSetLayouts = setLayouts.data();
    layoutCI.pushConstantRangeCount = (pushConstantSize > 0) ? 1 : 0;
    layoutCI.pPushConstantRanges = (pushConstantSize > 0) ? &pushRange : nullptr;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult result = vkCreatePipelineLayout(device_->device(), &layoutCI, nullptr, &layout);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("RenderSystemBase::createPipelineLayout() - Failed to create pipeline layout");
    }

    // Track for automatic cleanup
    trackPipelineLayout(layout);

    return layout;
}

RenderFormats RenderSystemBase::getRenderFormats(Scene& /*scene*/) const {
    // Standard render formats (SRGB color + D32 depth)
    // This matches the typical swapchain configuration
    RenderFormats formats{};
    formats.colorFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    formats.depthFormat = VK_FORMAT_D32_SFLOAT;
    return formats;
}

// ========================================================================
// Internal Cleanup
// ========================================================================

void RenderSystemBase::destroyResources() {
    if (!device_) {
        return;
    }

    VkDevice vkDevice = device_->device();

    // Destroy pipelines
    for (VkPipeline pipeline : pipelines_) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(vkDevice, pipeline, nullptr);
        }
    }
    pipelines_.clear();

    // Destroy pipeline layouts
    for (VkPipelineLayout layout : pipelineLayouts_) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(vkDevice, layout, nullptr);
        }
    }
    pipelineLayouts_.clear();

    // Destroy shader modules
    for (VkShaderModule module : shaderModules_) {
        if (module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(vkDevice, module, nullptr);
        }
    }
    shaderModules_.clear();
}

} // namespace hvk
