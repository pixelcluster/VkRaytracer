#include <ShaderModuleHelper.hpp>
#include <util/PipelineBuilder.hpp>
#include <volk.h>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

PipelineBuilder::PipelineBuilder(RayTracingDevice& device, MemoryAllocator& allocator,
								 OneTimeDispatcher& oneTimeDispatcher, VkDescriptorSetLayout textureSetLayout,
								 uint32_t maxRayRecursionDepth)
	: m_device(device) {
	VkDescriptorSetLayoutBinding imageBindings[2] = { { .binding = 0,
														.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
														.descriptorCount = 1,
														.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
													  { .binding = 1,
														.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
														.descriptorCount = 1,
														.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR } };
	VkDescriptorSetLayoutBinding geometryBindings[9] = {
		{ .binding = 0,
		  .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		  .descriptorCount = 1,
		  .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR },
		{ .binding = 1, // geometry index buffer
		  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		  .descriptorCount = 1,
		  .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR },
		{ .binding = 2, // geometry data buffer
		  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		  .descriptorCount = 1,
		  .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR },
		{ .binding = 3, // material data buffer
		  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		  .descriptorCount = 1,
		  .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR },
		{ .binding = 4, // index buffer
		  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		  .descriptorCount = 1,
		  .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR },
		{ .binding = 5, // normal buffer
		  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		  .descriptorCount = 1,
		  .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR },
		{ .binding = 6, // tangent buffer
		  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		  .descriptorCount = 1,
		  .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR },
		{ .binding = 7, // texcoord buffer
		  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		  .descriptorCount = 1,
		  .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR },
		{ .binding = 8, // sphere data buffer
		  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		  .descriptorCount = 1,
		  .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR }
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 2, .pBindings = imageBindings
	};

	verifyResult(vkCreateDescriptorSetLayout(m_device.device(), &descriptorSetLayoutCreateInfo, nullptr,
											 &m_imageDescriptorSetLayout));

	descriptorSetLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
									  .bindingCount = 9,
									  .pBindings = geometryBindings };

	verifyResult(vkCreateDescriptorSetLayout(m_device.device(), &descriptorSetLayoutCreateInfo, nullptr,
											 &m_generalDescriptorSetLayout));

	VkPushConstantRange constantRange = { .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
										  .offset = 0,
										  .size = sizeof(PushConstantData) };

	VkDescriptorSetLayout layouts[3] = { m_imageDescriptorSetLayout, m_generalDescriptorSetLayout, textureSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
															.setLayoutCount = 3,
															.pSetLayouts = layouts,
															.pushConstantRangeCount = 1,
															.pPushConstantRanges = &constantRange };

	verifyResult(vkCreatePipelineLayout(m_device.device(), &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));

	std::string executableFileName;

	#ifdef _WIN32
	executableFileName.resize(MAX_PATH);
	GetModuleFileNameA(NULL, executableFileName.data(), MAX_PATH);
	#elif defined(__linux__)
	constexpr size_t maxPath = 1024;
	executableFileName.resize(maxPath);
	readlink("/proc/self/exe", executableFileName.data(), maxPath);
	#endif

	std::filesystem::path executablePath = std::filesystem::path(executableFileName);

	VkShaderModule raygenShaderModule = createShaderModule(m_device.device(), executablePath.parent_path().string() + "/shaders/raytrace-rgen.spv");
	VkShaderModule closestHitShaderModule = createShaderModule(m_device.device(), executablePath.parent_path().string() + "/shaders/sphere-rchit.spv");
	VkShaderModule triangleHitShaderModule = createShaderModule(m_device.device(), executablePath.parent_path().string() + "/shaders/triangle-rchit.spv");
	VkShaderModule anyHitShaderModule = createShaderModule(m_device.device(), executablePath.parent_path().string() + "/shaders/raytrace-rahit.spv");
	VkShaderModule missShaderModule = createShaderModule(m_device.device(), executablePath.parent_path().string() + "/shaders/raytrace-rmiss.spv");
	VkShaderModule intersectionShaderModule = createShaderModule(m_device.device(), executablePath.parent_path().string() + "/shaders/raytrace-rint.spv");

	VkRayTracingShaderGroupCreateInfoKHR shaderGroupCreateInfos[4] = {
		{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		  .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
		  .generalShader = 0,
		  .closestHitShader = VK_SHADER_UNUSED_KHR,
		  .anyHitShader = VK_SHADER_UNUSED_KHR,
		  .intersectionShader = VK_SHADER_UNUSED_KHR },
		{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		  .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
		  .generalShader = VK_SHADER_UNUSED_KHR,
		  .closestHitShader = 1,
		  .anyHitShader = 2,
		  .intersectionShader = VK_SHADER_UNUSED_KHR },
		{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		  .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
		  .generalShader = VK_SHADER_UNUSED_KHR,
		  .closestHitShader = 3,
		  .anyHitShader = VK_SHADER_UNUSED_KHR,
		  .intersectionShader = 4 },
		{ .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		  .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
		  .generalShader = 5,
		  .closestHitShader = VK_SHADER_UNUSED_KHR,
		  .anyHitShader = VK_SHADER_UNUSED_KHR,
		  .intersectionShader = VK_SHADER_UNUSED_KHR }
	};

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[6] = {
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		  .module = raygenShaderModule,
		  .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		  .module = triangleHitShaderModule,
		  .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
		  .module = anyHitShaderModule,
		  .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		  .module = closestHitShaderModule,
		  .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
		  .module = intersectionShaderModule,
		  .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
		  .module = missShaderModule,
		  .pName = "main" }
	};

	VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = { .sType =
																 VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
															 .stageCount = 6,
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

	m_shaderGroupHandleSizeAligned = pipelineProperties.shaderGroupHandleSize;

	if (shaderGroupHandleSizeAlignmentRemainder) {
		m_shaderGroupHandleSizeAligned +=
			pipelineProperties.shaderGroupBaseAlignment - shaderGroupHandleSizeAlignmentRemainder;
	}

	// Create descriptor pools and sets

	VkDescriptorPoolSize poolSizes[3] = {
		{ .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 1 },
		{ .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = frameInFlightCount * 2 },
		{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 7 }
	};

	VkDescriptorPoolCreateInfo poolCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
												  .maxSets = frameInFlightCount + 1,
												  .poolSizeCount = 3,
												  .pPoolSizes = poolSizes };

	verifyResult(vkCreateDescriptorPool(m_device.device(), &poolCreateInfo, nullptr, &m_descriptorPool));

	VkDescriptorSetLayout imageLayouts[frameInFlightCount];
	for (size_t i = 0; i < frameInFlightCount; ++i) {
		imageLayouts[i] = m_imageDescriptorSetLayout;
	}

	VkDescriptorSetAllocateInfo imageSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
														 .descriptorPool = m_descriptorPool,
														 .descriptorSetCount = frameInFlightCount,
														 .pSetLayouts = imageLayouts };
	verifyResult(vkAllocateDescriptorSets(m_device.device(), &imageSetAllocateInfo, m_imageDescriptorSets));
	VkDescriptorSetAllocateInfo setAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
													.descriptorPool = m_descriptorPool,
													.descriptorSetCount = 1,
													.pSetLayouts = &m_generalDescriptorSetLayout };
	verifyResult(vkAllocateDescriptorSets(m_device.device(), &setAllocateInfo, &m_generalDescriptorSet));

	VkBufferCreateInfo sbtBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
											   .size = m_shaderGroupHandleSizeAligned * 4,
											   .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
														VK_BUFFER_USAGE_TRANSFER_DST_BIT |
														VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT };
	verifyResult(vkCreateBuffer(m_device.device(), &sbtBufferCreateInfo, nullptr, &m_sbtBuffer));
	allocator.bindDeviceBuffer(m_sbtBuffer, 0);

	VkBuffer sbtStagingBuffer;
	sbtBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	verifyResult(vkCreateBuffer(m_device.device(), &sbtBufferCreateInfo, nullptr, &sbtStagingBuffer));
	void* mappedSBTStagingBuffer = allocator.bindStagingBuffer(sbtStagingBuffer, 0);

	VkBufferDeviceAddressInfo sbtDeviceAddressInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
													   .buffer = m_sbtBuffer };
	m_sbtBufferDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &sbtDeviceAddressInfo);

	void* shaderGroupDataBegin = malloc(shaderGroupHandleSize * 4);
	vkGetRayTracingShaderGroupHandlesKHR(m_device.device(), m_pipeline, 0, 4, shaderGroupHandleSize * 4,
										 shaderGroupDataBegin);

	void* currentShaderGroup = shaderGroupDataBegin;
	void* currentSBTEntry = mappedSBTStagingBuffer;

	for (size_t i = 0; i < 4; ++i) {
		std::memcpy(currentSBTEntry, currentShaderGroup, shaderGroupHandleSize);

		currentShaderGroup = reinterpret_cast<uint8_t*>(currentShaderGroup) + shaderGroupHandleSize;
		currentSBTEntry = reinterpret_cast<uint8_t*>(currentSBTEntry) + m_shaderGroupHandleSizeAligned;
	}

	free(shaderGroupDataBegin);

	VkCommandBuffer sbtTransferBuffer = oneTimeDispatcher.allocateOneTimeSubmitBuffers(1)[0];
	VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
										   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	verifyResult(vkBeginCommandBuffer(sbtTransferBuffer, &beginInfo));

	VkBufferCopy sbtCopy = { .size = m_shaderGroupHandleSizeAligned * 4 };
	vkCmdCopyBuffer(sbtTransferBuffer, sbtStagingBuffer, m_sbtBuffer, 1, &sbtCopy);

	verifyResult(vkEndCommandBuffer(sbtTransferBuffer));

	oneTimeDispatcher.submit(sbtTransferBuffer, {});
	oneTimeDispatcher.waitForFence(sbtTransferBuffer, UINT64_MAX);

	vkDestroyBuffer(m_device.device(), sbtStagingBuffer, nullptr);
}

