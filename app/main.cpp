#include "hvk_window.h"
#include "hvk_device.h"
#include "hvk_swap_chain.h"
#include "hvk_renderer.h"
#include "hvk_pipeline.h"
#include "hvk_model.h"
#include "hvk_buffer.h"
#include "hvk_descriptors.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <string>

// Must match your GLSL GlobalUbo in model.vert/frag
struct GlobalUbo {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 inverseView;
    glm::vec4 ambientLightColor;
    int       numLights;
};

int main() {
    try {
        // 1) window + device
        hvk::HvkWindow window{ 800, 600, "Holy Vulkan" };
        hvk::HvkDevice device{ window };

        // 2) prepare global UBO data
        GlobalUbo ubo{};
        float aspect = window.getExtent().width / float(window.getExtent().height);
        ubo.projection = glm::perspective(glm::radians(60.f), aspect, 0.1f, 100.f);
        ubo.projection[1][1] *= -1; // GLM to Vulkan Y flip
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        ubo.inverseView = glm::inverse(ubo.view);
        ubo.ambientLightColor = { 1,1,1,1 };
        ubo.numLights = 0;

        // 3) upload UBO to GPU
        hvk::HvkBuffer uboBuffer{
            device,
            sizeof(GlobalUbo), 1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };
        uboBuffer.map();
        uboBuffer.writeToBuffer(&ubo);

        // 4) descriptor set layout for UBO (b0) + sampler (b1)
        auto layoutUniq = hvk::HvkDescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        auto setLayout = std::shared_ptr<hvk::HvkDescriptorSetLayout>(std::move(layoutUniq));

        // 5) descriptor pool (one UBO + one sampler)
        auto poolUniq = hvk::HvkDescriptorPool::Builder(device)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
            .setMaxSets(1)
            .build();
        auto pool = std::shared_ptr<hvk::HvkDescriptorPool>(std::move(poolUniq));

        // 6) load model (including its texture)
        std::string modelPath = "../../../../assets/models/Crystar_Kokoro_Fudoji.glb";
        auto model = hvk::HvkModel::createModelFromFile(device, modelPath);

        // 7) give the model our layout + pool so it can write its texture descriptor
        model->setDescriptorLayout(setLayout);
        model->setDescriptorPool(pool);

        // 8) allocate & write **one** descriptor set for both UBO & texture
        VkDescriptorSet globalSet;
        {
            auto writer = hvk::HvkDescriptorWriter(*setLayout, *pool);
            // binding 0 = UBO
            auto uboInfo = uboBuffer.descriptorInfo();
            writer.writeBuffer(0, &uboInfo);
            // binding 1 = texture, if present
            if (model->hasTexture()) {
                auto imageInfo = model->getImageInfo();
                writer.writeImage(1, &imageInfo);
            }
            if (!writer.build(globalSet)) {
                throw std::runtime_error("Failed to build global descriptor set");
            }
        }

        // 9) renderer + pipeline layout
        hvk::HvkRenderer renderer{ window, device };
        VkDescriptorSetLayout dsl = setLayout->getDescriptorSetLayout();
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(glm::mat4) * 2;

        VkPipelineLayoutCreateInfo pli{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        pli.setLayoutCount = 1;
        pli.pSetLayouts = &dsl;
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges = &pushRange;

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(device.device(), &pli, nullptr, &pipelineLayout)
            != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout");
        }

        // 10) build graphics pipeline
        hvk::PipelineConfigInfo config{};
        hvk::HvkPipeline::defaultPipelineConfigInfo(config);
        config.multisampleInfo.rasterizationSamples = device.getMsaaSamples();
        config.renderPass = renderer.getSwapChainRenderPass();
        config.pipelineLayout = pipelineLayout;

        auto pipeline = std::make_unique<hvk::HvkPipeline>(
            device,
            "../../../shaders/model.vert.spv",
            "../../../shaders/model.frag.spv",
            config
        );

        // 11) main loop
        while (!window.shouldClose()) {
            glfwPollEvents();

            VkCommandBuffer cmd = renderer.beginFrame();
            renderer.beginSwapChainRenderPass(cmd);

            pipeline->bind(cmd);
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                0, 1, &globalSet,
                0, nullptr
            );

            struct PushConstants { glm::mat4 model, normal; } pc{};
            pc.model = glm::mat4(1.0f);
            pc.normal = glm::transpose(glm::inverse(pc.model));
            vkCmdPushConstants(
                cmd,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(pc),
                &pc
            );

            model->bind(cmd, pipelineLayout);
            model->draw(cmd);

            renderer.endSwapChainRenderPass(cmd);
            renderer.endFrame();
        }

        vkDeviceWaitIdle(device.device());
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
