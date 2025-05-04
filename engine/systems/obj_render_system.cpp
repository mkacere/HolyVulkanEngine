// engine/systems/obj_render_system.cpp
#include "obj_render_system.h"
#include "hvk_pipeline.h"
#include <stdexcept>
#include <array>

namespace hvk {

    ObjRenderSystem::ObjRenderSystem(
        HvkDevice& device,
        VkRenderPass          renderPass,
        VkDescriptorSetLayout globalSetLayout)
        : device_(device)
    {
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
    }

    ObjRenderSystem::~ObjRenderSystem() {
        vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
    }

    void ObjRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(ObjPushConstant);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &globalSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ObjRenderSystem pipeline layout");
        }
    }

    void ObjRenderSystem::createPipeline(VkRenderPass renderPass) {
        PipelineConfigInfo config{};
        HvkPipeline::defaultPipelineConfigInfo(config);
        config.renderPass = renderPass;
        config.pipelineLayout = pipelineLayout_;

        pipeline_ = std::make_unique<HvkPipeline>(
            device_,
            "shaders/obj.vert.spv",
            "shaders/obj.frag.spv",
            config);
    }

    void ObjRenderSystem::render(FrameInfo const& frame) {
        // Bind pipeline and global (camera) descriptor
        pipeline_->bind(frame.commandBuffer);
        vkCmdBindDescriptorSets(
            frame.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0, 1, &frame.globalDescriptorSet,
            0, nullptr);

        // Draw each game object
        for (auto& kv : frame.gameObjects) {
            auto& obj = kv.second;
            if (!obj.model) continue;

            // Set up push constants
            ObjPushConstant pc{};
            pc.model = obj.transform.mat4();
            pc.normal = obj.transform.normalMatrix();
            vkCmdPushConstants(
                frame.commandBuffer,
                pipelineLayout_,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(pc), &pc);

            // Bind & draw mesh
            obj.model->bind(frame.commandBuffer, pipelineLayout_);
            obj.model->draw(frame.commandBuffer);
        }
    }

} // namespace hvk
