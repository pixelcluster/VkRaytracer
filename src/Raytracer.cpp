#include <DebugHelper.hpp>
#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <ShaderModuleHelper.hpp>
#include <cstring>
#include <numbers>
#include <cmath>
#include <volk.h>

HardwareSphereRaytracer::HardwareSphereRaytracer(size_t windowWidth, size_t windowHeight, size_t sphereCount)
	: m_device(windowWidth, windowHeight, true) {
	size_t objectCount = sphereCount + m_triangleObjectCount;

	// Create ray tracing pipeline

	VkDescriptorSetLayoutBinding bindings[4] = { { .binding = 0,
												   .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
												   .descriptorCount = 1,
												   .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
																 VK_SHADER_STAGE_RAYGEN_BIT_KHR },
												 { .binding = 1,
												   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
												   .descriptorCount = 1,
												   .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
												 { .binding = 2,
												   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
												   .descriptorCount = 1,
												   .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
												 { .binding = 3,
												   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
												   .descriptorCount = 1,
												   .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR } };

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 4, .pBindings = bindings
	};

	verifyResult(vkCreateDescriptorSetLayout(m_device.device(), &descriptorSetLayoutCreateInfo, nullptr,
											 &m_descriptorSetLayout));

	VkPushConstantRange constantRange = { .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
										  .offset = 0,
										  .size = sizeof(float) * 5 };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
															.setLayoutCount = 1,
															.pSetLayouts = &m_descriptorSetLayout,
															.pushConstantRangeCount = 1,
															.pPushConstantRanges = &constantRange };

	verifyResult(vkCreatePipelineLayout(m_device.device(), &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));

	VkShaderModule raygenShaderModule = createShaderModule(m_device.device(), "./shaders/raytrace-rgen.spv");
	VkShaderModule hitShaderModule = createShaderModule(m_device.device(), "./shaders/sphere-rchit.spv");
	VkShaderModule triangleHitShaderModule = createShaderModule(m_device.device(), "./shaders/triangle-rchit.spv");
	VkShaderModule missShaderModule = createShaderModule(m_device.device(), "./shaders/raytrace-rmiss.spv");
	VkShaderModule intersectionShaderModule = createShaderModule(m_device.device(), "./shaders/raytrace-rint.spv");

	VkRayTracingShaderGroupCreateInfoKHR shaderGroupCreateInfos[4] = {
		{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		  .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
		  .generalShader = 0,
		  .closestHitShader = VK_SHADER_UNUSED_KHR,
		  .anyHitShader = VK_SHADER_UNUSED_KHR,
		  .intersectionShader = VK_SHADER_UNUSED_KHR },
		{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		  .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
		  .generalShader = VK_SHADER_UNUSED_KHR,
		  .closestHitShader = 1,
		  .anyHitShader = VK_SHADER_UNUSED_KHR,
		  .intersectionShader = 3 },
		{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		  .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
		  .generalShader = VK_SHADER_UNUSED_KHR,
		  .closestHitShader = 4,
		  .anyHitShader = VK_SHADER_UNUSED_KHR,
		  .intersectionShader = VK_SHADER_UNUSED_KHR },
		{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		  .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
		  .generalShader = 2,
		  .closestHitShader = VK_SHADER_UNUSED_KHR,
		  .anyHitShader = VK_SHADER_UNUSED_KHR,
		  .intersectionShader = VK_SHADER_UNUSED_KHR }
	};

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[5] = {
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		  .module = raygenShaderModule,
		  .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		  .module = hitShaderModule,
		  .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
		  .module = missShaderModule,
		  .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
		  .module = intersectionShaderModule,
		  .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		  .module = triangleHitShaderModule,
		  .pName = "main" }
	};

	VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = { .sType =
																 VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
															 .stageCount = 5,
															 .pStages = shaderStageCreateInfos,
															 .groupCount = 4,
															 .pGroups = shaderGroupCreateInfos,
															 .maxPipelineRayRecursionDepth = 8,
															 .layout = m_pipelineLayout,
															 .basePipelineIndex = -1 };
	verifyResult(vkCreateRayTracingPipelinesKHR(m_device.device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1,
												&pipelineCreateInfo, nullptr, &m_pipeline));

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR pipelineProperties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
	};
	VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
												.pNext = &pipelineProperties };
	vkGetPhysicalDeviceProperties2(m_device.physicalDevice(), &properties2);

	size_t shaderGroupHandleSize = pipelineProperties.shaderGroupHandleSize;
	size_t shaderGroupHandleSizeAlignmentRemainder =
		pipelineProperties.shaderGroupHandleAlignment
			? shaderGroupHandleSize % pipelineProperties.shaderGroupBaseAlignment
			: 0;

	size_t shaderGroupHandleSizeAligned = pipelineProperties.shaderGroupHandleSize;

	if (shaderGroupHandleSizeAlignmentRemainder) {
		shaderGroupHandleSizeAligned +=
			pipelineProperties.shaderGroupBaseAlignment - shaderGroupHandleSizeAlignmentRemainder;
	}

	// Create descriptor pools and sets

	VkDescriptorPoolSize poolSizes[3] = {
		{ .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = frameInFlightCount },
		{ .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = frameInFlightCount },
		{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = frameInFlightCount }
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
															.maxSets = frameInFlightCount,
															.poolSizeCount = 3,
															.pPoolSizes = poolSizes };

	verifyResult(vkCreateDescriptorPool(m_device.device(), &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));

	VkDescriptorSetLayout layouts[frameInFlightCount];
	for (size_t i = 0; i < frameInFlightCount; ++i)
		layouts[i] = m_descriptorSetLayout;

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
															  .descriptorPool = m_descriptorPool,
															  .descriptorSetCount = frameInFlightCount,
															  .pSetLayouts = layouts };

	verifyResult(vkAllocateDescriptorSets(m_device.device(), &descriptorSetAllocateInfo, m_descriptorSets));

	VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
											  .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											  .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											  .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											  .unnormalizedCoordinates = VK_TRUE };
	verifyResult(vkCreateSampler(m_device.device(), &samplerCreateInfo, nullptr, &m_imageSampler));

	// Determine size and alignment of staging/data buffers per frame in flight (geometry data gets appended at end)

	size_t triangleDataSize = m_vertexDataSize + m_indexDataSize + m_transformDataSize;
	size_t triangleDataOffset = sizeof(VkAabbPositionsKHR);

	m_stagingFrameDataSize = (sizeof(VkAccelerationStructureInstanceKHR) + sizeof(float) * 4) * objectCount;
	m_accelerationStructureFrameDataSize = sizeof(VkAccelerationStructureInstanceKHR) * objectCount;
	m_objectFrameDataSize = sizeof(float) * 4 * objectCount;

	VkDeviceSize structureSizeRemainder = m_accelerationStructureFrameDataSize % 16;
	if (structureSizeRemainder) {
		m_accelerationStructureFrameDataSize += 16 - structureSizeRemainder;
	}

	// Create acceleration structures

	std::vector<AccelerationStructureInitInfo> blasInitInfos = {
		{ .geometries = { { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
							.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR,
							.geometry = { .aabbs = { VkAccelerationStructureGeometryAabbsDataKHR{
											  .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
											  .stride = sizeof(VkAabbPositionsKHR) } } } } },
		  .maxPrimitiveCount = 1,
		  .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR },
		{ .geometries = { { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
							.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
							.geometry = { .triangles = { VkAccelerationStructureGeometryTrianglesDataKHR{
											  .sType =
												  VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
											  .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
											  .vertexStride = 3 * sizeof(float),
											  .maxVertex = 3,
											  .indexType = VK_INDEX_TYPE_UINT16 } } } } },
		  .maxPrimitiveCount = 2,
		  .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR }
	};

	m_blasStructureData = AccelerationStructureManager::createData(m_device, blasInitInfos);

	AccelerationStructureInitInfo tlasInitInfo = {
		.geometries = { { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
						  .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
						  .geometry = { .instances = { VkAccelerationStructureGeometryInstancesDataKHR{
											.sType =
												VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
											.arrayOfPointers = VK_FALSE } } } } },
		.maxPrimitiveCount = static_cast<uint32_t>(objectCount),
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
	};

	for (size_t i = 0; i < frameInFlightCount; ++i) {
		m_tlasStructureData[i] = AccelerationStructureManager::createData(m_device, { tlasInitInfo });
		setGeometryTLASBatchNames(i);
	}

	// Allocate staging buffer

	// Staging buffer layout:
	// --------per frame in flight-------
	// instance transform matrices
	// sphere colors
	// ---------------------------------
	// sizeof(VkAabbPositions): AABB position storage
	// sizeof(float) * 3 * m_triangleUniqueVertexCount: Vertex position storage
	// sizeof(uint16_t) * m_triangleUniqueIndexCount: Index storage
	// sizeof(VkTransformMatrixKHR) * m_triangleTransformCount: Triangle mesh transform data
	// shaderGroupHandleSizeAligned * 4: Shader Binding Table

	BufferAllocationBatch stagingBatch = allocateBatch(
		m_device,
		{ { .size = m_stagingFrameDataSize * frameInFlightCount + shaderGroupHandleSize * 6 +
					sizeof(VkAabbPositionsKHR) + triangleDataSize,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			.requiredProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT } },
		0);
	m_stagingBuffer = stagingBatch.buffers[0];
	if (!m_stagingBuffer.dedicatedMemory) {
		m_stagingBuffer.dedicatedMemory = stagingBatch.sharedMemory;
	}
	verifyResult(vkMapMemory(m_device.device(), m_stagingBuffer.dedicatedMemory, 0,
							 m_stagingFrameDataSize * frameInFlightCount + shaderGroupHandleSize * 4 +
								 sizeof(VkAabbPositionsKHR) + triangleDataSize,
							 0, &m_mappedStagingBuffer));

	if (enableDebugUtils) {
		setObjectName(m_device.device(), VK_OBJECT_TYPE_DEVICE_MEMORY,
					  reinterpret_cast<uint64_t>(m_stagingBuffer.dedicatedMemory), "Staging buffer memory");
	}

	// Allocate device buffers
	BufferAllocationBatch deviceBatch =
		allocateBatch(m_device,
					  {
						  // Acceleration structure data buffer
						  { .size = m_accelerationStructureFrameDataSize * frameInFlightCount +
									sizeof(VkAabbPositionsKHR) + triangleDataSize,
							.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
									 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
									 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							.preferredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT },
						  // Object data (color, and normal data) buffer
						  { .size = m_objectFrameDataSize * frameInFlightCount + m_normalDataSize,
							.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
							.preferredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT },
						  // Shader binding table
						  { .size = shaderGroupHandleSizeAligned * 6,
							.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
									 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							.preferredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT },
					  },
					  VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
	m_deviceMemory = deviceBatch.sharedMemory;
	m_accelerationStructureDataBuffer = deviceBatch.buffers[0];
	m_objectDataBuffer = deviceBatch.buffers[1];
	m_shaderBindingTableBuffer = deviceBatch.buffers[2];

	if (enableDebugUtils) {
		setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(m_stagingBuffer.buffer),
					  "Sphere/Acceleration structure data staging buffer");

		setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER,
					  reinterpret_cast<uint64_t>(m_accelerationStructureDataBuffer.buffer),
					  "Acceleration structure buffer");
		setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(m_objectDataBuffer.buffer),
					  "Object data buffer");
		setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER,
					  reinterpret_cast<uint64_t>(m_shaderBindingTableBuffer.buffer), "Shader binding table buffer");
	}

	VkBufferDeviceAddressInfo deviceAddressInfo;
	deviceAddressInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
						  .buffer = m_accelerationStructureDataBuffer.buffer };

	m_accelerationStructureDataDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &deviceAddressInfo);

	deviceAddressInfo.buffer = m_shaderBindingTableBuffer.buffer;

	VkDeviceAddress shaderBindingTableDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &deviceAddressInfo);
	m_raygenShaderBindingTable = { .deviceAddress = shaderBindingTableDeviceAddress,
								   .stride = shaderGroupHandleSizeAligned,
								   .size = shaderGroupHandleSizeAligned };
	m_sphereHitShaderBindingTable = { .deviceAddress = shaderBindingTableDeviceAddress + shaderGroupHandleSizeAligned * 2,
									  .stride = shaderGroupHandleSizeAligned,
									  .size = shaderGroupHandleSizeAligned };
	m_triangleHitShaderBindingTable = { .deviceAddress =
											shaderBindingTableDeviceAddress + shaderGroupHandleSizeAligned * 3,
										.stride = shaderGroupHandleSizeAligned,
										.size = shaderGroupHandleSizeAligned };
	m_missShaderBindingTable = { .deviceAddress = shaderBindingTableDeviceAddress + shaderGroupHandleSizeAligned * 4,
								 .stride = shaderGroupHandleSizeAligned,
								 .size = shaderGroupHandleSizeAligned };

	// Write identity AABBs
	void* stagingBufferPositionsPointer = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) +
																  m_stagingFrameDataSize * frameInFlightCount);
	new (stagingBufferPositionsPointer)
		VkAabbPositionsKHR{ .minX = -0.5f, .minY = -0.5, .minZ = -0.5, .maxX = 0.5f, .maxY = 0.5f, .maxZ = 0.5f };

	void* triangleVertexDataPointer = reinterpret_cast<void*>(
		reinterpret_cast<uintptr_t>(stagingBufferPositionsPointer) + sizeof(VkAabbPositionsKHR));
	// clang-format off
	// vertices
	float vertices[4][3] = {
		{ 2500.0f, 0.0f,  2500.0f},
		{ 2500.0f, 0.0f,  -5.0f},
		{ -5.0f,   0.0f,  2500.0f},
		{ -5.0f,   0.0f,  -5.0f}
	};
	std::memcpy(triangleVertexDataPointer, vertices, m_vertexDataSize);
	// clang-format on

	void* triangleIndexDataPointer =
		reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(triangleVertexDataPointer) + m_vertexDataSize);
	uint16_t indices[6] = { 3, 2, 0, 1, 3, 0 };
	std::memcpy(triangleIndexDataPointer, indices, m_indexDataSize);

	void* triangleNormalsDataPointer =
		reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(triangleIndexDataPointer) + m_indexDataSize);
	float normals[2][4] = { { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } };
	std::memcpy(triangleNormalsDataPointer, normals, m_normalDataSize);

	void* shaderBindingTablePointer =
		reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(triangleVertexDataPointer) + triangleDataSize + m_normalDataSize);
	vkGetRayTracingShaderGroupHandlesKHR(m_device.device(), m_pipeline, 0, 4, shaderGroupHandleSize * 4,
										 shaderBindingTablePointer);

	vkDestroyShaderModule(m_device.device(), raygenShaderModule, nullptr);
	vkDestroyShaderModule(m_device.device(), hitShaderModule, nullptr);
	vkDestroyShaderModule(m_device.device(), triangleHitShaderModule, nullptr);
	vkDestroyShaderModule(m_device.device(), missShaderModule, nullptr);
	vkDestroyShaderModule(m_device.device(), intersectionShaderModule, nullptr);

	for (size_t i = 0; i < frameInFlightCount; ++i) {
		VkDescriptorBufferInfo bufferInfo = { .buffer = m_objectDataBuffer.buffer,
											  .offset = m_objectFrameDataSize * frameInFlightCount,
											  .range = m_normalDataSize };
		VkWriteDescriptorSet bufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											 .dstSet = m_descriptorSets[i],
											 .dstBinding = 3,
											 .dstArrayElement = 0,
											 .descriptorCount = 1,
											 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											 .pBufferInfo = &bufferInfo };

		vkUpdateDescriptorSets(m_device.device(), 1, &bufferWrite, 0, nullptr);
	}

	// Build acceleration structures
	VkCommandPoolCreateInfo poolCreateInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
											   .queueFamilyIndex = m_device.queueFamilyIndex() };
	verifyResult(vkCreateCommandPool(m_device.device(), &poolCreateInfo, nullptr, &m_oneTimeSubmitPool));

	VkFenceCreateInfo fenceCreateInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	verifyResult(vkCreateFence(m_device.device(), &fenceCreateInfo, nullptr, &m_oneTimeSubmitFence));

	VkCommandBuffer submitCommandBuffer;
	VkCommandBufferAllocateInfo allocateInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
												 .commandPool = m_oneTimeSubmitPool,
												 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												 .commandBufferCount = 1 };
	verifyResult(vkAllocateCommandBuffers(m_device.device(), &allocateInfo, &submitCommandBuffer));

	std::vector<VkAccelerationStructureBuildRangeInfoKHR> blasRangeInfos =
		std::vector<VkAccelerationStructureBuildRangeInfoKHR>(1 + m_triangleObjectCount);
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> ptrBLASRangeInfo =
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*>(1 + m_triangleObjectCount);
	blasRangeInfos[0] = { .primitiveCount = 1 };
	blasRangeInfos[1] = { .primitiveCount = 2 };
	for (size_t i = 0; i < 1 + m_triangleObjectCount; ++i) {
		ptrBLASRangeInfo[i] = &blasRangeInfos[i];
	}

	VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo = { .primitiveCount = static_cast<uint32_t>(objectCount) };
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> ptrTLASRangeInfo =
		std::vector<VkAccelerationStructureBuildRangeInfoKHR*>(frameInFlightCount, &tlasRangeInfo);

	std::vector<VkAccelerationStructureGeometryKHR> blasGeometries =
		std::vector<VkAccelerationStructureGeometryKHR>(1 + m_triangleObjectCount);
	std::vector<VkAccelerationStructureGeometryKHR> tlasGeometries =
		std::vector<VkAccelerationStructureGeometryKHR>(frameInFlightCount);
	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blasBuildInfos;
	blasBuildInfos.reserve(1 + m_triangleObjectCount);
	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> tlasBuildInfos;
	tlasBuildInfos.reserve(frameInFlightCount);
	std::vector<VkBufferMemoryBarrier> blasBuildBarriers;
	std::vector<VkBufferMemoryBarrier> tlasBuildBarriers;
	blasBuildBarriers.reserve(2 + m_triangleObjectCount);
	tlasBuildBarriers.reserve(frameInFlightCount + 1); // include shader binding table barrier

	tlasBuildBarriers.push_back({ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
								  .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
								  .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
								  .srcQueueFamilyIndex = m_device.queueFamilyIndex(),
								  .dstQueueFamilyIndex = m_device.queueFamilyIndex(),
								  .buffer = m_shaderBindingTableBuffer.buffer,
								  .offset = 0,
								  .size = shaderGroupHandleSizeAligned * 3 });

	std::vector<VkBufferCopy> bufferCopyRegions;
	bufferCopyRegions.reserve(frameInFlightCount + 1);

	blasBuildInfos.push_back(
		constructBLASGeometryInfo(VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, blasGeometries[m_sphereBLASIndex]));
	blasBuildInfos.push_back(constructPlaneBLASGeometryInfo(VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
															blasGeometries[m_planeBLASIndex]));

	for (size_t i = 0; i < 1 + m_triangleObjectCount; ++i) {
		blasBuildBarriers.push_back({ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
									  .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
									  .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
									  .srcQueueFamilyIndex = m_device.queueFamilyIndex(),
									  .dstQueueFamilyIndex = m_device.queueFamilyIndex(),
									  .buffer = m_blasStructureData.structureBuffer.buffer,
									  .offset = m_blasStructureData.structures[i].structureBufferOffset,
									  .size = m_blasStructureData.structures[i].accelerationStructureSize });
	}

	bufferCopyRegions.push_back({ .srcOffset = m_stagingFrameDataSize * frameInFlightCount,
								  .dstOffset = m_accelerationStructureFrameDataSize * frameInFlightCount,
								  .size = sizeof(VkAabbPositionsKHR) + triangleDataSize });

	for (size_t i = 0; i < frameInFlightCount; ++i) {
		void* mappedFrameSection =
			reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) + m_stagingFrameDataSize * i);
		std::vector<VkAccelerationStructureInstanceKHR> instances =
			std::vector<VkAccelerationStructureInstanceKHR>(objectCount);
		for (size_t j = 0; j < sphereCount; ++j) {
			instances[j] = { .transform = { .matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
														{ 0.0f, 1.0f, 0.0f, 0.0f },
														{ 0.0f, 0.0f, 1.0f, 0.0f } } },
							 .instanceCustomIndex = static_cast<uint32_t>(j),
							 .mask = 0xFFFFFFFF,
							 .accelerationStructureReference =
								 m_blasStructureData.structures[m_sphereBLASIndex].deviceAddress };
		}
		for (size_t j = 0; j < m_triangleObjectCount; ++j) {
			instances[sphereCount + j] = { .transform = { .matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
																	  { 0.0f, 1.0f, 0.0f, 0.0f },
																	  { 0.0f, 0.0f, 1.0f, 0.0f } } },
										   .instanceCustomIndex = static_cast<uint32_t>(j),
										   .mask = 0xFFFFFFFF,
										   .instanceShaderBindingTableRecordOffset = 1,
										   .accelerationStructureReference =
											   m_blasStructureData.structures[m_planeBLASIndex].deviceAddress };
		}
		std::memcpy(mappedFrameSection, instances.data(),
					instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

		bufferCopyRegions.push_back({ .srcOffset = m_stagingFrameDataSize * i,
									  .dstOffset = m_accelerationStructureFrameDataSize * i,
									  .size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR) });

		tlasBuildInfos.push_back(
			constructTLASGeometryInfo(i, VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, tlasGeometries[i]));
		tlasBuildBarriers.push_back({ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
									  .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
									  .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
									  .srcQueueFamilyIndex = m_device.queueFamilyIndex(),
									  .dstQueueFamilyIndex = m_device.queueFamilyIndex(),
									  .buffer = m_tlasStructureData[i].structureBuffer.buffer,
									  .offset = m_tlasStructureData[i].structures[0].structureBufferOffset,
									  .size = m_tlasStructureData[i].structures[0].accelerationStructureSize });
	}

	VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
										   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

	vkBeginCommandBuffer(submitCommandBuffer, &beginInfo);

	vkCmdCopyBuffer(submitCommandBuffer, m_stagingBuffer.buffer, m_accelerationStructureDataBuffer.buffer,
					static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

	VkBufferCopy shaderBindingTableCopies[6] = {
		{ .srcOffset = m_stagingFrameDataSize * frameInFlightCount + sizeof(VkAabbPositionsKHR) + triangleDataSize + m_normalDataSize,
		  .dstOffset = 0,
		  .size = shaderGroupHandleSize },
		{ .srcOffset = m_stagingFrameDataSize * frameInFlightCount + sizeof(VkAabbPositionsKHR) + triangleDataSize + m_normalDataSize,
		  .dstOffset = shaderGroupHandleSizeAligned,
		  .size = shaderGroupHandleSize },

		{ .srcOffset = m_stagingFrameDataSize * frameInFlightCount + sizeof(VkAabbPositionsKHR) + triangleDataSize + m_normalDataSize +
					   shaderGroupHandleSize,
		  .dstOffset = shaderGroupHandleSizeAligned * 2,
		  .size = shaderGroupHandleSize },
		{ .srcOffset = m_stagingFrameDataSize * frameInFlightCount + sizeof(VkAabbPositionsKHR) + triangleDataSize + m_normalDataSize +
					   shaderGroupHandleSize * 2,
		  .dstOffset = shaderGroupHandleSizeAligned * 3,
		  .size = shaderGroupHandleSize },

		{ .srcOffset = m_stagingFrameDataSize * frameInFlightCount + sizeof(VkAabbPositionsKHR) + triangleDataSize + m_normalDataSize +
					   shaderGroupHandleSize * 3,
		  .dstOffset = shaderGroupHandleSizeAligned * 4,
		  .size = shaderGroupHandleSize },
		{ .srcOffset = m_stagingFrameDataSize * frameInFlightCount + sizeof(VkAabbPositionsKHR) + triangleDataSize + m_normalDataSize +
					   shaderGroupHandleSize * 3,
		  .dstOffset = shaderGroupHandleSizeAligned * 5,
		  .size = shaderGroupHandleSize }
	};
	vkCmdCopyBuffer(submitCommandBuffer, m_stagingBuffer.buffer, m_shaderBindingTableBuffer.buffer, 6,
					shaderBindingTableCopies);

	VkBufferCopy normalDataCopy = { .srcOffset = m_stagingFrameDataSize * frameInFlightCount +
												 sizeof(VkAabbPositionsKHR) + m_vertexDataSize +
												 m_indexDataSize,
									.dstOffset = m_objectFrameDataSize * frameInFlightCount,
									.size = m_normalDataSize };
	vkCmdCopyBuffer(submitCommandBuffer, m_stagingBuffer.buffer, m_objectDataBuffer.buffer, 1, &normalDataCopy);

	VkBufferMemoryBarrier copyMemoryBarrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
												.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
												.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
												.srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												.dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												.buffer = m_accelerationStructureDataBuffer.buffer,
												.offset = 0,
												.size = m_accelerationStructureFrameDataSize * frameInFlightCount };

	vkCmdPipelineBarrier(submitCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 1, &copyMemoryBarrier,
						 0, nullptr);

	//vkCmdSetCheckpointNV(submitCommandBuffer, reinterpret_cast<void*>(6));

	vkCmdBuildAccelerationStructuresKHR(submitCommandBuffer, static_cast<uint32_t>(blasBuildInfos.size()),
										blasBuildInfos.data(), ptrBLASRangeInfo.data());

	vkCmdPipelineBarrier(submitCommandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr,
						 static_cast<uint32_t>(blasBuildBarriers.size()), blasBuildBarriers.data(), 0, nullptr);

	vkCmdBuildAccelerationStructuresKHR(submitCommandBuffer, static_cast<uint32_t>(tlasBuildInfos.size()),
										tlasBuildInfos.data(), ptrTLASRangeInfo.data());

	vkCmdPipelineBarrier(submitCommandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr,
						 static_cast<uint32_t>(tlasBuildBarriers.size()), tlasBuildBarriers.data(), 0, nullptr);

	//vkCmdSetCheckpointNV(submitCommandBuffer, reinterpret_cast<void*>(7));

	VkBufferMemoryBarrier normalCopyBarrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
												.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
												.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
												.srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												.dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												.buffer = m_objectDataBuffer.buffer,
												.offset = m_objectFrameDataSize * frameInFlightCount,
												.size = m_normalDataSize };
	vkCmdPipelineBarrier(submitCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 1, &normalCopyBarrier, 0,
						 nullptr);

	//vkCmdSetCheckpointNV(submitCommandBuffer, reinterpret_cast<void*>(8));

	verifyResult(vkEndCommandBuffer(submitCommandBuffer));

	VkSubmitInfo info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
						  .commandBufferCount = 1,
						  .pCommandBuffers = &submitCommandBuffer };
	vkQueueSubmit(m_device.queue(), 1, &info, m_oneTimeSubmitFence);
}

