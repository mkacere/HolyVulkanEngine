#include "pch.h"

#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <hvk/gfx/hvk_device.h>

namespace hvk {

    static inline void hash_combine(size_t& h, size_t v) {
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }
    static inline size_t hash_bytes(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        size_t h = 1469598103934665603ull;
        for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
        return h;
    }

    // ================= equality operators =================

    bool ShaderStageDesc::operator==(const ShaderStageDesc& o) const {
        if (stage != o.stage || module != o.module || entry != o.entry) return false;
        if (specMap.size() != o.specMap.size() || specData.size() != o.specData.size()) return false;
        if (!specMap.empty() && std::memcmp(specMap.data(), o.specMap.data(), specMap.size() * sizeof(VkSpecializationMapEntry)) != 0) return false;
        if (!specData.empty() && std::memcmp(specData.data(), o.specData.data(), specData.size()) != 0) return false;
        return true;
    }

    bool VertexInputDesc::operator==(const VertexInputDesc& o) const {
        if (bindings.size() != o.bindings.size() || attributes.size() != o.attributes.size()) return false;
        if (!bindings.empty() && std::memcmp(bindings.data(), o.bindings.data(), bindings.size() * sizeof(VkVertexInputBindingDescription)) != 0) return false;
        if (!attributes.empty() && std::memcmp(attributes.data(), o.attributes.data(), attributes.size() * sizeof(VkVertexInputAttributeDescription)) != 0) return false;
        return true;
    }

    bool ColorBlendState::operator==(const ColorBlendState& o) const {
        if (attachments.size() != o.attachments.size()) return false;
        if (!attachments.empty() && std::memcmp(attachments.data(), o.attachments.data(), attachments.size() * sizeof(VkPipelineColorBlendAttachmentState)) != 0) return false;
        for (int i = 0; i < 4; ++i) if (blendConstants[i] != o.blendConstants[i]) return false;
        return true;
    }

    bool DepthStencilState::operator==(const DepthStencilState& o) const {
        return depthTestEnable == o.depthTestEnable &&
            depthWriteEnable == o.depthWriteEnable &&
            depthCompare == o.depthCompare &&
            depthBoundsTestEnable == o.depthBoundsTestEnable &&
            stencilTestEnable == o.stencilTestEnable &&
            minDepthBounds == o.minDepthBounds &&
            maxDepthBounds == o.maxDepthBounds;
    }

    bool RasterState::operator==(const RasterState& o) const {
        return topology == o.topology &&
            polygonMode == o.polygonMode &&
            cullMode == o.cullMode &&
            frontFace == o.frontFace &&
            rasterSamples == o.rasterSamples &&
            alphaToCoverageEnable == o.alphaToCoverageEnable &&
            depthClampEnable == o.depthClampEnable &&
            rasterizerDiscardEnable == o.rasterizerDiscardEnable &&
            lineWidth == o.lineWidth;
    }

    bool RenderFormats::operator==(const RenderFormats& o) const {
        if (colorFormats.size() != o.colorFormats.size()) return false;
        if (!colorFormats.empty() && std::memcmp(colorFormats.data(), o.colorFormats.data(), colorFormats.size() * sizeof(VkFormat)) != 0) return false;
        return depthFormat == o.depthFormat && stencilFormat == o.stencilFormat;
    }

    bool GraphicsPipelineDesc::operator==(const GraphicsPipelineDesc& o) const {
        return layout == o.layout &&
            stages == o.stages &&
            vertexInput == o.vertexInput &&
            raster == o.raster &&
            depthStencil == o.depthStencil &&
            colorBlend == o.colorBlend &&
            formats == o.formats &&
            dynamicStates == o.dynamicStates &&
            flags == o.flags;
    }

    // ================= hasher =================

