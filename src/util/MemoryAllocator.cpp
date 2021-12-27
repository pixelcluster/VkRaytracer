#include <util/MemoryAllocator.hpp>
#include <volk.h>
#include <ErrorHelper.hpp>

MemoryAllocator::MemoryAllocator(RayTracingDevice& device) : m_device(device) {
	m_stagingMemoryTypeIndex =
		device.findBestMemoryIndex(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, 0);
	m_deviceMemoryTypeIndex = device.findBestMemoryIndex(0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
}

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

void MemoryAllocator::bindStagingBuffer(VkBuffer buffer, VkDeviceSize size, VkDeviceSize alignment) {
	bindResource(m_stagingBufferMemoryAllocations, buffer, vkBindBufferMemory, size, alignment,
				 m_stagingMemoryTypeIndex, nullptr);
}

void MemoryAllocator::bindDeviceBuffer(VkBuffer buffer, VkDeviceSize size, VkDeviceSize alignment) {
	VkMemoryAllocateFlagsInfo flagsInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
											.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };
	bindResource(m_deviceBufferMemoryAllocations, buffer, vkBindBufferMemory, size, alignment, m_deviceMemoryTypeIndex,
			   &flagsInfo);
}

void MemoryAllocator::bindDeviceImage(VkImage image, VkDeviceSize byteSize, VkDeviceSize alignment) {
	bindResource(m_deviceImageMemoryAllocations, image, vkBindImageMemory, byteSize, alignment,
				 m_deviceMemoryTypeIndex,
				 nullptr);
}