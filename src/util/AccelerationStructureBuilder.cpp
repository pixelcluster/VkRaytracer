#include <DebugHelper.hpp>
#include <util/AccelerationStructureBuilder.hpp>

// assign geometry to whichever AS it intersects with the most, undef this in order to assign geometries to ASes based
// on whichever generates less intersection area between all AABBS (is O(n^2) instead of O(n) with n=number of ASes
//#define AS_HEURISTIC_GEOMETRY_INTERSECTION

// cube root of this number should be an integer because the model AABB is subdivided into equally-sized cubes (3
// dimensions)
constexpr size_t numASSubdivisions = 8;

struct AccelerationStructureGeometryInfo {
	std::vector<VkAccelerationStructureGeometryKHR> geometries;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos;
	std::vector<size_t> geometryIndices;
};

AccelerationStructureBuilder::AccelerationStructureBuilder(RayTracingDevice& device, MemoryAllocator& memoryAllocator,
														   OneTimeDispatcher& dispatcher, ModelLoader& modelLoader,
														   const std::vector<Sphere> lightSpheres,
														   uint32_t triangleSBTIndex, uint32_t lightSphereSBTIndex)
	: m_device(device), m_allocator(memoryAllocator), m_dispatcher(dispatcher) {
	VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
	};
	VkPhysicalDeviceProperties2 properties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
											   .pNext = &accelerationStructureProperties };

	vkGetPhysicalDeviceProperties2(m_device.physicalDevice(), &properties);

	std::vector<AccelerationStructureGeometryInfo> asGeometryData;
	std::vector<AABB> asAABBs;

	asGeometryData.resize(numASSubdivisions);
	asAABBs.reserve(numASSubdivisions);

	const AABB& modelBounds = modelLoader.modelBounds();

	size_t numASesPerDimension = static_cast<size_t>(cbrtf(numASSubdivisions));

	float aabbChunkLength[3] = { (modelBounds.xmax - modelBounds.xmin) / numASesPerDimension,
								 (modelBounds.ymax - modelBounds.ymin) / numASesPerDimension,
								 (modelBounds.zmax - modelBounds.zmin) / numASesPerDimension };

	for (size_t i = 0; i < numASesPerDimension; ++i) {
		for (size_t j = 0; j < numASesPerDimension; ++j) {
			for (size_t k = 0; k < numASesPerDimension; ++k) {
				asAABBs.push_back({ .xmin = modelBounds.xmin + k * aabbChunkLength[0],
									.ymin = modelBounds.ymin + j * aabbChunkLength[1],
									.zmin = modelBounds.zmin + i * aabbChunkLength[2],
									.xmax = modelBounds.xmin + (k + 1) * aabbChunkLength[0],
									.ymax = modelBounds.ymin + (j + 1) * aabbChunkLength[1],
									.zmax = modelBounds.zmin + (i + 1) * aabbChunkLength[2] });
			}
		}
	}

	VkBuffer triangleTransformBuffer;
	VkBuffer triangleTransformStagingBuffer;
	VkDeviceAddress triangleTransformBufferDeviceAddress;

	VkBufferCreateInfo transformBufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = modelLoader.geometries().size() * sizeof(VkTransformMatrixKHR),
		.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
	};
	verifyResult(vkCreateBuffer(m_device.device(), &transformBufferCreateInfo, nullptr, &triangleTransformBuffer));
	m_allocator.bindDeviceBuffer(triangleTransformBuffer, 0);

	transformBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	verifyResult(
		vkCreateBuffer(m_device.device(), &transformBufferCreateInfo, nullptr, &triangleTransformStagingBuffer));
	void* mappedTransformStagingBuffer = m_allocator.bindStagingBuffer(triangleTransformStagingBuffer, 0);

	VkBufferDeviceAddressInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
									   .buffer = triangleTransformBuffer };
	triangleTransformBufferDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &info);

	std::vector<VkTransformMatrixKHR> transformMatrices;
	transformMatrices.reserve(modelLoader.geometries().size());

	size_t currentTransformBufferOffset = 0;
	size_t geometryIndex = 0;
	for (auto& geometry : modelLoader.geometries()) {
		size_t asIndex = bestAccelerationStructureIndex(asAABBs, modelBounds, geometry.aabb);
		asGeometryData[asIndex].geometries.push_back(
			{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
			  .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
			  .geometry = { .triangles = { .sType =
											   VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
										   .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
										   .vertexData = { .deviceAddress = modelLoader.vertexBufferDeviceAddress() +
																			geometry.vertexOffset },
										   .vertexStride = 3 * sizeof(float),
										   .maxVertex = static_cast<uint32_t>(geometry.vertexCount - 1),
										   .indexType = VK_INDEX_TYPE_UINT32,
										   .indexData = { .deviceAddress = modelLoader.indexBufferDeviceAddress() +
																		   geometry.indexOffset },
										   .transformData = { .deviceAddress = triangleTransformBufferDeviceAddress +
																			   currentTransformBufferOffset } } },
			  .flags = geometry.isAlphaTested ? 0U : VK_GEOMETRY_OPAQUE_BIT_KHR });
		VkTransformMatrixKHR transformMatrix = { .matrix = {
													 { geometry.transformMatrix[0], geometry.transformMatrix[4],
													   geometry.transformMatrix[8], geometry.transformMatrix[12] },
													 { geometry.transformMatrix[1], geometry.transformMatrix[5],
													   geometry.transformMatrix[9], geometry.transformMatrix[13] },
													 { geometry.transformMatrix[2], geometry.transformMatrix[6],
													   geometry.transformMatrix[10], geometry.transformMatrix[14] } } };
		asGeometryData[asIndex].rangeInfos.push_back(
			{ .primitiveCount = static_cast<uint32_t>(geometry.indexCount / 3) });
		transformMatrices.push_back(transformMatrix);
		currentTransformBufferOffset += sizeof(VkTransformMatrixKHR);

		asGeometryData[asIndex].geometryIndices.push_back(geometryIndex);
		++geometryIndex;
	}

	std::memcpy(mappedTransformStagingBuffer, transformMatrices.data(),
				transformMatrices.size() * sizeof(VkTransformMatrixKHR));

	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos;
	std::vector<std::vector<VkAccelerationStructureBuildRangeInfoKHR>> buildRangeInfos;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> ptrBuildRangeInfos;
	std::vector<VkBuffer> uncompactedASBackingBuffers;
	std::vector<VkAccelerationStructureKHR> uncompactedBLASes;
	std::vector<VkBuffer> triangleASBuildScratchBuffers;
	std::vector<size_t> geometryIndexBufferOffsets;

	std::vector<uint32_t> geometryIndices;

	buildInfos.reserve(numASSubdivisions);
	geometryIndices.reserve(modelLoader.geometries().size());

	VkBufferDeviceAddressInfo deviceAddressInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };

	for (auto& data : asGeometryData) {
		if (!data.geometries.empty()) {
			buildRangeInfos.push_back({});
			buildRangeInfos.back().reserve(data.geometries.size());

			buildInfos.push_back({ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
								   .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
								   .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR |
											VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
								   .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
								   .geometryCount = static_cast<uint32_t>(data.geometries.size()),
								   .pGeometries = data.geometries.data() });

			std::vector<uint32_t> primitiveCounts;
			primitiveCounts.reserve(data.rangeInfos.size());

			for (auto& info : data.rangeInfos) {
				buildRangeInfos.back().push_back(info);
				primitiveCounts.push_back(info.primitiveCount);
			}

			geometryIndexBufferOffsets.push_back(geometryIndices.size());
			for (auto& index : data.geometryIndices) {
				geometryIndices.push_back(index);
			}

			AccelerationStructureData accelerationStructureData = createAccelerationStructure(
				buildInfos.back(), primitiveCounts,
				accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment);

			buildInfos.back().dstAccelerationStructure = accelerationStructureData.accelerationStructure;
			buildInfos.back().scratchData = { .deviceAddress = accelerationStructureData.scratchBufferDeviceAddress };

			uncompactedASBackingBuffers.push_back(accelerationStructureData.backingBuffer);
			uncompactedBLASes.push_back(accelerationStructureData.accelerationStructure);
			triangleASBuildScratchBuffers.push_back(accelerationStructureData.scratchBuffer);
			ptrBuildRangeInfos.push_back(buildRangeInfos.back().data());
		}
	}

	VkBuffer lightDataStagingBuffer;
	VkBuffer sphereAABBBuffer;
	VkDeviceAddress sphereAABBBufferDeviceAddress;
	VkBuffer sphereAABBStagingBuffer;
	AccelerationStructureData sphereBLASData;

	if (lightSpheres.size() > 0) {
		VkBufferCreateInfo sphereAABBBufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(VkAabbPositionsKHR),
			.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
					 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		};
		verifyResult(vkCreateBuffer(m_device.device(), &sphereAABBBufferCreateInfo, nullptr, &sphereAABBBuffer));

		sphereAABBBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		verifyResult(vkCreateBuffer(m_device.device(), &sphereAABBBufferCreateInfo, nullptr, &sphereAABBStagingBuffer));

		m_allocator.bindDeviceBuffer(sphereAABBBuffer, 0);
		void* mappedSphereAABBStagingBuffer = m_allocator.bindStagingBuffer(sphereAABBStagingBuffer, 0);

		new (mappedSphereAABBStagingBuffer)
			VkAabbPositionsKHR{ .minX = -1.0f, .minY = -1.0f, .minZ = -1.0f, .maxX = 1.0f, .maxY = 1.0f, .maxZ = 1.0f };

		deviceAddressInfo.buffer = sphereAABBBuffer;
		sphereAABBBufferDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &deviceAddressInfo);

		VkAccelerationStructureGeometryKHR sphereGeometry = {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
			.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR,
			.geometry = { .aabbs = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
									 .data = sphereAABBBufferDeviceAddress,
									 .stride = sizeof(VkAabbPositionsKHR) } },
			.flags = VK_GEOMETRY_OPAQUE_BIT_KHR
		};

		VkAccelerationStructureBuildGeometryInfoKHR sphereBuildInfo = {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
			.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
			.flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR |
					 VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
			.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
			.geometryCount = 1,
			.pGeometries = &sphereGeometry
		};

		uint32_t sphereCount = lightSpheres.size();

		sphereBLASData =
			createAccelerationStructure(sphereBuildInfo, { 1 },
										accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment);

		sphereBuildInfo.dstAccelerationStructure = sphereBLASData.accelerationStructure;
		sphereBuildInfo.scratchData = { .deviceAddress = sphereBLASData.scratchBufferDeviceAddress };

		buildInfos.push_back(sphereBuildInfo);
		buildRangeInfos.push_back({ { .primitiveCount = 1 } });
		ptrBuildRangeInfos.push_back(buildRangeInfos.back().data());
		uncompactedBLASes.push_back(sphereBLASData.accelerationStructure);

		VkBufferCreateInfo sphereDataBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
														  .size = sphereCount * sizeof(Sphere),
														  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
																   VK_BUFFER_USAGE_TRANSFER_DST_BIT };

		verifyResult(vkCreateBuffer(m_device.device(), &sphereDataBufferCreateInfo, nullptr, &m_lightDataBuffer));

		sphereDataBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		verifyResult(
			vkCreateBuffer(m_device.device(), &sphereDataBufferCreateInfo, nullptr, &lightDataStagingBuffer));

		setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, m_lightDataBuffer, "Light Data buffer");
		setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, lightDataStagingBuffer, "Light Data staging buffer");

		m_allocator.bindDeviceBuffer(m_lightDataBuffer, 0);
		void* mappedLightDataStagingBuffer = m_allocator.bindStagingBuffer(lightDataStagingBuffer, 0);

		std::memcpy(mappedLightDataStagingBuffer, lightSpheres.data(), sphereCount * sizeof(Sphere));

		sphereBuildInfo.dstAccelerationStructure = sphereBLASData.accelerationStructure;
		sphereBuildInfo.scratchData = { .deviceAddress = sphereBLASData.scratchBufferDeviceAddress };

		m_lightDataBufferSize = sphereCount * sizeof(Sphere);
	}

	m_geometryIndexBufferSize = geometryIndices.size() * sizeof(uint32_t);
	VkBufferCreateInfo geometryIndexBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
														 .size = geometryIndices.size() * sizeof(uint32_t),
														 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
																  VK_BUFFER_USAGE_TRANSFER_DST_BIT };
	verifyResult(vkCreateBuffer(m_device.device(), &geometryIndexBufferCreateInfo, nullptr, &m_geometryIndexBuffer));
	geometryIndexBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VkBuffer geometryIndexStagingBuffer;
	verifyResult(vkCreateBuffer(m_device.device(), &geometryIndexBufferCreateInfo, nullptr, &geometryIndexStagingBuffer));

	m_allocator.bindDeviceBuffer(m_geometryIndexBuffer, 0);
	void* mappedGeometryIndexBuffer = m_allocator.bindStagingBuffer(geometryIndexStagingBuffer, 0);

	std::memcpy(mappedGeometryIndexBuffer, geometryIndices.data(), m_geometryIndexBufferSize);

	VkQueryPool compactionSizeQueryPool;

	VkQueryPoolCreateInfo blasSizeQueryPoolCreateInfo = { .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
														  .queryType =
															  VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
														  .queryCount = static_cast<uint32_t>(buildInfos.size()) };
	verifyResult(vkCreateQueryPool(m_device.device(), &blasSizeQueryPoolCreateInfo, nullptr, &compactionSizeQueryPool));

	std::vector<VkCommandBuffer> commandBuffers = m_dispatcher.allocateOneTimeSubmitBuffers(2);
	VkCommandBuffer blasBuildBuffer = commandBuffers[0];
	VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
										   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	verifyResult(vkBeginCommandBuffer(blasBuildBuffer, &beginInfo));

	vkCmdResetQueryPool(blasBuildBuffer, compactionSizeQueryPool, 0, static_cast<uint32_t>(buildInfos.size()));

	VkBufferCopy bufferCopy = { .size = transformMatrices.size() * sizeof(VkTransformMatrixKHR) };
	vkCmdCopyBuffer(blasBuildBuffer, triangleTransformStagingBuffer, triangleTransformBuffer, 1, &bufferCopy);

	bufferCopy.size = m_geometryIndexBufferSize;
	vkCmdCopyBuffer(blasBuildBuffer, geometryIndexStagingBuffer, m_geometryIndexBuffer, 1, &bufferCopy);

	if (lightSpheres.size() > 0) {
		bufferCopy.size = sizeof(VkAabbPositionsKHR);
		vkCmdCopyBuffer(blasBuildBuffer, sphereAABBStagingBuffer, sphereAABBBuffer, 1, &bufferCopy);

		bufferCopy.size = lightSpheres.size() * sizeof(Sphere);
		vkCmdCopyBuffer(blasBuildBuffer, lightDataStagingBuffer, m_lightDataBuffer, 1, &bufferCopy);
	}

	VkMemoryBarrier barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
								.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
								.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR };
	vkCmdPipelineBarrier(blasBuildBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
						 nullptr);

	vkCmdBuildAccelerationStructuresKHR(blasBuildBuffer, buildInfos.size(), buildInfos.data(),
										ptrBuildRangeInfos.data());

	barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	vkCmdPipelineBarrier(blasBuildBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
						 nullptr);

	vkCmdWriteAccelerationStructuresPropertiesKHR(
		blasBuildBuffer, static_cast<uint32_t>(uncompactedBLASes.size()), uncompactedBLASes.data(),
		VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, compactionSizeQueryPool, 0);

	verifyResult(vkEndCommandBuffer(blasBuildBuffer));

	m_dispatcher.submit(blasBuildBuffer, {});
	m_dispatcher.waitForFence(blasBuildBuffer, UINT64_MAX);

	std::vector<uint32_t> compactedASSizes = std::vector<uint32_t>(uncompactedBLASes.size());
	verifyResult(vkGetQueryPoolResults(m_device.device(), compactionSizeQueryPool, 0, uncompactedBLASes.size(),
									   compactedASSizes.size() * sizeof(uint32_t), compactedASSizes.data(),
									   sizeof(uint32_t), 0));

	std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
	tlasInstances.reserve(uncompactedASBackingBuffers.size()); // sphere BLAS data was not appended to this array

	AccelerationStructureData compactedSphereAccelerationStructureData;
	if (lightSpheres.size() > 0) {
		uint32_t compactedSphereASSize = compactedASSizes.back();
		compactedASSizes.pop_back();
		uncompactedBLASes.pop_back();

		compactedSphereAccelerationStructureData = createAccelerationStructure(compactedSphereASSize);

		for (auto& sphere : lightSpheres) {
			tlasInstances.push_back(
				{ .transform = { .matrix = { { 2 * sphere.radius, 0.0f, 0.0f, sphere.position[0] },
											 { 0.0f, 2 * sphere.radius, 0.0f, sphere.position[1] },
											 { 0.0f, 0.0f, 2 * sphere.radius, sphere.position[2] } } },
				  .instanceCustomIndex = 0U,
				  .mask = 0x01, //culled in ray gen
				  .instanceShaderBindingTableRecordOffset = lightSphereSBTIndex,
				  .accelerationStructureReference =
					  compactedSphereAccelerationStructureData.accelerationStructureDeviceAddress });
		}
	}

	if (lightSpheres.size() > 0) {
		m_sphereBLAS = compactedSphereAccelerationStructureData.accelerationStructure;
		m_sphereASBackingBuffer = compactedSphereAccelerationStructureData.backingBuffer;
	}

	m_triangleBLASes.reserve(compactedASSizes.size());
	m_triangleASBackingBuffers.reserve(compactedASSizes.size());

	uint32_t instanceIndex = 0;
	for (auto& size : compactedASSizes) {
		AccelerationStructureData data = createAccelerationStructure(size);
		setObjectName(m_device.device(), VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, data.accelerationStructure,
					  "Compacted BLAS " + std::to_string(instanceIndex));

		m_triangleBLASes.push_back(data.accelerationStructure);
		m_triangleASBackingBuffers.push_back(data.backingBuffer);

		tlasInstances.push_back({ .transform = { .matrix = { { 1.0f, 0.0f, 0.0f, 1.0f },
															 { 0.0f, -1.0f, 0.0f, 1.0f },
															 { 0.0f, 0.0f, 1.0f, 1.0f } } },
								  .instanceCustomIndex = static_cast<uint32_t>(geometryIndexBufferOffsets[instanceIndex]),
								  .mask = 0xFF,
								  .instanceShaderBindingTableRecordOffset = triangleSBTIndex,
								  .accelerationStructureReference = data.accelerationStructureDeviceAddress });
		++instanceIndex;
	}

	VkBuffer instanceBuffer;
	VkBuffer instanceStagingBuffer;

	VkBufferCreateInfo instanceBufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR),
		.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
	};
	verifyResult(vkCreateBuffer(m_device.device(), &instanceBufferCreateInfo, nullptr, &instanceBuffer));
	instanceBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	verifyResult(vkCreateBuffer(m_device.device(), &instanceBufferCreateInfo, nullptr, &instanceStagingBuffer));

	m_allocator.bindDeviceBuffer(instanceBuffer, 0);
	void* mappedInstanceStagingBuffer = m_allocator.bindStagingBuffer(instanceStagingBuffer, 0);

	std::memcpy(mappedInstanceStagingBuffer, tlasInstances.data(),
				tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR));

	deviceAddressInfo.buffer = instanceBuffer;
	VkDeviceAddress instanceBufferDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &deviceAddressInfo);

	VkAccelerationStructureGeometryKHR tlasGeometry = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
		.geometry = { .instances = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
									 .arrayOfPointers = VK_FALSE,
									 .data = instanceBufferDeviceAddress } }
	};

	VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.geometryCount = 1,
		.pGeometries = &tlasGeometry
	};

	AccelerationStructureData tlasData = createAccelerationStructure(
		tlasBuildInfo, { static_cast<uint32_t>(tlasInstances.size()) },
		accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment, true);

	tlasBuildInfo.dstAccelerationStructure = tlasData.accelerationStructure;
	tlasBuildInfo.scratchData = { .deviceAddress = tlasData.scratchBufferDeviceAddress };

	setObjectName(m_device.device(), VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, tlasData.accelerationStructure, "TLAS");

	m_tlas = tlasData.accelerationStructure;
	m_tlasBackingBuffer = tlasData.backingBuffer;

	VkCommandBuffer tlasBuildBuffer = commandBuffers[1];
	verifyResult(vkBeginCommandBuffer(tlasBuildBuffer, &beginInfo));

	bufferCopy.size = tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
	vkCmdCopyBuffer(tlasBuildBuffer, instanceStagingBuffer, instanceBuffer, 1, &bufferCopy);

	uint32_t compactedBLASIndex = 0;
	for (auto& blas : uncompactedBLASes) {
		VkCopyAccelerationStructureInfoKHR copy = { .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
													.src = blas,
													.dst = m_triangleBLASes[compactedBLASIndex],
													.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR };
		vkCmdCopyAccelerationStructureKHR(tlasBuildBuffer, &copy);
		++compactedBLASIndex;
	}

	if (lightSpheres.size() > 0) {
		VkCopyAccelerationStructureInfoKHR copy = { .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
													.src = sphereBLASData.accelerationStructure,
													.dst = m_sphereBLAS,
													.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR };
		vkCmdCopyAccelerationStructureKHR(tlasBuildBuffer, &copy);
	}

	VkMemoryBarrier memoryBarrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
									  .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
									  .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR };
	vkCmdPipelineBarrier(tlasBuildBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, nullptr, 0,
						 nullptr);

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = { .primitiveCount =
															   static_cast<uint32_t>(tlasInstances.size()) };
	VkAccelerationStructureBuildRangeInfoKHR* ptrRangeInfo = &rangeInfo;

	vkCmdBuildAccelerationStructuresKHR(tlasBuildBuffer, 1, &tlasBuildInfo, &ptrRangeInfo);

	verifyResult(vkEndCommandBuffer(tlasBuildBuffer));

	m_dispatcher.submit(tlasBuildBuffer, {});
	m_dispatcher.waitForFence(tlasBuildBuffer, UINT64_MAX);

	vkDestroyBuffer(m_device.device(), triangleTransformBuffer, nullptr);
	vkDestroyBuffer(m_device.device(), triangleTransformStagingBuffer, nullptr);

	for (auto& structure : uncompactedBLASes) {
		vkDestroyAccelerationStructureKHR(m_device.device(), structure, nullptr);
	}
	for (auto& buffer : uncompactedASBackingBuffers) {
		vkDestroyBuffer(m_device.device(), buffer, nullptr);
	}
	for (auto& buffer : triangleASBuildScratchBuffers) {
		vkDestroyBuffer(m_device.device(), buffer, nullptr);
	}
	if (lightSpheres.size() > 0) {
		vkDestroyBuffer(m_device.device(), sphereAABBBuffer, nullptr);
		vkDestroyBuffer(m_device.device(), sphereAABBStagingBuffer, nullptr);
		vkDestroyAccelerationStructureKHR(m_device.device(), sphereBLASData.accelerationStructure, nullptr);
		vkDestroyBuffer(m_device.device(), sphereBLASData.backingBuffer, nullptr);
		vkDestroyBuffer(m_device.device(), sphereBLASData.scratchBuffer, nullptr);
	}

	vkDestroyQueryPool(m_device.device(), compactionSizeQueryPool, nullptr);

	vkDestroyBuffer(m_device.device(), instanceBuffer, nullptr);
	vkDestroyBuffer(m_device.device(), instanceStagingBuffer, nullptr);
	vkDestroyBuffer(m_device.device(), tlasData.scratchBuffer, nullptr);

	if (lightSpheres.size() > 0)
		vkDestroyBuffer(m_device.device(), lightDataStagingBuffer, nullptr);
}

