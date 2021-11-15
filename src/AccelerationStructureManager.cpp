#include <AccelerationStructureManager.hpp>
#include <ErrorHelper.hpp>
#include <volk.h>

AccelerationStructureBatchData AccelerationStructureManager::createData(
	RayTracingDevice& device, const std::vector<AccelerationStructureInitInfo>& initInfos) {
	AccelerationStructureBatchData batchData = {};
	std::vector<AccelerationStructureData> accelerationStructures =
		std::vector<AccelerationStructureData>(initInfos.size(), AccelerationStructureData{});

	// Get alignment properties
	VkPhysicalDeviceAccelerationStructurePropertiesKHR structureProperties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
	};
	VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
												.pNext = static_cast<void*>(&structureProperties) };
	vkGetPhysicalDeviceProperties2(device.physicalDevice(), &properties2);

	VkDeviceSize accelerationStructureBufferSize = 0;
	VkDeviceSize scratchBufferSize = 0;
	VkDeviceSize dataBufferSize = 0;

	for (size_t i = 0; i < initInfos.size(); ++i) {
		VkAccelerationStructureBuildGeometryInfoKHR geometryInfo = {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
			.type = initInfos[i].type,
			.flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
					 VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
			.geometryCount = static_cast<uint32_t>(initInfos[i].geometries.size()),
			.pGeometries = initInfos[i].geometries.data()
		};

		VkAccelerationStructureBuildSizesInfoKHR sizesInfo = {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
		};
		vkGetAccelerationStructureBuildSizesKHR(device.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
												&geometryInfo, &initInfos[i].maxPrimitiveCount, &sizesInfo);
		VkDeviceSize scratchSize = std::max(sizesInfo.buildScratchSize, sizesInfo.updateScratchSize);
		accelerationStructures[i].accelerationStructureSize = sizesInfo.accelerationStructureSize;

		// Calculate buffer sizes from acceleration structure sizes + alignment from padding
		// Buffer memory layout (applies to both scratch buffers and acceleration structure storage):
		//---Start of buffer----
		//- AS 1
		//- padding to fit alignment, if necessary
		//- AS 2
		//- padding
		//- ...
		//----End of buffer----

		accelerationStructures[i].structureBufferOffset = accelerationStructureBufferSize;
		accelerationStructureBufferSize += sizesInfo.accelerationStructureSize;
		VkDeviceSize accelerationStructureBufferPaddingRemainder = sizesInfo.accelerationStructureSize % 256;
		if (accelerationStructureBufferPaddingRemainder && i < initInfos.size() - 1) {
			// apply padding
			accelerationStructureBufferSize += 256 - accelerationStructureBufferPaddingRemainder;
		}

		// Calculate total scratch buffer size

		accelerationStructures[i].scratchBufferBaseOffset = scratchBufferSize;
		VkDeviceSize scratchBufferPaddingRemainder =
			scratchSize % structureProperties.minAccelerationStructureScratchOffsetAlignment;
		scratchBufferSize += scratchSize;
		if (scratchBufferPaddingRemainder && i < initInfos.size() - 1) {
			scratchBufferSize +=
				structureProperties.minAccelerationStructureScratchOffsetAlignment - scratchBufferPaddingRemainder;
		}
	}

	// Create and bind buffers (to dedicated allocations if needed)

	VkDeviceSize mergedAllocationSize = 0;
	VkDeviceSize stagingAllocationSize = 0;

	VkBufferCreateInfo structureBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
													 .size = accelerationStructureBufferSize,
													 .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
													 .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
	VkBufferCreateInfo scratchBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
												   .size = scratchBufferSize,
												   .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
															VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
												   .sharingMode = VK_SHARING_MODE_EXCLUSIVE };

	BufferAllocationRequirements structureBufferRequirements;
	BufferAllocationRequirements scratchBufferRequirements;

	addDataBuffer(device, structureBufferCreateInfo, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, batchData.structureBuffer,
				  structureBufferRequirements, mergedAllocationSize);
	addDataBuffer(device, scratchBufferCreateInfo, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, batchData.scratchBuffer,
				  scratchBufferRequirements, mergedAllocationSize);

	if (mergedAllocationSize > 0) {
		VkMemoryAllocateFlagsInfo flagsInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
												.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };

		VkMemoryPropertyFlags requiredFlags = 0;
		requiredFlags |=
			structureBufferRequirements.makeDedicatedAllocation ? ~0U : structureBufferRequirements.memoryTypeBits;

		VkMemoryAllocateInfo memoryAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = scratchBufferRequirements.makeDedicatedAllocation ? nullptr : &flagsInfo,
			.allocationSize = mergedAllocationSize,
			.memoryTypeIndex = device.findBestMemoryIndex(0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ~requiredFlags)
		};
		verifyResult(vkAllocateMemory(device.device(), &memoryAllocateInfo, nullptr, &batchData.sharedStructureMemory));

		VkDeviceSize mergedMemoryOffset = 0;
		scratchBufferRequirements.alignment = std::lcm(
			scratchBufferRequirements.alignment, structureProperties.minAccelerationStructureScratchOffsetAlignment);
		if (!structureBufferRequirements.makeDedicatedAllocation)
			bindDataBuffer(device, batchData.structureBuffer, batchData.sharedStructureMemory,
						   structureBufferRequirements, mergedMemoryOffset);
		if (!scratchBufferRequirements.makeDedicatedAllocation)
			bindDataBuffer(device, batchData.scratchBuffer, batchData.sharedStructureMemory, scratchBufferRequirements,
						   mergedMemoryOffset);
	}

	for (size_t i = 0; i < accelerationStructures.size(); ++i) {
		VkAccelerationStructureCreateInfoKHR createInfo = {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
			.buffer = batchData.structureBuffer.buffer,
			.offset = accelerationStructures[i].structureBufferOffset,
			.size = accelerationStructures[i].accelerationStructureSize,
			.type = initInfos[i].type
		};
		verifyResult(vkCreateAccelerationStructureKHR(device.device(), &createInfo, nullptr,
													  &accelerationStructures[i].structure));

		VkAccelerationStructureDeviceAddressInfoKHR blasDeviceAddressInfo = {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
			.accelerationStructure = accelerationStructures[i].structure
		};

		accelerationStructures[i].deviceAddress =
			vkGetAccelerationStructureDeviceAddressKHR(device.device(), &blasDeviceAddressInfo);
	}

	// Retrieve needed buffer device addresses
	VkBufferDeviceAddressInfo deviceAddressInfo;
	deviceAddressInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
						  .buffer = batchData.scratchBuffer.buffer };

	batchData.scratchBufferDeviceAddress = vkGetBufferDeviceAddress(device.device(), &deviceAddressInfo);

	batchData.structures = std::move(accelerationStructures);
	return batchData;
}