HardwareSphereRaytracer::~HardwareSphereRaytracer() {
	m_device.waitAllFences();
	// One-time submit data
	vkWaitForFences(m_device.device(), 1, &m_oneTimeSubmitFence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(m_device.device(), m_oneTimeSubmitFence, nullptr);
	vkDestroyCommandPool(m_device.device(), m_oneTimeSubmitPool, nullptr);

	// TLASes
	for (size_t i = 0; i < frameInFlightCount; ++i) {
		for (auto& data : m_tlasStructureData[i].structures) {
			vkDestroyAccelerationStructureKHR(m_device.device(), data.structure, nullptr);
		}

		vkDestroyBuffer(m_device.device(), m_tlasStructureData[i].scratchBuffer.buffer, nullptr);
		vkFreeMemory(m_device.device(), m_tlasStructureData[i].scratchBuffer.dedicatedMemory, nullptr);
		vkDestroyBuffer(m_device.device(), m_tlasStructureData[i].structureBuffer.buffer, nullptr);
		vkFreeMemory(m_device.device(), m_tlasStructureData[i].structureBuffer.dedicatedMemory, nullptr);

		vkFreeMemory(m_device.device(), m_tlasStructureData[i].sharedStructureMemory, nullptr);
	}
	// BLASes
	for (auto& data : m_blasStructureData.structures) {
		vkDestroyAccelerationStructureKHR(m_device.device(), data.structure, nullptr);
	}

	vkDestroyBuffer(m_device.device(), m_blasStructureData.scratchBuffer.buffer, nullptr);
	vkFreeMemory(m_device.device(), m_blasStructureData.scratchBuffer.dedicatedMemory, nullptr);
	vkDestroyBuffer(m_device.device(), m_blasStructureData.structureBuffer.buffer, nullptr);
	vkFreeMemory(m_device.device(), m_blasStructureData.structureBuffer.dedicatedMemory, nullptr);

	vkFreeMemory(m_device.device(), m_blasStructureData.sharedStructureMemory, nullptr);

	// Descriptor data
	vkDestroyDescriptorPool(m_device.device(), m_descriptorPool, nullptr);
	vkDestroySampler(m_device.device(), m_imageSampler, nullptr);

	// Other buffers
	vkDestroyBuffer(m_device.device(), m_accelerationStructureDataBuffer.buffer, nullptr);
	vkFreeMemory(m_device.device(), m_accelerationStructureDataBuffer.dedicatedMemory, nullptr);
	vkDestroyBuffer(m_device.device(), m_objectDataBuffer.buffer, nullptr);
	vkFreeMemory(m_device.device(), m_objectDataBuffer.dedicatedMemory, nullptr);
	vkDestroyBuffer(m_device.device(), m_shaderBindingTableBuffer.buffer, nullptr);
	vkFreeMemory(m_device.device(), m_shaderBindingTableBuffer.dedicatedMemory, nullptr);
	vkDestroyBuffer(m_device.device(), m_stagingBuffer.buffer, nullptr);
	vkFreeMemory(m_device.device(), m_stagingBuffer.dedicatedMemory, nullptr);

	vkFreeMemory(m_device.device(), m_deviceMemory, nullptr);

	// Pipeline
	vkDestroyPipeline(m_device.device(), m_pipeline, nullptr);
	vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device.device(), m_descriptorSetLayout, nullptr);
}

