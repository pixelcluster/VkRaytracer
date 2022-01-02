#include <Raytracer.hpp>
#include <numbers>

TriangleMeshRaytracer::TriangleMeshRaytracer(RayTracingDevice& device, MemoryAllocator& allocator, ModelLoader& loader,
											 PipelineBuilder& pipelineBuilder,
											 AccelerationStructureBuilder& accelerationStructureBuilder)
	: m_device(device), m_allocator(allocator), m_modelLoader(loader), m_pipelineBuilder(pipelineBuilder),
	  m_accelerationStructureBuilder(accelerationStructureBuilder) {
	VkAccelerationStructureKHR tlas = accelerationStructureBuilder.tlas();
	VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWrite = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
		.accelerationStructureCount = 1,
		.pAccelerationStructures = &tlas
	};
	VkWriteDescriptorSet accelerationStructureSetWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
														   .pNext = &accelerationStructureWrite,
														   .dstSet = pipelineBuilder.generalSet(),
														   .dstBinding = 0,
														   .dstArrayElement = 0,
														   .descriptorCount = 1,
														   .descriptorType =
															   VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR };
	VkDescriptorBufferInfo geometryIndexBufferInfo = { .buffer = accelerationStructureBuilder.geometryIndexBuffer(),
													   .offset = 0,
													   .range =
														   accelerationStructureBuilder.geometryIndexBufferSize() };
	VkWriteDescriptorSet geometryIndexBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
													  .pNext = &accelerationStructureWrite,
													  .dstSet = pipelineBuilder.generalSet(),
													  .dstBinding = 1,
													  .dstArrayElement = 0,
													  .descriptorCount = 1,
													  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
													  .pBufferInfo = &geometryIndexBufferInfo };
	VkDescriptorBufferInfo geometryBufferInfo = { .buffer = loader.geometryBuffer(),
												  .offset = 0,
												  .range = loader.geometries().size() * sizeof(GPUGeometry) };
	VkWriteDescriptorSet geometryBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
												 .pNext = &accelerationStructureWrite,
												 .dstSet = pipelineBuilder.generalSet(),
												 .dstBinding = 2,
												 .dstArrayElement = 0,
												 .descriptorCount = 1,
												 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
												 .pBufferInfo = &geometryBufferInfo };
	VkDescriptorBufferInfo materialBufferInfo = { .buffer = loader.materialBuffer(),
												  .offset = 0,
												  .range = loader.materialCount() * sizeof(Material) };
	VkWriteDescriptorSet materialBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
												 .pNext = &accelerationStructureWrite,
												 .dstSet = pipelineBuilder.generalSet(),
												 .dstBinding = 3,
												 .dstArrayElement = 0,
												 .descriptorCount = 1,
												 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
												 .pBufferInfo = &materialBufferInfo };
	VkDescriptorBufferInfo indexBufferInfo = { .buffer = loader.indexBuffer(),
											   .offset = 0,
											   .range = loader.indexBufferSize() };
	VkWriteDescriptorSet indexBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											  .pNext = &accelerationStructureWrite,
											  .dstSet = pipelineBuilder.generalSet(),
											  .dstBinding = 4,
											  .dstArrayElement = 0,
											  .descriptorCount = 1,
											  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											  .pBufferInfo = &indexBufferInfo };
	VkDescriptorBufferInfo normalBufferInfo = { .buffer = loader.normalBuffer(),
												.offset = 0,
												.range = loader.normalBufferSize() };
	VkWriteDescriptorSet normalBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											   .pNext = &accelerationStructureWrite,
											   .dstSet = pipelineBuilder.generalSet(),
											   .dstBinding = 5,
											   .dstArrayElement = 0,
											   .descriptorCount = 1,
											   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											   .pBufferInfo = &normalBufferInfo };
	VkDescriptorBufferInfo tangentBufferInfo = { .buffer = loader.tangentBuffer(),
												 .offset = 0,
												 .range = loader.tangentBufferSize() };
	VkWriteDescriptorSet tangentBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
												.pNext = &accelerationStructureWrite,
												.dstSet = pipelineBuilder.generalSet(),
												.dstBinding = 6,
												.dstArrayElement = 0,
												.descriptorCount = 1,
												.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
												.pBufferInfo = &tangentBufferInfo };
	VkDescriptorBufferInfo texcoordBufferInfo = { .buffer = loader.uvBuffer(),
												  .offset = 0,
												  .range = loader.uvBufferSize() };
	VkWriteDescriptorSet texcoordBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
												 .pNext = &accelerationStructureWrite,
												 .dstSet = pipelineBuilder.generalSet(),
												 .dstBinding = 7,
												 .dstArrayElement = 0,
												 .descriptorCount = 1,
												 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
												 .pBufferInfo = &texcoordBufferInfo };
	VkDescriptorBufferInfo sphereDataBufferInfo = { .buffer = accelerationStructureBuilder.lightDataBuffer(),
													.offset = 0,
													.range = accelerationStructureBuilder.lightDataBufferSize() };
	VkWriteDescriptorSet sphereDataBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
												   .pNext = &accelerationStructureWrite,
												   .dstSet = pipelineBuilder.generalSet(),
												   .dstBinding = 8,
												   .dstArrayElement = 0,
												   .descriptorCount = 1,
												   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
												   .pBufferInfo = &sphereDataBufferInfo };

	VkWriteDescriptorSet setWrites[9] = { accelerationStructureSetWrite,
										  geometryIndexBufferWrite,
										  geometryBufferWrite,
										  materialBufferWrite,
										  indexBufferWrite,
										  normalBufferWrite,
										  tangentBufferWrite,
										  texcoordBufferWrite,
										  sphereDataBufferWrite };

	size_t writeCount = accelerationStructureBuilder.lightDataBufferSize() > 0 ? 9 : 8;
	vkUpdateDescriptorSets(m_device.device(), writeCount, setWrites, 0, nullptr);

	recreateAccumulationImage();

	std::memcpy(m_worldPos, loader.camera().position, 3 * sizeof(float));
	std::memcpy(m_worldDirection, loader.camera().direction, 3 * sizeof(float));
	std::memcpy(m_worldRight, loader.camera().right, 3 * sizeof(float));

	m_worldPos[1] *= -1.0f;
	m_worldDirection[1] *= -1.0f;
	m_worldRight[1] *= -1.0f;
}

