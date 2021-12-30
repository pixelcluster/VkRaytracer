#pragma once

#include <ErrorHelper.hpp>
#include <RayTracingDevice.hpp>
#include <util/MemoryLiterals.hpp>
#include <volk.h>

struct DeviceMemoryAllocation {
	void* mappedPointer = nullptr;

	VkDeviceMemory memory;
	VkDeviceSize freeSize;
	VkDeviceSize memoryOffset;
};

struct BindResult {
	size_t memoryIndex;
	VkDeviceSize offset;
	VkDeviceSize alignmentPadding;
	void* mappedMemoryPointer;
};

struct ImageAllocation {
	size_t memoryAllocationIndex;
	VkDeviceSize offset;
	VkDeviceSize size;
};

constexpr VkDeviceSize bufferMemorySize = 32_MiB;
constexpr VkDeviceSize imageMemorySize = 256_MiB;

class MemoryAllocator {
  public:
	MemoryAllocator(RayTracingDevice& device);
	// all buffers/images allocated should be destroyed before calling destructor
	~MemoryAllocator();

	// returns mapped buffer pointer
	void* bindStagingBuffer(VkBuffer buffer, VkDeviceSize alignment);
	void bindDeviceBuffer(VkBuffer buffer, VkDeviceSize alignment);

	ImageAllocation bindDeviceImage(VkImage image, VkDeviceSize alignment);

	void freeImage(const ImageAllocation& allocation);

  private:
	// generic function performing allocations and binding resources, returns mapped memory pointer (potentially invalid
	// if memory was unmapped)
	template <typename ResourceType>
	BindResult bindResource(std::vector<DeviceMemoryAllocation>& allocations, ResourceType resource,
							VkResult (*bindCommand)(VkDevice, ResourceType, VkDeviceMemory, VkDeviceSize),
							VkDeviceSize size, VkDeviceSize alignment, uint32_t memoryTypeIndex,
							const void* memoryAllocatePNext, VkDeviceSize memoryAllocateSize, bool mapMemory = false);

	std::vector<DeviceMemoryAllocation> m_stagingBufferMemoryAllocations;
	std::vector<DeviceMemoryAllocation> m_deviceBufferMemoryAllocations;

	std::vector<DeviceMemoryAllocation> m_deviceImageMemoryAllocations;

	RayTracingDevice& m_device;
};

template <typename ResourceType>
BindResult MemoryAllocator::bindResource(std::vector<DeviceMemoryAllocation>& allocations, ResourceType resource,
										 VkResult (*bindCommand)(VkDevice, ResourceType, VkDeviceMemory, VkDeviceSize),
										 VkDeviceSize size, VkDeviceSize alignment, uint32_t memoryTypeIndex,
										 const void* memoryAllocatePNext, VkDeviceSize memoryAllocateSize,
										 bool mapMemory) {
	// Linear bump allocation for now, don't need freeing

	VkMemoryAllocateInfo allocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										  .pNext = memoryAllocatePNext,
										  .allocationSize = std::max(memoryAllocateSize, size),
										  .memoryTypeIndex = memoryTypeIndex };
	DeviceMemoryAllocation memoryAllocation = { .freeSize = allocateInfo.allocationSize };

	if (allocations.empty()) {
		verifyResult(vkAllocateMemory(m_device.device(), &allocateInfo, nullptr, &memoryAllocation.memory));
		if (mapMemory) {
			verifyResult(vkMapMemory(m_device.device(), memoryAllocation.memory, 0, memoryAllocation.freeSize, 0,
									 &memoryAllocation.mappedPointer));
		}
		allocations.push_back(memoryAllocation);
	}

	for (size_t i = allocations.size() - 1; i < allocations.size(); --i) {
		size_t alignOffset = 0;
		if (alignment) {
			size_t alignmentRemainder = allocations[i].memoryOffset % alignment;
			if (alignmentRemainder) {
				alignOffset += alignment - alignmentRemainder;
			}
		}

		if (allocations[i].freeSize >= size + alignOffset) {
			verifyResult(bindCommand(m_device.device(), resource, allocations[i].memory,
									 allocations[i].memoryOffset + alignOffset));
			VkDeviceSize allocationOffset = allocations[i].memoryOffset;
			allocations[i].freeSize -= size + alignOffset;
			allocations[i].memoryOffset += size + alignOffset;
			return { .memoryIndex = i,
					 .offset = allocationOffset,
					 .alignmentPadding = alignOffset,
					 .mappedMemoryPointer = reinterpret_cast<uint8_t*>(allocations[i].mappedPointer) +
											allocations[i].memoryOffset - (size + alignOffset) };
		}
	}

	verifyResult(vkAllocateMemory(m_device.device(), &allocateInfo, nullptr, &memoryAllocation.memory));
	if (mapMemory) {
		verifyResult(vkMapMemory(m_device.device(), memoryAllocation.memory, 0, memoryAllocation.freeSize, 0,
								 &memoryAllocation.mappedPointer));
	}
	verifyResult(bindCommand(m_device.device(), resource, memoryAllocation.memory, memoryAllocation.memoryOffset));
	memoryAllocation.freeSize -= size;
	memoryAllocation.memoryOffset += size;
	allocations.push_back(memoryAllocation);

	return { .memoryIndex = allocations.size() - 1,
			 .offset = 0,
			 .mappedMemoryPointer = allocations.back().mappedPointer };
}