void AccelerationStructureManager::addDataBuffer(RayTracingDevice& device, const VkBufferCreateInfo& info,
												 VkMemoryPropertyFlags additionalRequiredFlags,
												 VkMemoryPropertyFlags preferredFlags,
												 BufferAllocation& targetAllocation,
												 BufferAllocationRequirements& targetRequirements,
												 VkDeviceSize& mergedMemorySize) {
	verifyResult(vkCreateBuffer(device.device(), &info, nullptr, &targetAllocation.buffer));

	targetRequirements = device.requirements(targetAllocation.buffer);
	if (targetRequirements.makeDedicatedAllocation)
		allocateDedicated(device, targetAllocation, targetRequirements.size, targetRequirements.memoryTypeBits, 0, 0);
	else {
		if (targetRequirements.alignment) {
			size_t alignmentRemainder = mergedMemorySize % targetRequirements.alignment;
			if (alignmentRemainder)
				mergedMemorySize += targetRequirements.alignment - alignmentRemainder;
		}
		mergedMemorySize += targetRequirements.size;
	}
}

void AccelerationStructureManager::bindDataBuffer(RayTracingDevice& device, BufferAllocation& allocation,
												  VkDeviceMemory memory, BufferAllocationRequirements& requirements,
												  VkDeviceSize& mergedMemoryOffset) {
	if (requirements.alignment) {
		size_t alignmentRemainder = mergedMemoryOffset % requirements.alignment;
		if (alignmentRemainder)
			mergedMemoryOffset += requirements.alignment - alignmentRemainder;
	}
	verifyResult(vkBindBufferMemory(device.device(), allocation.buffer, memory, mergedMemoryOffset));

	mergedMemoryOffset += requirements.size;
}
