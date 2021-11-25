#include <DebugHelper.hpp>
#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <ShaderModuleHelper.hpp>
#include <cmath>
#include <cstring>
#include <numbers>
#include <volk.h>

HardwareSphereRaytracer::HardwareSphereRaytracer(size_t windowWidth, size_t windowHeight, size_t sphereCount,
												 std::vector<size_t> emissiveSphereIndices)
	: m_device(windowWidth, windowHeight, true), m_emissiveSphereIndices(emissiveSphereIndices) {
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
	VkDescriptorSetLayoutBinding geometryBindings[3] = { { .binding = 0, // vertex position (data) buffer
														   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														   .descriptorCount = 1,
														   .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
														 { .binding = 1, // index buffer
														   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														   .descriptorCount = 1,
														   .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
														 { .binding = 2, // normal buffer
														   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														   .descriptorCount = 1,
														   .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR } };

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 4, .pBindings = bindings
	};

	verifyResult(vkCreateDescriptorSetLayout(m_device.device(), &descriptorSetLayoutCreateInfo, nullptr,
											 &m_descriptorSetLayout));

	descriptorSetLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
									  .bindingCount = 3,
									  .pBindings = geometryBindings };

	verifyResult(vkCreateDescriptorSetLayout(m_device.device(), &descriptorSetLayoutCreateInfo, nullptr,
											 &m_geometryDescriptorSetLayout));

	VkPushConstantRange constantRange = { .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
										  .offset = 0, .size = sizeof(PushConstantData) };

	VkDescriptorSetLayout layouts[2] = { m_descriptorSetLayout, m_geometryDescriptorSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
															.setLayoutCount = 2,
															.pSetLayouts = layouts,
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
		{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 2 * frameInFlightCount + 3 }
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
															.maxSets = frameInFlightCount + 1,
															.poolSizeCount = 3,
															.pPoolSizes = poolSizes };

	verifyResult(vkCreateDescriptorPool(m_device.device(), &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));

	VkDescriptorSetLayout allocationLayouts[frameInFlightCount + 1];
	for (size_t i = 0; i < frameInFlightCount; ++i)
		allocationLayouts[i] = m_descriptorSetLayout;
	allocationLayouts[frameInFlightCount] = m_geometryDescriptorSetLayout;

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
															  .descriptorPool = m_descriptorPool,
															  .descriptorSetCount = frameInFlightCount + 1,
															  .pSetLayouts = allocationLayouts };

	VkDescriptorSet descriptorSets[frameInFlightCount + 1];

	verifyResult(vkAllocateDescriptorSets(m_device.device(), &descriptorSetAllocateInfo, descriptorSets));

	std::memcpy(m_descriptorSets, descriptorSets, frameInFlightCount * sizeof(VkDescriptorSet));
	m_geometryDescriptorSet = descriptorSets[frameInFlightCount];

	VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
											  .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											  .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											  .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
											  .unnormalizedCoordinates = VK_TRUE };
	verifyResult(vkCreateSampler(m_device.device(), &samplerCreateInfo, nullptr, &m_imageSampler));

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
	// light sphere information
	// ---------------------------------
	// sizeof(VkAabbPositions): AABB position storage
	// sizeof(PerVertexData) * m_triangleUniqueVertexCount: Vertex position storage
	// sizeof(uint16_t) * m_triangleUniqueIndexCount: Index storage
	// sizeof(VkTransformMatrixKHR) * m_triangleTransformCount: Triangle mesh transform data
	// shaderGroupHandleSizeAligned * 4: Shader Binding Table

	BufferInfo stagingBufferInfo = { .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
									 .requiredProperties =
										 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

	for (size_t i = 0; i < frameInFlightCount; ++i) {
		m_objectDataStagingStorage[i] = addSuballocation(stagingBufferInfo, sizeof(PerObjectData) * objectCount);
		m_objectInstanceStagingStorage[i] =
			addSuballocation(stagingBufferInfo, sizeof(VkAccelerationStructureInstanceKHR) * objectCount, 16);
		m_lightStagingStorage[i] =
			addSuballocation(stagingBufferInfo, sizeof(LightData) * m_emissiveSphereIndices.size());
	}

	m_aabbStagingStorage = addSuballocation(stagingBufferInfo, sizeof(VkAabbPositionsKHR));
	m_vertexStagingStorage = addSuballocation(stagingBufferInfo, vertexDataSize());
	m_indexStagingStorage = addSuballocation(stagingBufferInfo, indexDataSize());
	m_transformStagingStorage = addSuballocation(stagingBufferInfo, transformDataSize());
	m_normalStagingStorage = addSuballocation(stagingBufferInfo, normalDataSize());
	m_sbtStagingStorage = addSuballocation(stagingBufferInfo, m_stagingSBTCount * shaderGroupHandleSizeAligned);

	BufferAllocationBatch stagingBatch = allocateBatch(m_device, { stagingBufferInfo }, 0);
	m_stagingBuffer = stagingBatch.buffers[0];
	if (!m_stagingBuffer.dedicatedMemory) {
		m_stagingBuffer.dedicatedMemory = stagingBatch.sharedMemory;
	}
	verifyResult(vkMapMemory(m_device.device(), m_stagingBuffer.dedicatedMemory, 0, stagingBufferInfo.size, 0,
							 &m_mappedStagingBuffer));

	if (enableDebugUtils) {
		setObjectName(m_device.device(), VK_OBJECT_TYPE_DEVICE_MEMORY,
					  reinterpret_cast<uint64_t>(m_stagingBuffer.dedicatedMemory), "Staging buffer memory");
	}

	BufferInfo accelerationStructureDataInfo = {
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
				 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
				 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.preferredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};

	BufferInfo sbtDataInfo = {
		.size = shaderGroupHandleSizeAligned * 6,
		.requiredAlignment = pipelineProperties.shaderGroupBaseAlignment,
		.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.preferredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	};

	BufferInfo objectDataInfo = {
		.requiredAlignment = properties2.properties.limits.minStorageBufferOffsetAlignment,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.preferredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,

	};

	for (size_t i = 0; i < frameInFlightCount; ++i) {
		m_instanceDataStorage[i] = addSuballocation(accelerationStructureDataInfo,
													sizeof(VkAccelerationStructureInstanceKHR) * objectCount, 16);

		m_objectDataStorage[i] = addSuballocation(objectDataInfo, sizeof(PerObjectData) * objectCount,
												  properties2.properties.limits.minStorageBufferOffsetAlignment);

		m_lightDataStorage[i] = addSuballocation(objectDataInfo, sizeof(LightData) * m_emissiveSphereIndices.size(),
		properties2.properties.limits.minStorageBufferOffsetAlignment);
	}

	m_normalDataStorage = addSuballocation(
		objectDataInfo, normalDataSize(), properties2.properties.limits.minStorageBufferOffsetAlignment);

	m_aabbDataStorage = addSuballocation(accelerationStructureDataInfo, sizeof(VkAabbPositionsKHR),
										 properties2.properties.limits.minStorageBufferOffsetAlignment,
										 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	m_vertexDataStorage = addSuballocation(accelerationStructureDataInfo, vertexDataSize(),
										   properties2.properties.limits.minStorageBufferOffsetAlignment,
										   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	m_indexDataStorage = addSuballocation(accelerationStructureDataInfo, indexDataSize(),
										  properties2.properties.limits.minStorageBufferOffsetAlignment,
										  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	m_transformDataStorage = addSuballocation(accelerationStructureDataInfo, transformDataSize(),
											  properties2.properties.limits.minStorageBufferOffsetAlignment,
											  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	// Allocate device buffers
	BufferAllocationBatch deviceBatch = allocateBatch(m_device,
													  {
														  accelerationStructureDataInfo,
														  objectDataInfo,
														  sbtDataInfo,
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

	VkDeviceAddress accelerationStructureDataDeviceAddress =
		vkGetBufferDeviceAddress(m_device.device(), &deviceAddressInfo);

	for (size_t i = 0; i < frameInFlightCount; ++i) {
		m_instanceDataStorage[i].address = accelerationStructureDataDeviceAddress + m_instanceDataStorage[i].offset;
	}

	m_aabbDataStorage.address = accelerationStructureDataDeviceAddress + m_aabbDataStorage.offset;
	m_vertexDataStorage.address = accelerationStructureDataDeviceAddress + m_vertexDataStorage.offset;
	m_indexDataStorage.address = accelerationStructureDataDeviceAddress + m_indexDataStorage.offset;
	m_transformDataStorage.address = accelerationStructureDataDeviceAddress + m_transformDataStorage.offset;

	deviceAddressInfo.buffer = m_shaderBindingTableBuffer.buffer;

	VkDeviceAddress shaderBindingTableDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &deviceAddressInfo);
	m_raygenShaderBindingTable = { .deviceAddress = shaderBindingTableDeviceAddress,
								   .stride = shaderGroupHandleSizeAligned,
								   .size = shaderGroupHandleSizeAligned };
	m_sphereHitShaderBindingTable = { .deviceAddress =
										  shaderBindingTableDeviceAddress + shaderGroupHandleSizeAligned * 2,
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
	void* stagingBufferPositionsPointer =
		reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) + m_aabbStagingStorage.offset);
	new (stagingBufferPositionsPointer)
		VkAabbPositionsKHR{ .minX = -0.5f, .minY = -0.5, .minZ = -0.5, .maxX = 0.5f, .maxY = 0.5f, .maxZ = 0.5f };

	void* triangleVertexDataPointer =
		reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) + m_vertexStagingStorage.offset);
	// clang-format off
	// vertices
	float vertices[4][4] = {
		{ 2500.0f, 0.0f,  2500.0f, 1.0f},
		{ 2500.0f, 0.0f,  -5.0f,   1.0f},
		{ -5.0f,   0.0f,  2500.0f, 1.0f},
		{ -5.0f,   0.0f,  -5.0f,   1.0f}
	};
	std::memcpy(triangleVertexDataPointer, vertices, vertexDataSize());
	// clang-format on

	void* triangleIndexDataPointer =
		reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) + m_indexStagingStorage.offset);
	uint16_t indices[6] = { 3, 2, 0, 1, 3, 0 };
	std::memcpy(triangleIndexDataPointer, indices, indexDataSize());

	void* triangleNormalsDataPointer =
		reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) + m_normalStagingStorage.offset);
	float normals[2][4] = { { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } };
	std::memcpy(triangleNormalsDataPointer, normals, normalDataSize());

	void* shaderBindingTablePointer =
		reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) + m_sbtStagingStorage.offset);
	vkGetRayTracingShaderGroupHandlesKHR(m_device.device(), m_pipeline, 0, m_stagingSBTCount,
										 shaderGroupHandleSize * m_stagingSBTCount, shaderBindingTablePointer);

	vkDestroyShaderModule(m_device.device(), raygenShaderModule, nullptr);
	vkDestroyShaderModule(m_device.device(), hitShaderModule, nullptr);
	vkDestroyShaderModule(m_device.device(), triangleHitShaderModule, nullptr);
	vkDestroyShaderModule(m_device.device(), missShaderModule, nullptr);
	vkDestroyShaderModule(m_device.device(), intersectionShaderModule, nullptr);

	VkDescriptorBufferInfo vertexBufferInfo = { .buffer = m_accelerationStructureDataBuffer.buffer,
												.offset = m_vertexDataStorage.offset,
												.range = m_vertexDataStorage.size };
	VkDescriptorBufferInfo indexBufferInfo = { .buffer = m_accelerationStructureDataBuffer.buffer,
											   .offset = m_indexDataStorage.offset,
											   .range = m_indexDataStorage.size };
	VkDescriptorBufferInfo normalBufferInfo = { .buffer = m_objectDataBuffer.buffer,
												.offset = m_normalDataStorage.offset,
												.range = m_normalDataStorage.size };

	VkWriteDescriptorSet setWrites[3] = { { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											.dstSet = m_geometryDescriptorSet,
											.dstBinding = 0,
											.dstArrayElement = 0,
											.descriptorCount = 1,
											.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											.pBufferInfo = &vertexBufferInfo },
										  { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											.dstSet = m_geometryDescriptorSet,
											.dstBinding = 0,
											.dstArrayElement = 0,
											.descriptorCount = 1,
											.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											.pBufferInfo = &indexBufferInfo },
										  { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											.dstSet = m_geometryDescriptorSet,
											.dstBinding = 2,
											.dstArrayElement = 0,
											.descriptorCount = 1,
											.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											.pBufferInfo = &normalBufferInfo } };

	vkUpdateDescriptorSets(m_device.device(), 3, setWrites, 0, nullptr);

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
	bufferCopyRegions.reserve(frameInFlightCount + 3);

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

	bufferCopyRegions.push_back({ .srcOffset = m_aabbStagingStorage.offset,
								  .dstOffset = m_aabbDataStorage.offset,
								  .size = m_aabbStagingStorage.size });
	bufferCopyRegions.push_back({ .srcOffset = m_vertexStagingStorage.offset,
								  .dstOffset = m_vertexDataStorage.offset,
								  .size = m_vertexStagingStorage.size });
	bufferCopyRegions.push_back({ .srcOffset = m_indexStagingStorage.offset,
								  .dstOffset = m_indexDataStorage.offset,
								  .size = m_indexStagingStorage.size });
	if (m_transformStagingStorage.size) {
		bufferCopyRegions.push_back({ .srcOffset = m_transformStagingStorage.offset,
									  .dstOffset = m_transformDataStorage.offset,
									  .size = m_transformStagingStorage.size });
	}

	for (size_t i = 0; i < frameInFlightCount; ++i) {
		void* mappedFrameSection = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) +
														   m_objectInstanceStagingStorage[i].offset);
		std::vector<VkAccelerationStructureInstanceKHR> instances =
			std::vector<VkAccelerationStructureInstanceKHR>(objectCount);
		for (size_t j = 0; j < sphereCount; ++j) {
			instances[j] = { .transform = { .matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
														{ 0.0f, 1.0f, 0.0f, 0.0f },
														{ 0.0f, 0.0f, 1.0f, 0.0f } } },
							 .instanceCustomIndex = static_cast<uint32_t>(j),
							 .mask = 0xFF,
							 .accelerationStructureReference =
								 m_blasStructureData.structures[m_sphereBLASIndex].deviceAddress };
		}
		for (size_t j = 0; j < m_triangleObjectCount; ++j) {
			instances[sphereCount + j] = { .transform = { .matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
																	  { 0.0f, 1.0f, 0.0f, 0.0f },
																	  { 0.0f, 0.0f, 1.0f, 0.0f } } },
										   .instanceCustomIndex = static_cast<uint32_t>(j),
										   .mask = 0xFF,
										   .instanceShaderBindingTableRecordOffset = 1,
										   .accelerationStructureReference =
											   m_blasStructureData.structures[m_planeBLASIndex].deviceAddress };
		}
		std::memcpy(mappedFrameSection, instances.data(),
					instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

		bufferCopyRegions.push_back({ .srcOffset = m_objectInstanceStagingStorage[i].offset,
									  .dstOffset = m_instanceDataStorage[i].offset,
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
		{ .srcOffset = m_sbtStagingStorage.offset, .dstOffset = 0, .size = shaderGroupHandleSize },
		{ .srcOffset = m_sbtStagingStorage.offset,
		  .dstOffset = shaderGroupHandleSizeAligned,
		  .size = shaderGroupHandleSize },

		{ .srcOffset = m_sbtStagingStorage.offset + shaderGroupHandleSize,
		  .dstOffset = shaderGroupHandleSizeAligned * 2,
		  .size = shaderGroupHandleSize },
		{ .srcOffset = m_sbtStagingStorage.offset + shaderGroupHandleSize * 2,
		  .dstOffset = shaderGroupHandleSizeAligned * 3,
		  .size = shaderGroupHandleSize },

		{ .srcOffset = m_sbtStagingStorage.offset + shaderGroupHandleSize * 3,
		  .dstOffset = shaderGroupHandleSizeAligned * 4,
		  .size = shaderGroupHandleSize },
		{ .srcOffset = m_sbtStagingStorage.offset + shaderGroupHandleSize * 3,
		  .dstOffset = shaderGroupHandleSizeAligned * 5,
		  .size = shaderGroupHandleSize }
	};
	vkCmdCopyBuffer(submitCommandBuffer, m_stagingBuffer.buffer, m_shaderBindingTableBuffer.buffer, 6,
					shaderBindingTableCopies);

	VkBufferCopy normalDataCopy = { .srcOffset = m_normalStagingStorage.offset,
									.dstOffset = m_normalDataStorage.offset,
									.size = m_normalDataStorage.size };
	vkCmdCopyBuffer(submitCommandBuffer, m_stagingBuffer.buffer, m_objectDataBuffer.buffer, 1, &normalDataCopy);

	VkBufferMemoryBarrier copyMemoryBarrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
												.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
												.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
												.srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												.dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												.buffer = m_accelerationStructureDataBuffer.buffer,
												.offset = 0,
												.size = VK_WHOLE_SIZE };

	vkCmdPipelineBarrier(submitCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 1, &copyMemoryBarrier,
						 0, nullptr);

	// vkCmdSetCheckpointNV(submitCommandBuffer, reinterpret_cast<void*>(6));

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

	// vkCmdSetCheckpointNV(submitCommandBuffer, reinterpret_cast<void*>(7));

	VkBufferMemoryBarrier normalCopyBarrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
												.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
												.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
												.srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												.dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												.buffer = m_objectDataBuffer.buffer,
												.offset = m_normalDataStorage.offset,
												.size = m_normalDataStorage.size };
	vkCmdPipelineBarrier(submitCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 1, &normalCopyBarrier, 0,
						 nullptr);

	// vkCmdSetCheckpointNV(submitCommandBuffer, reinterpret_cast<void*>(8));

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
	vkDestroyDescriptorSetLayout(m_device.device(), m_geometryDescriptorSetLayout, nullptr);
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

	VkDescriptorBufferInfo colorBufferInfo = { .buffer = m_objectDataBuffer.buffer,
											   .offset = m_objectDataStorage[frameData.frameIndex].offset,
											   .range = m_objectDataStorage[frameData.frameIndex].size };
	VkWriteDescriptorSet colorBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											  .dstSet = m_descriptorSets[frameData.frameIndex],
											  .dstBinding = 2,
											  .dstArrayElement = 0,
											  .descriptorCount = 1,
											  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											  .pBufferInfo = &colorBufferInfo };

	VkDescriptorBufferInfo lightBufferInfo = { .buffer = m_objectDataBuffer.buffer,
											   .offset = m_lightDataStorage[frameData.frameIndex].offset,
											   .range = m_emissiveSphereIndices.size() * sizeof(LightData) };
	VkWriteDescriptorSet lightBufferWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											  .dstSet = m_descriptorSets[frameData.frameIndex],
											  .dstBinding = 3,
											  .dstArrayElement = 0,
											  .descriptorCount = 1,
											  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											  .pBufferInfo = &lightBufferInfo };

	VkWriteDescriptorSet writes[] = { accelerationStructureWrite, imageWrite, colorBufferWrite, lightBufferWrite };

	vkUpdateDescriptorSets(m_device.device(), 4, writes, 0, nullptr);

	// vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(0));

	// Write updated data

	void* instanceDataSection = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) +
														m_objectInstanceStagingStorage[frameData.frameIndex].offset);
	void* objectDataSection = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) +
													  m_objectDataStagingStorage[frameData.frameIndex].offset);
	void* lightDataSection = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_mappedStagingBuffer) +
													 m_lightStagingStorage[frameData.frameIndex].offset);

	std::vector<VkAccelerationStructureInstanceKHR> instances =
		std::vector<VkAccelerationStructureInstanceKHR>(spheres.size() + m_triangleObjectCount);
	std::vector<PerObjectData> objectData = std::vector<PerObjectData>(spheres.size() + m_triangleObjectCount);
	std::vector<LightData> lightData = std::vector<LightData>(m_emissiveSphereIndices.size());
	for (size_t i = 0; i < spheres.size(); ++i) {
		instances[i] = { .transform = { .matrix = { { 2 * spheres[i].radius, 0.0f, 0.0f, spheres[i].position[0] },
													{ 0.0f, 2 * spheres[i].radius, 0.0f, spheres[i].position[1] },
													{ 0.0f, 0.0f, 2 * spheres[i].radius, spheres[i].position[2] } } },
						 .instanceCustomIndex = static_cast<uint32_t>(i),
						 .mask = 0xFF,
						 .accelerationStructureReference =
							 m_blasStructureData.structures[m_sphereBLASIndex].deviceAddress };
		std::memcpy(objectData[i].color, spheres[i].color, sizeof(float) * 4);
	}
	for (size_t i = 0; i < m_triangleObjectCount; ++i) {
		instances[spheres.size() + i] = { .transform = { .matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
																	 { 0.0f, 1.0f, 0.0f, 0.0f },
																	 { 0.0f, 0.0f, 1.0f, 0.0f } } },
										  .instanceCustomIndex = static_cast<uint32_t>(i),
										  .mask = 0xFF,
										  .instanceShaderBindingTableRecordOffset = 1,
										  .accelerationStructureReference =
											  m_blasStructureData.structures[m_planeBLASIndex].deviceAddress };
		float color[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
		std::memcpy(objectData[spheres.size() + i].color, color, sizeof(float) * 4);
	}
	for (size_t i = 0; i < m_emissiveSphereIndices.size(); ++i) {
		std::memcpy(lightData[i].position, spheres[m_emissiveSphereIndices[i]].position, 4 * sizeof(float));
		lightData[i].radius = spheres[m_emissiveSphereIndices[i]].radius;
	}
	std::memcpy(instanceDataSection, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
	std::memcpy(objectDataSection, objectData.data(), objectData.size() * sizeof(PerObjectData));
	std::memcpy(lightDataSection, lightData.data(), lightData.size() * sizeof(LightData));

	VkBufferCopy instanceCopyRegion = { .srcOffset = m_objectInstanceStagingStorage[frameData.frameIndex].offset,
										.dstOffset = m_instanceDataStorage[frameData.frameIndex].offset,
										.size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR) };

	vkCmdCopyBuffer(frameData.commandBuffer, m_stagingBuffer.buffer, m_accelerationStructureDataBuffer.buffer, 1,
					&instanceCopyRegion);

	VkBufferCopy dataCopyRegions[2] = { { .srcOffset = m_objectDataStagingStorage[frameData.frameIndex].offset,
										  .dstOffset = m_objectDataStorage[frameData.frameIndex].offset,
										  .size = m_objectDataStagingStorage[frameData.frameIndex].size },
										{ .srcOffset = m_lightStagingStorage[frameData.frameIndex].offset,
										  .dstOffset = m_lightDataStorage[frameData.frameIndex].offset,
										  .size = m_lightStagingStorage[frameData.frameIndex].size } };

	vkCmdCopyBuffer(frameData.commandBuffer, m_stagingBuffer.buffer, m_objectDataBuffer.buffer, 2, dataCopyRegions);

	// vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(1));

	VkBufferMemoryBarrier copyMemoryBarrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
												.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
												.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
												.srcQueueFamilyIndex = m_device.queueFamilyIndex(),
												.dstQueueFamilyIndex = m_device.queueFamilyIndex(),
												.buffer = m_accelerationStructureDataBuffer.buffer,
												.offset = m_instanceDataStorage[frameData.frameIndex].offset,
												.size = m_instanceDataStorage[frameData.frameIndex].size };

	vkCmdPipelineBarrier(frameData.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 1, &copyMemoryBarrier,
						 0, nullptr);

	// vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(2));

	// Build acceleration structures

	VkAccelerationStructureGeometryKHR tlasGeometry;
	VkAccelerationStructureBuildGeometryInfoKHR tlasAccelerationStructureBuildInfo =
		constructTLASGeometryInfo(frameData.frameIndex, VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR, tlasGeometry);

	VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo = { .primitiveCount = static_cast<uint32_t>(
																   spheres.size() + m_triangleObjectCount) };
	VkAccelerationStructureBuildRangeInfoKHR* ptrTLASRangeInfo = &tlasRangeInfo;

	vkCmdBuildAccelerationStructuresKHR(frameData.commandBuffer, 1, &tlasAccelerationStructureBuildInfo,
										&ptrTLASRangeInfo);

	// vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(3));

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
													  .offset = m_objectDataStorage[frameData.frameIndex].offset,
													  .size = m_objectDataStorage[frameData.frameIndex].size };
	VkBufferMemoryBarrier lightDataMemoryBarrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
													  .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
													  .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
													  .srcQueueFamilyIndex = m_device.queueFamilyIndex(),
													  .dstQueueFamilyIndex = m_device.queueFamilyIndex(),
													  .buffer = m_objectDataBuffer.buffer,
													  .offset = m_lightDataStorage[frameData.frameIndex].offset,
													  .size = m_lightDataStorage[frameData.frameIndex].size };

	VkBufferMemoryBarrier dataMemoryBarriers[2] = { objectDataMemoryBarrier, lightDataMemoryBarrier };

	VkMemoryBarrier buildMemoryBarrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
										   .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
										   .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR };

	vkCmdPipelineBarrier(frameData.commandBuffer,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
							 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &buildMemoryBarrier, 2,
						 dataMemoryBarriers, 1, &memoryBarrierBefore);

	vkCmdBindPipeline(frameData.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);

	PushConstantData data = { .worldOffset = { m_worldPos[0], -m_worldPos[1], m_worldPos[2] },
							  .aspectRatio = static_cast<float>(m_device.window().width()) /
											 static_cast<float>(m_device.window().height()),
							  .tanHalfFov = tanf((75.0f / 180.0f) * std::numbers::pi / 2.0f),
							  .time = static_cast<float>(glfwGetTime()) };
	vkCmdPushConstants(frameData.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0,
					   sizeof(PushConstantData), &data);

	VkDescriptorSet sets[2] = { m_descriptorSets[frameData.frameIndex], m_geometryDescriptorSet };

	vkCmdBindDescriptorSets(frameData.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipelineLayout, 0, 2,
							sets, 0, nullptr);

	// vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(4));

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

	// vkCmdSetCheckpointNV(frameData.commandBuffer, reinterpret_cast<void*>(5));

	return m_device.endFrame();
}