bool HardwareSphereRaytracer::update(const std::vector<Sphere>& spheres) {
	FrameData frameData = m_device.beginFrame();
	if (!frameData.commandBuffer) {
		return !m_device.window().shouldWindowClose();
	}

	double currentTime = glfwGetTime();
	double deltaTime = currentTime - m_lastTime;
	m_lastTime = currentTime;

	if (m_device.window().keyPressed(GLFW_KEY_W)) {
		m_worldPos[2] += 2.0f * deltaTime;
	}
	if (m_device.window().keyPressed(GLFW_KEY_S)) {
		m_worldPos[2] -= 2.0f * deltaTime;
	}
	if (m_device.window().keyPressed(GLFW_KEY_A)) {
		m_worldPos[0] -= 2.0f * deltaTime;
	}
	if (m_device.window().keyPressed(GLFW_KEY_D)) {
		m_worldPos[0] += 2.0f * deltaTime;
	}
	if (m_device.window().keyPressed(GLFW_KEY_LEFT_SHIFT)) {
		m_worldPos[1] += 2.0f * deltaTime;
	}
	if (m_device.window().keyPressed(GLFW_KEY_LEFT_CONTROL)) {
		m_worldPos[1] -= 2.0f * deltaTime;
	}

	// Write descriptor sets

	VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteData = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
		.accelerationStructureCount = 1,
		.pAccelerationStructures = &m_tlasStructureData[frameData.frameIndex].structures[0].structure
	};

	VkWriteDescriptorSet accelerationStructureWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
														.pNext = &accelerationStructureWriteData,
														.dstSet = m_descriptorSets[frameData.frameIndex],
														.dstBinding = 0,
														.dstArrayElement = 0,
														.descriptorCount = 1,
														.descriptorType =
															VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR };

	VkDescriptorImageInfo imageInfo = { .sampler = m_imageSampler,
										.imageView = frameData.swapchainImageView,
										.imageLayout = VK_IMAGE_LAYOUT_GENERAL };

	VkWriteDescriptorSet imageWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
										.dstSet = m_descriptorSets[frameData.frameIndex],
										.dstBinding = 1,
										.dstArrayElement = 0,
										.descriptorCount = 1,
										.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
										.pImageInfo = &imageInfo };

	VkDescriptorBufferInfo bufferInfo = { .buffer = m_objectDataBuffer.buffer,
										  .offset = m_objectFrameDataSize * frameData.frameIndex,
										  .range = m_objectFrameDataSize };
	VkWriteDescriptorSet bufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
										 .dstSet = m_descriptorSets[frameData.frameIndex],
										 .dstBinding = 2,
										 .dstArrayElement = 0,
										 .descriptorCount = 1,
										 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
										 .pBufferInfo = &bufferInfo };

	VkWriteDescriptorSet writes[] = { accelerationStructureWrite, imageWrite, bufferWrite };

	vkUpdateDescriptorSets(m_device.device(), 3, writes, 0, nullptr);

	//vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(0));

	// Write updated data

	void* mappedFrameSection = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) +
													   m_stagingFrameDataSize * frameData.frameIndex);
	void* sphereDataSection = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(mappedFrameSection) +
													  (spheres.size() + m_triangleObjectCount) * sizeof(VkAccelerationStructureInstanceKHR));
	std::vector<VkAccelerationStructureInstanceKHR> instances =
		std::vector<VkAccelerationStructureInstanceKHR>(spheres.size() + m_triangleObjectCount);
	std::vector<float> objectColors = std::vector<float>((spheres.size() + m_triangleObjectCount) * 4);
	for (size_t i = 0; i < spheres.size(); ++i) {
		instances[i] = { .transform = { .matrix = { { 2 * spheres[i].radius, 0.0f, 0.0f, spheres[i].position[0] },
													{ 0.0f, 2 * spheres[i].radius, 0.0f, spheres[i].position[1] },
													{ 0.0f, 0.0f, 2 * spheres[i].radius, spheres[i].position[2] } } },
						 .instanceCustomIndex = static_cast<uint32_t>(i),
						 .mask = 0xFFFFFFFF,
						 .accelerationStructureReference =
							 m_blasStructureData.structures[m_sphereBLASIndex].deviceAddress };
		objectColors[i * 4 + 0] = spheres[i].color[0];
		objectColors[i * 4 + 1] = spheres[i].color[1];
		objectColors[i * 4 + 2] = spheres[i].color[2];
		objectColors[i * 4 + 3] = spheres[i].color[3];
	}
	for (size_t i = 0; i < m_triangleObjectCount; ++i) {
		instances[spheres.size() + i] = { .transform = { .matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
																	 { 0.0f, 1.0f, 0.0f, 0.0f },
																	 { 0.0f, 0.0f, 1.0f, 0.0f } } },
										  .instanceCustomIndex = static_cast<uint32_t>(i),
										  .mask = 0xFFFFFFFF,
										  .instanceShaderBindingTableRecordOffset = 1,
										  .accelerationStructureReference =
											  m_blasStructureData.structures[m_planeBLASIndex].deviceAddress };
		objectColors[(spheres.size() + i) * 4 + 0] = 1.0f;
		objectColors[(spheres.size() + i) * 4 + 1] = 1.0f;
		objectColors[(spheres.size() + i) * 4 + 2] = 1.0f;
		objectColors[(spheres.size() + i) * 4 + 3] = 0.0f;
	}
	std::memcpy(mappedFrameSection, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
	std::memcpy(sphereDataSection, objectColors.data(), objectColors.size() * sizeof(float));

	VkBufferCopy instanceCopyRegion = { .srcOffset = m_stagingFrameDataSize * frameData.frameIndex,
										.dstOffset = m_accelerationStructureFrameDataSize * frameData.frameIndex,
										.size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR) };

	vkCmdCopyBuffer(frameData.commandBuffer, m_stagingBuffer.buffer, m_accelerationStructureDataBuffer.buffer, 1,
					&instanceCopyRegion);

	VkBufferCopy sphereCopyRegion = { .srcOffset = m_stagingFrameDataSize * frameData.frameIndex +
												   instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
									  .dstOffset = m_objectFrameDataSize * frameData.frameIndex,
									  .size = objectColors.size() * sizeof(float) };

	vkCmdCopyBuffer(frameData.commandBuffer, m_stagingBuffer.buffer, m_objectDataBuffer.buffer, 1, &sphereCopyRegion);

	//vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(1));

	VkBufferMemoryBarrier copyMemoryBarrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
												.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
												.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
												.srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												.dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												.buffer = m_accelerationStructureDataBuffer.buffer,
												.offset = m_accelerationStructureFrameDataSize * frameData.frameIndex,
												.size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR) };

	vkCmdPipelineBarrier(frameData.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 1, &copyMemoryBarrier,
						 0, nullptr);

	//vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(2));

	// Build acceleration structures

	VkAccelerationStructureGeometryKHR tlasGeometry;
	VkAccelerationStructureBuildGeometryInfoKHR tlasAccelerationStructureBuildInfo =
		constructTLASGeometryInfo(frameData.frameIndex, VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR, tlasGeometry);

	VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo = { .primitiveCount =
																   static_cast<uint32_t>(spheres.size() + m_triangleObjectCount) };
	VkAccelerationStructureBuildRangeInfoKHR* ptrTLASRangeInfo = &tlasRangeInfo;

	vkCmdBuildAccelerationStructuresKHR(frameData.commandBuffer, 1, &tlasAccelerationStructureBuildInfo,
										&ptrTLASRangeInfo);

	//vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(3));

	VkImageSubresourceRange imageRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
										   .baseMipLevel = 0,
										   .levelCount = 1,
										   .baseArrayLayer = 0,
										   .layerCount = 1 };

	VkImageMemoryBarrier memoryBarrierBefore = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
												 .srcAccessMask = 0,
												 .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
												 .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
												 .newLayout = VK_IMAGE_LAYOUT_GENERAL,
												 .srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												 .dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												 .image = frameData.swapchainImage,
												 .subresourceRange = imageRange };

	VkBufferMemoryBarrier objectDataMemoryBarrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
													  .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
													  .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
													  .srcQueueFamilyIndex = m_device.queueFamilyIndex(),
													  .dstQueueFamilyIndex = m_device.queueFamilyIndex(),
													  .buffer = m_objectDataBuffer.buffer,
													  .offset =
														  m_objectFrameDataSize * frameData.frameIndex,
													  .size = objectColors.size() * sizeof(float) };

	VkMemoryBarrier buildMemoryBarrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
										   .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
										   .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR };

	vkCmdPipelineBarrier(frameData.commandBuffer,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
							 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &buildMemoryBarrier, 1, &objectDataMemoryBarrier, 1,
						 &memoryBarrierBefore);

	vkCmdBindPipeline(frameData.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);

	PushConstantData data = { .worldOffset = { m_worldPos[0], -m_worldPos[1], m_worldPos[2] },
							  .aspectRatio = static_cast<float>(m_device.window().width()) /
											 static_cast<float>(m_device.window().height()),
							  .tanHalfFov = tanf((75.0f / 180.0f) * std::numbers::pi / 2.0f) };
	vkCmdPushConstants(frameData.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0,
					   sizeof(PushConstantData), &data);

	vkCmdBindDescriptorSets(frameData.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipelineLayout, 0, 1,
							&m_descriptorSets[frameData.frameIndex], 0, nullptr);

	//vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(4));

	VkStridedDeviceAddressRegionKHR nullRegion = {};

	vkCmdTraceRaysKHR(frameData.commandBuffer, &m_raygenShaderBindingTable, &m_missShaderBindingTable,
					  &m_sphereHitShaderBindingTable, &nullRegion, static_cast<uint32_t>(m_device.window().width()),
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

	//vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(5));

	return m_device.endFrame();
}

