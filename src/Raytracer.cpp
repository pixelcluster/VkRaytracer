#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <volk.h>

HardwareSphereRaytracer::HardwareSphereRaytracer(size_t windowWidth, size_t windowHeight, size_t sphereCount)
	: m_device(windowWidth, windowHeight, false) {

	// Get acceleration structure sizes
	VkAccelerationStructureBuildSizesInfoKHR blasSizeInfo = blasSize(sphereCount);
	VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo = tlasSize(sphereCount);

	// Get alignment properties
	VkPhysicalDeviceAccelerationStructurePropertiesKHR structureProperties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
	};
	VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
												.pNext = static_cast<void*>(&structureProperties) };
	vkGetPhysicalDeviceProperties2(m_device.physicalDevice(), &properties2);

	size_t scratchSize = std::max(blasSizeInfo.buildScratchSize, blasSizeInfo.updateScratchSize);
	size_t tlasScratchSize = std::max(tlasSizeInfo.buildScratchSize, tlasSizeInfo.updateScratchSize);

	VkDeviceSize accelerationStructureBufferSize = 0;
	VkDeviceSize scratchBufferSize = 0;

	// Calculate buffer sizes from acceleration structure sizes + alignment from padding
	// Buffer memory layout (applies to both scratch buffers and acceleration structure storage):
	//---Start of buffer----
	//- BLAS 1
	//- padding to fit alignment, if necessary
	//- BLAS 2
	//- padding
	//- BLAS ...
	//- padding
	//- BLAS N
	//- padding
	//- TLAS
	//----End of buffer----

	VkDeviceSize accelerationStructureBufferPaddingRemainder = blasSizeInfo.accelerationStructureSize % 256;
	if (accelerationStructureBufferPaddingRemainder) {
		// apply padding
		accelerationStructureBufferSize =
			sphereCount * (blasSizeInfo.accelerationStructureSize + 256 - accelerationStructureBufferPaddingRemainder);
	} else {
		accelerationStructureBufferSize = sphereCount * blasSizeInfo.accelerationStructureSize;
	}
	// is already aligned because each structure has proper padding appended
	// (including the one before the TLAS)
	accelerationStructureBufferSize += tlasSizeInfo.accelerationStructureSize;

	//Calculate total scratch buffer size

	VkDeviceSize scratchBufferPaddingRemainder =
		scratchSize % structureProperties.minAccelerationStructureScratchOffsetAlignment;
	if (scratchBufferPaddingRemainder) {
		scratchBufferSize =
			sphereCount * (scratchSize + structureProperties.minAccelerationStructureScratchOffsetAlignment -
						   scratchBufferPaddingRemainder);
	} else {
		scratchBufferSize = sphereCount * scratchSize;
	}
	// is already aligned because each scratch buffer has proper padding appended
	// (including the one before the TLAS scratch buffer)
	scratchBufferSize += tlasScratchSize;

	//Create and bind buffers (to dedicated allocations if needed)

	VkDeviceSize mergedAllocationSize = 0;
	VkDeviceSize stagingAllocationSize = 0;

	VkBufferCreateInfo stagingBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
												   .size = static_cast<uint32_t>(
													   (sizeof(VkAabbPositionsKHR) + sizeof(float) * 4) * sphereCount),
												   .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
												   .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
	VkBufferCreateInfo structureBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
													 .size = accelerationStructureBufferSize,
													 .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
															  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
													 .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
	VkBufferCreateInfo scratchBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
												   .size = scratchBufferSize,
												   .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
												   .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
	VkBufferCreateInfo dataBufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(VkAabbPositionsKHR) * sphereCount,
		.usage =
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VkBufferCreateInfo sphereDataBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
													  .size = sizeof(float) * 4 * sphereCount,
													  .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
															   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
													  .sharingMode = VK_SHARING_MODE_EXCLUSIVE };

	BufferAllocationRequirements stagingBufferRequirements;
	BufferAllocationRequirements structureBufferRequirements;
	BufferAllocationRequirements scratchBufferRequirements;
	BufferAllocationRequirements dataBufferRequirements;
	BufferAllocationRequirements sphereDataBufferRequirements;

	addDataBuffer(stagingBufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  m_stagingBuffer, stagingBufferRequirements, stagingAllocationSize);

	addDataBuffer(structureBufferCreateInfo, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_accelerationStructureBuffer,
				  structureBufferRequirements, mergedAllocationSize);
	addDataBuffer(scratchBufferCreateInfo, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_accelerationStructureScratchBuffer,
				  scratchBufferRequirements, mergedAllocationSize);
	addDataBuffer(dataBufferCreateInfo, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_accelerationStructureDataBuffer,
				  dataBufferRequirements, mergedAllocationSize);
	addDataBuffer(sphereDataBufferCreateInfo, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_sphereDataBuffer,
				  sphereDataBufferRequirements, mergedAllocationSize);

	if (mergedAllocationSize > 0) {
		VkMemoryPropertyFlags requiredFlags = 0;
		requiredFlags |=
			structureBufferRequirements.makeDedicatedAllocation ? 0 : structureBufferRequirements.memoryTypeBits;
		requiredFlags |= dataBufferRequirements.makeDedicatedAllocation ? 0 : dataBufferRequirements.memoryTypeBits;
		requiredFlags |=
			sphereDataBufferRequirements.makeDedicatedAllocation ? 0 : sphereDataBufferRequirements.memoryTypeBits;

		VkMemoryAllocateInfo memoryAllocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
													.allocationSize = mergedAllocationSize,
													.memoryTypeIndex = m_device.findBestMemoryIndex(
														requiredFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) };
		verifyResult(vkAllocateMemory(m_device.device(), &memoryAllocateInfo, nullptr, &m_deviceMemory));

		VkDeviceSize mergedMemoryOffset = 0;
		if (!structureBufferRequirements.makeDedicatedAllocation)
			bindDataBuffer(m_accelerationStructureBuffer, m_deviceMemory, structureBufferRequirements,
						   mergedMemoryOffset);
		if (!scratchBufferRequirements.makeDedicatedAllocation)
			bindDataBuffer(m_accelerationStructureScratchBuffer, m_deviceMemory, scratchBufferRequirements,
						   mergedMemoryOffset);
		if (!dataBufferRequirements.makeDedicatedAllocation)
			bindDataBuffer(m_accelerationStructureDataBuffer, m_deviceMemory, dataBufferRequirements,
						   mergedMemoryOffset);
		if (!sphereDataBufferRequirements.makeDedicatedAllocation)
			bindDataBuffer(m_sphereDataBuffer, m_deviceMemory, sphereDataBufferRequirements, mergedMemoryOffset);
	}

	if (stagingAllocationSize > 0) {
		VkMemoryAllocateInfo memoryAllocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
													.allocationSize = stagingAllocationSize,
													.memoryTypeIndex = m_device.findBestMemoryIndex(
														stagingBufferRequirements.memoryTypeBits |
															VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
														VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0) };
		verifyResult(
			vkAllocateMemory(m_device.device(), &memoryAllocateInfo, nullptr, &m_stagingBuffer.dedicatedMemory));

		vkBindBufferMemory(m_device.device(), m_stagingBuffer.buffer, m_stagingBuffer.dedicatedMemory, 0);
	}

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {

	};
}

