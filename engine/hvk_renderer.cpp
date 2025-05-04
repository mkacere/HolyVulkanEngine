#include "hvk_renderer.h"

#include <stdexcept>
#include <array>


namespace hvk {

	HvkRenderer::HvkRenderer(HvkWindow& window, HvkDevice& device) :
		hvkWindow_(window), hvkDevice_(device)
	{
		recreateSwapChain();
		createCommandBuffers();
	}

	HvkRenderer::~HvkRenderer()
	{
		freeCommandBuffers();
	}

	void HvkRenderer::drawFrame(float frameTime, HvkCamera& camera, VkDescriptorSet globalDescriptorSet, HvkGameObject::Map& gameObjects) {
		VkCommandBuffer cmd = beginFrame();
		beginSwapChainRenderPass(cmd);

		FrameInfo frameInfo{
			currentFrameIndex_,
			frameTime,
			cmd,
			hvkSwapChain_->getRenderPass(),
			hvkSwapChain_->getFrameBuffer(currentFrameIndex_),
			hvkSwapChain_->getSwapChainExtent(),
			camera,
			globalDescriptorSet,
			gameObjects
		};

		for (auto* sys : renderSystems_) {
			sys->render(frameInfo);
		}

		endSwapChainRenderPass(cmd);
		endFrame();
	}

	void HvkRenderer::addRenderSystem(IRenderSystem* system) 
	{
		renderSystems_.push_back(system);
	}

	void HvkRenderer::recreateSwapChain()
	{
		auto extent = hvkWindow_.getExtent();
		while (extent.width == 0 || extent.height == 0) {
			extent = hvkWindow_.getExtent();
			glfwWaitEvents();
		}
		vkDeviceWaitIdle(hvkDevice_.device());

		if (hvkSwapChain_ == nullptr) {
			hvkSwapChain_ = std::make_unique<HvkSwapChain>(hvkDevice_, extent);
		}
		else {
			std::shared_ptr<HvkSwapChain> oldSwapChain = std::move(hvkSwapChain_);
			hvkSwapChain_ = std::make_unique<HvkSwapChain>(hvkDevice_, extent, oldSwapChain);

			if (!oldSwapChain->compareSwapFormats(*hvkSwapChain_.get())) {
				throw std::runtime_error("Swap chain image(or depth) format has changed!");
			}
		}
	}

	VkCommandBuffer HvkRenderer::beginFrame()
	{
		assert(!isFrameStarted_ && "Can't call beginFrame while already in progress");
		auto result = hvkSwapChain_->acquireNextImage(&currentImageIndex_);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return nullptr;
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		isFrameStarted_ = true;

		auto commandBuffer = getCurrentCommandBuffer();
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}
		return commandBuffer;
	}

	void HvkRenderer::endFrame()
	{
		assert(isFrameStarted_ && "Can't call endFrame while frame is not in progress");
		auto commandBuffer = getCurrentCommandBuffer();
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}

		auto result = hvkSwapChain_->submitCommandBuffers(&commandBuffer, &currentImageIndex_);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
			hvkWindow_.wasWindowResized()) {
			hvkWindow_.resetWindowResizedFlag();
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		isFrameStarted_ = false;
		currentFrameIndex_ = (currentFrameIndex_ + 1) % HvkSwapChain::MAX_FRAMES_IN_FLIGHT;
	}

	void HvkRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer)
	{
		assert(isFrameStarted_ && "Can't call beginSwapChainRenderPass if frame is not in progress");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = hvkSwapChain_->getRenderPass();
		renderPassInfo.framebuffer = hvkSwapChain_->getFrameBuffer(currentImageIndex_);

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = hvkSwapChain_->getSwapChainExtent();

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.01f, 0.01f, 0.01f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(hvkSwapChain_->getSwapChainExtent().width);
		viewport.height = static_cast<float>(hvkSwapChain_->getSwapChainExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0, 0}, hvkSwapChain_->getSwapChainExtent() };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void HvkRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
	{
		assert(isFrameStarted_ && "Can't call endSwapChainRenderPass if frame is not in progress");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame");
		vkCmdEndRenderPass(commandBuffer);
	}

	void HvkRenderer::createCommandBuffers()
	{
		commandBuffers_.resize(HvkSwapChain::MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = hvkDevice_.getCommandPool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

		if (vkAllocateCommandBuffers(hvkDevice_.device(), &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void HvkRenderer::freeCommandBuffers()
	{
		vkFreeCommandBuffers(hvkDevice_.device(), hvkDevice_.getCommandPool(), static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
		commandBuffers_.clear();
	}

}