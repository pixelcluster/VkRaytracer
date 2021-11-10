#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <volk.h>

HardwareSphereRaytracer::HardwareSphereRaytracer(size_t windowWidth, size_t windowHeight, size_t sphereCount)
	: m_device(windowWidth, windowHeight, false) {
	m_stagingBuffer.dataSize =
		sizeof(VkAabbPositionsKHR) + (sizeof(VkAccelerationStructureInstanceKHR) + sizeof(float) * 4) * sphereCount;

	std::vector<AccelerationStructureInitInfo> initInfos = {
		{ .geometries = { { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
							.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR,
							.geometry = { .aabbs = { VkAccelerationStructureGeometryAabbsDataKHR{
											  .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
											  .stride = sizeof(VkAabbPositionsKHR) } } } } },
		  .maxPrimitiveCount = 1,
		  .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR },
		{ .geometries = { { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
							.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
							.geometry = { .instances = { VkAccelerationStructureGeometryInstancesDataKHR{
											  .sType =
												  VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
											  .arrayOfPointers = VK_FALSE } } } } },
		  .maxPrimitiveCount = 1,
		  .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR }
	};

	m_structureData = AccelerationStructureManager::createData(m_device, initInfos);

	VkBufferCreateInfo stagingBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
												   .size = m_stagingBuffer.dataSize * frameInFlightCount,
												   .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
												   .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
	VkBufferCreateInfo dataBufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = m_accelerationStructureDataBuffer.dataSize * frameInFlightCount,
		.usage =
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VkBufferCreateInfo sphereDataBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
													  .size = m_sphereDataBuffer.dataSize * frameInFlightCount,
													  .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
															   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
													  .sharingMode = VK_SHARING_MODE_EXCLUSIVE };

	verifyResult(vkCreateBuffer(m_device.device(), &stagingBufferCreateInfo, nullptr, &m_stagingBuffer.buffer));
	verifyResult(
		vkCreateBuffer(m_device.device(), &dataBufferCreateInfo, nullptr, &m_accelerationStructureDataBuffer.buffer));
	verifyResult(vkCreateBuffer(m_device.device(), &sphereDataBufferCreateInfo, nullptr, &m_sphereDataBuffer.buffer));

	BufferAllocationRequirements stagingBufferRequirements = m_device.requirements(m_stagingBuffer.buffer);
	BufferAllocationRequirements dataBufferRequirements =
		m_device.requirements(m_accelerationStructureDataBuffer.buffer);
	BufferAllocationRequirements sphereDataBufferRequirements = m_device.requirements(m_sphereDataBuffer.buffer);

	m_accelerationStructureDataBuffer.dataSize =
		sizeof(VkAabbPositionsKHR) + sizeof(VkAccelerationStructureInstanceKHR) * sphereCount;

	VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
															.buffer = m_stagingBuffer.buffer };
	VkMemoryAllocateInfo memoryAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = stagingBufferRequirements.makeDedicatedAllocation ? &dedicatedAllocateInfo : nullptr,
		.allocationSize = stagingBufferRequirements.size,
		.memoryTypeIndex =
			m_device.findBestMemoryIndex(stagingBufferRequirements.memoryTypeBits |
											 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
										 0, 0)
	};
	verifyResult(vkAllocateMemory(m_device.device(), &memoryAllocateInfo, nullptr, &m_stagingBuffer.dedicatedMemory));

	vkBindBufferMemory(m_device.device(), m_stagingBuffer.buffer, m_stagingBuffer.dedicatedMemory, 0);
	verifyResult(vkMapMemory(m_device.device(), m_stagingBuffer.dedicatedMemory, 0, stagingBufferRequirements.size, 0,
							 &m_mappedStagingBuffer));

	VkDeviceSize sharedDeviceAllocateSize = 0;
	VkDeviceSize sphereDataOffset = 0;
	if (dataBufferRequirements.makeDedicatedAllocation) {
		dedicatedAllocateInfo.buffer = m_accelerationStructureDataBuffer.buffer;
		memoryAllocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
							   .pNext = &dedicatedAllocateInfo,
							   .allocationSize = dataBufferRequirements.size,
							   .memoryTypeIndex = m_device.findBestMemoryIndex(
								   dataBufferRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) };
		verifyResult(vkAllocateMemory(m_device.device(), &memoryAllocateInfo, nullptr,
									  &m_accelerationStructureDataBuffer.dedicatedMemory));

		vkBindBufferMemory(m_device.device(), m_accelerationStructureDataBuffer.buffer,
						   m_accelerationStructureDataBuffer.dedicatedMemory, 0);
	} else
		sharedDeviceAllocateSize += dataBufferRequirements.size;

	if (sphereDataBufferRequirements.makeDedicatedAllocation) {
		dedicatedAllocateInfo.buffer = m_sphereDataBuffer.buffer;
		memoryAllocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
							   .pNext = &dedicatedAllocateInfo,
							   .allocationSize = sphereDataBufferRequirements.size,
							   .memoryTypeIndex =
								   m_device.findBestMemoryIndex(sphereDataBufferRequirements.memoryTypeBits,
																VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) };
		verifyResult(
			vkAllocateMemory(m_device.device(), &memoryAllocateInfo, nullptr, &m_sphereDataBuffer.dedicatedMemory));

		vkBindBufferMemory(m_device.device(), m_sphereDataBuffer.buffer, m_sphereDataBuffer.dedicatedMemory, 0);
	} else {
		if (sphereDataBufferRequirements.alignment) {
			VkDeviceSize alignmentRemainder = sharedDeviceAllocateSize % sphereDataBufferRequirements.alignment;
			if (alignmentRemainder) {
				sharedDeviceAllocateSize += sphereDataBufferRequirements.alignment - alignmentRemainder;
			}
		}
		sphereDataOffset = sharedDeviceAllocateSize;
		sharedDeviceAllocateSize += dataBufferRequirements.size;
	}

	if (sharedDeviceAllocateSize) {
		memoryAllocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
							   .pNext = nullptr,
							   .allocationSize = sharedDeviceAllocateSize,
							   .memoryTypeIndex = m_device.findBestMemoryIndex(
								   sphereDataBufferRequirements.memoryTypeBits | dataBufferRequirements.memoryTypeBits,
								   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) };
		verifyResult(vkAllocateMemory(m_device.device(), &memoryAllocateInfo, nullptr, &m_deviceMemory));

		if (!dataBufferRequirements.makeDedicatedAllocation) {
			verifyResult(
				vkBindBufferMemory(m_device.device(), m_accelerationStructureDataBuffer.buffer, m_deviceMemory, 0));
		}
		if (!sphereDataBufferRequirements.makeDedicatedAllocation) {
			verifyResult(
				vkBindBufferMemory(m_device.device(), m_sphereDataBuffer.buffer, m_deviceMemory, sphereDataOffset));
		}
	}

	VkBufferDeviceAddressInfo deviceAddressInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
													.buffer = m_structureData.scratchBuffer.buffer };

	VkDeviceAddress scratchBufferDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &deviceAddressInfo);

	deviceAddressInfo.buffer = m_accelerationStructureDataBuffer.buffer;
	VkDeviceAddress dataBufferDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &deviceAddressInfo);

	
	// Write identity AABBs
	for (size_t i = 0; i < frameInFlightCount; ++i) {
		VkAabbPositionsKHR{ .minX = -0.5f, .minY = -0.5, .minZ = -0.5, .maxX = 0.5f, .maxY = 0.5f, .maxZ = 0.5f };
	}
}

HardwareSphereRaytracer::~HardwareSphereRaytracer() {}

bool HardwareSphereRaytracer::update(const std::vector<Sphere>& spheres) {
	FrameData frameData = m_device.beginFrame();
	if (!frameData.commandBuffer) {
		return !m_device.window().shouldWindowClose();
	}

	void* mappedFrameSection = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) +
													   m_stagingBuffer.dataSize * frameData.frameIndex);

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