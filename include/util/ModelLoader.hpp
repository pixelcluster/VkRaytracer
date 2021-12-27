#pragma once

#include <string_view>
#include <RayTracingDevice.hpp>
#include <util/MemoryAllocator.hpp>

struct AABB {
	float xmin, ymin, zmin;
	float xmax, ymax, zmax;

	float intersectionArea(const AABB& other) const {
		float intersectedCubeWidth = 0.0f;
		float intersectedCubeHeight = 0.0f;
		float intersectedCubeDepth = 0.0f;

		if (xmax > other.xmin && xmax < other.xmax) {
			intersectedCubeWidth = xmax - other.xmin;
		} else if (xmin > other.xmin && xmin < other.xmax) {
			intersectedCubeWidth = xmin - other.xmin;
		}

		if (ymax > other.ymin && ymax < other.ymax) {
			intersectedCubeHeight = ymax - other.ymin;
		} else if (ymin > other.ymin && ymin < other.ymax) {
			intersectedCubeHeight = ymin - other.ymin;
		}

		if (zmax > other.zmin && zmax < other.zmax) {
			intersectedCubeDepth = zmax - other.zmin;
		} else if (zmin > other.zmin && zmin < other.zmax) {
			intersectedCubeDepth = zmin - other.zmin;
		}

		return intersectedCubeWidth * intersectedCubeHeight * intersectedCubeDepth;
	}

	bool intersects(const AABB& other) const { return intersectionArea(other) > 0.0f; }
};

struct Geometry {
	size_t vertexStride;
	size_t uvStride;
	size_t normalStride;
	size_t indexStride;

	float* vertexData;
	float* uvData;
	float* normalData;
	float* indexData;
};

class ModelLoader {
  public:
	ModelLoader(RayTracingDevice& device, MemoryAllocator& allocator, const std::string_view& gltfFilename);
	~ModelLoader();

	const AABB& modelBounds() const { return m_modelBounds; }

	const std::vector<Geometry>& geometries() const;

  private:
	RayTracingDevice& m_device;
	MemoryAllocator& m_allocator;

	VkBuffer m_vertexBuffer;
	VkBuffer m_indexBuffer;
	VkBuffer m_geometryIndexOffsetBuffer;

	AABB m_modelBounds;

	std::vector<Geometry> m_geometries;

	float* m_vertexData;
	float* m_uvData;
	float* m_normalData;
	float* m_indexData;
};