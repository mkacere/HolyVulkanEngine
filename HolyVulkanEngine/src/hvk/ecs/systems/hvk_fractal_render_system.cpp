#include <hvk/ecs/systems/hvk_fractal_render_system.hpp>
#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/ecs/hvk_components.hpp>
#include <hvk/gfx/hvk_global_descriptors.hpp>
#include <hvk/gfx/hvk_cmd_list.hpp>
#include <hvk/core/hvk_time.hpp>
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
        throw std::runtime_error(std::string("FractalRenderSystem: Failed to open SPIR-V: ") + path);
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
    if (bytes.size() % 4) {
        throw std::runtime_error("FractalRenderSystem: SPIR-V file size not multiple of 4");
    }
    std::vector<uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return words;
}

void FractalRenderSystem::init(Scene& scene) {
    device_ = &scene.resources().device();
    pipelineCache_ = scene.pipelineCache();

    if (!pipelineCache_) {
        throw std::runtime_error("FractalRenderSystem: No pipeline cache available");
    }

    // Initialize default parameters
    initDefaultParams();

    // Get global descriptor layout (Set 0: Camera + Scene + Lights)
    const DescriptorSetLayout* globalLayout = scene.globalDescriptorLayout();
    if (!globalLayout) {
        throw std::runtime_error("FractalRenderSystem: Missing global descriptor layout");
    }

    // ========================================================================
    // Create Pipeline Layout
    // ========================================================================

    // Only use global descriptors (Set 0) - we get camera matrices from there
    std::vector<VkDescriptorSetLayout> setLayouts = {
        globalLayout->handle()
    };

    // Push constants for fractal parameters
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(FractalParams);

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

    auto vs_spirv = loadSpirv(PROJECT_ROOT "/shaders/fractal.vert.spv");
    auto fs_spirv = loadSpirv(PROJECT_ROOT "/shaders/fractal.frag.spv");

    VkShaderModuleCreateInfo vsCI{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    vsCI.codeSize = vs_spirv.size() * 4;
    vsCI.pCode = vs_spirv.data();
    VK_CHECK(vkCreateShaderModule(device_->device(), &vsCI, nullptr, &vertexShader_));

    VkShaderModuleCreateInfo fsCI{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    fsCI.codeSize = fs_spirv.size() * 4;
    fsCI.pCode = fs_spirv.data();
    VK_CHECK(vkCreateShaderModule(device_->device(), &fsCI, nullptr, &fragmentShader_));

    // ========================================================================
    // Create Pipeline
    // ========================================================================

    createPipeline(scene);
}

void FractalRenderSystem::createPipeline(Scene& /*scene*/) {
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

    // No vertex input (fullscreen triangle is procedural)
    VertexInputDesc vertexInput{};

    // Rasterization state
    RasterState raster{};
    raster.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    raster.cullMode = VK_CULL_MODE_NONE; // Fullscreen triangle covers everything
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.rasterSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth/Stencil: No depth testing (fullscreen)
    DepthStencilState depthStencil{};
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    // Color blending: No blending (opaque fractal)
    ColorBlendState colorBlend{};
    colorBlend.attachments.resize(1);
    colorBlend.attachments[0].blendEnable = VK_FALSE;
    colorBlend.attachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Render formats (hardcoded - standard SRGB format with depth)
    RenderFormats formats{};
    formats.colorFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    formats.depthFormat = VK_FORMAT_D32_SFLOAT; // Must match render pass depth format

    // ========================================================================
    // Create Pipeline via PipelineCache
    // ========================================================================

    GraphicsPipelineDesc desc{};
    desc.layout = pipelineLayout_;
    desc.stages = { vs, fs };
    desc.vertexInput = vertexInput;
    desc.raster = raster;
    desc.depthStencil = depthStencil;
    desc.colorBlend = colorBlend;
    desc.formats = formats;

    pipeline_ = pipelineCache_->get(desc);
}

void FractalRenderSystem::render(Scene& scene, CmdList& cmd) {
    // Bind pipeline
    cmd.bindGraphicsPipeline(pipeline_);

    // Get global descriptor set (Set 0: Camera + Scene + Lights)
    GlobalDescriptorSet* globalDescSet = scene.globalDescriptorSet();
    if (!globalDescSet) {
        return; // No global descriptors available
    }

    VkDescriptorSet descSet = globalDescSet->get(scene.frameIndex());
    cmd.bindGraphicsDescriptorSets(pipelineLayout_, 0, 1, &descSet);

    // Update time for animation
    if (params_.enableAnimation) {
        params_.time = Time::totalTime();
    }

    // Push constants (fractal parameters)
    cmd.pushConstants(pipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, params_);

    // Draw fullscreen triangle (3 vertices, no vertex buffer)
    cmd.draw(3);
}

void FractalRenderSystem::cleanup() {
    if (!device_) return;

    VkDevice dev = device_->device();

    if (pipeline_ != VK_NULL_HANDLE) {
        // Note: Pipeline is managed by GraphicsPipelineCache, don't destroy it manually
        pipeline_ = VK_NULL_HANDLE;
    }

    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    if (vertexShader_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(dev, vertexShader_, nullptr);
        vertexShader_ = VK_NULL_HANDLE;
    }

    if (fragmentShader_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(dev, fragmentShader_, nullptr);
        fragmentShader_ = VK_NULL_HANDLE;
    }
}

void FractalRenderSystem::initDefaultParams() {
    params_ = {};

    // Fractal type
    params_.fractalType = static_cast<uint32_t>(FractalType::Mandelbulb);
    params_.power = 8.0f;              // Classic Mandelbulb power
    params_.maxIterations = 20;        // Escape iterations
    params_.bailout = 2.0f;            // Escape radius

    // Julia set parameters (default)
    params_.juliaC = glm::vec4(0.18f, 0.88f, 0.24f, 0.16f);

    // Raymarching quality
    params_.maxSteps = 128;            // Good quality/performance balance
    params_.epsilon = 0.001f;          // Surface precision
    params_.maxDistance = 20.0f;       // Max render distance

    // Lighting
    params_.lightDir = glm::normalize(glm::vec3(-0.5f, 0.7f, -0.3f));
    params_.ambientStrength = 0.3f;
    params_.lightColor = glm::vec3(1.0f, 0.95f, 0.9f); // Warm white
    params_.aoStrength = 0.5f;

    // Coloring
    params_.color1 = glm::vec3(0.9f, 0.6f, 0.3f);  // Orange
    params_.color2 = glm::vec3(0.2f, 0.4f, 0.8f);  // Blue
    params_.colorMix = 0.5f;

    // Animation
    params_.time = 0.0f;
    params_.enableAnimation = 0;

    // Psychedelic effects
    params_.glowIntensity = 0.3f;
    params_.colorCycleSpeed = 0.5f;
    params_.depthColorShift = 2.0f;
    params_.iterationColorMix = 0.7f;
}

void FractalRenderSystem::resetParams() {
    initDefaultParams();
}

} // namespace hvk