VkAccelerationStructureBuildGeometryInfoKHR HardwareSphereRaytracer::constructBLASGeometryInfo(
	VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureGeometryKHR& targetGeometry) {
	targetGeometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
					   .geometryType = VK_GEOMETRY_TYPE_AABBS_KHR,
					   .geometry = { .aabbs = { VkAccelerationStructureGeometryAabbsDataKHR{
										 .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
										 .data = m_accelerationStructureDataDeviceAddress +
												 m_accelerationStructureFrameDataSize * frameInFlightCount,
										 .stride = sizeof(VkAabbPositionsKHR) } } } };

	return { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
			 .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
			 .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
					  VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
			 .mode = mode,
			 .srcAccelerationStructure = mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
											 ? m_blasStructureData.structures[m_sphereBLASIndex].structure
											 : VK_NULL_HANDLE,
			 .dstAccelerationStructure = m_blasStructureData.structures[m_sphereBLASIndex].structure,
			 .geometryCount = 1,
			 .pGeometries = &targetGeometry,
			 .scratchData = m_blasStructureData.scratchBufferDeviceAddress +
							m_blasStructureData.structures[m_sphereBLASIndex].scratchBufferBaseOffset };
}

VkAccelerationStructureBuildGeometryInfoKHR HardwareSphereRaytracer::constructPlaneBLASGeometryInfo(
	VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureGeometryKHR& targetGeometry) {
	targetGeometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
					   .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
					   .geometry = { .triangles = { VkAccelerationStructureGeometryTrianglesDataKHR{
										 .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
										 .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
										 .vertexData = m_accelerationStructureDataDeviceAddress +
													   m_accelerationStructureFrameDataSize * frameInFlightCount +
													   sizeof(VkAabbPositionsKHR),
										 .vertexStride = 3 * sizeof(float),
										 .maxVertex = 3,
										 .indexType = VK_INDEX_TYPE_UINT16,
										 .indexData = m_accelerationStructureDataDeviceAddress +
													  m_accelerationStructureFrameDataSize * frameInFlightCount +
													  sizeof(VkAabbPositionsKHR) + m_vertexDataSize } } } };

	return { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
			 .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
			 .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
					  VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
			 .mode = mode,
			 .srcAccelerationStructure = mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
											 ? m_blasStructureData.structures[m_planeBLASIndex].structure
											 : VK_NULL_HANDLE,
			 .dstAccelerationStructure = m_blasStructureData.structures[m_planeBLASIndex].structure,
			 .geometryCount = 1,
			 .pGeometries = &targetGeometry,
			 .scratchData = m_blasStructureData.scratchBufferDeviceAddress +
							m_blasStructureData.structures[m_planeBLASIndex].scratchBufferBaseOffset };
}

