#include <hvk/ecs/systems/hvk_billboard_render_system.hpp>
#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/ecs/hvk_components.hpp>
#include <hvk/ecs/hvk_render_components.hpp>
#include <hvk/gfx/hvk_global_descriptors.hpp>
#include <hvk/resources/hvk_mesh.h>
#include <fstream>
#include <cstring>

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
#endif

namespace hvk {

// Helper function to load SPIR-V bytecode from file
static std::vector<uint32_t> loadSpirv(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error(std::string("BillboardRenderSystem: Failed to open SPIR-V: ") + path);
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
    if (bytes.size() % 4) {
        throw std::runtime_error("BillboardRenderSystem: SPIR-V file size not multiple of 4");
    }
    std::vector<uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return words;
}

void BillboardRenderSystem::init(Scene& scene) {
    device_ = &scene.resources().device();
    pipelineCache_ = scene.pipelineCache();

    if (!pipelineCache_) {
        throw std::runtime_error("BillboardRenderSystem: No pipeline cache available");
    }

    // Create shared quad geometry
    createQuadGeometry();

    // Get global descriptor layout
    const DescriptorSetLayout* globalLayout = scene.globalDescriptorLayout();
    if (!globalLayout) {
        throw std::runtime_error("BillboardRenderSystem: Missing global descriptor layout");
    }

    // ========================================================================
    // Create Pipeline Layout
    // ========================================================================

    // For now, just global descriptors (Set 0)
    // Later: Add texture descriptors (Set 1)
    std::vector<VkDescriptorSetLayout> setLayouts = {
        globalLayout->handle()
    };

    // Push constants for instance data (used in non-instanced path)
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(BillboardPushConstants);

    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutCI.pSetLayouts = setLayouts.data();
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pushRange;

    VK_CHECK(vkCreatePipelineLayout(device_->device(), &layoutCI, nullptr, &pipelineLayout_));

    // ========================================================================
    // Load Shaders
    // ========================================================================

    auto vs_spirv = loadSpirv(PROJECT_ROOT "/shaders/billboard.vert.spv");
    auto fs_spirv = loadSpirv(PROJECT_ROOT "/shaders/billboard.frag.spv");

    VkShaderModuleCreateInfo vsCI{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    vsCI.codeSize = vs_spirv.size() * 4;
    vsCI.pCode = vs_spirv.data();
    VK_CHECK(vkCreateShaderModule(device_->device(), &vsCI, nullptr, &vertexShader_));

    VkShaderModuleCreateInfo fsCI{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    fsCI.codeSize = fs_spirv.size() * 4;
    fsCI.pCode = fs_spirv.data();
    VK_CHECK(vkCreateShaderModule(device_->device(), &fsCI, nullptr, &fragmentShader_));

    // ========================================================================
    // Configure Pipeline State
    // ========================================================================

    ShaderStageDesc vs{};
    vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs.module = vertexShader_;
    vs.entry = "main";

    ShaderStageDesc fs{};
    fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs.module = fragmentShader_;
    fs.entry = "main";

    // Vertex input: quad corners + instance data
    VertexInputDesc vertexInput{};

    // Binding 0: Quad vertices (per-vertex)
    VkVertexInputBindingDescription quadBinding{};
    quadBinding.binding = 0;
    quadBinding.stride = sizeof(BillboardVertex);
    quadBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInput.bindings.push_back(quadBinding);

    // Binding 1: Instance data (per-instance)
    VkVertexInputBindingDescription instanceBinding{};
    instanceBinding.binding = 1;
    instanceBinding.stride = sizeof(BillboardInstance);
    instanceBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    vertexInput.bindings.push_back(instanceBinding);

    // Attributes
    VkVertexInputAttributeDescription cornerAttr{};
    cornerAttr.binding = 0;
    cornerAttr.location = 0;
    cornerAttr.format = VK_FORMAT_R32G32_SFLOAT;
    cornerAttr.offset = offsetof(BillboardVertex, corner);
    vertexInput.attributes.push_back(cornerAttr);

    VkVertexInputAttributeDescription posAttr{};
    posAttr.binding = 1;
    posAttr.location = 1;
    posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    posAttr.offset = offsetof(BillboardInstance, position);
    vertexInput.attributes.push_back(posAttr);

    VkVertexInputAttributeDescription colorAttr{};
    colorAttr.binding = 1;
    colorAttr.location = 2;
    colorAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    colorAttr.offset = offsetof(BillboardInstance, color);
    vertexInput.attributes.push_back(colorAttr);

    VkVertexInputAttributeDescription sizeAttr{};
    sizeAttr.binding = 1;
    sizeAttr.location = 3;
    sizeAttr.format = VK_FORMAT_R32G32_SFLOAT;
    sizeAttr.offset = offsetof(BillboardInstance, size);
    vertexInput.attributes.push_back(sizeAttr);

    VkVertexInputAttributeDescription modeAttr{};
    modeAttr.binding = 1;
    modeAttr.location = 4;
    modeAttr.format = VK_FORMAT_R32_UINT;
    modeAttr.offset = offsetof(BillboardInstance, mode);
    vertexInput.attributes.push_back(modeAttr);

    VkVertexInputAttributeDescription uvAttr{};
    uvAttr.binding = 1;
    uvAttr.location = 5;
    uvAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    uvAttr.offset = offsetof(BillboardInstance, uvRect);
    vertexInput.attributes.push_back(uvAttr);

    // Rasterization
    RasterState raster{};
    raster.cullMode = VK_CULL_MODE_NONE;  // Billboards are double-sided
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.rasterSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil (read depth, no write - billboards don't occlude)
    DepthStencilState depthStencil{};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;  // Transparent objects don't write depth
    depthStencil.depthCompare = VK_COMPARE_OP_LESS;

    // Render formats
    RenderFormats formats{};
    formats.colorFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    formats.depthFormat = VK_FORMAT_D32_SFLOAT;

    // ========================================================================
    // Pipeline 1: Alpha Blending
    // ========================================================================

    ColorBlendState cbsAlpha{};
    cbsAlpha.attachments.resize(1);
    cbsAlpha.attachments[0].blendEnable = VK_TRUE;
    cbsAlpha.attachments[0].colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbsAlpha.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbsAlpha.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbsAlpha.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    cbsAlpha.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbsAlpha.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbsAlpha.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

    GraphicsPipelineDesc gpAlpha{};
    gpAlpha.layout = pipelineLayout_;
    gpAlpha.stages = { vs, fs };
    gpAlpha.vertexInput = vertexInput;
    gpAlpha.raster = raster;
    gpAlpha.depthStencil = depthStencil;
    gpAlpha.colorBlend = cbsAlpha;
    gpAlpha.formats = formats;
    gpAlpha.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    alphaPipeline_ = pipelineCache_->get(gpAlpha);

    // ========================================================================
    // Pipeline 2: Additive Blending
    // ========================================================================

    ColorBlendState cbsAdditive{};
    cbsAdditive.attachments.resize(1);
    cbsAdditive.attachments[0].blendEnable = VK_TRUE;
    cbsAdditive.attachments[0].colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbsAdditive.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbsAdditive.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Additive
    cbsAdditive.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    cbsAdditive.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbsAdditive.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbsAdditive.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

    GraphicsPipelineDesc gpAdditive{};
    gpAdditive.layout = pipelineLayout_;
    gpAdditive.stages = { vs, fs };
    gpAdditive.vertexInput = vertexInput;
    gpAdditive.raster = raster;
    gpAdditive.depthStencil = depthStencil;
    gpAdditive.colorBlend = cbsAdditive;
    gpAdditive.formats = formats;
    gpAdditive.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    additivePipeline_ = pipelineCache_->get(gpAdditive);

    std::cout << "BillboardRenderSystem: Initialized with 2 pipelines (alpha, additive)" << std::endl;
}

void BillboardRenderSystem::createQuadGeometry() {
    // Create shared quad (4 vertices, 6 indices)
    BillboardVertex vertices[4] = {
        { glm::vec2(-1.0f, -1.0f) },  // Bottom-left
        { glm::vec2( 1.0f, -1.0f) },  // Bottom-right
        { glm::vec2(-1.0f,  1.0f) },  // Top-left
        { glm::vec2( 1.0f,  1.0f) }   // Top-right
    };

    uint16_t indices[6] = {
        0, 1, 2,  // Triangle 1
        2, 1, 3   // Triangle 2
    };

    // Create vertex buffer
    GpuBufferCreateInfo vertexCI{};
    vertexCI.device = device_;
    vertexCI.size = sizeof(vertices);
    vertexCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexCI.memUsage = VMA_MEMORY_USAGE_AUTO;
    vertexCI.allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    vertexCI.debugName = "billboard_quad_vertices";

    quadVertexBuffer_ = GpuBuffer(vertexCI);

    // Upload vertex data
    void* data = quadVertexBuffer_.map();
    memcpy(data, vertices, sizeof(vertices));
    quadVertexBuffer_.unmap();

    // Create index buffer
    GpuBufferCreateInfo indexCI{};
    indexCI.device = device_;
    indexCI.size = sizeof(indices);
    indexCI.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    indexCI.memUsage = VMA_MEMORY_USAGE_AUTO;
    indexCI.allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                         VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    indexCI.debugName = "billboard_quad_indices";

    quadIndexBuffer_ = GpuBuffer(indexCI);

    // Upload index data
    data = quadIndexBuffer_.map();
    memcpy(data, indices, sizeof(indices));
    quadIndexBuffer_.unmap();
}

void BillboardRenderSystem::render(Scene& scene, CmdList& cmd) {
    auto& registry = scene.registry();

    // Get global descriptor set
    GlobalDescriptorSet* globalDescSet = scene.globalDescriptorSet();
    if (!globalDescSet) {
        return;
    }

    VkDescriptorSet globalDescriptorSet = globalDescSet->get(scene.frameIndex());

    // Collect billboards and separate by blend mode
    instanceData_.clear();
    std::vector<BillboardInstance> alphaInstances;
    std::vector<BillboardInstance> additiveInstances;

    auto view = registry.view<BillboardComponent, TransformComponent>();

    for (auto entity : view) {
        const auto& billboard = view.get<BillboardComponent>(entity);
        const auto& transform = view.get<TransformComponent>(entity);

        if (!billboard.visible) {
            continue;
        }

        // Build instance data
        BillboardInstance instance{};
        instance.position = transform.position;
        instance.color = billboard.color;
        instance.size = billboard.size;
        instance.mode = static_cast<uint32_t>(billboard.mode);
        instance.uvRect = billboard.uvRect;

        // Separate by blend mode
        if (billboard.additiveBlend) {
            additiveInstances.push_back(instance);
        } else {
            alphaInstances.push_back(instance);
        }
    }

    if (alphaInstances.empty() && additiveInstances.empty()) {
        return;
    }

    // Bind global descriptors
    vkCmdBindDescriptorSets(
        cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_, 0, 1, &globalDescriptorSet, 0, nullptr
    );

    // Bind quad geometry
    VkBuffer vertexBuffers[] = { quadVertexBuffer_.handle() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd.handle(), 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd.handle(), quadIndexBuffer_.handle(), 0, VK_INDEX_TYPE_UINT16);

    // ========================================================================
    // Pass 1: Alpha-blended billboards
    // ========================================================================
    if (!alphaInstances.empty()) {
        updateInstanceBuffer(alphaInstances);

        VkBuffer instanceBuffers[] = { instanceBuffer_.handle() };
        VkDeviceSize instanceOffsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd.handle(), 1, 1, instanceBuffers, instanceOffsets);

        cmd.bindGraphicsPipeline(alphaPipeline_);
        vkCmdDrawIndexed(cmd.handle(), 6, static_cast<uint32_t>(alphaInstances.size()), 0, 0, 0);
    }

    // ========================================================================
    // Pass 2: Additive-blended billboards (glows)
    // ========================================================================
    if (!additiveInstances.empty()) {
        updateInstanceBuffer(additiveInstances);

        VkBuffer instanceBuffers[] = { instanceBuffer_.handle() };
        VkDeviceSize instanceOffsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd.handle(), 1, 1, instanceBuffers, instanceOffsets);

        cmd.bindGraphicsPipeline(additivePipeline_);
        vkCmdDrawIndexed(cmd.handle(), 6, static_cast<uint32_t>(additiveInstances.size()), 0, 0, 0);
    }
}

void BillboardRenderSystem::updateInstanceBuffer(const std::vector<BillboardInstance>& instances) {
    size_t requiredSize = instances.size() * sizeof(BillboardInstance);

    // Recreate buffer if too small
    if (instanceBuffer_.handle() == VK_NULL_HANDLE || instanceBuffer_.size() < requiredSize) {
        GpuBufferCreateInfo bufferCI{};
        bufferCI.device = device_;
        bufferCI.size = requiredSize * 2;  // Allocate 2x for growth
        bufferCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferCI.memUsage = VMA_MEMORY_USAGE_AUTO;
        bufferCI.allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
        bufferCI.persistentMap = true;  // Keep mapped for per-frame updates
        bufferCI.debugName = "billboard_instances";

        instanceBuffer_ = GpuBuffer(bufferCI);
    }

    // Upload instance data
    void* data = instanceBuffer_.map();
    memcpy(data, instances.data(), requiredSize);
    instanceBuffer_.flush();
    // No unmap - persistentMap keeps it mapped
}

void BillboardRenderSystem::cleanup() {
    if (!device_) {
        return;
    }

    VkDevice dev = device_->device();

    // Destroy buffers (RAII handles cleanup automatically via move-assignment to empty buffer)
    quadVertexBuffer_ = GpuBuffer{};
    quadIndexBuffer_ = GpuBuffer{};
    instanceBuffer_ = GpuBuffer{};

    // Destroy pipelines (managed by cache, just null handles)
    alphaPipeline_ = VK_NULL_HANDLE;
    additivePipeline_ = VK_NULL_HANDLE;

    // Destroy shaders
    if (vertexShader_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(dev, vertexShader_, nullptr);
        vertexShader_ = VK_NULL_HANDLE;
    }
    if (fragmentShader_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(dev, fragmentShader_, nullptr);
        fragmentShader_ = VK_NULL_HANDLE;
    }

    // Destroy pipeline layout
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    device_ = nullptr;
    pipelineCache_ = nullptr;
}

} // namespace hvk