AccelerationStructureBuilder::~AccelerationStructureBuilder() {

	vkDestroyAccelerationStructureKHR(m_device.device(), m_tlas, nullptr);
	vkDestroyBuffer(m_device.device(), m_tlasBackingBuffer, nullptr);

	if (m_sphereBLAS) {
		vkDestroyBuffer(m_device.device(), m_lightDataBuffer, nullptr);
		vkDestroyAccelerationStructureKHR(m_device.device(), m_sphereBLAS, nullptr);
		vkDestroyBuffer(m_device.device(), m_sphereASBackingBuffer, nullptr);
	}

	for (auto& structure : m_triangleBLASes) {
		vkDestroyAccelerationStructureKHR(m_device.device(), structure, nullptr);
	}
	for (auto& buffer : m_triangleASBackingBuffers) {
		vkDestroyBuffer(m_device.device(), buffer, nullptr);
	}
}

size_t AccelerationStructureBuilder::bestAccelerationStructureIndex(std::vector<AABB>& asBoundingBoxes,
																	const AABB& modelBounds,
																	const AABB& geometryBoundingBox) {
	size_t chosenIndex = -1U;
	float chosenIntersectionArea;

	float modelBoundsIntersectionArea = modelBounds.intersectionArea(geometryBoundingBox);

#ifdef AS_HEURISTIC_GEOMETRY_INTERSECTION
	chosenIntersectionArea = 0.0f;
#else
	chosenIntersectionArea = 3e38f;
	// duplicate AS bounding boxes, modify the copy to test intersection area between AABBs and preserve the
	// original
	std::vector<AABB> duplicatedASBoundingBoxes = asBoundingBoxes;
#endif

	for (size_t i = 0; i < asBoundingBoxes.size(); ++i) {
#ifdef AS_HEURISTIC_GEOMETRY_INTERSECTION
		float intersectionArea = geometryBoundingBox.intersectionArea(asBoundingBoxes[i]);

		if (intersectionArea > chosenIntersectionArea) {
			chosenIndex = i;
			chosenIntersectionArea = intersectionArea;
		}
#else
		duplicatedASBoundingBoxes[i].xmin = std::min(geometryBoundingBox.xmin, asBoundingBoxes[i].xmin);
		duplicatedASBoundingBoxes[i].ymin = std::min(geometryBoundingBox.ymin, asBoundingBoxes[i].ymin);
		duplicatedASBoundingBoxes[i].zmin = std::min(geometryBoundingBox.zmin, asBoundingBoxes[i].zmin);
		duplicatedASBoundingBoxes[i].xmax = std::max(geometryBoundingBox.xmax, asBoundingBoxes[i].xmax);
		duplicatedASBoundingBoxes[i].ymax = std::max(geometryBoundingBox.ymax, asBoundingBoxes[i].ymax);
		duplicatedASBoundingBoxes[i].zmax = std::max(geometryBoundingBox.zmax, asBoundingBoxes[i].zmax);

		float totalIntersectionArea = 0.0f;
		for (size_t i = 0; i < duplicatedASBoundingBoxes.size(); ++i) {
			for (size_t j = i + 1; j < duplicatedASBoundingBoxes.size(); ++j) {
				totalIntersectionArea += duplicatedASBoundingBoxes[j].intersectionArea(duplicatedASBoundingBoxes[i]);
			}
		}
		if (totalIntersectionArea < chosenIntersectionArea) {
			chosenIntersectionArea = totalIntersectionArea;
			chosenIndex = i;
		}
		duplicatedASBoundingBoxes = asBoundingBoxes;
#endif
	}

#ifndef AS_HEURISTIC_GEOMETRY_INTERSECTION // only useful when miminizing intersection area
	asBoundingBoxes[chosenIndex].xmin = std::min(geometryBoundingBox.xmin, asBoundingBoxes[chosenIndex].xmin);
	asBoundingBoxes[chosenIndex].ymin = std::min(geometryBoundingBox.ymin, asBoundingBoxes[chosenIndex].ymin);
	asBoundingBoxes[chosenIndex].zmin = std::min(geometryBoundingBox.zmin, asBoundingBoxes[chosenIndex].zmin);
	asBoundingBoxes[chosenIndex].xmax = std::max(geometryBoundingBox.xmax, asBoundingBoxes[chosenIndex].xmax);
	asBoundingBoxes[chosenIndex].ymax = std::max(geometryBoundingBox.ymax, asBoundingBoxes[chosenIndex].ymax);
	asBoundingBoxes[chosenIndex].zmax = std::max(geometryBoundingBox.zmax, asBoundingBoxes[chosenIndex].zmax);
#endif

	return chosenIndex;
}

