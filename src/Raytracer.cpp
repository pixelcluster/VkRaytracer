#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <volk.h>

HardwareSphereRaytracer::HardwareSphereRaytracer(size_t windowWidth, size_t windowHeight, size_t sphereCount)
	: m_device(windowWidth, windowHeight, false) {
	VkBufferCreateInfo stagingBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
												   .size = static_cast<uint32_t>(
													   (sizeof(VkAabbPositionsKHR) + sizeof(float) * 4) * sphereCount),
												   .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
												   .sharingMode = VK_SHARING_MODE_EXCLUSIVE };

	VkAccelerationStructureGeometryAabbsDataKHR geometryData{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR, .stride = sizeof(VkAabbPositionsKHR)
	};

	VkAccelerationStructureGeometryKHR geometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
													.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR,
													.geometry = { .aabbs = geometryData } };

	std::vector<VkAccelerationStructureGeometryKHR*> geometryPointers =
		std::vector<VkAccelerationStructureGeometryKHR*>(sphereCount, &geometry);

	std::vector<uint32_t> maxPrimitiveCounts = std::vector<uint32_t>(sphereCount, 1);

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.geometryCount = static_cast<uint32_t>(sphereCount),
		.ppGeometries = geometryPointers.data()
	};

	VkAccelerationStructureBuildSizesInfoKHR sizesInfo;

	vkGetAccelerationStructureBuildSizesKHR(m_device.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
											&buildGeometryInfo, maxPrimitiveCounts.data(), &sizesInfo);

	VkPhysicalDeviceAccelerationStructurePropertiesKHR structureProperties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
	};
	VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
												.pNext = static_cast<void*>(&structureProperties) };
	vkGetPhysicalDeviceProperties2(m_device.physicalDevice(), &properties2);

	size_t scratchSize = std::max(sizesInfo.buildScratchSize, sizesInfo.updateScratchSize);

	size_t deviceMemorySize = sizesInfo.accelerationStructureSize + sphereCount * sizeof(VkAabbPositionsKHR) +
							  scratchSize + structureProperties.minAccelerationStructureScratchOffsetAlignment;

	verifyResult(vkAllocateMemory(m_device.device(), ))

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