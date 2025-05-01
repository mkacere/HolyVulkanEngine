#include "hvk_buffer.h"
#include <cassert>

namespace hvk {
	HvkBuffer::HvkBuffer(HvkDevice& device, VkDeviceSize instanceSize, uint32_t instanceCount, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize minOffsetAlignment) :
		hvkDevice_(device), instanceSize_(instanceSize), instanceCount_(instanceCount), usageFlags_(usageFlags), memoryPropertyFlags_(memoryPropertyFlags)
	{
		alignmentSize_ = getAlignment(instanceSize_, minOffsetAlignment);
		bufferSize_ = alignmentSize_ * instanceCount_;
		device.createBuffer(bufferSize_, usageFlags_, memoryPropertyFlags_, buffer_, memory_);
	}

	HvkBuffer::~HvkBuffer()
	{
		unmap();
		vkDestroyBuffer(hvkDevice_.device(), buffer_, nullptr);
		vkFreeMemory(hvkDevice_.device(), memory_, nullptr);
	}

	VkResult HvkBuffer::map(VkDeviceSize size, VkDeviceSize offset)
	{
		assert(buffer_ && memory_ && "Called map on buffer before create");
		return vkMapMemory(hvkDevice_.device(), memory_, offset, size, 0, &mapped_);
	}

	void HvkBuffer::unmap()
	{
		if (mapped_) {
			vkUnmapMemory(hvkDevice_.device(), memory_);
			mapped_ = nullptr;
		}
	}

	void HvkBuffer::writeToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset)
	{
		assert(mapped_ && "Cannot copy to unmapped buffer");

		if (size == VK_WHOLE_SIZE) {
			memcpy(mapped_, data, bufferSize_);
		}
		else {
			char* memOffset = (char*)mapped_;
			memOffset += offset;
			memcpy(memOffset, data, size);
		}
	}

	VkResult HvkBuffer::flush(VkDeviceSize size, VkDeviceSize offset)
	{
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = memory_;
		mappedRange.offset = offset;
		mappedRange.size = size;
		return vkFlushMappedMemoryRanges(hvkDevice_.device(), 1, &mappedRange);
	}

	VkDescriptorBufferInfo HvkBuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset)
	{
		return VkDescriptorBufferInfo{ buffer_, offset, size };
	}

	VkResult HvkBuffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
	{
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = memory_;
		mappedRange.offset = offset;
		mappedRange.size = size;
		return vkInvalidateMappedMemoryRanges(hvkDevice_.device(), 1, &mappedRange);
	}

	void HvkBuffer::writeToIndex(void* data, int index)
	{
		writeToBuffer(data, instanceSize_, index * alignmentSize_);
	}

	VkResult HvkBuffer::flushIndex(int index)
	{
		return flush(alignmentSize_, index * alignmentSize_);
	}

	VkDescriptorBufferInfo HvkBuffer::descriptorInfoForIndex(int index)
	{
		return descriptorInfo(alignmentSize_, index * alignmentSize_);
	}

	VkResult HvkBuffer::invalidateIndex(int index)
	{
		return invalidate(alignmentSize_, index * alignmentSize_);
	}

	VkDeviceSize HvkBuffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment)
	{
		if (minOffsetAlignment > 0) {
			return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
		}
		return instanceSize;
	}
}