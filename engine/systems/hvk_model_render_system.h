#pragma once
#include "hvk_irender_system.hpp"
#include "hvk_device.h"
#include "hvk_pipeline.h"
#include "hvk_model.h"
#include "hvk_buffer.h"
#include "hvk_descriptors.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace hvk {

    class ModelRenderSystem : public IRenderSystem {
    public:
        // ctor takes the Vulkan device and the path to your .glb
        ModelRenderSystem(
            HvkDevice& device,
            const std::string& modelPath
        );
        ~ModelRenderSystem();

        // IRenderSystem interface:
        void init(VkRenderPass renderPass, VkExtent2D extent) override;
        void cleanup() override;
        void onResize(VkRenderPass renderPass, VkExtent2D extent) override;
        void render(FrameInfo const& frameInfo) override;

    private:
        // must match your GLSL push‐constant block:
        struct PushConstants {
            glm::mat4 model;
            glm::mat4 normal;
        };
        // must match your GLSL uniform‐block:
        struct GlobalUbo {
            glm::mat4 projection;
            glm::mat4 view;
            glm::mat4 inverseView;
            glm::vec4 ambientLightColor;
            int       numLights;
        };

        HvkDevice& device_;

        // the GLTF model
        std::unique_ptr<HvkModel> model_;

        // our UBO + mapping
        HvkBuffer               uboBuffer_;
        std::shared_ptr<HvkDescriptorSetLayout> descriptorSetLayout_;
        std::shared_ptr<HvkDescriptorPool>      descriptorPool_;
        VkDescriptorSet         descriptorSet_{};

        // pipeline + layout
        VkPipelineLayout         pipelineLayout_{};
        std::unique_ptr<HvkPipeline> pipeline_;

        // cache the aspect‐fixed projection so we only recalc on resize:
        glm::mat4 projection_;
    };
}
