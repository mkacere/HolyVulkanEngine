#ifndef HVK_IRENDERSYSTEM_HPP
#define HVK_IRENDERSYSTEM_HPP

#include <hvk/gfx/hvk_frame_info.hpp>
#include <vulkan/vulkan.h>

namespace hvk {

    class IRenderSystem {
    public:
        virtual ~IRenderSystem() = default;

        // Called once after swap chain (and render pass) creation
        virtual void init(VkRenderPass renderPass, VkExtent2D extent) = 0;

        // Record draw commands for this pass
        virtual void render(FrameInfo const& frameInfo) = 0;

        // Called when swap chain is recreated (e.g. window resize)
        virtual void onResize(VkRenderPass renderPass, VkExtent2D extent) = 0;

        // Cleanup Vulkan objects created by this system
        virtual void cleanup() = 0;
    };

} // namespace hvk

#endif // HVK_IRENDERSYSTEM_HPP