HardwareSphereRaytracer::~HardwareSphereRaytracer() {}

bool HardwareSphereRaytracer::update() {
	FrameData frameData = m_device.beginFrame();
	if (!frameData.commandBuffer) {
		return !m_device.window().shouldWindowClose();
	}

	VkImageSubresourceRange imageRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
										   .baseMipLevel = 0,
										   .levelCount = 1,
										   .baseArrayLayer = 0,
										   .layerCount = 1 };

	VkImageMemoryBarrier memoryBarrierBefore = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
												 .srcAccessMask = 0,
												 .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
												 .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
												 .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
												 .srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												 .dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												 .image = frameData.swapchainImage,
												 .subresourceRange = imageRange };

	vkCmdPipelineBarrier(frameData.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
						 nullptr, 0, nullptr, 1, &memoryBarrierBefore);

	VkClearColorValue clearValue = { .float32 = { 0.7f, 0.2f, 0.7f, 1.0f } };

	vkCmdClearColorImage(frameData.commandBuffer, frameData.swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						 &clearValue, 1, &imageRange);

	VkImageMemoryBarrier memoryBarrierAfter = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
												.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
												.dstAccessMask = 0,
												.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
												.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
												.srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												.dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												.image = frameData.swapchainImage,
												.subresourceRange = imageRange };

	vkCmdPipelineBarrier(frameData.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
						 nullptr, 0, nullptr, 1, &memoryBarrierAfter);

	return m_device.endFrame();
}

