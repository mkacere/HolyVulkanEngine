#include <hvk/gfx/hvk_pipeline_builder.hpp>
#include <stdexcept>
#include <cstring>

namespace hvk {

PipelineBuilder::PipelineBuilder() {
    // Set sensible defaults
    desc_.raster.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc_.raster.polygonMode = VK_POLYGON_MODE_FILL;
    desc_.raster.cullMode = VK_CULL_MODE_BACK_BIT;
    desc_.raster.frontFace = VK_FRONT_FACE_CLOCKWISE; // Vulkan Y-flip convention
    desc_.raster.rasterSamples = VK_SAMPLE_COUNT_1_BIT;
    desc_.raster.alphaToCoverageEnable = VK_FALSE;

    desc_.depthStencil.depthTestEnable = VK_TRUE;
    desc_.depthStencil.depthWriteEnable = VK_TRUE;
    desc_.depthStencil.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;

    // Default color blend attachment (no blending)
    VkPipelineColorBlendAttachmentState defaultAttachment{};
    defaultAttachment.blendEnable = VK_FALSE;
    defaultAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    desc_.colorBlend.attachments.push_back(defaultAttachment);
}

// ========================================================================
// Required Configuration
// ========================================================================

PipelineBuilder& PipelineBuilder::setShaders(const ShaderModules& shaders) {
    desc_.stages = shaders.stages;
    hasShaders_ = true;
    return *this;
}

PipelineBuilder& PipelineBuilder::setLayout(VkPipelineLayout layout) {
    desc_.layout = layout;
    hasLayout_ = true;
    return *this;
}

// ========================================================================
// Vertex Input Configuration
// ========================================================================

PipelineBuilder& PipelineBuilder::setVertexInput(const VertexInputDesc& vertexInput) {
    desc_.vertexInput = vertexInput;
    return *this;
}

PipelineBuilder& PipelineBuilder::noVertexInput() {
    desc_.vertexInput.bindings.clear();
    desc_.vertexInput.attributes.clear();
    return *this;
}

// ========================================================================
// Preset Configurations
// ========================================================================

PipelineBuilder& PipelineBuilder::makeOpaque() {
    desc_.raster.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc_.raster.polygonMode = VK_POLYGON_MODE_FILL;
    desc_.raster.cullMode = VK_CULL_MODE_BACK_BIT;
    desc_.raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    desc_.raster.alphaToCoverageEnable = VK_FALSE;

    desc_.depthStencil.depthTestEnable = VK_TRUE;
    desc_.depthStencil.depthWriteEnable = VK_TRUE;
    desc_.depthStencil.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;

    // No blending
    desc_.colorBlend.attachments.clear();
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = VK_FALSE;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    desc_.colorBlend.attachments.push_back(attachment);

    return *this;
}

PipelineBuilder& PipelineBuilder::makeMasked() {
    desc_.raster.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc_.raster.polygonMode = VK_POLYGON_MODE_FILL;
    desc_.raster.cullMode = VK_CULL_MODE_NONE;
    desc_.raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    desc_.raster.alphaToCoverageEnable = VK_TRUE; // Alpha masking

    desc_.depthStencil.depthTestEnable = VK_TRUE;
    desc_.depthStencil.depthWriteEnable = VK_TRUE;
    desc_.depthStencil.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;

    // No blending (use alpha-to-coverage instead)
    desc_.colorBlend.attachments.clear();
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = VK_FALSE;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    desc_.colorBlend.attachments.push_back(attachment);

    return *this;
}

PipelineBuilder& PipelineBuilder::makeTransparent() {
    desc_.raster.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc_.raster.polygonMode = VK_POLYGON_MODE_FILL;
    desc_.raster.cullMode = VK_CULL_MODE_NONE;
    desc_.raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    desc_.raster.alphaToCoverageEnable = VK_FALSE;

    desc_.depthStencil.depthTestEnable = VK_TRUE;
    desc_.depthStencil.depthWriteEnable = VK_FALSE; // No depth writes for transparency
    desc_.depthStencil.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;

    // Alpha blending
    desc_.colorBlend.attachments.clear();
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = VK_TRUE;
    attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.colorBlendOp = VK_BLEND_OP_ADD;
    attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    desc_.colorBlend.attachments.push_back(attachment);

    return *this;
}

PipelineBuilder& PipelineBuilder::makeAdditive() {
    desc_.raster.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc_.raster.polygonMode = VK_POLYGON_MODE_FILL;
    desc_.raster.cullMode = VK_CULL_MODE_NONE;
    desc_.raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    desc_.raster.alphaToCoverageEnable = VK_FALSE;

    desc_.depthStencil.depthTestEnable = VK_TRUE;
    desc_.depthStencil.depthWriteEnable = VK_FALSE; // No depth writes for additive
    desc_.depthStencil.depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;

    // Additive blending
    desc_.colorBlend.attachments.clear();
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = VK_TRUE;
    attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE; // Additive
    attachment.colorBlendOp = VK_BLEND_OP_ADD;
    attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    desc_.colorBlend.attachments.push_back(attachment);

    return *this;
}

PipelineBuilder& PipelineBuilder::makeFullscreen() {
    // No vertex input
    noVertexInput();

    desc_.raster.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc_.raster.polygonMode = VK_POLYGON_MODE_FILL;
    desc_.raster.cullMode = VK_CULL_MODE_NONE;
    desc_.raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    desc_.raster.alphaToCoverageEnable = VK_FALSE;

    desc_.depthStencil.depthTestEnable = VK_FALSE;
    desc_.depthStencil.depthWriteEnable = VK_FALSE;

    // No blending
    desc_.colorBlend.attachments.clear();
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.blendEnable = VK_FALSE;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    desc_.colorBlend.attachments.push_back(attachment);

    return *this;
}

// ========================================================================
// Individual State Overrides
// ========================================================================

PipelineBuilder& PipelineBuilder::setTopology(VkPrimitiveTopology topology) {
    desc_.raster.topology = topology;
    return *this;
}

PipelineBuilder& PipelineBuilder::setPolygonMode(VkPolygonMode mode) {
    desc_.raster.polygonMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setCullMode(VkCullModeFlags mode) {
    desc_.raster.cullMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setFrontFace(VkFrontFace frontFace) {
    desc_.raster.frontFace = frontFace;
    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthTest(
    bool testEnable,
    bool writeEnable,
    VkCompareOp compareOp
) {
    desc_.depthStencil.depthTestEnable = testEnable ? VK_TRUE : VK_FALSE;
    desc_.depthStencil.depthWriteEnable = writeEnable ? VK_TRUE : VK_FALSE;
    desc_.depthStencil.depthCompare = compareOp;
    return *this;
}

PipelineBuilder& PipelineBuilder::setBlending(
    bool enable,
    VkBlendFactor srcColor,
    VkBlendFactor dstColor,
    VkBlendFactor srcAlpha,
    VkBlendFactor dstAlpha
) {
    if (desc_.colorBlend.attachments.empty()) {
        desc_.colorBlend.attachments.resize(1);
    }

    auto& attachment = desc_.colorBlend.attachments[0];
    attachment.blendEnable = enable ? VK_TRUE : VK_FALSE;
    attachment.srcColorBlendFactor = srcColor;
    attachment.dstColorBlendFactor = dstColor;
    attachment.colorBlendOp = VK_BLEND_OP_ADD;
    attachment.srcAlphaBlendFactor = srcAlpha;
    attachment.dstAlphaBlendFactor = dstAlpha;
    attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    return *this;
}

PipelineBuilder& PipelineBuilder::setAlphaToCoverage(bool enable) {
    desc_.raster.alphaToCoverageEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::setSampleCount(VkSampleCountFlagBits samples) {
    desc_.raster.rasterSamples = samples;
    return *this;
}

// ========================================================================
// Build
// ========================================================================

VkPipeline PipelineBuilder::build(GraphicsPipelineCache& cache, const RenderFormats& formats) {
    // Validate required configuration
    if (!hasShaders_) {
        throw std::runtime_error("PipelineBuilder::build() - Shaders not set! Call setShaders() first.");
    }
    if (!hasLayout_) {
        throw std::runtime_error("PipelineBuilder::build() - Pipeline layout not set! Call setLayout() first.");
    }

    // Set render formats
    desc_.formats = formats;

    // Create pipeline via cache
    return cache.get(desc_);
}

} // namespace hvk
