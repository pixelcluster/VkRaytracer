#pragma once

#include <RayTracingDevice.hpp>
#include <cgltf.h>
#include <string_view>
#include <util/MemoryAllocator.hpp>
#include <util/OneTimeDispatcher.hpp>
#include <vector>
#include <glm/glm.hpp>
#define GLM_FORCE_QUAT_DATA_XYZW
#include <glm/gtc/quaternion.hpp>

struct AABB {
	float xmin, ymin, zmin;
	float xmax, ymax, zmax;

	float intersectionArea(const AABB& other) const {
		float intersectedCubeWidth = 0.0f;
		float intersectedCubeHeight = 0.0f;
		float intersectedCubeDepth = 0.0f;

		if (xmax > other.xmin && xmin < other.xmax) {
			intersectedCubeWidth = std::min(xmax - other.xmin, other.xmax - other.xmin);
		} else if (xmin < other.xmax && xmax > other.xmin) {
			intersectedCubeWidth = other.xmax - xmin;
		}

		if (ymax > other.ymin && ymin < other.ymax) {
			intersectedCubeHeight = std::min(ymax - other.ymin, other.ymax - other.ymin);
		} else if (ymin < other.ymax && ymax > other.ymin) {
			intersectedCubeHeight = ymin - other.ymin;
		}

		if (zmax > other.zmin && zmin < other.zmax) {
			intersectedCubeDepth = std::min(zmax - other.zmin, other.zmax - other.zmin);
		} else if (zmin < other.zmax && zmax > other.zmin) {
			intersectedCubeDepth = zmin - other.zmin;
		}

		return intersectedCubeWidth * intersectedCubeHeight * intersectedCubeDepth;
	}

	bool intersects(const AABB& other) const { return intersectionArea(other) > 0.0f; }
};

struct Geometry {
	bool isAlphaTested;
	float transformMatrix[16];
	float normalTransformMatrix[16];

	AABB aabb;

	size_t vertexOffset;
	size_t uvOffset;
	size_t normalOffset;
	size_t tangentOffset;
	size_t indexOffset;

	size_t vertexCount;
	size_t indexCount;

	uint32_t materialIndex;
};

struct GPUGeometry {
	uint32_t vertexOffset;
	uint32_t uvOffset;
	uint32_t normalOffset;
	uint32_t tangentOffset;
	uint32_t indexOffset;

	uint32_t materialIndex;

	float normalTransformMatrix[9];
};

struct Material {
	float alphaCutoff;

	float albedoScale[4];

	float roughnessFactor;
	float metallicFactor;
	float normalMapFactor;

	float ior = 1.5f;

	float emissiveFactor[4];

	uint16_t albedoTextureIndex;
	uint16_t metallicRoughnessTextureIndex;
	uint16_t emissiveTextureIndex;
	uint16_t normalTextureIndex;
};

struct Texture {
	VkImageView view;
	VkSampler sampler;
};

struct ImageData {
	unsigned char* data;
	size_t size;
	int width, height;
};

struct Camera {
	float position[3] = { -2.0f, 0.0f, 1.0f };
	float direction[3] = { 1.0f, 0.0f, 0.0f };
	float right[3] = { 0.0f, 0.0f, -1.0f };
	float fov;
	float znear;
	float zfar = 10000.0f;
};

struct CopiedAccessor {
	cgltf_accessor* accessor;
	size_t bufferOffset;
};

class ModelLoader {
  public:
	ModelLoader(RayTracingDevice& device, MemoryAllocator& allocator, OneTimeDispatcher& dispatcher,
				const std::vector<std::string_view>& gltfFilenames);
	~ModelLoader();

	const AABB& modelBounds() const { return m_modelBounds; }

	VkBuffer vertexBuffer() const { return m_vertexBuffer; }
	VkBuffer uvBuffer() const { return m_uvBuffer; }
	VkBuffer normalBuffer() const { return m_normalBuffer; }
	VkBuffer tangentBuffer() const { return m_tangentBuffer; }
	VkBuffer indexBuffer() const { return m_indexBuffer; }
	VkBuffer materialBuffer() const { return m_materialBuffer; }
	VkBuffer geometryBuffer() const { return m_geometryBuffer; }

	VkDeviceAddress vertexBufferDeviceAddress() const { return m_vertexBufferDeviceAddress; }
	VkDeviceAddress indexBufferDeviceAddress() const { return m_indexBufferDeviceAddress; }

