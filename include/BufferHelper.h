#pragma once

#define VK_NO_PROTOTYPES
#include <ErrorHelper.hpp>
#include <RayTracingDevice.hpp>
#include <numeric>
#include <vector>
#include <volk.h>
#include <vulkan/vulkan.h>

struct BufferAllocation {
	VkBuffer buffer;
	VkDeviceMemory dedicatedMemory = VK_NULL_HANDLE;
};

struct BufferAllocationBatch {
	VkDeviceMemory sharedMemory = VK_NULL_HANDLE;
	std::vector<BufferAllocation> buffers;
};

struct BufferSubAllocation {
	VkDeviceSize offset;
	VkDeviceSize size;
	//Must be filled by user
	VkDeviceAddress address;
};

struct BufferInfo {
	VkDeviceSize size = 0;
	VkDeviceSize requiredAlignment = 0;
	VkBufferUsageFlags usage = 0;
	VkMemoryPropertyFlags requiredProperties = 0;
	VkMemoryPropertyFlags preferredProperties = 0;
	VkMemoryPropertyFlags forbiddenProperties = 0;
};

struct SharedMemoryBufferInfo {
	size_t bufferIndex;
	VkDeviceSize memoryOffset;
};

inline BufferSubAllocation addSuballocation(BufferInfo& info, VkDeviceSize size, VkDeviceSize alignment = 0,
	VkBufferUsageFlags usage = 0, VkMemoryPropertyFlags requiredProperties = 0,
	VkMemoryPropertyFlags preferredProperties = 0,
	VkMemoryPropertyFlags forbiddenProperties = 0) {
	VkDeviceSize newOffset = info.size;
	if (alignment) {
		newOffset = std::lcm(info.size, alignment);
		if (info.requiredAlignment)
			info.requiredAlignment = std::lcm(info.requiredAlignment, alignment);
		else
			info.requiredAlignment = alignment;
	}

	info.size = newOffset + size;
	info.usage |= usage;
	info.requiredProperties |= requiredProperties;
	info.preferredProperties |= preferredProperties;
	info.forbiddenProperties |= forbiddenProperties;
	return BufferSubAllocation{ .offset = newOffset, .size = size };
}

inline BufferAllocationBatch allocateBatch(RayTracingDevice& device, const std::vector<BufferInfo>& bufferInfos,
										   VkMemoryAllocateFlags memoryAllocateFlags) {
	std::vector<SharedMemoryBufferInfo> sharedBufferInfos = {};

	VkMemoryAllocateFlagsInfo flagsInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
											.flags = memoryAllocateFlags };

	VkDeviceSize sharedAllocationSize = 0;
	VkMemoryPropertyFlags sharedRequiredFlags = 0;
	VkMemoryPropertyFlags sharedPreferredFlags = 0;
	VkMemoryPropertyFlags sharedForbiddenFlags = 0;

	BufferAllocationBatch batch;
	batch.buffers.reserve(bufferInfos.size());
	for (size_t i = 0; i < bufferInfos.size(); ++i) {
		BufferAllocation newAllocation;

		VkBufferCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
										  .size = bufferInfos[i].size,
										  .usage = bufferInfos[i].usage,
										  .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
		verifyResult(vkCreateBuffer(device.device(), &createInfo, nullptr, &newAllocation.buffer));

		BufferAllocationRequirements requirements = device.requirements(newAllocation.buffer);
		if (requirements.makeDedicatedAllocation) {
			VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
				.pNext = memoryAllocateFlags ? &flagsInfo : nullptr,
				.buffer = newAllocation.buffer
			};
			VkMemoryAllocateInfo allocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
														   .pNext = &dedicatedAllocateInfo,
														   .allocationSize = requirements.size,
														   .memoryTypeIndex = device.findBestMemoryIndex(
															   bufferInfos[i].requiredProperties,
															   bufferInfos[i].preferredProperties,
															   bufferInfos[i].forbiddenProperties) };
			verifyResult(vkAllocateMemory(device.device(), &allocateInfo, nullptr, &newAllocation.dedicatedMemory));
			
			vkBindBufferMemory(device.device(), newAllocation.buffer, newAllocation.dedicatedMemory, 0);
		} else {
			VkDeviceSize realAlignment;
			if (!bufferInfos[i].requiredAlignment) {
				realAlignment = requirements.alignment;
			} else if (!requirements.alignment) {
				realAlignment = bufferInfos[i].requiredAlignment;
			} else {
				realAlignment = std::lcm(requirements.alignment, bufferInfos[i].requiredAlignment);
			}

			if (sharedAllocationSize && realAlignment) {
				size_t alignmentRemainder = sharedAllocationSize % realAlignment;
				if (alignmentRemainder) {
					sharedAllocationSize += realAlignment - alignmentRemainder;
				}
			}

			sharedBufferInfos.push_back({ .bufferIndex = i, .memoryOffset = sharedAllocationSize });
			sharedAllocationSize += requirements.size;

			sharedRequiredFlags |= bufferInfos[i].requiredProperties;
			sharedPreferredFlags |= bufferInfos[i].preferredProperties;
			sharedForbiddenFlags |= bufferInfos[i].forbiddenProperties | ~(requirements.memoryTypeBits);
		}
		batch.buffers.push_back(std::move(newAllocation));
	}

	VkMemoryAllocateInfo allocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										  .pNext = memoryAllocateFlags ? &flagsInfo : nullptr,
										  .allocationSize = sharedAllocationSize,
										  .memoryTypeIndex = device.findBestMemoryIndex(
											  sharedRequiredFlags, sharedPreferredFlags, sharedForbiddenFlags) };
	verifyResult(vkAllocateMemory(device.device(), &allocateInfo, nullptr, &batch.sharedMemory));

	for (auto& info : sharedBufferInfos) {
		vkBindBufferMemory(device.device(), batch.buffers[info.bufferIndex].buffer, batch.sharedMemory,
						   info.memoryOffset);
	}
	return batch;
}

inline void allocateDedicated(RayTracingDevice& device, BufferAllocation& allocation, VkDeviceSize requiredSize,
							  VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred,
							  VkMemoryPropertyFlags forbidden) {
	VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
															.buffer = allocation.buffer };
	VkMemoryAllocateInfo allocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										  .pNext = &dedicatedAllocateInfo,
										  .allocationSize = requiredSize,
										  .memoryTypeIndex =
											  device.findBestMemoryIndex(required, preferred, forbidden) };

	vkAllocateMemory(device.device(), &allocateInfo, nullptr, &allocation.dedicatedMemory);

	vkBindBufferMemory(device.device(), allocation.buffer, allocation.dedicatedMemory, 0);
}