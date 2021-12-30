#include <ErrorHelper.hpp>
#include <numeric>
#include <util/MemoryAllocator.hpp>
#include <volk.h>

MemoryAllocator::MemoryAllocator(RayTracingDevice& device) : m_device(device) {}

MemoryAllocator::~MemoryAllocator() {
	for (auto& memory : m_stagingBufferMemoryAllocations) {
		vkFreeMemory(m_device.device(), memory.memory, nullptr);
	}
	for (auto& memory : m_deviceBufferMemoryAllocations) {
		vkFreeMemory(m_device.device(), memory.memory, nullptr);
	}
	for (auto& memory : m_deviceImageMemoryAllocations) {
		vkFreeMemory(m_device.device(), memory.memory, nullptr);
	}
}

void* MemoryAllocator::bindStagingBuffer(VkBuffer buffer, VkDeviceSize alignment) {
	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(m_device.device(), buffer, &requirements);

	uint32_t memoryTypeIndex = m_device.findBestMemoryIndex(
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, ~requirements.memoryTypeBits);
	VkDeviceSize allocationAlignment;
	if (alignment) {
		allocationAlignment = std::lcm(alignment, requirements.alignment);
	} else {
		allocationAlignment = requirements.alignment;
	}

	return bindResource(m_stagingBufferMemoryAllocations, buffer, vkBindBufferMemory, requirements.size,
						allocationAlignment, memoryTypeIndex, nullptr, bufferMemorySize, true).mappedMemoryPointer;
}

void MemoryAllocator::bindDeviceBuffer(VkBuffer buffer, VkDeviceSize alignment) {
	VkMemoryAllocateFlagsInfo flagsInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
											.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(m_device.device(), buffer, &requirements);

	uint32_t memoryTypeIndex =
		m_device.findBestMemoryIndex(0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ~requirements.memoryTypeBits);
	VkDeviceSize allocationAlignment;
	if (alignment) {
		if (requirements.alignment % alignment == 0 || alignment % requirements.alignment == 0) {
			allocationAlignment = std::max(requirements.alignment, alignment);
		} else
			allocationAlignment = std::lcm(alignment, requirements.alignment);
	} else {
		allocationAlignment = requirements.alignment;
	}

	bindResource(m_deviceBufferMemoryAllocations, buffer, vkBindBufferMemory, requirements.size, allocationAlignment,
				 memoryTypeIndex, &flagsInfo, bufferMemorySize);
}

ImageAllocation MemoryAllocator::bindDeviceImage(VkImage image, VkDeviceSize alignment) {
	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(m_device.device(), image, &requirements);

	uint32_t memoryTypeIndex =
		m_device.findBestMemoryIndex(0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ~requirements.memoryTypeBits);

	VkDeviceSize allocationAlignment;
	if (alignment) {
		allocationAlignment = std::lcm(alignment, requirements.alignment);
	} else {
		allocationAlignment = requirements.alignment;
	}
	BindResult result = bindResource(m_deviceImageMemoryAllocations, image, vkBindImageMemory, requirements.size,
									 allocationAlignment, memoryTypeIndex, nullptr, imageMemorySize);
	return { .memoryAllocationIndex = result.memoryIndex,
			 .offset = result.offset,
			 .size = requirements.size + result.alignmentPadding };
}

void MemoryAllocator::freeImage(const ImageAllocation& allocation) {
	if (allocation.memoryAllocationIndex >= m_deviceImageMemoryAllocations.size())
		return;
	if (m_deviceImageMemoryAllocations[allocation.memoryAllocationIndex].memoryOffset ==
		allocation.offset + allocation.size) {
		m_deviceImageMemoryAllocations[allocation.memoryAllocationIndex].memoryOffset -=
			allocation.offset + allocation.size;
	}
}
