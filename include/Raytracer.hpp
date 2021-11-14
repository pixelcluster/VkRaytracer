#pragma once

#include <AccelerationStructureManager.hpp>
#include <BufferHelper.h>
#include <RayTracingDevice.hpp>

struct Sphere {
	float position[4];
	float radius;
	float color[4];
};

class HardwareSphereRaytracer {
  public:
	HardwareSphereRaytracer(size_t windowWidth, size_t windowHeight, size_t sphereCount);
	~HardwareSphereRaytracer();
	// RayTracingDevice already deletes/defaults copy/move constructors

	bool update(const std::vector<Sphere>& spheres);

  private:
	VkAccelerationStructureBuildGeometryInfoKHR constructBLASGeometryInfo(
		VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureGeometryKHR& targetGeometry);
	VkAccelerationStructureBuildGeometryInfoKHR constructPlaneBLASGeometryInfo(
		VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureGeometryKHR& targetGeometry);
	VkAccelerationStructureBuildGeometryInfoKHR constructTLASGeometryInfo(
		uint32_t frameIndex, VkBuildAccelerationStructureModeKHR mode,
		VkAccelerationStructureGeometryKHR& targetGeometry);

	void setGeometryBLASBatchNames();
	void setGeometryTLASBatchNames(size_t frameIndex);

	static constexpr size_t m_sphereBLASIndex = 0;
	static constexpr size_t m_triangleBLASIndex = 1;

	static constexpr size_t m_triangleObjectCount = 1;
	static constexpr size_t m_triangleUniqueVertexCount = 4;
	static constexpr size_t m_triangleUniqueNormalCount = 2;
	static constexpr size_t m_triangleUniqueIndexCount = 6;
	static constexpr size_t m_triangleTransformCount = 0;

	static constexpr size_t m_vertexDataSize = sizeof(float) * 3 * m_triangleUniqueVertexCount;
	static constexpr size_t m_indexDataSize = sizeof(uint16_t) * m_triangleUniqueIndexCount;
	static constexpr size_t m_transformDataSize = sizeof(VkTransformMatrixKHR) * m_triangleTransformCount;
	static constexpr size_t m_normalDataSize = sizeof(float) * 4 * m_triangleTransformCount;

	struct PushConstantData {
		float worldOffset[3];
		float aspectRatio;
		float tanHalfFov;
	};

	AccelerationStructureBatchData m_blasStructureData;
	AccelerationStructureBatchData m_tlasStructureData[frameInFlightCount];

	VkDeviceMemory m_deviceMemory;
	BufferAllocation m_accelerationStructureDataBuffer;
	BufferAllocation m_objectDataBuffer;
	BufferAllocation m_shaderBindingTableBuffer;

	VkDeviceSize m_accelerationStructureFrameDataSize;
	VkDeviceSize m_objectFrameDataSize;

	VkDeviceAddress m_accelerationStructureDataDeviceAddress;

	VkStridedDeviceAddressRegionKHR m_sphereRaygenShaderBindingTable;
	VkStridedDeviceAddressRegionKHR m_sphereHitShaderBindingTable;
	VkStridedDeviceAddressRegionKHR m_sphereMissShaderBindingTable;

	VkPipeline m_pipeline;
	VkPipelineLayout m_pipelineLayout;

	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	VkDescriptorSet m_descriptorSets[frameInFlightCount];

	VkSampler m_imageSampler;

	BufferAllocation m_stagingBuffer;
	void* m_mappedStagingBuffer;
	VkDeviceSize m_stagingFrameDataSize;

	VkCommandPool m_oneTimeSubmitPool;
	VkFence m_oneTimeSubmitFence;

	float m_worldPos[3] = { 0.0f, 0.0f, -5.0f };
	double m_lastTime = 0.0f;

	RayTracingDevice m_device;
};