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
	//RayTracingDevice already deletes/defaults copy/move constructors

	bool update();

	void buildAccelerationStructures(const std::vector<Sphere> spheres);

  private:
	size_t prevSphereCount = 0;

	VkDeviceMemory m_accelerationStructureMemory;
	VkBuffer m_accelerationStructureBuffer;
	VkBuffer m_scratchBuffer;

	VkDeviceMemory m_dataStagingMemory;
	VkBuffer m_stagingBuffer;

	VkCommandPool m_buildCommandPool;
	VkCommandBuffer m_buildCommandBuffer;

	VkFence m_buildSyncFence;

	VkAccelerationStructureKHR m_accelerationStructure;

	RayTracingDevice m_device;
};