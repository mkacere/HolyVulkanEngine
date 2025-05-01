#include "hvk_descriptors.h"

#include <cassert>
#include <stdexcept>

namespace hvk {

	HvkDescriptorSetLayout::Builder& HvkDescriptorSetLayout::Builder::addBinding(
		uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count)
	{
		assert(bindings_.count(binding) == 0 && "Binding already in use");
		VkDescriptorSetLayoutBinding layoutBinding{};
		layoutBinding.binding = binding;
		layoutBinding.descriptorType = descriptorType;
		layoutBinding.descriptorCount = count;
		layoutBinding.stageFlags = stageFlags;
		bindings_[binding] = layoutBinding;
		return *this;
	}

	std::unique_ptr<HvkDescriptorSetLayout> HvkDescriptorSetLayout::Builder::build() const {
		return std::make_unique<HvkDescriptorSetLayout>(hvkDevice_, bindings_);
	}

	HvkDescriptorSetLayout::HvkDescriptorSetLayout(HvkDevice& device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings) :
		hvkDevice_(device), bindings_(bindings)
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
		for (auto& kv : bindings) {
			setLayoutBindings.push_back(kv.second);
		}

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
		descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

		if (vkCreateDescriptorSetLayout(hvkDevice_.device(), &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}

	HvkDescriptorSetLayout::~HvkDescriptorSetLayout()
	{
		vkDestroyDescriptorSetLayout(hvkDevice_.device(), descriptorSetLayout_, nullptr);
	}

	HvkDescriptorPool::Builder& HvkDescriptorPool::Builder::addPoolSize(VkDescriptorType descriptorType, uint32_t count)
	{
		poolSizes_.push_back({ descriptorType, count });
		return *this;
	}

	HvkDescriptorPool::Builder& HvkDescriptorPool::Builder::setPoolFlags(VkDescriptorPoolCreateFlags flags)
	{
		poolFlags_ = flags;
		return *this;
	}

	HvkDescriptorPool::Builder& HvkDescriptorPool::Builder::setMaxSets(uint32_t count)
	{
		maxSets_ = count;
		return *this;
	}

	std::unique_ptr<HvkDescriptorPool> HvkDescriptorPool::Builder::build() const {
		return std::make_unique<HvkDescriptorPool>(hvkDevice_, maxSets_, poolFlags_, poolSizes_);
	}


	HvkDescriptorPool::HvkDescriptorPool(HvkDevice& device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize>& poolSizes) :
		hvkDevice_(device)
	{
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		descriptorPoolInfo.maxSets = maxSets;
		descriptorPoolInfo.flags = poolFlags;

		if (vkCreateDescriptorPool(hvkDevice_.device(), &descriptorPoolInfo, nullptr, &descriptorPool_) !=
			VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	HvkDescriptorPool::~HvkDescriptorPool()
	{
		vkDestroyDescriptorPool(hvkDevice_.device(), descriptorPool_, nullptr);
	}

	bool HvkDescriptorPool::allocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool_;
		allocInfo.pSetLayouts = &descriptorSetLayout;
		allocInfo.descriptorSetCount = 1;

		if (vkAllocateDescriptorSets(hvkDevice_.device(), &allocInfo, &descriptor) != VK_SUCCESS) {
			return false;
		}
		return true;
	}

	void HvkDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const
	{
		vkFreeDescriptorSets(hvkDevice_.device(), descriptorPool_, static_cast<uint32_t>(descriptors.size()), descriptors.data());
	}

	void HvkDescriptorPool::resetPool()
	{
		vkResetDescriptorPool(hvkDevice_.device(), descriptorPool_, 0);
	}

	HvkDescriptorWriter::HvkDescriptorWriter(HvkDescriptorSetLayout& setLayout, HvkDescriptorPool& pool) :
		setLayout_(setLayout), pool_(pool) {
	}

	HvkDescriptorWriter& HvkDescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo)
	{
		assert(setLayout_.bindings_.count(binding) == 1 && "Layout does not contain specified binding");

		auto& bindingDescription = setLayout_.bindings_[binding];

		assert(
			bindingDescription.descriptorCount == 1 &&
			"Binding single descriptor info, but binding expects multiple");

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.descriptorType;
		write.dstBinding = binding;
		write.pBufferInfo = bufferInfo;
		write.descriptorCount = 1;

		writes_.push_back(write);
		return *this;
	}

	HvkDescriptorWriter& HvkDescriptorWriter::writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo)
	{
		assert(setLayout_.bindings_.count(binding) == 1 && "Layout does not contain specified binding");

		auto& bindingDescription = setLayout_.bindings_[binding];

		assert(
			bindingDescription.descriptorCount == 1 &&
			"Binding single descriptor info, but binding expects multiple");

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.descriptorType;
		write.dstBinding = binding;
		write.pImageInfo = imageInfo;
		write.descriptorCount = 1;

		writes_.push_back(write);
		return *this;
	}

	bool HvkDescriptorWriter::build(VkDescriptorSet& set)
	{
		bool success = pool_.allocateDescriptor(setLayout_.getDescriptorSetLayout(), set);
		if (!success) {
			return false;
		}
		overwrite(set);
		return true;
	}

	void HvkDescriptorWriter::overwrite(VkDescriptorSet& set)
	{
		for (auto& write : writes_) {
			write.dstSet = set;
		}
		vkUpdateDescriptorSets(pool_.hvkDevice_.device(), writes_.size(), writes_.data(), 0, nullptr);
	}
}