    size_t GraphicsPipelineCache::KeyHasher::operator()(const GraphicsPipelineDesc& k) const noexcept {
        size_t h = 0;
        hash_combine(h, std::hash<uint64_t>{}(reinterpret_cast<uint64_t>(k.layout)));

        // stages
        hash_combine(h, std::hash<size_t>{}(k.stages.size()));
        for (const auto& s : k.stages) {
            hash_combine(h, std::hash<uint32_t>{}(s.stage));
            hash_combine(h, std::hash<uint64_t>{}(reinterpret_cast<uint64_t>(s.module)));
            hash_combine(h, std::hash<std::string>{}(s.entry));
            if (!s.specMap.empty()) hash_combine(h, hash_bytes(s.specMap.data(), s.specMap.size() * sizeof(VkSpecializationMapEntry)));
            if (!s.specData.empty()) hash_combine(h, hash_bytes(s.specData.data(), s.specData.size()));
        }

        // vertex input
        if (!k.vertexInput.bindings.empty())   hash_combine(h, hash_bytes(k.vertexInput.bindings.data(), k.vertexInput.bindings.size() * sizeof(VkVertexInputBindingDescription)));
        if (!k.vertexInput.attributes.empty()) hash_combine(h, hash_bytes(k.vertexInput.attributes.data(), k.vertexInput.attributes.size() * sizeof(VkVertexInputAttributeDescription)));

        // raster
        hash_combine(h, std::hash<uint32_t>{}(k.raster.topology));
        hash_combine(h, std::hash<uint32_t>{}(k.raster.polygonMode));
        hash_combine(h, std::hash<uint32_t>{}(k.raster.cullMode));
        hash_combine(h, std::hash<uint32_t>{}(k.raster.frontFace));
        hash_combine(h, std::hash<uint32_t>{}(k.raster.rasterSamples));
        hash_combine(h, std::hash<uint32_t>{}(k.raster.alphaToCoverageEnable));
        hash_combine(h, std::hash<uint32_t>{}(k.raster.depthClampEnable));
        hash_combine(h, std::hash<uint32_t>{}(k.raster.rasterizerDiscardEnable));
        hash_combine(h, std::hash<float>{}(k.raster.lineWidth));

        // depth/stencil
        hash_combine(h, std::hash<uint32_t>{}(k.depthStencil.depthTestEnable));
        hash_combine(h, std::hash<uint32_t>{}(k.depthStencil.depthWriteEnable));
        hash_combine(h, std::hash<uint32_t>{}(k.depthStencil.depthCompare));
        hash_combine(h, std::hash<uint32_t>{}(k.depthStencil.depthBoundsTestEnable));
        hash_combine(h, std::hash<uint32_t>{}(k.depthStencil.stencilTestEnable));
        hash_combine(h, std::hash<float>{}(k.depthStencil.minDepthBounds));
        hash_combine(h, std::hash<float>{}(k.depthStencil.maxDepthBounds));

        // color blend
        if (!k.colorBlend.attachments.empty()) hash_combine(h, hash_bytes(k.colorBlend.attachments.data(), k.colorBlend.attachments.size() * sizeof(VkPipelineColorBlendAttachmentState)));
        for (int i = 0; i < 4; ++i) hash_combine(h, std::hash<float>{}(k.colorBlend.blendConstants[i]));

        // dynamic rendering formats
        if (!k.formats.colorFormats.empty()) hash_combine(h, hash_bytes(k.formats.colorFormats.data(), k.formats.colorFormats.size() * sizeof(VkFormat)));
        hash_combine(h, std::hash<uint32_t>{}(k.formats.depthFormat));
        hash_combine(h, std::hash<uint32_t>{}(k.formats.stencilFormat));

        // dynamic states
        if (!k.dynamicStates.empty()) hash_combine(h, hash_bytes(k.dynamicStates.data(), k.dynamicStates.size() * sizeof(VkDynamicState)));

        hash_combine(h, std::hash<uint32_t>{}(k.flags));
        return h;
    }

    // ================= cache class =================

    GraphicsPipelineCache::GraphicsPipelineCache(const Device* device, std::string_view debugBase)
        : device_(device)
        , debugBase_(debugBase.empty() ? "gp_cache" : std::string(debugBase))
    {
        if (!device_) throw std::invalid_argument("GraphicsPipelineCache: device is null");
        // Create driver-level pipeline cache (empty)
        VkPipelineCacheCreateInfo ci{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
        VK_CHECK(vkCreatePipelineCache(device_->device(), &ci, nullptr, &driverCache_));
        if (!debugBase_.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_PIPELINE_CACHE,
                reinterpret_cast<uint64_t>(driverCache_), debugBase_ + "/driver_cache");
        }
    }