	size_t materialCount() const { return m_materials.size(); }
	size_t uvBufferSize() const { return m_totalUVCount * sizeof(float) * 2; }
	size_t normalBufferSize() const { return m_totalNormalCount * sizeof(float) * 3; }
	size_t tangentBufferSize() const { return m_totalTangentCount * sizeof(float) * 4; }
	size_t indexBufferSize() const { return m_totalIndexCount * sizeof(uint32_t); }

	const std::vector<Geometry>& geometries() const { return m_geometries; }

	const std::vector<VkImage>& textureImages() const { return m_textureImages; }
	const std::vector<VkSampler>& textureSamplers() const { return m_textureSamplers; }
	const std::vector<Texture>& textures() const { return m_textures; }

	VkDescriptorSet textureDescriptorSet() const { return m_textureDescriptorSet; }
	VkDescriptorSetLayout textureDescriptorSetLayout() const { return m_textureDescriptorSetLayout; }

	const Camera& camera() const { return m_camera; }

  private:
	void addScene(cgltf_data* data, cgltf_scene* scene);
	void addNode(cgltf_data* data, cgltf_node* node, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);

	void copySceneGeometries(cgltf_data* data, cgltf_scene* scene, size_t& currentGeometryIndex);
	void copyNodeGeometries(cgltf_data* data, cgltf_node* node, size_t& currentGeometryIndex);

	void addMaterial(cgltf_data* data, cgltf_material* material);

	void addTexture(cgltf_data* data, cgltf_texture* texture);

	void addImage(cgltf_data* data, cgltf_image* image, const std::string_view& gltfPath);

	void addSampler(cgltf_data* data, cgltf_sampler* sampler);

	RayTracingDevice& m_device;
	MemoryAllocator& m_allocator;
	OneTimeDispatcher& m_dispatcher;

	Camera m_camera;

	VkDeviceAddress m_vertexBufferDeviceAddress;
	VkDeviceAddress m_indexBufferDeviceAddress;

	VkBuffer m_vertexBuffer;
	VkBuffer m_uvBuffer;
	VkBuffer m_normalBuffer;
	VkBuffer m_tangentBuffer;
	VkBuffer m_indexBuffer;
	VkBuffer m_geometryBuffer;
	VkBuffer m_materialBuffer;

	VkBuffer m_vertexStagingBuffer;
	VkBuffer m_uvStagingBuffer;
	VkBuffer m_normalStagingBuffer;
	VkBuffer m_tangentStagingBuffer;
	VkBuffer m_indexStagingBuffer;
	VkBuffer m_geometryStagingBuffer;
	VkBuffer m_materialStagingBuffer;

	AABB m_modelBounds = { .xmin = 3e38, .ymin = 3e38, .zmin = 3e38, .xmax = -3e38, .ymax = -3e38, .zmax = -3e38 };

	std::vector<Geometry> m_geometries;
	std::vector<GPUGeometry> m_gpuGeometries;

	VkBuffer m_imageStagingBuffer;
	std::vector<VkImage> m_textureImages;
	std::vector<VkImageView> m_textureImageViews;
	std::vector<VkSampler> m_textureSamplers;
	std::vector<Texture> m_textures;
	std::vector<Material> m_materials;
	std::vector<bool> m_textureImageNormalUsage;

	VkSampler m_fallbackSampler;

	std::vector<ImageData> m_imageData;

	VkDescriptorPool m_textureDescriptorPool;
	VkDescriptorSet m_textureDescriptorSet;
	VkDescriptorSetLayout m_textureDescriptorSetLayout;

	// tempoary model loading metadata

	std::vector<CopiedAccessor> m_copiedVertexDataAccessors;
	std::vector<CopiedAccessor> m_copiedIndexDataAccessors;

	size_t m_totalVertexCount = 0;
	size_t m_totalUVCount = 0;
	size_t m_totalNormalCount = 0;
	size_t m_totalTangentCount = 0;
	size_t m_totalIndexCount = 0;

	size_t m_currentVertexDataOffset = 0;
	size_t m_currentUVDataOffset = 0;
	size_t m_currentNormalDataOffset = 0;
	size_t m_currentTangentDataOffset = 0;
	size_t m_currentIndexDataOffset = 0;

	size_t m_combinedImageSize = 0;
	size_t m_maxImageSize = 0;

	size_t m_globalMaterialIndexOffset = 0;
	size_t m_globalSamplerIndexOffset = 0;
	size_t m_globalImageIndexOffset = 0;
	size_t m_globalTextureIndexOffset = 0;

	float* m_vertexData;
	float* m_uvData;
	float* m_normalData;
	float* m_tangentData;
	uint32_t* m_indexData;
};