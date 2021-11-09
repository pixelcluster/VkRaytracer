#pragma once

#include <RayTracingDevice.hpp>

struct Sphere {
	float position[3];
	float radius;
};

class HardwareSphereRaytracer {
  public:
	HardwareSphereRaytracer(size_t windowWidth, size_t windowHeight, size_t sphereCount);
	~HardwareSphereRaytracer();
	// RayTracingDevice already deletes/defaults copy/move constructors

	bool update();

	void buildAccelerationStructures(const std::vector<Sphere>& spheres);

  private:
	void addDataBuffer(const VkBufferCreateInfo& info, VkMemoryPropertyFlags additionalRequiredFlags,
					   VkMemoryPropertyFlags preferredFlags, BufferAllocation& targetAllocation,
					   BufferAllocationRequirements& targetRequirements, VkDeviceSize& mergedMemorySize);

	void bindDataBuffer(BufferAllocation& allocation, VkDeviceMemory memory, BufferAllocationRequirements& requirements,
						VkDeviceSize& mergedMemoryOffset);

	VkAccelerationStructureBuildSizesInfoKHR blasSize(size_t sphereCount);
	VkAccelerationStructureBuildSizesInfoKHR tlasSize(size_t sphereCount);

	size_t prevSphereCount = 0;

	VkDeviceMemory m_deviceMemory;

	BufferAllocation m_accelerationStructureBuffer;
	BufferAllocation m_accelerationStructureScratchBuffer;
	BufferAllocation m_accelerationStructureDataBuffer;
	BufferAllocation m_sphereDataBuffer;

	BufferAllocation m_stagingBuffer;

	VkCommandPool m_buildCommandPool;
	VkCommandBuffer m_buildCommandBuffer;

	VkFence m_buildSyncFence;

	VkAccelerationStructureKHR m_accelerationStructure;

	RayTracingDevice m_device;
};