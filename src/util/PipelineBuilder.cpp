#include <ShaderModuleHelper.hpp>
#include <util/PipelineBuilder.hpp>
#include <volk.h>

PipelineBuilder::PipelineBuilder(RayTracingDevice& device, uint32_t maxRayRecursionDepth) : m_device(device) {
	VkDescriptorSetLayoutBinding bindings[5] = { { .binding = 0,
												   .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
												   .descriptorCount = 1,
												   .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
																 VK_SHADER_STAGE_RAYGEN_BIT_KHR },
												 { .binding = 1,
												   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
												   .descriptorCount = 1,
												   .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
												 { .binding = 2,
												   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
												   .descriptorCount = 1,
												   .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
												 { .binding = 3,
												   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
												   .descriptorCount = 1,
												   .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
												 { .binding = 4,
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
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 5, .pBindings = bindings
	};

	verifyResult(vkCreateDescriptorSetLayout(m_device.device(), &descriptorSetLayoutCreateInfo, nullptr,
											 &m_descriptorSetLayout));

	descriptorSetLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
									  .bindingCount = 3,
									  .pBindings = geometryBindings };

	verifyResult(vkCreateDescriptorSetLayout(m_device.device(), &descriptorSetLayoutCreateInfo, nullptr,
											 &m_geometryDescriptorSetLayout));

	VkPushConstantRange constantRange = { .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
										  .offset = 0,
										  .size = sizeof(PushConstantData) };

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

	std::memcpy(m_poolSizes, poolSizes, 3 * sizeof(VkDescriptorPoolSize));

	m_poolCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
						 .maxSets = frameInFlightCount + 1,
						 .poolSizeCount = 3,
						 .pPoolSizes = poolSizes };
}

PipelineBuilder::~PipelineBuilder() {
	vkDestroyPipeline(m_device.device(), m_pipeline, nullptr);
	vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device.device(), m_descriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device.device(), m_geometryDescriptorSetLayout, nullptr);
}

VkStridedDeviceAddressRegionKHR PipelineBuilder::hitDeviceAddressRegion(VkDeviceAddress bufferAddress) const {
	return { .deviceAddress = bufferAddress,
			 .stride = m_shaderGroupHandleSizeAligned,
			 .size = m_shaderGroupHandleSizeAligned };
}

VkStridedDeviceAddressRegionKHR PipelineBuilder::missDeviceAddressRegion(VkDeviceAddress bufferAddress) const {
	return { .deviceAddress = bufferAddress,
			 .stride = m_shaderGroupHandleSizeAligned,
			 .size = m_shaderGroupHandleSizeAligned };
}

VkStridedDeviceAddressRegionKHR PipelineBuilder::raygenDeviceAddressRegion(VkDeviceAddress bufferAddress) const {
	return { .deviceAddress = bufferAddress,
			 .stride = m_shaderGroupHandleSizeAligned,
			 .size = m_shaderGroupHandleSizeAligned };
}
