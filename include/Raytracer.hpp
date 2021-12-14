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
	HardwareSphereRaytracer(size_t windowWidth, size_t windowHeight, size_t sphereCount, std::vector<size_t> emissiveSphereIndices);
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

	void recreateAccumulationImage();

	constexpr size_t vertexDataSize();
	constexpr size_t indexDataSize();
	constexpr size_t transformDataSize();
	constexpr size_t normalDataSize();

	constexpr size_t uniqueVertexCount();
	constexpr size_t indexCount();
	constexpr size_t transformCount();
	constexpr size_t normalCount();

	static constexpr size_t m_sphereBLASIndex = 0;
	static constexpr size_t m_planeBLASIndex = 1;
	static constexpr size_t m_blasCount = 2;

	static constexpr size_t m_triangleObjectCount = 1;

	static constexpr size_t m_stagingSBTCount = 4;
	static constexpr size_t m_sbtCount = 6;

	static constexpr TriangleObject m_triangleObjects[m_triangleObjectCount]{
		{ .vertexCount = 4, .indexCount = 6, .transformCount = 0 }
	};

	struct PushConstantData {
		float worldOffset[3];
		float aspectRatio;
		float tanHalfFov;
		float time;
		uint32_t accumulatedSampleCount;
	};

	struct PerObjectData {
		float color[4];
	};

	struct PerVertexData {
		float position[4];
	};

	struct LightData {
		float position[4];
		float radius;
		float padding[3];
	};

	std::vector<size_t> m_emissiveSphereIndices;

	AccelerationStructureBatchData m_blasStructureData;
	AccelerationStructureBatchData m_tlasStructureData[frameInFlightCount];

	VkDeviceMemory m_deviceMemory;

	BufferAllocation m_accelerationStructureDataBuffer;
	BufferSubAllocation m_instanceDataStorage[frameInFlightCount];
	BufferSubAllocation m_aabbDataStorage;
	BufferSubAllocation m_vertexDataStorage;
	BufferSubAllocation m_indexDataStorage;
	BufferSubAllocation m_transformDataStorage;

	BufferAllocation m_objectDataBuffer;
	BufferSubAllocation m_objectDataStorage[frameInFlightCount];
	BufferSubAllocation m_lightDataStorage[frameInFlightCount];
	BufferSubAllocation m_normalDataStorage;

	BufferAllocation m_shaderBindingTableBuffer;

	VkStridedDeviceAddressRegionKHR m_raygenShaderBindingTable;
	VkStridedDeviceAddressRegionKHR m_sphereHitShaderBindingTable;
	VkStridedDeviceAddressRegionKHR m_triangleHitShaderBindingTable;
	VkStridedDeviceAddressRegionKHR m_missShaderBindingTable;

	BufferAllocation m_stagingBuffer;
	void* m_mappedStagingBuffer;

	BufferSubAllocation m_objectDataStagingStorage[frameInFlightCount];
	BufferSubAllocation m_objectInstanceStagingStorage[frameInFlightCount];
	BufferSubAllocation m_lightStagingStorage[frameInFlightCount];

	BufferSubAllocation m_aabbStagingStorage;
	BufferSubAllocation m_vertexStagingStorage;
	BufferSubAllocation m_indexStagingStorage;
	BufferSubAllocation m_transformStagingStorage;
	BufferSubAllocation m_normalStagingStorage;
	BufferSubAllocation m_sbtStagingStorage;

	VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
	VkImage m_accumulationImage = VK_NULL_HANDLE;
	VkImageView m_accumulationImageView = VK_NULL_HANDLE;
	VkExtent3D m_accumulationImageExtent;

	VkPipeline m_pipeline;
	VkPipelineLayout m_pipelineLayout;

	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	VkDescriptorSet m_descriptorSets[frameInFlightCount];
	VkDescriptorSetLayout m_geometryDescriptorSetLayout;
	VkDescriptorSet m_geometryDescriptorSet;

	VkSampler m_imageSampler;

	VkCommandPool m_oneTimeSubmitPool;
	VkFence m_oneTimeSubmitFence;

	float m_worldPos[3] = { 0.0f, 2.0f, -5.0f };
	double m_lastTime = 0.0f;
	uint32_t m_accumulatedSampleCount = 0;

	RayTracingDevice m_device;
};