TriangleMeshRaytracer::~TriangleMeshRaytracer() {
	vkDeviceWaitIdle(m_device.device());
	vkDestroyImageView(m_device.device(), m_accumulationImageView, nullptr);
	vkDestroyImage(m_device.device(), m_accumulationImage, nullptr);
}

bool TriangleMeshRaytracer::update() {
	FrameData frameData = m_device.beginFrame();
	if (!frameData.commandBuffer) {
		return !m_device.window().shouldWindowClose();
	}
	if (frameData.windowSizeChanged) {
		recreateAccumulationImage();
		resetSampleCount();
	}

	double currentTime = glfwGetTime();
	double deltaTime = currentTime - m_lastTime;
	m_lastTime = currentTime;

	if (fabs(m_device.window().mouseMoveX()) > 0.8f || fabs(m_device.window().mouseMoveY()) > 0.8f) {
		m_cameraPhi += m_device.window().mouseMoveX() * 0.2f * deltaTime;
		m_cameraTheta -= m_device.window().mouseMoveY() * 0.2f * deltaTime;

		if (m_cameraTheta > 2 * std::numbers::pi) {
			m_cameraTheta -= 2 * std::numbers::pi;
		}
		if (m_cameraPhi > 2 * std::numbers::pi) {
			m_cameraPhi -= 2 * std::numbers::pi;
		}
		if (m_cameraTheta < -2 * std::numbers::pi) {
			m_cameraTheta += 2 * std::numbers::pi;
		}
		if (m_cameraPhi < -2 * std::numbers::pi) {
			m_cameraPhi += 2 * std::numbers::pi;
		}

		m_worldDirection[0] = cos(m_cameraTheta) * sin(m_cameraPhi);
		m_worldDirection[1] = sin(m_cameraTheta);
		m_worldDirection[2] = cos(m_cameraTheta) * cos(m_cameraPhi);

		m_worldRight[0] = sin(m_cameraPhi - std::numbers::pi * 0.5);
		m_worldRight[1] = 0.0f;
		m_worldRight[2] = cos(m_cameraPhi - std::numbers::pi * 0.5);

		resetSampleCount();
	}

	float worldUp[3];
	worldUp[0] = m_worldDirection[1] * m_worldRight[2] - m_worldDirection[2] * m_worldRight[1];
	worldUp[1] = m_worldDirection[0] * m_worldRight[2] - m_worldDirection[2] * m_worldRight[0];
	worldUp[2] = m_worldDirection[0] * m_worldRight[1] - m_worldDirection[1] * m_worldRight[0];

	if (m_device.window().keyPressed(GLFW_KEY_W)) {
		m_worldPos[0] += 2.0f * deltaTime * m_worldDirection[0];
		m_worldPos[1] += 2.0f * deltaTime * m_worldDirection[1];
		m_worldPos[2] += 2.0f * deltaTime * m_worldDirection[2];
		resetSampleCount();
	}
	if (m_device.window().keyPressed(GLFW_KEY_S)) {
		m_worldPos[0] -= 2.0f * deltaTime * m_worldDirection[0];
		m_worldPos[1] -= 2.0f * deltaTime * m_worldDirection[1];
		m_worldPos[2] -= 2.0f * deltaTime * m_worldDirection[2];
		resetSampleCount();
	}
	if (m_device.window().keyPressed(GLFW_KEY_A)) {
		m_worldPos[0] -= 2.0f * deltaTime * m_worldRight[0];
		m_worldPos[1] -= 2.0f * deltaTime * m_worldRight[1];
		m_worldPos[2] -= 2.0f * deltaTime * m_worldRight[2];
		resetSampleCount();
	}
	if (m_device.window().keyPressed(GLFW_KEY_D)) {
		m_worldPos[0] += 2.0f * deltaTime * m_worldRight[0];
		m_worldPos[1] += 2.0f * deltaTime * m_worldRight[1];
		m_worldPos[2] += 2.0f * deltaTime * m_worldRight[2];
		resetSampleCount();
	}
	if (m_device.window().keyPressed(GLFW_KEY_LEFT_SHIFT)) {
		m_worldPos[0] += 2.0f * deltaTime * worldUp[0];
		m_worldPos[1] += 2.0f * deltaTime * worldUp[1];
		m_worldPos[2] += 2.0f * deltaTime * worldUp[2];
		resetSampleCount();
	}
	if (m_device.window().keyPressed(GLFW_KEY_LEFT_CONTROL)) {
		m_worldPos[0] -= 2.0f * deltaTime * worldUp[0];
		m_worldPos[1] -= 2.0f * deltaTime * worldUp[1];
		m_worldPos[2] -= 2.0f * deltaTime * worldUp[2];
		resetSampleCount();
	}

	if (m_accumulatedSampleCount < m_maxSamples) {
		++m_accumulatedSampleCount;
		m_accumulatedSampleTime += deltaTime;
	}
	else if (m_accumulatedSampleCount != -1U) {
		printf("Max. sample count reached. Time=%f s\n", m_accumulatedSampleTime);
		m_accumulatedSampleCount = -1U;
	}

	VkDescriptorImageInfo accumulationImageInfo = { .sampler = VK_NULL_HANDLE,
													.imageView = m_accumulationImageView,
													.imageLayout = VK_IMAGE_LAYOUT_GENERAL };

	VkWriteDescriptorSet accumulationImageWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
													.dstSet = m_pipelineBuilder.imageSet(frameData.frameIndex),
													.dstBinding = 0,
													.dstArrayElement = 0,
													.descriptorCount = 1,
													.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
													.pImageInfo = &accumulationImageInfo };

	VkDescriptorImageInfo swapchainImageInfo = { .sampler = VK_NULL_HANDLE,
												 .imageView = frameData.swapchainImageView,
												 .imageLayout = VK_IMAGE_LAYOUT_GENERAL };

	VkWriteDescriptorSet swapchainImageWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
												 .dstSet = m_pipelineBuilder.imageSet(frameData.frameIndex),
												 .dstBinding = 1,
												 .dstArrayElement = 0,
												 .descriptorCount = 1,
												 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
												 .pImageInfo = &swapchainImageInfo };

	VkWriteDescriptorSet descriptorWrites[2] = { accumulationImageWrite, swapchainImageWrite };

	vkUpdateDescriptorSets(m_device.device(), 2, descriptorWrites, 0, nullptr);

	VkImageSubresourceRange imageRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
										   .baseMipLevel = 0,
										   .levelCount = 1,
										   .baseArrayLayer = 0,
										   .layerCount = 1 };

	VkImageMemoryBarrier swapchainMemoryBarrierBefore = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
														  .srcAccessMask = 0,
														  .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
														  .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
														  .newLayout = VK_IMAGE_LAYOUT_GENERAL,
														  .srcQueueFamilyIndex = m_device.queueFamilyIndex(),
														  .dstQueueFamilyIndex = m_device.queueFamilyIndex(),
														  .image = frameData.swapchainImage,
														  .subresourceRange = imageRange };
	VkImageMemoryBarrier accumulationMemoryBarrierBefore = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
															 .srcAccessMask = 0,
															 .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
															 .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
															 .newLayout = VK_IMAGE_LAYOUT_GENERAL,
															 .srcQueueFamilyIndex = m_device.queueFamilyIndex(),
															 .dstQueueFamilyIndex = m_device.queueFamilyIndex(),
															 .image = m_accumulationImage,
															 .subresourceRange = imageRange };

	VkImageMemoryBarrier imageMemoryBarriers[2] = { swapchainMemoryBarrierBefore, accumulationMemoryBarrierBefore };
	vkCmdPipelineBarrier(frameData.commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
						 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 2,
						 imageMemoryBarriers);

	vkCmdBindPipeline(frameData.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipelineBuilder.pipeline());

	PushConstantData data = { .worldOffset = { m_worldPos[0], m_worldPos[1], m_worldPos[2] },
							  .worldDirection = { m_worldDirection[0], m_worldDirection[1], m_worldDirection[2] },
							  .worldRight = { m_worldRight[0], m_worldRight[1], m_worldRight[2] },
							  .worldUp = { worldUp[0], -worldUp[1], worldUp[2] },
							  .aspectRatio = static_cast<float>(m_device.window().width()) /
											 static_cast<float>(m_device.window().height()),
							  .tanHalfFov = tanf((45.0f / 180.0f) * std::numbers::pi / 2.0f),
							  .time = static_cast<float>(fmod(glfwGetTime(), 20000.0f)),
							  .accumulatedSampleCount = m_accumulatedSampleCount };
	vkCmdPushConstants(frameData.commandBuffer, m_pipelineBuilder.pipelineLayout(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0,
					   sizeof(PushConstantData), &data);

	VkDescriptorSet sets[3] = { m_pipelineBuilder.imageSet(frameData.frameIndex), m_pipelineBuilder.generalSet(),
								m_modelLoader.textureDescriptorSet() };

	vkCmdBindDescriptorSets(frameData.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
							m_pipelineBuilder.pipelineLayout(), 0, 3, sets, 0, nullptr);

	VkStridedDeviceAddressRegionKHR nullRegion = {};
	VkStridedDeviceAddressRegionKHR raygenRegion = m_pipelineBuilder.raygenDeviceAddressRegion();
	VkStridedDeviceAddressRegionKHR missRegion = m_pipelineBuilder.missDeviceAddressRegion();
	VkStridedDeviceAddressRegionKHR hitRegion = m_pipelineBuilder.hitDeviceAddressRegion();

	vkCmdTraceRaysKHR(frameData.commandBuffer, &raygenRegion, &missRegion, &hitRegion, &nullRegion,
					  static_cast<uint32_t>(m_device.window().width()),
					  static_cast<uint32_t>(m_device.window().height()), 1);

	VkImageMemoryBarrier memoryBarrierAfter = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
												.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
												.dstAccessMask = 0,
												.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
												.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
												.srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												.dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												.image = frameData.swapchainImage,
												.subresourceRange = imageRange };

	vkCmdPipelineBarrier(frameData.commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
						 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1,
						 &memoryBarrierAfter);

	return m_device.endFrame();
}