    GraphicsPipelineCache::~GraphicsPipelineCache() { destroy(); }

    GraphicsPipelineCache::GraphicsPipelineCache(GraphicsPipelineCache&& o) noexcept {
        device_ = o.device_; o.device_ = nullptr;
        map_ = std::move(o.map_);
        driverCache_ = o.driverCache_; o.driverCache_ = VK_NULL_HANDLE;
        debugBase_ = std::move(o.debugBase_);
    }

    GraphicsPipelineCache& GraphicsPipelineCache::operator=(GraphicsPipelineCache&& o) noexcept {
        if (this != &o) {
            destroy();
            device_ = o.device_; o.device_ = nullptr;
            map_ = std::move(o.map_);
            driverCache_ = o.driverCache_; o.driverCache_ = VK_NULL_HANDLE;
            debugBase_ = std::move(o.debugBase_);
        }
        return *this;
    }

    void GraphicsPipelineCache::destroy() {
        if (!device_) return;
        clearPipelines();
        if (driverCache_) {
            vkDestroyPipelineCache(device_->device(), driverCache_, nullptr);
            driverCache_ = VK_NULL_HANDLE;
        }
    }

    void GraphicsPipelineCache::clearPipelines() {
        if (!device_) return;
        for (auto& kv : map_) vkDestroyPipeline(device_->device(), kv.second, nullptr);
        map_.clear();
    }

    std::vector<uint8_t> GraphicsPipelineCache::getCacheBlob() const {
        if (!driverCache_) return {};
        size_t sz = 0;
        VK_CHECK(vkGetPipelineCacheData(device_->device(), driverCache_, &sz, nullptr));
        std::vector<uint8_t> blob(sz);
        if (sz) VK_CHECK(vkGetPipelineCacheData(device_->device(), driverCache_, &sz, blob.data()));
        return blob;
    }

    void GraphicsPipelineCache::mergeFrom(const std::vector<std::vector<uint8_t>>& blobs) {
        if (!driverCache_ || blobs.empty()) return;
        std::vector<VkPipelineCache> temps;
        temps.reserve(blobs.size());
        for (const auto& b : blobs) {
            VkPipelineCacheCreateInfo ci{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
            ci.initialDataSize = b.size();
            ci.pInitialData = b.data();
            VkPipelineCache tmp = VK_NULL_HANDLE;
            VK_CHECK(vkCreatePipelineCache(device_->device(), &ci, nullptr, &tmp));
            temps.push_back(tmp);
        }
        if (!temps.empty()) {
            VK_CHECK(vkMergePipelineCaches(device_->device(), driverCache_,
                static_cast<uint32_t>(temps.size()), temps.data()));
            for (auto t : temps) vkDestroyPipelineCache(device_->device(), t, nullptr);
        }
    }

    VkPipeline GraphicsPipelineCache::get(const GraphicsPipelineDesc& desc) {
        auto it = map_.find(desc);
        if (it != map_.end()) return it->second;

        VkPipeline p = buildPipeline(desc);
        if (!debugBase_.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_PIPELINE,
                reinterpret_cast<uint64_t>(p), debugBase_ + "/pipeline#" + std::to_string(map_.size()));
        }
        map_.emplace(desc, p);
        return p;
    }

