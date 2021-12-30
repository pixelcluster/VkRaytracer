#pragma once

#include <RayTracingDevice.hpp>
#include <util/MemoryAllocator.hpp>
#include <util/OneTimeDispatcher.hpp>

struct PushConstantData {
	float worldOffset[4];
	float worldDirection[4];
	float worldRight[4];
	float worldUp[4];
	float aspectRatio;
	float tanHalfFov;
	float time;
	uint32_t accumulatedSampleCount;
};

class PipelineBuilder {
  public:
	PipelineBuilder(RayTracingDevice& device, MemoryAllocator& allocator, OneTimeDispatcher& oneTimeDispatcher, VkDescriptorSetLayout textureSetLayout, uint32_t maxRayRecursionDepth);
	//copy/move implicitly deleted by reference to RayTracingDevice
	~PipelineBuilder();

	VkDescriptorSetLayout descriptorSetLayout() const { return m_imageDescriptorSetLayout; }
	VkDescriptorSetLayout geometryDataDescriptorSetLayout() const { return m_generalDescriptorSetLayout; }

	VkDescriptorSet imageSet(size_t frameIndex) const { return m_imageDescriptorSets[frameIndex]; }
	VkDescriptorSet generalSet() const { return m_generalDescriptorSet; }

	VkPipeline pipeline() const { return m_pipeline; }
	VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }

	VkStridedDeviceAddressRegionKHR hitDeviceAddressRegion() const;
	VkStridedDeviceAddressRegionKHR missDeviceAddressRegion() const;
	VkStridedDeviceAddressRegionKHR raygenDeviceAddressRegion() const;

  private:
	RayTracingDevice& m_device;

	VkPipeline m_pipeline;
	VkPipelineLayout m_pipelineLayout;

	VkBuffer m_sbtBuffer;
	VkDeviceAddress m_sbtBufferDeviceAddress;

	VkDescriptorSetLayout m_imageDescriptorSetLayout;
	VkDescriptorSetLayout m_generalDescriptorSetLayout;
	
	VkDescriptorPool m_descriptorPool;
	VkDescriptorSet m_imageDescriptorSets[frameInFlightCount];
	VkDescriptorSet m_generalDescriptorSet;

	VkDeviceSize m_shaderGroupHandleSizeAligned;
};