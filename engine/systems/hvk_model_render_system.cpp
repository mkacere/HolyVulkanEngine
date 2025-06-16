#include "hvk_model_render_system.h"

namespace hvk {

    ModelRenderSystem::ModelRenderSystem(
        HvkDevice& device,
        const std::string& modelPath
    )
        : device_(device),
        // load model (including its texture) right away
        model_(HvkModel::createModelFromFile(device, modelPath)),
        // create & map a host‐visible UBO
        uboBuffer_{
          device,
          sizeof(GlobalUbo), 1,
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        }
    {
        uboBuffer_.map();
    }

    ModelRenderSystem::~ModelRenderSystem() {
        cleanup();
    }

    void ModelRenderSystem::init(VkRenderPass renderPass, VkExtent2D extent) {
        // 1) build descriptor‐set layout (b0 = UBO, b1 = albedo sampler)
        descriptorSetLayout_ = HvkDescriptorSetLayout::Builder(device_)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();

        // 2) build descriptor‐pool for exactly one UBO + one sampler
        descriptorPool_ = HvkDescriptorPool::Builder(device_)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
            .setMaxSets(1)
            .build();

        // 3) allocate & write the one descriptor set
        {
            HvkDescriptorWriter writer(*descriptorSetLayout_, *descriptorPool_);

            // Instead of &uboBuffer_.descriptorInfo(), do:
            auto bufferInfo = uboBuffer_.descriptorInfo();
            writer.writeBuffer(0, &bufferInfo);

            if (model_->hasTexture()) {
                // Likewise for the image info
                auto imageInfo = model_->getImageInfo();
                writer.writeImage(1, &imageInfo);
            }

            writer.build(descriptorSet_);
        }

        // 4) compute a fixed projection matrix (only on resize or init)
        float aspect = float(extent.width) / float(extent.height);
        projection_ = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
        projection_[1][1] *= -1; // GLM to Vulkan

        // 5) create pipeline layout (one set + push constants)
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pli{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        pli.setLayoutCount = 1;
        VkDescriptorSetLayout descriptorSetLayout = descriptorSetLayout_->getDescriptorSetLayout();
        pli.pSetLayouts = &descriptorSetLayout;
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(device_.device(), &pli, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("ModelRenderSystem: failed to create pipeline layout");
        }

        // 6) build the graphics pipeline
        HvkPipelineConfigInfo config{};
        HvkPipeline::defaultPipelineConfigInfo(config);
        config.renderPass = renderPass;
        config.pipelineLayout = pipelineLayout_;
        config.multisampleInfo.rasterizationSamples = device_.getMsaaSamples();
        // ensure depth test is on if you want it:
        config.depthStencilInfo.depthTestEnable = VK_TRUE;
        config.depthStencilInfo.depthWriteEnable = VK_TRUE;

        pipeline_ = std::make_unique<HvkPipeline>(
            device_,
            "/shaders/model.vert.spv",
            "/shaders/model.frag.spv",
            config
        );
    }

    void ModelRenderSystem::cleanup() {
        if (pipeline_) {
            pipeline_.reset();
            vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
        }
        // descriptorPool_ and descriptorSetLayout_ are RAII’d by shared_ptr
    }

    void ModelRenderSystem::onResize(VkRenderPass renderPass, VkExtent2D extent) {
        cleanup();
        init(renderPass, extent);
    }

    void ModelRenderSystem::render(FrameInfo const& frameInfo) {
        // 1) update UBO every frame
        GlobalUbo ubo{};
        ubo.projection = projection_;
        ubo.view = frameInfo.camera.getView();
        ubo.inverseView = glm::inverse(ubo.view);
        ubo.ambientLightColor = { 1.f, 1.f, 1.f, 1.f };
        ubo.numLights = 0;
        uboBuffer_.writeToBuffer(&ubo);

        // 2) bind pipeline & descriptor set
        pipeline_->bind(frameInfo.commandBuffer);
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0, 1, &descriptorSet_,
            0, nullptr
        );

        // 3) rotate the model in‐place
        PushConstants pc{};
        float angle = frameInfo.frameTime * glm::radians(45.0f);
        pc.model = glm::rotate(glm::mat4(1.f), angle, { 0,1,0 });
        pc.normal = glm::transpose(glm::inverse(pc.model));

        vkCmdPushConstants(
            frameInfo.commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PushConstants),
            &pc
        );

        // 4) draw
        model_->bind(frameInfo.commandBuffer, pipelineLayout_);
        model_->draw(frameInfo.commandBuffer);
    }

} // namespace hvk
