#ifndef HVK_DEVICE
#define HVK_DEVICE

#include "hvk_window.h"

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace hvk {

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete() const {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	class HvkDevice
	{
	public:
		HvkDevice(HvkWindow &window);
		~HvkDevice();

		HvkDevice(const HvkDevice&) = delete;
		HvkDevice& operator=(const HvkDevice&) = delete;
		HvkDevice(HvkDevice&&) = delete;
		HvkDevice& operator=(HvkDevice&&) = delete;

		VkCommandPool getCommandPool() const { return commandPool_; }
		VkDevice device() const { return device_; }
		VkSurfaceKHR surface() const { return surface_; }
		VkQueue graphicsQueue() const { return graphicsQueue_; }
		VkQueue presentQueue() const { return presentQueue_; }
		VkSampleCountFlagBits getMsaaSamples() const { return msaaSamples_; }

		SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice_); }
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice_); }
		VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		
		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		
		VkCommandBuffer beginSingleTimeCommands();

		void endSingleTimeCommands(VkCommandBuffer commandBuffer);

		void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

		void createImageWithInfo(const VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

		VkPhysicalDeviceProperties properties_;

	private:

		void createInstance();
		void setupDebugMessenger();
		void createSurface();
		void pickPhysicalDevice();
		void createLogicalDevice();
		void createCommandPool();

		bool isDeviceSuitable(VkPhysicalDevice device);
		std::vector<const char*> getRequiredExtensions();
		bool checkValidationLayerSupport();
		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
		void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		bool checkDeviceExtensionSupport(VkPhysicalDevice device);
		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
		VkSampleCountFlagBits getMaxUsableSampleCount() const;

		VkInstance instance_;
		VkDebugUtilsMessengerEXT debugMessenger_;
		VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
		HvkWindow& window_;
		VkCommandPool commandPool_;

		VkDevice device_;
		VkSurfaceKHR surface_;
		VkQueue graphicsQueue_;
		VkQueue presentQueue_;
		VkSampleCountFlagBits msaaSamples_;
	};
}

#endif // HVK_DEVICE