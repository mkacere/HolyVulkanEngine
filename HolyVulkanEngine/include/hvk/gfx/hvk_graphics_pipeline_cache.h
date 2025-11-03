#ifndef HVK_GRAPHICS_PIPELINE_CACHE_H
#define HVK_GRAPHICS_PIPELINE_CACHE_H

#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <cstdint>

namespace hvk {

    class Device; // fwd

    // ---- Shader stage description (minimal, supports specialization optionally) --
    struct ShaderStageDesc {
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
        VkShaderModule        module = VK_NULL_HANDLE;
        std::string           entry = "main";

        // Optional specialization
        std::vector<VkSpecializationMapEntry> specMap;
        std::vector<uint8_t>                  specData;

        bool operator==(const ShaderStageDesc& o) const;
    };

    // ---- Vertex input state (optional; empty for mesh/vertexless pipelines) ------
    struct VertexInputDesc {
        std::vector<VkVertexInputBindingDescription>   bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
        bool operator==(const VertexInputDesc& o) const;
    };

    // ---- Color blend for each color attachment ----------------------------------
    struct ColorBlendState {
        std::vector<VkPipelineColorBlendAttachmentState> attachments; // size = colorAttachments
        float blendConstants[4] = { 0,0,0,0 };
        bool operator==(const ColorBlendState& o) const;
    };

    // ---- Depth/stencil subset (minimal, common fields) --------------------------
    struct DepthStencilState {
        VkBool32 depthTestEnable = VK_TRUE;
        VkBool32 depthWriteEnable = VK_TRUE;
        VkCompareOp depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;
        VkBool32 depthBoundsTestEnable = VK_FALSE;
        VkBool32 stencilTestEnable = VK_FALSE;
        float minDepthBounds = 0.f, maxDepthBounds = 1.f;
        bool operator==(const DepthStencilState& o) const;
    };

    // ---- Rasterizer / IA / MSAA subset -----------------------------------------
    struct RasterState {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkSampleCountFlagBits rasterSamples = VK_SAMPLE_COUNT_1_BIT;
        VkBool32 alphaToCoverageEnable = VK_FALSE;  // AAA technique for hair/foliage
        VkBool32 depthClampEnable = VK_FALSE;
        VkBool32 rasterizerDiscardEnable = VK_FALSE;
        float lineWidth = 1.0f;
        bool operator==(const RasterState& o) const;
    };

    // ---- Dynamic rendering formats ----------------------------------------------
    struct RenderFormats {
        std::vector<VkFormat> colorFormats;      // size = color attachments
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkFormat stencilFormat = VK_FORMAT_UNDEFINED; // often same as depth
        bool operator==(const RenderFormats& o) const;
    };

    // ---- Pipeline description passed to cache (layout is part of key) -----------
    struct GraphicsPipelineDesc {
        VkPipelineLayout layout = VK_NULL_HANDLE; // required
        std::vector<ShaderStageDesc> stages;      // >=1
        VertexInputDesc  vertexInput;             // optional
        RasterState      raster;
        DepthStencilState depthStencil;
        ColorBlendState  colorBlend;
        RenderFormats    formats;                 // dynamic rendering
        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        // Optional: pipeline flags
        VkPipelineCreateFlags flags = 0;

        bool operator==(const GraphicsPipelineDesc& o) const;
    };

    // ---- Cache class -------------------------------------------------------------
    class GraphicsPipelineCache {
    public:
        GraphicsPipelineCache() = default;
        explicit GraphicsPipelineCache(const Device* device, std::string_view debugBaseName = {});
        ~GraphicsPipelineCache();

        GraphicsPipelineCache(const GraphicsPipelineCache&) = delete;
        GraphicsPipelineCache& operator=(const GraphicsPipelineCache&) = delete;

        GraphicsPipelineCache(GraphicsPipelineCache&&) noexcept;
        GraphicsPipelineCache& operator=(GraphicsPipelineCache&&) noexcept;

        // Returns a cached VkPipeline, creating it if needed.
        VkPipeline get(const GraphicsPipelineDesc& desc);

        // Optional: serialize/merge the driver VkPipelineCache
        std::vector<uint8_t> getCacheBlob() const;
        void mergeFrom(const std::vector<std::vector<uint8_t>>& blobs);

        // Clear all pipelines (destroy); keeps the driver cache alive for faster rebuilds.
        void clearPipelines();

        const Device* device() const { return device_; }

        struct KeyHasher {
            size_t operator()(const GraphicsPipelineDesc& k) const noexcept;
        };

    private:

        void destroy();

        // build helper
        VkPipeline buildPipeline(const GraphicsPipelineDesc& d);

    private:
        const Device* device_ = nullptr;
        std::unordered_map<GraphicsPipelineDesc, VkPipeline, KeyHasher> map_;
        VkPipelineCache driverCache_ = VK_NULL_HANDLE;
        std::string debugBase_;
    };

} // namespace hvk

// Provide std::hash so GraphicsPipelineDesc can be a key (delegates to our hasher)
namespace std {
    template<> struct hash<hvk::GraphicsPipelineDesc> {
        size_t operator()(const hvk::GraphicsPipelineDesc& k) const noexcept {
            return hvk::GraphicsPipelineCache::KeyHasher{}(k);
        }
    };
}

#endif // HVK_GRAPHICS_PIPELINE_CACHE_H