void HardwareSphereRaytracer::buildAccelerationStructures(const std::vector<Sphere>& spheres) {}

void HardwareSphereRaytracer::addDataBuffer(const VkBufferCreateInfo& info,
											VkMemoryPropertyFlags additionalRequiredFlags,
											VkMemoryPropertyFlags preferredFlags, BufferAllocation& targetAllocation,
											BufferAllocationRequirements& targetRequirements,
											VkDeviceSize& mergedMemorySize) {
	vkCreateBuffer(m_device.device(), &info, nullptr, &targetAllocation.buffer);

	targetRequirements = m_device.requirements(targetAllocation.buffer);
	if (targetRequirements.makeDedicatedAllocation)
		m_device.allocateDedicated(targetAllocation, targetRequirements.size,
								   targetRequirements.memoryTypeBits | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0, 0);
	else {
		if (targetRequirements.alignment) {
			size_t alignmentRemainder = mergedMemorySize % targetRequirements.alignment;
			if (alignmentRemainder)
				mergedMemorySize += targetRequirements.alignment - alignmentRemainder;
		}
		mergedMemorySize += targetRequirements.size;
	}
}

void HardwareSphereRaytracer::bindDataBuffer(BufferAllocation& allocation, VkDeviceMemory memory,
											 BufferAllocationRequirements& requirements,
											 VkDeviceSize& mergedMemoryOffset) {
	if (requirements.alignment) {
		size_t alignmentRemainder = mergedMemoryOffset % requirements.alignment;
		if (alignmentRemainder)
			mergedMemoryOffset += requirements.alignment - alignmentRemainder;
	}
	verifyResult(vkBindBufferMemory(m_device.device(), allocation.buffer, memory, mergedMemoryOffset));

	mergedMemoryOffset += requirements.size;
}

VkAccelerationStructureBuildSizesInfoKHR HardwareSphereRaytracer::blasSize(size_t sphereCount) {
	VkAccelerationStructureGeometryAabbsDataKHR geometryData = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR, .stride = sizeof(VkAabbPositionsKHR)
	};

	VkAccelerationStructureGeometryKHR geometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
													.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR,
													.geometry = { .aabbs = geometryData } };

	std::vector<uint32_t> maxPrimitiveCounts = std::vector<uint32_t>(sphereCount, 1);

	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildGeometryInfos =
		std::vector<VkAccelerationStructureBuildGeometryInfoKHR>(
			sphereCount, { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
						   .geometryCount = 1,
						   .pGeometries = &geometry });

	VkAccelerationStructureBuildSizesInfoKHR sizesInfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};

	if (sphereCount > 0)
		vkGetAccelerationStructureBuildSizesKHR(m_device.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
												&buildGeometryInfos[0], maxPrimitiveCounts.data(), &sizesInfo);
	return sizesInfo;
}

VkAccelerationStructureBuildSizesInfoKHR HardwareSphereRaytracer::tlasSize(size_t sphereCount) {
	return VkAccelerationStructureBuildSizesInfoKHR();
}