void TriangleMeshRaytracer::recreateAccumulationImage() {
	// resize calls vkDeviceWaitIdle, so this should be safe
	vkDestroyImageView(m_device.device(), m_accumulationImageView, nullptr);
	vkDestroyImage(m_device.device(), m_accumulationImage, nullptr);
	m_allocator.freeImage(
		m_accumulationImageAllocation); // if the allocation is all zero, freeImage will not have any effect

	m_accumulationImageExtent = { .width = static_cast<uint32_t>(m_device.window().width()),
								  .height = static_cast<uint32_t>(m_device.window().height()),
								  .depth = 1 };

	// Create target image for value accumulation
	VkImageCreateInfo imageCreateInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
										  .imageType = VK_IMAGE_TYPE_2D,
										  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
										  .extent = m_accumulationImageExtent,
										  .mipLevels = 1,
										  .arrayLayers = 1,
										  .samples = VK_SAMPLE_COUNT_1_BIT,
										  .tiling = VK_IMAGE_TILING_OPTIMAL,
										  .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
										  .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
										  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
	verifyResult(vkCreateImage(m_device.device(), &imageCreateInfo, nullptr, &m_accumulationImage));
	m_allocator.bindDeviceImage(m_accumulationImage, 0);

	VkImageViewCreateInfo imageViewCreateInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
												  .image = m_accumulationImage,
												  .viewType = VK_IMAGE_VIEW_TYPE_2D,
												  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
												  .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
																		.baseMipLevel = 0,
																		.levelCount = 1,
																		.baseArrayLayer = 0,
																		.layerCount = 1 } };
	verifyResult(vkCreateImageView(m_device.device(), &imageViewCreateInfo, nullptr, &m_accumulationImageView));
}

void TriangleMeshRaytracer::resetSampleCount() {
	m_accumulatedSampleCount = 0;
	m_accumulatedSampleTime = 0.0;
}