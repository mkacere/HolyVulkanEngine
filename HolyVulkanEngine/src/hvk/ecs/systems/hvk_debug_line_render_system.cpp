#include <hvk/ecs/systems/hvk_debug_line_render_system.hpp>
#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/gfx/hvk_global_descriptors.hpp>
#include <hvk/gfx/hvk_cmd_list.hpp>
#include <iostream>
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
        throw std::runtime_error(std::string("DebugLineRenderSystem: Failed to open SPIR-V: ") + path);
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
    if (bytes.size() % 4) {
        throw std::runtime_error("DebugLineRenderSystem: SPIR-V file size not multiple of 4");
    }
    std::vector<uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return words;
}

// ============================================================================
// LineVertex - Vertex Input Description
// ============================================================================

VkVertexInputBindingDescription DebugLineRenderSystem::LineVertex::getBindingDescription() {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(LineVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::vector<VkVertexInputAttributeDescription> DebugLineRenderSystem::LineVertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attrs(2);

    // Position (location = 0)
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(LineVertex, position);

    // Color (location = 1)
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(LineVertex, color);

    return attrs;
}

// ============================================================================
// DebugLineRenderSystem - Initialization
// ============================================================================

void DebugLineRenderSystem::init(Scene& scene) {
    device_ = &scene.resources().device();
    pipelineCache_ = scene.pipelineCache();

    if (!pipelineCache_) {
        throw std::runtime_error("DebugLineRenderSystem: No pipeline cache available");
    }

    // Reserve vertex storage for efficiency
    vertices_.reserve(MAX_VERTICES);

    // ========================================================================
    // Create Vertex Buffer (host-visible for dynamic updates)
    // ========================================================================

    GpuBufferCreateInfo vbCI{};
    vbCI.device = device_;
    vbCI.size = MAX_VERTICES * sizeof(LineVertex);
    vbCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbCI.memUsage = VMA_MEMORY_USAGE_AUTO;
    vbCI.allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    vbCI.persistentMap = true;  // Keep buffer mapped for fast updates
    vbCI.debugName = "debug_line_vertices";
    vertexBuffer_.create(vbCI);

    // ========================================================================
    // Create Pipeline Layout
    // ========================================================================

    // Get descriptor set layouts
    const DescriptorSetLayout* globalLayout = scene.globalDescriptorLayout();
    if (!globalLayout) {
        throw std::runtime_error("DebugLineRenderSystem: Missing global descriptor layout");
    }

    // Build descriptor set layouts array (only Set 0: global)
    std::vector<VkDescriptorSetLayout> setLayouts = { globalLayout->handle() };

    // No push constants needed
    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutCI.pSetLayouts = setLayouts.data();
    layoutCI.pushConstantRangeCount = 0;
    layoutCI.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(device_->device(), &layoutCI, nullptr, &pipelineLayout_));

    // ========================================================================
    // Load Shaders
    // ========================================================================

    auto vs_spirv = loadSpirv(PROJECT_ROOT "/shaders/debug_line.vert.spv");
    auto fs_spirv = loadSpirv(PROJECT_ROOT "/shaders/debug_line.frag.spv");

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
    // Configure Pipeline State
    // ========================================================================

    // Shader stages
    ShaderStageDesc vs{};
    vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs.module = vertexShader_;
    vs.entry = "main";

    ShaderStageDesc fs{};
    fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs.module = fragmentShader_;
    fs.entry = "main";

    // Vertex input
    VertexInputDesc vertexInput{};
    vertexInput.bindings.push_back(LineVertex::getBindingDescription());
    vertexInput.attributes = LineVertex::getAttributeDescriptions();

    // Rasterization state
    RasterState raster{};
    raster.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;  // LINE_LIST topology
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;  // No culling for lines
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth = 1.0f;
    raster.rasterSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil state - Normal depth testing
    DepthStencilState dss{};
    dss.depthTestEnable = VK_TRUE;      // Test depth
    dss.depthWriteEnable = VK_TRUE;     // Write depth so lines occlude each other
    dss.depthCompare = VK_COMPARE_OP_LESS;  // Standard less-than comparison

    // Color blend state - no blending
    ColorBlendState cbs{};
    cbs.attachments.resize(1);
    cbs.attachments[0].blendEnable = VK_FALSE;
    cbs.attachments[0].colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Render formats (hardcoded to match Application defaults)
    RenderFormats formats{};
    formats.colorFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    formats.depthFormat = VK_FORMAT_D32_SFLOAT;

    // ========================================================================
    // Create Pipeline
    // ========================================================================

    GraphicsPipelineDesc gpDesc{};
    gpDesc.layout = pipelineLayout_;
    gpDesc.stages = { vs, fs };
    gpDesc.vertexInput = vertexInput;
    gpDesc.raster = raster;
    gpDesc.depthStencil = dss;
    gpDesc.colorBlend = cbs;
    gpDesc.formats = formats;
    gpDesc.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    pipeline_ = pipelineCache_->get(gpDesc);

    std::cout << "DebugLineRenderSystem: Initialized (max " << MAX_LINES << " lines)" << std::endl;
}