VkAccelerationStructureBuildGeometryInfoKHR HardwareSphereRaytracer::constructBLASGeometryInfo(
	VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureGeometryKHR& targetGeometry) {
	targetGeometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
					   .geometryType = VK_GEOMETRY_TYPE_AABBS_KHR,
					   .geometry = { .aabbs = { VkAccelerationStructureGeometryAabbsDataKHR{
										 .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
										 .data = { .deviceAddress = m_aabbDataStorage.address },
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
			 .scratchData = { .deviceAddress =
								  m_blasStructureData.scratchBufferDeviceAddress +
								  m_blasStructureData.structures[m_sphereBLASIndex].scratchBufferBaseOffset } };
}

VkAccelerationStructureBuildGeometryInfoKHR HardwareSphereRaytracer::constructPlaneBLASGeometryInfo(
	VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureGeometryKHR& targetGeometry) {
	targetGeometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
					   .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
					   .geometry = { .triangles = { VkAccelerationStructureGeometryTrianglesDataKHR{
										 .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
										 .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
										 .vertexData = { .deviceAddress = m_vertexDataStorage.address },
										 .vertexStride = sizeof(PerVertexData),
										 .maxVertex = 3,
										 .indexType = VK_INDEX_TYPE_UINT16,
										 .indexData = { .deviceAddress = m_indexDataStorage.address } } } } };

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
			 .scratchData = { .deviceAddress =
								  m_blasStructureData.scratchBufferDeviceAddress +
								  m_blasStructureData.structures[m_planeBLASIndex].scratchBufferBaseOffset } };
}

