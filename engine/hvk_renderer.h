#ifndef HVK_RENDERER
#define HVK_RENDERER

#include "hvk_window.h"
#include "hvk_device.h"
#include "hvk_swap_chain.h"

#include "hvk_frame_info.hpp"
#include "hvk_irender_system.hpp"

#include <cassert>
#include <memory>
#include <vector>


namespace hvk {

	class HvkRenderer
	{
	public:
		HvkRenderer(HvkWindow& window, HvkDevice& device);
		~HvkRenderer();

		HvkRenderer(const HvkRenderer&) = delete;
		HvkRenderer& operator=(const HvkRenderer&) = delete;

		void drawFrame(float frameTime, HvkCamera& camera, VkDescriptorSet globalDescriptorSet, HvkGameObject::Map& gameObjects);
		void addRenderSystem(IRenderSystem* system);

		VkRenderPass getSwapChainRenderPass() const { return hvkSwapChain_->getRenderPass(); }
		float getAspectRatio() const { return hvkSwapChain_->extentAspectRatio(); }
		bool isFrameInProgress() const { return isFrameStarted_; }

		VkCommandBuffer getCurrentCommandBuffer() const {
			assert(isFrameStarted_ && "Cannot get command buffer when frame is not in progress");
			return commandBuffers_[currentFrameIndex_];
		}

		int getFrameIndex() const {
			assert(isFrameStarted_ && "Cannot get frame index when frame not in progress");
			return currentFrameIndex_;
		}

		VkCommandBuffer beginFrame();
		void endFrame();
		void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
		void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

	private:
		void createCommandBuffers();
		void freeCommandBuffers();
		void recreateSwapChain();

		HvkWindow& hvkWindow_;
		HvkDevice& hvkDevice_;
		std::unique_ptr<HvkSwapChain> hvkSwapChain_;
		std::vector<VkCommandBuffer> commandBuffers_;

		std::vector<IRenderSystem*> renderSystems_;

		uint32_t currentImageIndex_;
		int currentFrameIndex_ = 0;
		bool isFrameStarted_ = false;
	};

}

#endif // HVK_RENDERER