// ============================================================================
// DebugLineRenderSystem - Rendering
// ============================================================================

void DebugLineRenderSystem::render(Scene& scene, CmdList& cmd) {
    if (vertices_.empty()) {
        return;  // Nothing to render
    }

    // Upload vertices to GPU
    uploadVertices();

    // Bind pipeline
    cmd.bindGraphicsPipeline(pipeline_);

    // Bind global descriptors (Set 0: Scene + Camera + Lights)
    GlobalDescriptorSet* globalDescSet = scene.globalDescriptorSet();
    if (!globalDescSet) {
        clearLines();
        return;
    }

    VkDescriptorSet descSet = globalDescSet->get(scene.frameIndex());
    vkCmdBindDescriptorSets(
        cmd.handle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_,
        0, 1, &descSet,
        0, nullptr
    );

    // Bind vertex buffer
    VkBuffer vb = vertexBuffer_.handle();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd.handle(), 0, 1, &vb, &offset);

    // Draw lines
    uint32_t vertexCount = static_cast<uint32_t>(vertices_.size());
    cmd.draw(vertexCount);

    // Clear lines for next frame (immediate mode)
    // CAUSES FLICKERING UNKNOWN ROOT CAUSE???
    //clearLines();
}

// ============================================================================
// DebugLineRenderSystem - Public API
// ============================================================================

void DebugLineRenderSystem::addLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color) {
    if (vertices_.size() + 2 > MAX_VERTICES) {
        return;  // Buffer full - silently skip
    }

    vertices_.push_back({ start, color });
    vertices_.push_back({ end, color });
}

void DebugLineRenderSystem::addLines(const std::vector<LineVertex>& lines) {
    if (vertices_.size() + lines.size() > MAX_VERTICES) {
        // Partial add - fit what we can
        size_t availableSpace = MAX_VERTICES - vertices_.size();
        vertices_.insert(vertices_.end(), lines.begin(), lines.begin() + availableSpace);
        return;
    }

    vertices_.insert(vertices_.end(), lines.begin(), lines.end());
}

void DebugLineRenderSystem::clearLines() {
    vertices_.clear();
}

// ============================================================================
// DebugLineRenderSystem - Cleanup
// ============================================================================

void DebugLineRenderSystem::cleanup() {
    if (!device_) {
        return;  // Not initialized or already cleaned up
    }

    VkDevice dev = device_->device();

    // Destroy pipeline (managed by pipeline cache, we don't destroy it directly)
    // Pipeline cache owns the pipeline, so we just null our handle
    pipeline_ = VK_NULL_HANDLE;

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

    // Vertex buffer is destroyed automatically by GpuBuffer destructor (RAII)

    device_ = nullptr;
    pipelineCache_ = nullptr;
}

// ============================================================================
// DebugLineRenderSystem - Private Helpers
// ============================================================================

void DebugLineRenderSystem::uploadVertices() {
    if (vertices_.empty()) return;

    // Map buffer (persistent mapping, should already be mapped)
    void* mapped = vertexBuffer_.map();
    if (!mapped) {
        std::cerr << "DebugLineRenderSystem: Failed to map vertex buffer" << std::endl;
        return;
    }

    // Copy vertex data
    size_t dataSize = vertices_.size() * sizeof(LineVertex);
    std::memcpy(mapped, vertices_.data(), dataSize);

    // Flush to make writes visible to GPU (only needed if memory is not coherent)
    vertexBuffer_.flush();

    // Keep buffer mapped (persistent mapping - don't unmap)
}

} // namespace hvk