VkAccelerationStructureBuildGeometryInfoKHR HardwareSphereRaytracer::constructTLASGeometryInfo(
	uint32_t frameIndex, VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureGeometryKHR& targetGeometry) {
	targetGeometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
					   .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
					   .geometry = {
						   .instances = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
										  .arrayOfPointers = VK_FALSE,
										  .data = { .deviceAddress = m_instanceDataStorage[frameIndex].address } } } };

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
			 .scratchData = { .deviceAddress =
								  m_tlasStructureData[frameIndex].scratchBufferDeviceAddress +
								  m_tlasStructureData[frameIndex].structures[0].scratchBufferBaseOffset } };
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

constexpr size_t HardwareSphereRaytracer::vertexDataSize() { return uniqueVertexCount() * sizeof(PerVertexData); }

constexpr size_t HardwareSphereRaytracer::indexDataSize() { return indexCount() * sizeof(uint16_t); }

constexpr size_t HardwareSphereRaytracer::transformDataSize() {
	return transformCount() * sizeof(VkTransformMatrixKHR);
}

constexpr size_t HardwareSphereRaytracer::normalDataSize() { return normalCount() * sizeof(float) * 4; }

constexpr size_t HardwareSphereRaytracer::uniqueVertexCount() {
	size_t vertexCount = 0;
	for (size_t i = 0; i < m_triangleObjectCount; ++i) {
		vertexCount += m_triangleObjects[i].vertexCount;
	}
	return vertexCount;
}

constexpr size_t HardwareSphereRaytracer::indexCount() {
	size_t indexCount = 0;
	for (size_t i = 0; i < m_triangleObjectCount; ++i) {
		indexCount += m_triangleObjects[i].indexCount;
	}
	return indexCount;
}

constexpr size_t HardwareSphereRaytracer::transformCount() {
	size_t transformCount = 0;
	for (size_t i = 0; i < m_triangleObjectCount; ++i) {
		transformCount += m_triangleObjects[i].transformCount;
	}
	return transformCount;
}

constexpr size_t HardwareSphereRaytracer::normalCount() {
	size_t normalCount = 0;
	for (size_t i = 0; i < m_triangleObjectCount; ++i) {
		normalCount += m_triangleObjects[i].indexCount / 3;
	}
	return normalCount;
}