VkAccelerationStructureBuildGeometryInfoKHR HardwareSphereRaytracer::constructTLASGeometryInfo(
	uint32_t frameIndex, VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureGeometryKHR& targetGeometry) {
	targetGeometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
					   .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
					   .geometry = {
						   .instances = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
										  .arrayOfPointers = VK_FALSE,
										  .data = m_accelerationStructureDataDeviceAddress +
												  m_accelerationStructureFrameDataSize * frameIndex } } };

	return { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
			 .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
			 .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
					  VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
			 .mode = mode,
			 .srcAccelerationStructure = mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
											 ? m_tlasStructureData[frameIndex].structures[0].structure
											 : VK_NULL_HANDLE,
			 .dstAccelerationStructure = m_tlasStructureData[frameIndex].structures[0].structure,
			 .geometryCount = 1,
			 .pGeometries = &targetGeometry,
			 .scratchData = m_tlasStructureData[frameIndex].scratchBufferDeviceAddress +
							m_tlasStructureData[frameIndex].structures[0].scratchBufferBaseOffset };
}

void HardwareSphereRaytracer::setGeometryBLASBatchNames() {
	if (m_blasStructureData.sharedStructureMemory) {
		setObjectName(m_device.device(), VK_OBJECT_TYPE_DEVICE_MEMORY,
					  reinterpret_cast<uint64_t>(m_blasStructureData.sharedStructureMemory),
					  "BLAS/scratch buffer memory");
	}
	if (m_blasStructureData.scratchBuffer.dedicatedMemory) {
		setObjectName(m_device.device(), VK_OBJECT_TYPE_DEVICE_MEMORY,
					  reinterpret_cast<uint64_t>(m_blasStructureData.sharedStructureMemory),
					  "Dedicated TLAS scratch buffer memory");
	}
	if (m_blasStructureData.structureBuffer.dedicatedMemory) {
		setObjectName(m_device.device(), VK_OBJECT_TYPE_DEVICE_MEMORY,
					  reinterpret_cast<uint64_t>(m_blasStructureData.sharedStructureMemory),
					  "Dedicated BLAS buffer memory");
	}

	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER,
				  reinterpret_cast<uint64_t>(m_blasStructureData.scratchBuffer.buffer),
				  "BLAS scratch buffer for frame in flight ");

	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER,
				  reinterpret_cast<uint64_t>(m_blasStructureData.structureBuffer.buffer),
				  "BLAS buffer for frame in flight ");

	setObjectName(m_device.device(), VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,
				  reinterpret_cast<uint64_t>(m_blasStructureData.structures[m_sphereBLASIndex].structure),
				  "Sphere BLAS");
}

