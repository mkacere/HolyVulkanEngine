#ifndef HVK_PIPELINE
#define HVK_PIPELINE

#include "hvk_device.h"

#include <string>
#include <vector>

namespace hvk {
	struct HvkPipelineConfigInfo {
		HvkPipelineConfigInfo() = default;
		HvkPipelineConfigInfo(const HvkPipelineConfigInfo&) = delete;
		HvkPipelineConfigInfo& operator=(const HvkPipelineConfigInfo&) = delete;

		std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
		VkPipelineViewportStateCreateInfo viewportInfo{};
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
		VkPipelineMultisampleStateCreateInfo multisampleInfo{};
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
		std::vector<VkDynamicState> dynamicStateEnables{};
		VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
		VkPipelineLayout pipelineLayout = nullptr;
		VkRenderPass renderPass = nullptr;
		uint32_t subpass = 0;
	};

	class HvkPipeline
	{
	public:
		HvkPipeline(HvkDevice& device, const std::string& vertFilepath, const std::string& fragFilePath, const HvkPipelineConfigInfo& configInfo);
		~HvkPipeline();

		HvkPipeline(const HvkPipeline&) = delete;
		HvkPipeline& operator=(const HvkPipeline&) = delete;

		void bind(VkCommandBuffer commandBuffer);

		static void defaultPipelineConfigInfo(HvkPipelineConfigInfo& configInfo);
		static void enableAlphaBlending(HvkPipelineConfigInfo& configInfo);
		VkPipelineLayout getLayout() const { return pipelineLayout_; }

	private:
		static std::vector<char> readFile(const std::string& filepath);

		void createGrapicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const HvkPipelineConfigInfo& configInfo);
		void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

		HvkDevice& hvkDevice_;
		VkPipeline graphicsPipeline_;
		VkPipelineLayout pipelineLayout_{ VK_NULL_HANDLE };
		VkShaderModule vertShaderModule_;
		VkShaderModule fragShaderModule_;
	};

}

#endif // HVK_PIPELINE
