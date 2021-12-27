#pragma once

#include <RayTracingDevice.hpp>
#include <volk.h>
#include <util/MemoryLiterals.hpp>
#include <ErrorHelper.hpp>

struct DeviceMemoryAllocation {
	VkDeviceMemory memory;
	VkDeviceSize freeSize;
	VkDeviceSize memoryOffset;
};

constexpr VkDeviceSize bufferMemorySize = 32_MiB;
constexpr VkDeviceSize imageMemorySize = 256_MiB;

class MemoryAllocator {
  public:
	MemoryAllocator(RayTracingDevice& device);
	//all buffers/images allocated should be destroyed before calling destructor
	~MemoryAllocator();

	void bindStagingBuffer(VkBuffer buffer, VkDeviceSize size, VkDeviceSize alignment);
	void bindDeviceBuffer(VkBuffer buffer, VkDeviceSize size, VkDeviceSize alignment);

	void bindDeviceImage(VkImage image, VkDeviceSize byteSize, VkDeviceSize alignment);

  private:
	//generic function performing allocations and binding resources
	template<typename ResourceType>
	void bindResource(std::vector<DeviceMemoryAllocation>& allocations, ResourceType resource,
					VkResult (*bindCommand)(VkDevice, ResourceType, VkDeviceMemory, VkDeviceSize),
					VkDeviceSize size,
					VkDeviceSize alignment, uint32_t memoryTypeIndex, const void* memoryAllocatePNext);

	std::vector<DeviceMemoryAllocation> m_stagingBufferMemoryAllocations;
	std::vector<DeviceMemoryAllocation> m_deviceBufferMemoryAllocations;

	std::vector<DeviceMemoryAllocation> m_deviceImageMemoryAllocations;

	RayTracingDevice& m_device;

	uint32_t m_stagingMemoryTypeIndex;
	uint32_t m_deviceMemoryTypeIndex;
};

template <typename ResourceType>
void MemoryAllocator::bindResource(std::vector<DeviceMemoryAllocation>& allocations, ResourceType resource,
											   VkResult (*bindCommand)(VkDevice, ResourceType, VkDeviceMemory,
																	   VkDeviceSize),
											   VkDeviceSize size, VkDeviceSize alignment, uint32_t memoryTypeIndex,
											   const void* memoryAllocatePNext) {
	// Linear bump allocation for now, don't need freeing

	VkMemoryAllocateInfo allocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										  .pNext = memoryAllocatePNext,
										  .allocationSize = std::min(bufferMemorySize, size),
										  .memoryTypeIndex = memoryTypeIndex };
	DeviceMemoryAllocation memoryAllocation = { .freeSize = allocateInfo.allocationSize };

	if (allocations.empty()) {
		verifyResult(vkAllocateMemory(m_device.device(), &allocateInfo, nullptr, &memoryAllocation.memory));
		allocations.push_back(memoryAllocation);
	}

	for (size_t i = allocations.size() - 1; i < allocations.size(); --i) {
		size_t alignedSize = size;
		if (alignment) {
			size_t alignmentRemainder = allocations[i].memoryOffset % alignment;
			if (alignmentRemainder) {
				alignedSize += alignment - alignmentRemainder;
			}
		}

		if (allocations[i].freeSize >= alignedSize) {
			verifyResult(
				bindCommand(m_device.device(), resource, allocations[i].memory, allocations[i].memoryOffset));
			allocations[i].freeSize -= alignedSize;
			allocations[i].memoryOffset += alignedSize;
			return;
		}
	}

	verifyResult(vkAllocateMemory(m_device.device(), &allocateInfo, nullptr, &memoryAllocation.memory));
	verifyResult(bindCommand(m_device.device(), resource, memoryAllocation.memory, memoryAllocation.memoryOffset));
	memoryAllocation.freeSize -= size;
	memoryAllocation.memoryOffset += size;
	allocations.push_back(memoryAllocation);
}
