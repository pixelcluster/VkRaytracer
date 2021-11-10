#pragma once

#include <RayTracingDevice.hpp>
#include <AccelerationStructureManager.hpp>

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
	static constexpr size_t m_blasIndex = 0;
	static constexpr size_t m_tlasIndex = 1;

	bool m_hasBuiltAccelerationStructure = false;

	AccelerationStructureBatchData m_structureData;

	VkDeviceMemory m_deviceMemory;
	BufferAllocation m_accelerationStructureDataBuffer;
	BufferAllocation m_sphereDataBuffer;

	VkDeviceAddress m_accelerationStructureDataDeviceAddress;
	VkDeviceAddress m_scratchBufferBaseDeviceAddress;

	BufferAllocation m_stagingBuffer;
	void* m_mappedStagingBuffer;

	RayTracingDevice m_device;
};