void HardwareSphereRaytracer::setGeometryTLASBatchNames(size_t frameIndex) {
	std::string frameInFlightIndexString = std::to_string(frameIndex);
	if (m_tlasStructureData[frameIndex].sharedStructureMemory) {
		setObjectName(m_device.device(), VK_OBJECT_TYPE_DEVICE_MEMORY,
					  reinterpret_cast<uint64_t>(m_tlasStructureData[frameIndex].sharedStructureMemory),
					  "TLAS/scratch buffer memory for frame in flight " + frameInFlightIndexString);
	}
	if (m_tlasStructureData[frameIndex].scratchBuffer.dedicatedMemory) {
		setObjectName(m_device.device(), VK_OBJECT_TYPE_DEVICE_MEMORY,
					  reinterpret_cast<uint64_t>(m_tlasStructureData[frameIndex].sharedStructureMemory),
					  "Dedicated TLAS scratch buffer memory for frame in flight " + frameInFlightIndexString);
	}
	if (m_tlasStructureData[frameIndex].structureBuffer.dedicatedMemory) {
		setObjectName(m_device.device(), VK_OBJECT_TYPE_DEVICE_MEMORY,
					  reinterpret_cast<uint64_t>(m_tlasStructureData[frameIndex].sharedStructureMemory),
					  "Dedicated TLAS buffer memory for frame in flight " + frameInFlightIndexString);
	}

	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER,
				  reinterpret_cast<uint64_t>(m_tlasStructureData[frameIndex].scratchBuffer.buffer),
				  "TLAS Scratch buffer for frame in flight " + frameInFlightIndexString);

	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER,
				  reinterpret_cast<uint64_t>(m_tlasStructureData[frameIndex].structureBuffer.buffer),
				  "TLAS buffer for frame in flight " + frameInFlightIndexString);

	setObjectName(m_device.device(), VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,
				  reinterpret_cast<uint64_t>(m_tlasStructureData[frameIndex].structures[0].structure),
				  "TLAS for frame in flight " + frameInFlightIndexString);
}
