// engine/systems/obj_render_system.hpp
#pragma once

#include "hvk_irender_system.hpp"
#include "hvk_pipeline.h"
#include "hvk_device.h"
#include "hvk_model.h"
#include <glm/glm.hpp>
#include <memory>

namespace hvk {

    // Push-constant layout: model matrix + normal matrix
    struct ObjPushConstant {
        glm::mat4 model;
        glm::mat4 normal;
    };

    class ObjRenderSystem : public IRenderSystem {
    public:
        ObjRenderSystem(
            HvkDevice& device,
            VkRenderPass             renderPass,
            VkDescriptorSetLayout    globalSetLayout);
        ~ObjRenderSystem();

        // IRenderSystem interface
        void render(FrameInfo const& frame) override;

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        HvkDevice& device_;
        VkPipelineLayout                  pipelineLayout_{};
        std::unique_ptr<HvkPipeline>      pipeline_;
    };

} // namespace hvk
