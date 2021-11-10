#pragma once

#include <RayTracingDevice.hpp>

struct AccelerationStructureData {
	VkAccelerationStructureKHR structure;
	VkDeviceSize structureBufferOffset;
	VkDeviceSize scratchBufferBaseOffset;
	VkDeviceSize accelerationStructureSize;
};

struct AccelerationStructureBatchData {
	BufferAllocation structureBuffer;
	BufferAllocation scratchBuffer; //contains mirrors for each frame in flight
	VkDeviceMemory sharedStructureMemory;
	std::vector<AccelerationStructureData> structures;
};

struct AccelerationStructureInitInfo {
	std::vector<VkAccelerationStructureGeometryKHR> geometries;
	uint32_t maxPrimitiveCount;
	VkAccelerationStructureTypeKHR type;
};

class AccelerationStructureManager {
  public:
	AccelerationStructureManager() {}
	AccelerationStructureManager(const AccelerationStructureManager&) = delete;
	AccelerationStructureManager& operator=(const AccelerationStructureManager&) = delete;
	AccelerationStructureManager(AccelerationStructureManager&&) noexcept = default;
	AccelerationStructureManager& operator=(AccelerationStructureManager&&) noexcept = default;

	static AccelerationStructureBatchData createData(RayTracingDevice& device, const std::vector<AccelerationStructureInitInfo>& initInfos);

  private:
	static void addDataBuffer(RayTracingDevice& device, const VkBufferCreateInfo& info,
					   VkMemoryPropertyFlags additionalRequiredFlags,
					   VkMemoryPropertyFlags preferredFlags, BufferAllocation& targetAllocation,
					   BufferAllocationRequirements& targetRequirements, VkDeviceSize& mergedMemorySize);

	static void bindDataBuffer(RayTracingDevice& device, BufferAllocation& allocation, VkDeviceMemory memory,
						BufferAllocationRequirements& requirements,
						VkDeviceSize& mergedMemoryOffset);
};