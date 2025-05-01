#ifndef HVK_DESCRIPTORS
#define HVK_DESCRIPTORS

#include "hvk_device.h"

#include <unordered_map>
#include <memory>
#include <vector>

namespace hvk {
	class HvkDescriptorSetLayout
	{
	public:
		class Builder
		{
		public:
			Builder(HvkDevice& device) : hvkDevice_(device) {}

			Builder& addBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count = 1);
			std::unique_ptr<HvkDescriptorSetLayout> build() const;
		private:
			HvkDevice& hvkDevice_;
			std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings_{};

		};

		HvkDescriptorSetLayout(HvkDevice& device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
		~HvkDescriptorSetLayout();
		HvkDescriptorSetLayout(const HvkDescriptorSetLayout&) = delete;
		HvkDescriptorSetLayout& operator=(const HvkDescriptorSetLayout&) = delete;

		VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }
	private:
		HvkDevice& hvkDevice_;
		VkDescriptorSetLayout descriptorSetLayout_;
		std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings_;

		friend class HvkDescriptorWriter;
	};

	class HvkDescriptorPool
	{
	public:
		class Builder
		{
		public:
			Builder(HvkDevice& device) : hvkDevice_(device) {}

			Builder& addPoolSize(VkDescriptorType descriptorType, uint32_t count);
			Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);
			Builder& setMaxSets(uint32_t count);
			std::unique_ptr<HvkDescriptorPool> build() const;
		private:
			HvkDevice& hvkDevice_;
			std::vector<VkDescriptorPoolSize> poolSizes_{};
			uint32_t maxSets_ = 1000;
			VkDescriptorPoolCreateFlags poolFlags_ = 0;
		};

		HvkDescriptorPool(HvkDevice& device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize>& poolSizes);
		~HvkDescriptorPool();

		HvkDescriptorPool(const HvkDescriptorPool&) = delete;
		HvkDescriptorPool& operator=(const HvkDescriptorPool&) = delete;

		bool allocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;

		void freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;

		void resetPool();
	private:
		HvkDevice& hvkDevice_;
		VkDescriptorPool descriptorPool_;
		friend class HvkDescriptorWriter;

	};

	class HvkDescriptorWriter
	{
	public:
		HvkDescriptorWriter(HvkDescriptorSetLayout& setLayout, HvkDescriptorPool& pool);

		HvkDescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
		HvkDescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

		bool build(VkDescriptorSet& set);
		void overwrite(VkDescriptorSet& set);
	private:
		HvkDescriptorSetLayout& setLayout_;
		HvkDescriptorPool& pool_;
		std::vector<VkWriteDescriptorSet> writes_;

	};

}

#endif // HVK_DESCRIPTORS