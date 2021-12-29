#pragma once

#include <RayTracingDevice.hpp>
#include <util/MemoryAllocator.hpp>
#include <util/ModelLoader.hpp>

struct Sphere {
	float position[3];
	float radius;
	float color[4]; //r, g, b, a = intensity scale
};

struct AccelerationStructureData {
	VkAccelerationStructureKHR accelerationStructure;
	VkDeviceAddress accelerationStructureDeviceAddress;
	VkBuffer backingBuffer;
	VkBuffer scratchBuffer;
	VkDeviceAddress scratchBufferDeviceAddress;
};

class AccelerationStructureBuilder {
  public:
	AccelerationStructureBuilder(RayTracingDevice& device, MemoryAllocator& memoryAllocator, OneTimeDispatcher& dispatcher, ModelLoader& modelLoader,
								 const std::vector<Sphere> lightSpheres, uint32_t triangleSBTIndex, uint32_t lightSphereSBTIndex);
	~AccelerationStructureBuilder();

	VkBuffer lightDataBuffer() const { return m_lightDataBuffer; }

	VkAccelerationStructureKHR tlas() const { return m_tlas; }

  private:
	size_t bestAccelerationStructureIndex(std::vector<AABB>& asBoundingBoxes, const AABB& modelBounds,
										  const AABB& geometryBoundingBox);
	AccelerationStructureData createAccelerationStructure(
		const VkAccelerationStructureBuildGeometryInfoKHR& buildInfos,
														  const std::vector<uint32_t>& maxPrimitiveCounts,
														  uint32_t scratchBufferAlignment);
	AccelerationStructureData createAccelerationStructure(uint32_t compactedSize);

	RayTracingDevice& m_device;
	MemoryAllocator& m_allocator;
	OneTimeDispatcher& m_dispatcher;

	VkBuffer m_lightDataBuffer;
	VkBuffer m_lightDataStagingBuffer;

	VkAccelerationStructureKHR m_tlas;
	VkBuffer m_tlasBackingBuffer;

	std::vector<VkAccelerationStructureKHR> m_triangleBLASes;
	std::vector<VkDeviceAddress> m_blasDeviceAddresses;
	std::vector<VkBuffer> m_triangleASBackingBuffers;

	VkAccelerationStructureKHR m_sphereBLAS = VK_NULL_HANDLE;
	VkBuffer m_sphereASBackingBuffer;
};