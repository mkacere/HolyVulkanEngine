#include <hvk/ecs/systems/hvk_mesh_render_system.hpp>
#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/ecs/hvk_components.hpp>
#include <hvk/ecs/hvk_render_components.hpp>
#include <hvk/gfx/hvk_global_descriptors.hpp>
#include <hvk/gfx/hvk_pipeline_layout_cache.h>
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
        throw std::runtime_error(std::string("MeshRenderSystem: Failed to open SPIR-V: ") + path);
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
    if (bytes.size() % 4) {
        throw std::runtime_error("MeshRenderSystem: SPIR-V file size not multiple of 4");
    }
    std::vector<uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return words;
}

void MeshRenderSystem::init(Scene& scene) {
    device_ = &scene.resources().device();
    pipelineCache_ = scene.pipelineCache();

    if (!pipelineCache_) {
        throw std::runtime_error("MeshRenderSystem: No pipeline cache available");
    }

    // Get descriptor set layouts
    const DescriptorSetLayout* globalLayout = scene.globalDescriptorLayout();
    const DescriptorSetLayout* materialLayout = scene.resources().materialDescriptorLayout();

    if (!globalLayout || !materialLayout) {
        throw std::runtime_error("MeshRenderSystem: Missing descriptor layouts");
    }

    // ========================================================================
    // Create Pipeline Layout
    // ========================================================================

    // Build descriptor set layouts array
    std::vector<VkDescriptorSetLayout> setLayouts = {
        globalLayout->handle(),
        materialLayout->handle()
    };

    // Push constants: model matrix (64) + normal matrix (64) + MaterialParams (80) = 208 bytes
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = 208;

    // Create the pipeline layout
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

    auto vs_spirv = loadSpirv(PROJECT_ROOT "/shaders/model.vert.spv");
    auto fs_spirv = loadSpirv(PROJECT_ROOT "/shaders/model.frag.spv");

    // Create vertex shader module
    VkShaderModuleCreateInfo vsCI{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    vsCI.codeSize = vs_spirv.size() * 4;
    vsCI.pCode = vs_spirv.data();
    VK_CHECK(vkCreateShaderModule(device_->device(), &vsCI, nullptr, &vertexShader_));

    // Create fragment shader module
    VkShaderModuleCreateInfo fsCI{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    fsCI.codeSize = fs_spirv.size() * 4;
    fsCI.pCode = fs_spirv.data();
    VK_CHECK(vkCreateShaderModule(device_->device(), &fsCI, nullptr, &fragmentShader_));

    // ========================================================================
    // Configure Shared Pipeline State
    // ========================================================================

    // Shader stages (shared by all pipelines)
    ShaderStageDesc vs{};
    vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs.module = vertexShader_;
    vs.entry = "main";

    ShaderStageDesc fs{};
    fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs.module = fragmentShader_;
    fs.entry = "main";

    // Vertex input (matches hvk::Vertex)
    VertexInputDesc vertexInput{};
    vertexInput.bindings.push_back(Vertex::getBindingDescription());
    vertexInput.attributes = Vertex::getAttributeDescriptions();

    // Rasterization state (shared by all pipelines)
    RasterState raster{};
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.rasterSamples = VK_SAMPLE_COUNT_1_BIT;  // Application uses MSAA=1

    // Render formats (hardcoded to match Application defaults)
    // Note: Application uses VK_FORMAT_B8G8R8A8_SRGB or VK_FORMAT_R8G8B8A8_SRGB
    // and VK_FORMAT_D32_SFLOAT for depth
    RenderFormats formats{};
    formats.colorFormats = { VK_FORMAT_B8G8R8A8_SRGB };  // Most common swapchain format
    formats.depthFormat = VK_FORMAT_D32_SFLOAT;

    // ========================================================================
    // Pipeline 1: Opaque Materials
    // ========================================================================

    DepthStencilState dssOpaque{};
    dssOpaque.depthTestEnable = VK_TRUE;
    dssOpaque.depthWriteEnable = VK_TRUE;   // Write depth
    dssOpaque.depthCompare = VK_COMPARE_OP_LESS;

    ColorBlendState cbsOpaque{};
    cbsOpaque.attachments.resize(1);
    cbsOpaque.attachments[0].blendEnable = VK_FALSE;  // No blending
    cbsOpaque.attachments[0].colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    GraphicsPipelineDesc gpOpaque{};
    gpOpaque.layout = pipelineLayout_;
    gpOpaque.stages = { vs, fs };
    gpOpaque.vertexInput = vertexInput;
    gpOpaque.raster = raster;
    gpOpaque.depthStencil = dssOpaque;
    gpOpaque.colorBlend = cbsOpaque;
    gpOpaque.formats = formats;
    gpOpaque.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    opaquePipeline_ = pipelineCache_->get(gpOpaque);

    // ========================================================================
    // Pipeline 2: Masked Materials (Alpha Cutout with Alpha-to-Coverage)
    // ========================================================================

    DepthStencilState dssMasked{};
    dssMasked.depthTestEnable = VK_TRUE;
    dssMasked.depthWriteEnable = VK_TRUE;   // Write depth (cutout acts like opaque)
    dssMasked.depthCompare = VK_COMPARE_OP_LESS;

    ColorBlendState cbsMasked{};
    cbsMasked.attachments.resize(1);
    cbsMasked.attachments[0].blendEnable = VK_FALSE;  // No blending (alpha-to-coverage handles edges)
    cbsMasked.attachments[0].colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Enable alpha-to-coverage for smooth hair/eyelash edges (AAA industry standard)
    RasterState rasterMasked = raster;
    rasterMasked.alphaToCoverageEnable = VK_TRUE;

    GraphicsPipelineDesc gpMasked{};
    gpMasked.layout = pipelineLayout_;
    gpMasked.stages = { vs, fs };
    gpMasked.vertexInput = vertexInput;
    gpMasked.raster = rasterMasked;  // Use raster state with alpha-to-coverage
    gpMasked.depthStencil = dssMasked;
    gpMasked.colorBlend = cbsMasked;
    gpMasked.formats = formats;
    gpMasked.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    maskedPipeline_ = pipelineCache_->get(gpMasked);

    // ========================================================================
    // Pipeline 3: Blended Materials (True Transparency)
    // ========================================================================

    DepthStencilState dssBlended{};
    dssBlended.depthTestEnable = VK_TRUE;
    dssBlended.depthWriteEnable = VK_FALSE;  // NO depth write (transparency fix!)
    dssBlended.depthCompare = VK_COMPARE_OP_LESS;

    ColorBlendState cbsBlended{};
    cbsBlended.attachments.resize(1);
    cbsBlended.attachments[0].blendEnable = VK_TRUE;  // Enable blending
    cbsBlended.attachments[0].colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Standard alpha blending: srcAlpha * srcColor + (1 - srcAlpha) * dstColor
    cbsBlended.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbsBlended.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbsBlended.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    cbsBlended.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbsBlended.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbsBlended.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

    GraphicsPipelineDesc gpBlended{};
    gpBlended.layout = pipelineLayout_;
    gpBlended.stages = { vs, fs };
    gpBlended.vertexInput = vertexInput;
    gpBlended.raster = raster;
    gpBlended.depthStencil = dssBlended;
    gpBlended.colorBlend = cbsBlended;
    gpBlended.formats = formats;
    gpBlended.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    blendedPipeline_ = pipelineCache_->get(gpBlended);

    std::cout << "MeshRenderSystem: Initialized with 3 pipelines (opaque, masked, blended)" << std::endl;
}

void MeshRenderSystem::render(Scene& scene, CmdList& cmd) {
    auto& registry = scene.registry();

    // Get global descriptor set from scene
    GlobalDescriptorSet* globalDescSet = scene.globalDescriptorSet();
    if (!globalDescSet) {
        // No global descriptors available - can't render
        return;
    }

    // Get current frame's descriptor set
    VkDescriptorSet globalDescriptorSet = globalDescSet->get(scene.frameIndex());

    // Get camera position for transparent object sorting
    glm::vec3 cameraPosition(0.0f, 5.0f, 10.0f);  // Default fallback
    auto activeCamera = scene.getActiveCamera();
    if (activeCamera != entt::null) {
        const auto* camTransform = registry.try_get<TransformComponent>(activeCamera);
        if (camTransform) {
            cameraPosition = camTransform->position;
        }
    }

    // Bind global descriptor set (Set 0) - shared by all passes
    vkCmdBindDescriptorSets(
        cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_, 0, 1, &globalDescriptorSet, 0, nullptr
    );

    // ========================================================================
    // OPTIMIZATION: Single-pass iteration over all entities
    // Collect entities by render pass to avoid iterating 3 times
    // ========================================================================

    struct RenderBatch {
        const Model* model;
        const glm::mat4* transform;
        float distanceFromCamera;  // For transparency sorting
    };

    std::vector<RenderBatch> opaqueBatch;
    std::vector<RenderBatch> maskedBatch;
    std::vector<RenderBatch> blendedBatch;

    opaqueBatch.reserve(64);
    maskedBatch.reserve(16);
    blendedBatch.reserve(16);

    // OPTIMIZATION: Simple distance-based culling
    // Skip entities beyond a reasonable render distance
    constexpr float MAX_RENDER_DISTANCE_SQ = 1000.0f * 1000.0f;  // 1000 units

    // Single iteration: sort entities into batches
    auto view = registry.view<MeshComponent, LocalToWorld>();
    for (auto entity : view) {
        const auto& [meshComp, ltw] = view.get<MeshComponent, LocalToWorld>(entity);

        // Skip if not visible
        if (!meshComp.visible) {
            continue;
        }

        // Get model from resource manager
        const Model* model = scene.resources().getModel(meshComp.modelHandle);
        if (!model) {
            continue;
        }

        // Calculate distance from camera for sorting and culling
        glm::vec3 entityPos = glm::vec3(ltw.matrix[3]);
        float distSq = glm::distance2(cameraPosition, entityPos);

        // OPTIMIZATION: Distance-based culling (skip objects too far away)
        if (distSq > MAX_RENDER_DISTANCE_SQ) [[unlikely]] {
            continue;
        }

        // Check which render passes this model needs
        // Note: Models can have mixed materials (opaque + masked + blended)
        // For now, we add to all relevant batches
        // TODO: Per-material batching for even better performance

        RenderBatch batch{ model, &ltw.matrix, distSq };

        // Models may have multiple material types, so we batch them for all passes
        // The Model::draw* methods will filter internally
        opaqueBatch.push_back(batch);
        maskedBatch.push_back(batch);
        blendedBatch.push_back(batch);
    }

    // ========================================================================
    // Pass 1: Render Opaque Materials
    // ========================================================================

    cmd.bindGraphicsPipeline(opaquePipeline_);

    for (const auto& batch : opaqueBatch) {
        batch.model->drawOpaque(cmd, pipelineLayout_, globalDescriptorSet, *batch.transform);
    }

    // ========================================================================
    // Pass 2: Render Masked Materials (Alpha Cutout)
    // ========================================================================

    cmd.bindGraphicsPipeline(maskedPipeline_);

    for (const auto& batch : maskedBatch) {
        batch.model->drawMasked(cmd, pipelineLayout_, globalDescriptorSet, *batch.transform);
    }

    // ========================================================================
    // Pass 3: Render Blended Materials (True Transparency)
    // ========================================================================
    // Sort back-to-front for correct transparency

    std::sort(blendedBatch.begin(), blendedBatch.end(),
        [](const RenderBatch& a, const RenderBatch& b) noexcept {
            return a.distanceFromCamera > b.distanceFromCamera;  // Farthest first
        });

    cmd.bindGraphicsPipeline(blendedPipeline_);

    for (const auto& batch : blendedBatch) {
        batch.model->drawBlended(cmd, pipelineLayout_, globalDescriptorSet, cameraPosition, *batch.transform);
    }
}

void MeshRenderSystem::cleanup() {
    if (!device_) {
        return;  // Not initialized or already cleaned up
    }

    VkDevice dev = device_->device();

    // Destroy pipelines (managed by pipeline cache, we don't destroy them directly)
    // Pipeline cache owns the pipelines, so we just null our handles
    opaquePipeline_ = VK_NULL_HANDLE;
    maskedPipeline_ = VK_NULL_HANDLE;
    blendedPipeline_ = VK_NULL_HANDLE;

    // Destroy shader modules (we own these)
    if (vertexShader_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(dev, vertexShader_, nullptr);
        vertexShader_ = VK_NULL_HANDLE;
    }
    if (fragmentShader_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(dev, fragmentShader_, nullptr);
        fragmentShader_ = VK_NULL_HANDLE;
    }

    // Destroy pipeline layout (we own this)
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    device_ = nullptr;
    pipelineCache_ = nullptr;
}

} // namespace hvk
