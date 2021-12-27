#pragma once

#include <RayTracingDevice.hpp>

struct PushConstantData {
	float worldOffset[3];
	float aspectRatio;
	float tanHalfFov;
	float time;
	uint32_t accumulatedSampleCount;
};

class PipelineBuilder {
  public:
	PipelineBuilder(RayTracingDevice& device, uint32_t maxRayRecursionDepth);
	//copy/move implicitly deleted by reference to RayTracingDevice
	~PipelineBuilder();

	VkDescriptorSetLayout descriptorSetLayout() const { return m_descriptorSetLayout; }
	VkDescriptorSetLayout geometryDataDescriptorSetLayout() const { return m_geometryDescriptorSetLayout; }

	const VkDescriptorPoolCreateInfo& poolCreateInfo() const { return m_poolCreateInfo; }

	VkStridedDeviceAddressRegionKHR hitDeviceAddressRegion(VkDeviceAddress bufferAddress) const;
	VkStridedDeviceAddressRegionKHR missDeviceAddressRegion(VkDeviceAddress bufferAddress) const;
	VkStridedDeviceAddressRegionKHR raygenDeviceAddressRegion(VkDeviceAddress bufferAddress) const;

  private:
	RayTracingDevice& m_device;

	VkPipeline m_pipeline;
	VkPipelineLayout m_pipelineLayout;

	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorSetLayout m_geometryDescriptorSetLayout;

	VkDescriptorPoolSize m_poolSizes[3];
	VkDescriptorPoolCreateInfo m_poolCreateInfo;

	VkDeviceSize m_shaderGroupHandleSizeAligned;
};