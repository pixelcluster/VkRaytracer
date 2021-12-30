#pragma once

#include <util/AccelerationStructureBuilder.hpp>
#include <util/PipelineBuilder.hpp>
#include <numbers>

class TriangleMeshRaytracer {
  public:
	TriangleMeshRaytracer(RayTracingDevice& device, MemoryAllocator& allocator, ModelLoader& loader, PipelineBuilder& pipelineBuilder,
						  AccelerationStructureBuilder& accelerationStructureBuilder);
	~TriangleMeshRaytracer();

	bool update();

  private:
	void recreateAccumulationImage();
	void resetSampleCount();

	RayTracingDevice& m_device;
	MemoryAllocator& m_allocator;
	ModelLoader& m_modelLoader;
	PipelineBuilder& m_pipelineBuilder;
	AccelerationStructureBuilder& m_accelerationStructureBuilder;

	VkImage m_accumulationImage = VK_NULL_HANDLE;
	VkImageView m_accumulationImageView = VK_NULL_HANDLE;
	VkExtent3D m_accumulationImageExtent;
	ImageAllocation m_accumulationImageAllocation = {};

	float m_worldPos[3] = { 2.0f, -2.0f, 0.0f };
	float m_worldDirection[3] = { 1.0f, 0.0f, 0.0f };
	float m_worldRight[3] = { 0.0f, 0.0f, 1.0f };

	float m_cameraPhi = 0.0f;
	float m_cameraTheta = std::numbers::pi;

	double m_lastTime = 0.0f;
	uint32_t m_accumulatedSampleCount = 0;
	uint32_t m_maxSamples = 1024;
};