PipelineBuilder::~PipelineBuilder() {
	vkDestroyPipeline(m_device.device(), m_pipeline, nullptr);
	vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device.device(), m_imageDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device.device(), m_generalDescriptorSetLayout, nullptr);

	vkDestroyDescriptorPool(m_device.device(), m_descriptorPool, nullptr);

	vkDestroyBuffer(m_device.device(), m_sbtBuffer, nullptr);
}

VkStridedDeviceAddressRegionKHR PipelineBuilder::hitDeviceAddressRegion() const {
	return { .deviceAddress = m_sbtBufferDeviceAddress + m_shaderGroupHandleSizeAligned,
			 .stride = m_shaderGroupHandleSizeAligned,
			 .size = m_shaderGroupHandleSizeAligned };
}

VkStridedDeviceAddressRegionKHR PipelineBuilder::missDeviceAddressRegion() const {
	return { .deviceAddress = m_sbtBufferDeviceAddress + m_shaderGroupHandleSizeAligned * 3,
			 .stride = m_shaderGroupHandleSizeAligned,
			 .size = m_shaderGroupHandleSizeAligned };
}

VkStridedDeviceAddressRegionKHR PipelineBuilder::raygenDeviceAddressRegion() const {
	return { .deviceAddress = m_sbtBufferDeviceAddress,
			 .stride = m_shaderGroupHandleSizeAligned,
			 .size = m_shaderGroupHandleSizeAligned };
}