    VkPipeline GraphicsPipelineCache::buildPipeline(const GraphicsPipelineDesc& d) {
        if (!d.layout) throw std::invalid_argument("GraphicsPipelineCache::buildPipeline: layout is null");
        if (d.stages.empty()) throw std::invalid_argument("GraphicsPipelineCache::buildPipeline: no shader stages");
        // ---- stages ----
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<VkSpecializationInfo> specs; specs.reserve(d.stages.size());
        stages.reserve(d.stages.size());
        for (const auto& s : d.stages) {
            VkPipelineShaderStageCreateInfo ci{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            ci.stage = s.stage;
            ci.module = s.module;
            ci.pName = s.entry.c_str();
            if (!s.specMap.empty() || !s.specData.empty()) {
                specs.emplace_back();
                auto& sp = specs.back();
                sp.mapEntryCount = static_cast<uint32_t>(s.specMap.size());
                sp.pMapEntries = s.specMap.data();
                sp.dataSize = s.specData.size();
                sp.pData = s.specData.data();
                ci.pSpecializationInfo = &sp;
            }
            stages.push_back(ci);
        }

        // ---- vertex input ----
        VkPipelineVertexInputStateCreateInfo vi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vi.vertexBindingDescriptionCount = static_cast<uint32_t>(d.vertexInput.bindings.size());
        vi.pVertexBindingDescriptions = d.vertexInput.bindings.data();
        vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(d.vertexInput.attributes.size());
        vi.pVertexAttributeDescriptions = d.vertexInput.attributes.data();

        // ---- input assembly ----
        VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        ia.topology = d.raster.topology;
        ia.primitiveRestartEnable = VK_FALSE;

        // ---- viewport/scissor (dynamic; just set counts = 1) ----
        VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        vp.viewportCount = 1; vp.scissorCount = 1;

        // ---- rasterization ----
        VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rs.depthClampEnable = d.raster.depthClampEnable;
        rs.rasterizerDiscardEnable = d.raster.rasterizerDiscardEnable;
        rs.polygonMode = d.raster.polygonMode;
        rs.cullMode = d.raster.cullMode;
        rs.frontFace = d.raster.frontFace;
        rs.lineWidth = d.raster.lineWidth;

        // ---- multisample ----
        VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        ms.rasterizationSamples = d.raster.rasterSamples;
        ms.alphaToCoverageEnable = d.raster.alphaToCoverageEnable;
        ms.minSampleShading = 0.f;

        // ---- depth/stencil ----
        VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        ds.depthTestEnable = d.depthStencil.depthTestEnable;
        ds.depthWriteEnable = d.depthStencil.depthWriteEnable;
        ds.depthCompareOp = d.depthStencil.depthCompare;
        ds.depthBoundsTestEnable = d.depthStencil.depthBoundsTestEnable;
        ds.stencilTestEnable = d.depthStencil.stencilTestEnable;
        ds.minDepthBounds = d.depthStencil.minDepthBounds;
        ds.maxDepthBounds = d.depthStencil.maxDepthBounds;

        // ---- color blend ----
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        cb.attachmentCount = static_cast<uint32_t>(d.colorBlend.attachments.size());
        cb.pAttachments = d.colorBlend.attachments.data();
        std::memcpy(cb.blendConstants, d.colorBlend.blendConstants, sizeof(cb.blendConstants));

        // ---- dynamic states ----
        VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dyn.dynamicStateCount = static_cast<uint32_t>(d.dynamicStates.size());
        dyn.pDynamicStates = d.dynamicStates.data();

        // ---- pipeline rendering (dynamic rendering) ----
        VkPipelineRenderingCreateInfo rendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        rendering.colorAttachmentCount = static_cast<uint32_t>(d.formats.colorFormats.size());
        rendering.pColorAttachmentFormats = d.formats.colorFormats.data();
        rendering.depthAttachmentFormat = d.formats.depthFormat;
        rendering.stencilAttachmentFormat = d.formats.stencilFormat;

        // ---- create info ----
        VkGraphicsPipelineCreateInfo gp{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gp.pNext = &rendering;
        gp.flags = d.flags;
        gp.stageCount = static_cast<uint32_t>(stages.size());
        gp.pStages = stages.data();
        gp.pVertexInputState = &vi;
        gp.pInputAssemblyState = &ia;
        gp.pViewportState = &vp;
        gp.pRasterizationState = &rs;
        gp.pMultisampleState = &ms;
        gp.pDepthStencilState = (d.formats.depthFormat == VK_FORMAT_UNDEFINED && d.formats.stencilFormat == VK_FORMAT_UNDEFINED) ? nullptr : &ds;
        gp.pColorBlendState = &cb;
        gp.pDynamicState = &dyn;
        gp.layout = d.layout;
        gp.renderPass = VK_NULL_HANDLE;   // dynamic rendering path
        gp.subpass = 0;

        VkPipeline pipeline = VK_NULL_HANDLE;
        VK_CHECK(vkCreateGraphicsPipelines(device_->device(), driverCache_, 1, &gp, nullptr, &pipeline));
        return pipeline;
    }

} // namespace hvk
