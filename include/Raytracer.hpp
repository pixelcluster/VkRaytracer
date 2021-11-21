#pragma once

#include <AccelerationStructureManager.hpp>
#include <BufferHelper.h>
#include <RayTracingDevice.hpp>

struct Sphere {
	float position[4];
	float radius;
	float color[4];
};

struct TriangleObject {
	size_t vertexCount;
	size_t indexCount;
	size_t transformCount;
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

	constexpr size_t vertexDataSize();
	constexpr size_t indexDataSize();
	constexpr size_t transformDataSize();
	constexpr size_t normalDataSize();

	static constexpr size_t m_sphereBLASIndex = 0;
	static constexpr size_t m_planeBLASIndex = 1;
	static constexpr size_t m_blasCount = 2;

	static constexpr size_t m_triangleObjectCount = 1;

	static constexpr TriangleObject objects[m_triangleObjectCount]{
		{ .vertexCount = 4, .indexCount = 6, .transformCount = 0 }
	};

	static constexpr size_t m_triangleUniqueVertexCount = 4;
	static constexpr size_t m_triangleUniqueNormalCount = 2;
	static constexpr size_t m_triangleUniqueIndexCount = 6;
	static constexpr size_t m_triangleTransformCount = 0;

	struct PushConstantData {
		float worldOffset[3];
		float aspectRatio;
		float tanHalfFov;
	};

	AccelerationStructureBatchData m_blasStructureData;
	AccelerationStructureBatchData m_tlasStructureData[frameInFlightCount];

	VkDeviceMemory m_deviceMemory;

	BufferAllocation m_accelerationStructureDataBuffer;
	BufferSubAllocation m_instanceDataStorage[frameInFlightCount];
	BufferSubAllocation m_blasStructureStorage[m_blasCount];

	BufferAllocation m_objectDataBuffer;
	BufferSubAllocation m_colorDataStorage[frameInFlightCount];
	BufferSubAllocation m_vertexDataStorage;

	BufferAllocation m_shaderBindingTableBuffer;

	VkDeviceAddress m_accelerationStructureDataDeviceAddress;

	VkStridedDeviceAddressRegionKHR m_raygenShaderBindingTable;
	VkStridedDeviceAddressRegionKHR m_sphereHitShaderBindingTable;
	VkStridedDeviceAddressRegionKHR m_triangleHitShaderBindingTable;
	VkStridedDeviceAddressRegionKHR m_missShaderBindingTable;

	BufferAllocation m_stagingBuffer;
	void* m_mappedStagingBuffer;
	VkDeviceSize m_stagingFrameDataSize;

	BufferSubAllocation m_aabbStagingStorage;
	BufferSubAllocation m_vertexStagingStorage;
	BufferSubAllocation m_indexStagingStorage;

	VkPipeline m_pipeline;
	VkPipelineLayout m_pipelineLayout;

	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	VkDescriptorSet m_descriptorSets[frameInFlightCount];

	VkSampler m_imageSampler;

	VkCommandPool m_oneTimeSubmitPool;
	VkFence m_oneTimeSubmitFence;

	float m_worldPos[3] = { 0.0f, 2.0f, -5.0f };
	double m_lastTime = 0.0f;

	RayTracingDevice m_device;
};