AccelerationStructureData AccelerationStructureBuilder::createAccelerationStructure(
	const VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const std::vector<uint32_t>& maxPrimitiveCounts,
	uint32_t scratchBufferAlignment, bool topLevel) {
	AccelerationStructureData result = {};

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};
	vkGetAccelerationStructureBuildSizesKHR(m_device.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
											&buildInfo, maxPrimitiveCounts.data(), &sizeInfo);

	VkBufferCreateInfo accelerationStructureStorageCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeInfo.accelerationStructureSize,
		.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
	};
	verifyResult(
		vkCreateBuffer(m_device.device(), &accelerationStructureStorageCreateInfo, nullptr, &result.backingBuffer));
	m_allocator.bindDeviceBuffer(result.backingBuffer, 0);

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = result.backingBuffer,
		.size = sizeInfo.accelerationStructureSize,
		.type =
			topLevel ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
	};
	verifyResult(vkCreateAccelerationStructureKHR(m_device.device(), &accelerationStructureCreateInfo, nullptr,
												  &result.accelerationStructure));

	accelerationStructureStorageCreateInfo.size = sizeInfo.buildScratchSize;
	accelerationStructureStorageCreateInfo.usage =
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	verifyResult(
		vkCreateBuffer(m_device.device(), &accelerationStructureStorageCreateInfo, nullptr, &result.scratchBuffer));
	m_allocator.bindDeviceBuffer(result.scratchBuffer, scratchBufferAlignment);

	VkBufferDeviceAddressInfo deviceAddressInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
													.buffer = result.scratchBuffer };
	result.scratchBufferDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &deviceAddressInfo);

	return result;
}

AccelerationStructureData AccelerationStructureBuilder::createAccelerationStructure(uint32_t compactedSize) {
	AccelerationStructureData result = {};
	VkBufferCreateInfo accelerationStructureStorageCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = compactedSize,
		.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
	};
	verifyResult(
		vkCreateBuffer(m_device.device(), &accelerationStructureStorageCreateInfo, nullptr, &result.backingBuffer));
	m_allocator.bindDeviceBuffer(result.backingBuffer, 0);

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = result.backingBuffer,
		.size = compactedSize,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
	};
	verifyResult(vkCreateAccelerationStructureKHR(m_device.device(), &accelerationStructureCreateInfo, nullptr,
												  &result.accelerationStructure));
	VkAccelerationStructureDeviceAddressInfoKHR deviceAddressInfo = {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		.accelerationStructure = result.accelerationStructure
	};
	result.accelerationStructureDeviceAddress =
		vkGetAccelerationStructureDeviceAddressKHR(m_device.device(), &deviceAddressInfo);
	return result;
}
