#ifndef HVK_FRAME_INFO
#define HVK_FRAME_INFO

#include "hvk_camera.h"
#include "hvk_game_object.h"
#include <vulkan/vulkan.h>

namespace hvk {

    struct FrameInfo {
        int             frameIndex;
        float           frameTime;
        VkCommandBuffer commandBuffer;
        VkRenderPass    renderPass;
        VkFramebuffer   framebuffer;
        VkExtent2D      extent;
        HvkCamera& camera;
        HvkGameObject::Map& gameObjects;
    };

} // namespace hvk

#endif // HVK_FRAME_INFO
