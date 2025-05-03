// main.cpp

#include "hvk_window.h"
#include "hvk_device.h"
#include "hvk_renderer.h"
#include "hvk_pipeline.h"
#include "hvk_descriptors.h"
#include "hvk_buffer.h"
#include "hvk_model.h"
#include "hvk_game_object.h"
#include "hvk_camera.h"
#include "hvk_frame_info.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include <cassert>
#include <stdexcept>

static const uint32_t WIDTH = 1280;
static const uint32_t HEIGHT = 720;
// You have two frames in flight in your swap chain:
static const int MAX_FRAMES_IN_FLIGHT = 2;

int main() {
    // 1) Create the window & Vulkan device
    hvk::HvkWindow window{ WIDTH, HEIGHT, "Model Viewer" };
    hvk::HvkDevice device{ window };

    // 2) Create the renderer (owns swap chain + command buffers)
    hvk::HvkRenderer renderer{ window, device };

    // 3) Build a descriptor‐set layout for a single UBO at set=0,binding=0
    auto globalSetLayout = hvk::HvkDescriptorSetLayout::Builder{ device }
        .addBinding(
            /*binding=*/0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        )
        .build();

    // 4) Build a descriptor‐pool that can allocate MAX_FRAMES_IN_FLIGHT UBO sets
    auto descriptorPool = hvk::HvkDescriptorPool::Builder{ device }
        .setMaxSets(MAX_FRAMES_IN_FLIGHT)
        .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT)
        .build();

    // 5) Create one host‐visible buffer for your GlobalUbo
    auto globalUboBuffer = std::make_unique<hvk::HvkBuffer>(
        device,
        sizeof(hvk::GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // 6) Allocate & write one VkDescriptorSet per frame
    std::vector<VkDescriptorSet> globalDescriptorSets(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // grab the buffer‐info for our UBO
        VkDescriptorBufferInfo bufferInfo = globalUboBuffer->descriptorInfo();

        // write into a fresh set
        hvk::HvkDescriptorWriter{ *globalSetLayout, *descriptorPool }
            .writeBuffer(0, &bufferInfo)
            .build(globalDescriptorSets[i]);
    }

    // 7) Create a VkPipelineLayout tying in our descriptor‐set layout:
    VkPipelineLayout pipelineLayout;
    {
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        VkDescriptorSetLayout rawLayout = globalSetLayout->getDescriptorSetLayout();
        layoutInfo.pSetLayouts = &rawLayout;
        layoutInfo.pushConstantRangeCount = 0;
        layoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device.device(), &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }
    }

    // 8) Configure & build our graphics pipeline
    hvk::PipelineConfigInfo pipelineConfig{};
    hvk::HvkPipeline::defaultPipelineConfigInfo(pipelineConfig);

    pipelineConfig.multisampleInfo.rasterizationSamples = device.getMsaaSamples();

    // feed in our vertex‐layout from HvkModel::Vertex
    pipelineConfig.bindingDescriptions = hvk::HvkModel::Vertex::getBindingDescriptions();
    pipelineConfig.attributeDescriptions = hvk::HvkModel::Vertex::getAttributeDescriptions();

    // use the renderer’s render pass & our new pipelineLayout
    pipelineConfig.renderPass = renderer.getSwapChainRenderPass();
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipelineConfig.subpass = 0;

    // finally build it (loads SPIR-V, creates shader modules, etc.)
    auto pipeline = std::make_unique<hvk::HvkPipeline>(
        device,
        "../../../shaders/model.vert.spv",
        "../../../shaders/model.frag.spv",
        pipelineConfig
    );

    // 9) Load your model into a single game‐object
    hvk::HvkGameObject::Map gameObjects;
    {
        auto modelPtr = hvk::HvkModel::createModelFromFile(
            device,
            "../../../assets/models/Crystar_Kokoro_Fudoji.glb"
        );
        auto obj = hvk::HvkGameObject::createGameObject();
        obj.model = std::move(modelPtr);
        obj.transform.translation = { 0.f, 0.f, 0.f };
        obj.transform.scale = { 1.f, 1.f, 1.f };
        gameObjects.emplace(obj.getId(), std::move(obj));
    }

    // 10) Create a camera
    hvk::HvkCamera camera{};

    // 11) Main loop
    while (!window.shouldClose()) {
        glfwPollEvents();

        // a) Begin frame & get current command buffer + frame index
        VkCommandBuffer cmd = renderer.beginFrame();
        int frameIndex = renderer.getFrameIndex();

        // b) Update our global UBO (projection + view matrices)
        hvk::GlobalUbo ubo{};
        ubo.projection = camera.getProjection();
        ubo.view = camera.getView();
        ubo.inverseView = glm::inverse(ubo.view);
        ubo.ambientLightColor = glm::vec4(1.f, 1.f, 1.f, 0.02f);
        ubo.numLights = 0;  // no point lights yet

        globalUboBuffer->map();
        globalUboBuffer->writeToBuffer(&ubo);
        globalUboBuffer->unmap();

        // c) Record draw commands
        renderer.beginSwapChainRenderPass(cmd);

        pipeline->bind(cmd);
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 1,
            &globalDescriptorSets[frameIndex],
            0, nullptr
        );

        for (auto& [id, obj] : gameObjects) {
            obj.model->bind(cmd);
            obj.model->draw(cmd);
        }

        renderer.endSwapChainRenderPass(cmd);
        renderer.endFrame();
    }

    vkDeviceWaitIdle(device.device());
    return 0;
}
