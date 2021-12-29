#define CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <DebugHelper.hpp>
#include <filesystem>
#include <stb_image.h>
#include <util/ModelLoader.hpp>

void multiply_mat4(float mat1[16], const float mat2[16]) {
	float tmp[16];

	//clang-format off
	for (size_t i = 0; i < 4; ++i) {
		tmp[i * 4 + 0] = mat1[i] * mat2[0] + mat1[i + 4] * mat2[1] + mat1[i + 8] * mat2[2] + mat1[i + 12] * mat2[3];
		tmp[i * 4 + 1] = mat1[i] * mat2[4] + mat1[i + 4] * mat2[5] + mat1[i + 8] * mat2[6] + mat1[i + 12] * mat2[7];
		tmp[i * 4 + 2] = mat1[i] * mat2[8] + mat1[i + 4] * mat2[9] + mat1[i + 8] * mat2[10] + mat1[i + 12] * mat2[11];
		tmp[i * 4 + 3] = mat1[i] * mat2[12] + mat1[i + 4] * mat2[13] + mat1[i + 8] * mat2[14] + mat1[i + 12] * mat2[15];
	}
	//clang-format on
	std::memcpy(mat1, tmp, 16 * sizeof(float));
}

float dot_vec3(const float vec1[3], const float vec2[3]) {
	return vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec1[2] * vec2[2];
}

// multiply 4x4 matrix with vec(x, y, z, 1)
void multiply_mat4_vec3_w1(float vec[3], const float mat[16]) {
	float tmp[3];
	tmp[0] = dot_vec3(vec, mat);
	tmp[1] = dot_vec3(vec, mat + 4);
	tmp[2] = dot_vec3(vec, mat + 8);
	tmp[0] += mat[3];
	tmp[0] += mat[7];
	tmp[0] += mat[11];
	std::memcpy(vec, tmp, 3 * sizeof(float));
}

void multiply_mat4_vec3_w0(float vec[3], const float mat[16]) {
	float tmp[3];
	tmp[0] = dot_vec3(vec, mat);
	tmp[1] = dot_vec3(vec, mat + 4);
	tmp[2] = dot_vec3(vec, mat + 8);
	std::memcpy(vec, tmp, 3 * sizeof(float));
}

void cross_vec3(float vec1[3], const float vec2[3]) {
	float tmp[3];
	tmp[0] = vec1[1] * vec2[2] - vec1[2] * vec2[1];
	tmp[1] = vec1[2] * vec2[0] - vec1[0] * vec2[2];
	tmp[2] = vec1[0] * vec2[1] - vec1[1] * vec2[0];
	std::memcpy(vec1, tmp, 3 * sizeof(float));
}

void vec_add(float vec1[3], const float vec2[3]) {
	vec1[0] += vec2[0];
	vec1[1] += vec2[1];
	vec1[2] += vec2[2];
}

void vec_mul(float vec1[3], float s) {
	vec1[0] *= s;
	vec1[1] *= s;
	vec1[2] *= s;
}

void rotate_quat(float vec[3], const float quat[4]) {
	// https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
	float qvec[3] = { quat[0], quat[1], quat[2] };
	float qvec_length2 = dot_vec3(qvec, qvec);
	float qscal = quat[3];

	float vec_tmp[3] = { vec[0], vec[1], vec[2] };
	float vec_quat_cross[3] = { vec[0], vec[1], vec[2] };
	cross_vec3(vec_quat_cross, qvec);

	vec_mul(qvec, 2.0f * dot_vec3(qvec, vec_tmp));
	vec_mul(vec_tmp, qscal * qscal - qvec_length2);
	vec_mul(vec_quat_cross, 2.0f * qscal);

	vec_add(vec_tmp, qvec);
	vec_add(vec_tmp, vec_quat_cross);

	std::memcpy(vec, vec_tmp, 3 * sizeof(float));
}

void checkCGLTFResult(cgltf_result result, const std::string_view& gltfFilename) {
	if (result != cgltf_result_success) {
		switch (result) {
			case cgltf_result_invalid_gltf:
				printf("glTF file %s was invalid!\n", gltfFilename.data());
				break;
			case cgltf_result_invalid_json:
				printf("glTF file %s contained invalid JSON data!\n", gltfFilename.data());
				break;
			case cgltf_result_file_not_found:
				printf("glTF file %s not found.\n", gltfFilename.data());
				break;
			case cgltf_result_out_of_memory:
				printf("Out Of Memory when loading glTF file %s!\n", gltfFilename.data());
				break;
			case cgltf_result_legacy_gltf:
				printf("Trying to load a legacy glTF file, please convert to a newer version %s!\n",
					   gltfFilename.data());
				break;
			default:
				break;
		}
		std::exit(4);
	}
}

struct BlitImage {
	VkImage image;
	int32_t width, height;
};

ModelLoader::ModelLoader(RayTracingDevice& device, MemoryAllocator& allocator, OneTimeDispatcher& dispatcher,
						 const std::vector<std::string_view>& gltfFilenames)
	: m_device(device), m_allocator(allocator), m_dispatcher(dispatcher) {
	if (gltfFilenames.empty())
		return;
	std::vector<cgltf_data*> gltfData;
	gltfData.reserve(gltfFilenames.size());

	for (auto& gltfFilename : gltfFilenames) {
		cgltf_options options = {};
		cgltf_data* data;
		cgltf_result result = cgltf_parse_file(&options, gltfFilename.data(), &data);
		checkCGLTFResult(result, gltfFilename);
		result = cgltf_load_buffers(&options, data, gltfFilename.data());
		checkCGLTFResult(result, gltfFilename);
		gltfData.push_back(data);

		if (data->scene) {
			addScene(data, data->scene);
		} else {
			for (cgltf_size i = 0; i < data->scenes_count; ++i) {
				addScene(data, data->scenes + i);
			}
		}
	}

	size_t vertexDataSize =
		(m_totalVertexCount * 3 + m_totalUVCount * 2 + m_totalNormalCount * 3 + m_totalTangentCount * 4) *
		sizeof(float);
	size_t indexDataSize = m_totalIndexCount * sizeof(uint32_t);

	m_vertexData = reinterpret_cast<float*>(malloc(vertexDataSize));
	m_indexData = reinterpret_cast<uint32_t*>(malloc(indexDataSize));

	size_t gltfDataIndex = 0;
	for (auto& gltfFilename : gltfFilenames) {
		cgltf_data* data = gltfData[gltfDataIndex];
		if (data->scene) {
			copySceneGeometries(data, data->scene);
		} else {
			for (cgltf_size i = 0; i < data->scenes_count; ++i) {
				copySceneGeometries(data, data->scenes + i);
			}
		}

		for (cgltf_size i = 0; i < data->images_count; ++i) {
			addImage(data, data->images + i, gltfFilename);
		}
		for (cgltf_size i = 0; i < data->samplers_count; ++i) {
			addSampler(data, data->samplers + i);
		}
		for (cgltf_size i = 0; i < data->samplers_count; ++i) {
			addTexture(data, data->textures + i);
		}
		for (cgltf_size i = 0; i < data->materials_count; ++i) {
			addMaterial(data, data->materials + i);
		}

		++gltfDataIndex;
	}

	VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
											  .magFilter = VK_FILTER_NEAREST,
											  .minFilter = VK_FILTER_NEAREST,
											  .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
											  .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
											  .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT };
	vkCreateSampler(m_device.device(), &samplerCreateInfo, nullptr, &m_fallbackSampler);
	setObjectName(m_device.device(), VK_OBJECT_TYPE_SAMPLER, m_fallbackSampler, "Fallback sampler");

	// Allocate buffers (both staging and device local)

	VkBufferCreateInfo bufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
											.size = vertexDataSize,
											.usage =
												VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
												VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT };
	VkBufferCreateInfo stagingBufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
												   .size = vertexDataSize,
												   .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT };
	verifyResult(vkCreateBuffer(m_device.device(), &bufferCreateInfo, nullptr, &m_vertexBuffer));
	verifyResult(vkCreateBuffer(m_device.device(), &stagingBufferCreateInfo, nullptr, &m_vertexStagingBuffer));

	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, m_vertexBuffer, "Vertex buffer");
	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, m_vertexStagingBuffer, "Vertex staging buffer");

	bufferCreateInfo.size = indexDataSize;
	stagingBufferCreateInfo.size = indexDataSize;

	verifyResult(vkCreateBuffer(m_device.device(), &bufferCreateInfo, nullptr, &m_indexBuffer));
	verifyResult(vkCreateBuffer(m_device.device(), &stagingBufferCreateInfo, nullptr, &m_indexStagingBuffer));

	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, m_indexBuffer, "Index buffer");
	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, m_indexStagingBuffer, "Index staging buffer");

	bufferCreateInfo.usage &= ~(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	bufferCreateInfo.size = m_materials.size() * sizeof(Material);
	stagingBufferCreateInfo.size = m_materials.size() * sizeof(Material);

	verifyResult(vkCreateBuffer(m_device.device(), &bufferCreateInfo, nullptr, &m_materialBuffer));
	verifyResult(vkCreateBuffer(m_device.device(), &stagingBufferCreateInfo, nullptr, &m_materialStagingBuffer));

	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, m_materialBuffer, "Material buffer");
	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, m_materialStagingBuffer, "Material staging buffer");

	bufferCreateInfo.size = m_gpuGeometries.size() * sizeof(GPUGeometry);
	stagingBufferCreateInfo.size = m_gpuGeometries.size() * sizeof(GPUGeometry);

	verifyResult(vkCreateBuffer(m_device.device(), &bufferCreateInfo, nullptr, &m_geometryBuffer));
	verifyResult(vkCreateBuffer(m_device.device(), &stagingBufferCreateInfo, nullptr, &m_geometryStagingBuffer));

	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, m_geometryBuffer, "Geometry buffer");
	setObjectName(m_device.device(), VK_OBJECT_TYPE_BUFFER, m_geometryStagingBuffer, "Geometry staging buffer");

	void* vertexStagingBufferData = m_allocator.bindStagingBuffer(m_vertexStagingBuffer, 0);
	void* indexStagingBufferData = m_allocator.bindStagingBuffer(m_indexStagingBuffer, 0);
	void* materialStagingBufferData = m_allocator.bindStagingBuffer(m_materialStagingBuffer, 0);
	void* geometryStagingBufferData = m_allocator.bindStagingBuffer(m_geometryStagingBuffer, 0);
	m_allocator.bindDeviceBuffer(m_vertexBuffer, 0);
	m_allocator.bindDeviceBuffer(m_indexBuffer, 0);
	m_allocator.bindDeviceBuffer(m_materialBuffer, 0);
	m_allocator.bindDeviceBuffer(m_geometryBuffer, 0);

	stagingBufferCreateInfo.size = m_combinedImageSize;
	verifyResult(vkCreateBuffer(m_device.device(), &stagingBufferCreateInfo, nullptr, &m_imageStagingBuffer));
	void* imageStagingBufferData = m_allocator.bindStagingBuffer(m_imageStagingBuffer, 0);

	// Copy vertex data

	std::memcpy(vertexStagingBufferData, m_vertexData, vertexDataSize);
	std::memcpy(indexStagingBufferData, m_indexData, indexDataSize);

	// Copy material/geometry data

	std::memcpy(materialStagingBufferData, m_materials.data(), m_materials.size() * sizeof(Material));
	std::memcpy(geometryStagingBufferData, m_gpuGeometries.data(), m_gpuGeometries.size() * sizeof(GPUGeometry));

	// Copy image data and prepare blits for mipmaps

	std::vector<BlitImage> blitImages;
	std::vector<VkBufferImageCopy> copies;
	std::vector<VkImageMemoryBarrier> layoutTransferTransitionBarriers;
	std::vector<VkImageMemoryBarrier> layoutGeneralTransitionBarriers;
	std::vector<VkImageMemoryBarrier> layoutSampledTransitionBarriers;
	blitImages.reserve(m_textureImages.size());
	copies.reserve(m_textureImages.size());
	layoutTransferTransitionBarriers.reserve(m_textureImages.size());
	layoutSampledTransitionBarriers.reserve(m_textureImages.size());

	size_t currentImageStagingOffset = 0;

	for (auto& image : m_imageData) {
		std::memcpy(reinterpret_cast<uint8_t*>(imageStagingBufferData) + currentImageStagingOffset, image.data,
					image.size);
		copies.push_back({ .bufferOffset = currentImageStagingOffset,
						   .bufferRowLength = 0,
						   .bufferImageHeight = 0,
						   .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
												 .mipLevel = 0,
												 .baseArrayLayer = 0,
												 .layerCount = 1 },
						   .imageOffset = {},
						   .imageExtent = { .width = static_cast<uint32_t>(image.width),
											.height = static_cast<uint32_t>(image.height),
											.depth = 1 } });
		currentImageStagingOffset += image.size;
		blitImages.push_back({ .width = image.width, .height = image.height });
	}

	size_t blitImageIndex = 0;
	for (auto& image : m_textureImages) {
		layoutTransferTransitionBarriers.push_back(
			{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			  .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
			  .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			  .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			  .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			  .image = image,
			  .subresourceRange = {
				  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				  .baseMipLevel = 0,
				  .levelCount = static_cast<uint32_t>(
					  log2(std::min(blitImages[blitImageIndex].height, blitImages[blitImageIndex].width))),
				  .baseArrayLayer = 0,
				  .layerCount = 1,
			  } });
		layoutGeneralTransitionBarriers.push_back(
			{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			  .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			  .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			  .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			  .newLayout = VK_IMAGE_LAYOUT_GENERAL,
			  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			  .image = image,
			  .subresourceRange = {
				  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				  .baseMipLevel = 0,
				  .levelCount = static_cast<uint32_t>(
					  log2(std::min(blitImages[blitImageIndex].height, blitImages[blitImageIndex].width))),
				  .baseArrayLayer = 0,
				  .layerCount = 1,
			  } });
		layoutSampledTransitionBarriers.push_back(
			{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			  .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			  .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			  .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			  .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			  .image = image,
			  .subresourceRange = {
				  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				  .baseMipLevel = 0,
				  .levelCount = static_cast<uint32_t>(
					  log2(std::min(blitImages[blitImageIndex].height, blitImages[blitImageIndex].width))),
				  .baseArrayLayer = 0,
				  .layerCount = 1,
			  } });
		blitImages[blitImageIndex].image = image;
		++blitImageIndex;
	}

	VkCommandBuffer commandBuffer = dispatcher.allocateOneTimeSubmitBuffers(1)[0];

	VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
										   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	verifyResult(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	VkBufferCopy bufferCopy = { .size = vertexDataSize };

	vkCmdCopyBuffer(commandBuffer, m_vertexStagingBuffer, m_vertexBuffer, 1, &bufferCopy);
	bufferCopy.size = indexDataSize;
	vkCmdCopyBuffer(commandBuffer, m_indexStagingBuffer, m_indexBuffer, 1, &bufferCopy);
	bufferCopy.size = m_materials.size() * sizeof(Material);
	vkCmdCopyBuffer(commandBuffer, m_materialStagingBuffer, m_materialBuffer, 1, &bufferCopy);
	bufferCopy.size = m_gpuGeometries.size() * sizeof(GPUGeometry);
	vkCmdCopyBuffer(commandBuffer, m_geometryStagingBuffer, m_geometryBuffer, 1, &bufferCopy);

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
						 nullptr, layoutTransferTransitionBarriers.size(), layoutTransferTransitionBarriers.data());

	size_t copyIndex = 0;
	for (auto& image : m_textureImages) {
		vkCmdCopyBufferToImage(commandBuffer, m_imageStagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
							   &copies[copyIndex]);
		++copyIndex;
	}

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
						 0, nullptr, layoutGeneralTransitionBarriers.size(), layoutGeneralTransitionBarriers.data());

	VkMemoryBarrier transferBarrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
										.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
										.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT };

	uint32_t mipIndex = 0;
	while (!blitImages.empty()) {
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
							 &transferBarrier, 0, nullptr, 0, nullptr);
		for (auto& image : blitImages) {
			int32_t mipHeight = image.height / 2;
			int32_t mipWidth = image.width / 2;

			VkImageBlit blit = {
				.srcSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
									.mipLevel = mipIndex,
									.baseArrayLayer = 0,
									.layerCount = 1 },
				.srcOffsets = { {}, { .x = image.width, .y = image.height, .z = 1 } },
				.dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
									.mipLevel = mipIndex + 1,
									.baseArrayLayer = 0,
									.layerCount = 1 },
				.dstOffsets = { {}, { .x = mipWidth, .y = mipHeight, .z = 1 } },
			};

			vkCmdBlitImage(commandBuffer, image.image, VK_IMAGE_LAYOUT_GENERAL, image.image,
						   VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_LINEAR);
			image.height = mipHeight;
			image.width = mipWidth;
		}

		for (size_t i = 0; i < blitImages.size(); ++i) {
			if (static_cast<uint32_t>(log2f(std::min(blitImages[i].width, blitImages[i].height))) == mipIndex + 1) {
				blitImages.erase(blitImages.begin() + i);
				--i;
			}
		}
		++mipIndex;
	}

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
						 0, nullptr, 0, nullptr, layoutSampledTransitionBarriers.size(),
						 layoutSampledTransitionBarriers.data());

	verifyResult(vkEndCommandBuffer(commandBuffer));

	dispatcher.submit(commandBuffer, {});
	dispatcher.waitForFence(commandBuffer, UINT64_MAX); // bad but ¯\_()_/¯

	VkBufferDeviceAddressInfo addressInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
											  .buffer = m_vertexBuffer };
	m_vertexBufferDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &addressInfo);
	addressInfo.buffer = m_indexBuffer;
	m_indexBufferDeviceAddress = vkGetBufferDeviceAddress(m_device.device(), &addressInfo);

	VkDescriptorSetLayoutBinding binding = { .binding = 0,
											 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
											 .descriptorCount = static_cast<uint32_t>(m_textures.size()),
											 .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
														   VK_SHADER_STAGE_ANY_HIT_BIT_KHR };

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &binding,
	};
	verifyResult(
		vkCreateDescriptorSetLayout(m_device.device(), &layoutCreateInfo, nullptr, &m_textureDescriptorSetLayout));

	VkDescriptorPoolSize sampledImageSize = { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
											  .descriptorCount = static_cast<uint32_t>(m_textures.size()) };
	VkDescriptorPoolCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
											  .maxSets = 1,
											  .poolSizeCount = 1,
											  .pPoolSizes = &sampledImageSize };
	verifyResult(vkCreateDescriptorPool(m_device.device(), &createInfo, nullptr, &m_textureDescriptorPool));

	VkDescriptorSetAllocateInfo setAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
													.descriptorPool = m_textureDescriptorPool,
													.descriptorSetCount = 1,
													.pSetLayouts = &m_textureDescriptorSetLayout };
	verifyResult(vkAllocateDescriptorSets(m_device.device(), &setAllocateInfo, &m_textureDescriptorSet));

	setObjectName(m_device.device(), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, m_textureDescriptorSetLayout,
				  "Texture descriptor set layout");
	setObjectName(m_device.device(), VK_OBJECT_TYPE_DESCRIPTOR_POOL, m_textureDescriptorPool,
				  "Texture descriptor pool");
	setObjectName(m_device.device(), VK_OBJECT_TYPE_DESCRIPTOR_SET, m_textureDescriptorSet, "Texture descriptor set");

	for (auto& data : gltfData) {
		cgltf_free(data);
	}
	for (auto& data : m_imageData) {
		stbi_image_free(data.data);
	}

	free(m_vertexData);
	free(m_indexData);
}

ModelLoader::~ModelLoader() {
	vkDestroyBuffer(m_device.device(), m_vertexBuffer, nullptr);
	vkDestroyBuffer(m_device.device(), m_indexBuffer, nullptr);
	vkDestroyBuffer(m_device.device(), m_vertexStagingBuffer, nullptr);
	vkDestroyBuffer(m_device.device(), m_indexStagingBuffer, nullptr);
	vkDestroyBuffer(m_device.device(), m_imageStagingBuffer, nullptr);

	for (auto& image : m_textureImages) {
		vkDestroyImage(m_device.device(), image, nullptr);
	}
	for (auto& sampler : m_textureSamplers) {
		vkDestroySampler(m_device.device(), sampler, nullptr);
	}
	vkDestroySampler(m_device.device(), m_fallbackSampler, nullptr);

	vkDestroyDescriptorPool(m_device.device(), m_textureDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_device.device(), m_textureDescriptorSetLayout, nullptr);
}

void ModelLoader::addScene(cgltf_data* data, cgltf_scene* scene) {
	for (cgltf_size i = 0; i < scene->nodes_count; ++i) {
		float translation[3] = { 0.0f, 0.0f, 0.0f };
		float rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		float scale[3] = { 1.0f, 1.0f, 1.0f };
		addNode(data, scene->nodes[i], translation, rotation, scale);
	}
}

void ModelLoader::addNode(cgltf_data* data, cgltf_node* node, float translation[3], float rotation[4], float scale[3]) {
	// local -> global transform
	float localTranslation[3] = {};
	float localRotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float localScale[3] = { 1.0f, 1.0f, 1.0f };
	// resolve child transforms and make global
	if (node->has_translation) {
		for (size_t i = 0; i < 3; ++i) {
			localTranslation[i] = translation[i] + node->translation[i];
		}
	}
	if (node->has_rotation) {
		// multiply quaternions
		// https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/arithmetic/index.htm
		localRotation[0] = rotation[0] * node->rotation[0] + rotation[1] * node->rotation[1] +
						   rotation[2] * node->rotation[2] + rotation[3] * node->rotation[3];
		localRotation[1] = rotation[1] * node->rotation[0] + rotation[0] * node->rotation[1] +
						   rotation[2] * node->rotation[3] + rotation[3] * node->rotation[2];
		localRotation[2] = rotation[0] * node->rotation[2] + rotation[1] * node->rotation[3] +
						   rotation[2] * node->rotation[0] + rotation[3] * node->rotation[1];
		localRotation[3] = rotation[0] * node->rotation[3] + rotation[1] * node->rotation[2] +
						   rotation[2] * node->rotation[1] + rotation[3] * node->rotation[0];
	}
	if (node->has_scale) {
		for (size_t i = 0; i < 3; ++i) {
			localScale[i] = scale[i] * node->scale[i];
		}
	}

	// clang-format off
	//https://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
	//assumed column-major matrix layout
	float quatRotationMatrix[16] = {
		 localRotation[3],  localRotation[2], -localRotation[1], localRotation[0],
		-localRotation[2],  localRotation[3],  localRotation[0], localRotation[1],
		 localRotation[1], -localRotation[0],  localRotation[3], localRotation[2],
		-localRotation[0], -localRotation[1], -localRotation[2], localRotation[3],
	};
	float quatRotationMatrixFactor[16] = {
		 localRotation[3],  localRotation[2], -localRotation[1], -localRotation[0],
		-localRotation[2],  localRotation[3],  localRotation[0], -localRotation[1],
		 localRotation[1], -localRotation[0],  localRotation[3], -localRotation[2],
		 localRotation[0],  localRotation[1],  localRotation[2],  localRotation[3],
	};

	float transformMatrix[16] = { //only translation at first, becomes transform matrix through matrix multiplications
		1.0f, 0.0f, 0.0f, localTranslation[0],
		0.0f, 1.0f, 0.0f, localTranslation[1],
		0.0f, 0.0f, 1.0f, localTranslation[2],
		0.0f, 0.0f, 0.0f, 1.0f
	};

	float scaleMatrix[16] = {
		localScale[0], 0.0f,		  0.0f,			 0.0f,
		0.0f,		   localScale[1], 0.0f,			 0.0f,
		0.0f,		   0.0f,		  localScale[2], 0.0f,
		0.0f,		   0.0f,		  0.0f,			 1.0f
	};
	// clang-format on

	float noRotationTransformMatrix[16];
	std::memcpy(noRotationTransformMatrix, transformMatrix, 16 * sizeof(float));

	multiply_mat4(quatRotationMatrix, quatRotationMatrixFactor);
	multiply_mat4(quatRotationMatrix, scaleMatrix);
	multiply_mat4(transformMatrix, quatRotationMatrix);

	multiply_mat4(noRotationTransformMatrix, scaleMatrix);

	if (node->camera && node->camera->type == cgltf_camera_type_perspective) {
		float baseDirection[3] = { 0.0f, 0.0f, -1.0f };
		rotate_quat(baseDirection, localRotation);
		m_camera = { .fov = node->camera->data.perspective.yfov, .znear = node->camera->data.perspective.znear };

		std::memcpy(m_camera.direction, baseDirection, 3 * sizeof(float));
		std::memcpy(m_camera.position, localTranslation, 3 * sizeof(float));

		if (node->camera->data.perspective.has_zfar) {
			m_camera.zfar = node->camera->data.perspective.zfar;
		}
	}

	if (node->mesh) {
		// TODO: this essentially de-instantiates meshes, so no instancing - maybe later?
		for (cgltf_size i = 0; i < node->mesh->primitives_count; ++i) {
			cgltf_primitive* primitive = node->mesh->primitives + i;

			if (primitive->type != cgltf_primitive_type_triangles) {
				printf("glTF primitives other than triangles are not supported, skipping primitive.\n");
				continue;
			}

			Geometry geometry{};

			float aabbMin[3], aabbMax[3];

			for (cgltf_size j = 0; j < primitive->attributes_count; ++j) {
				cgltf_attribute* attribute = primitive->attributes + j;

				bool hasConsideredVertexAccessor =
					std::find_if(m_copiedVertexDataAccessors.begin(), m_copiedVertexDataAccessors.end(),
								 [attribute](const CopiedAccessor& accessor) {
									 return attribute->data == accessor.accessor;
								 }) != m_copiedVertexDataAccessors.end();

				switch (attribute->type) {
					case cgltf_attribute_type_position:
						std::memcpy(aabbMin, attribute->data->min, 3 * sizeof(float));
						std::memcpy(aabbMax, attribute->data->max, 3 * sizeof(float));

						geometry.vertexCount = attribute->data->count;

						if (!hasConsideredVertexAccessor)
							m_totalVertexCount += attribute->data->count;
						break;
					case cgltf_attribute_type_normal:
						if (!hasConsideredVertexAccessor)
							m_totalNormalCount += attribute->data->count;
						break;
					case cgltf_attribute_type_tangent:
						if (!hasConsideredVertexAccessor)
							m_totalTangentCount += attribute->data->count;
						break;
					case cgltf_attribute_type_texcoord:
						if (!hasConsideredVertexAccessor)
							m_totalUVCount += attribute->data->count;
						break;
				}
			}

			bool hasConsideredVertexAccessor =
				std::find_if(m_copiedIndexDataAccessors.begin(), m_copiedIndexDataAccessors.end(),
							 [primitive](const CopiedAccessor& accessor) {
								 return primitive->indices == accessor.accessor;
							 }) != m_copiedIndexDataAccessors.end();
			if (!hasConsideredVertexAccessor) {
				m_totalIndexCount += primitive->indices->count;
			}

			geometry.indexCount = primitive->indices->count;

			multiply_mat4_vec3_w1(aabbMin, noRotationTransformMatrix);
			multiply_mat4_vec3_w1(aabbMax, noRotationTransformMatrix);

			m_modelBounds.xmin = std::min(m_modelBounds.xmin, aabbMin[0]);
			m_modelBounds.ymin = std::min(m_modelBounds.ymin, aabbMin[1]);
			m_modelBounds.zmin = std::min(m_modelBounds.zmin, aabbMin[2]);
			m_modelBounds.xmax = std::max(m_modelBounds.xmax, aabbMax[0]);
			m_modelBounds.ymax = std::max(m_modelBounds.ymax, aabbMax[1]);
			m_modelBounds.zmax = std::max(m_modelBounds.zmax, aabbMax[2]);

			geometry.aabb = { .xmin = aabbMin[0],
							  .ymin = aabbMin[1],
							  .zmin = aabbMin[2],
							  .xmax = aabbMax[0],
							  .ymax = aabbMax[1],
							  .zmax = aabbMax[2] };

			if (primitive->material) {
				geometry.materialIndex =
					primitive->material -
					data->materials; // sure hope the material is in that array, otherwise bad stuff ensues

				geometry.isAlphaTested = primitive->material->alpha_mode == cgltf_alpha_mode_mask;
			}

			std::memcpy(geometry.transformMatrix, transformMatrix, 16 * sizeof(float));

			m_gpuGeometries.push_back({ .vertexOffset = static_cast<uint32_t>(geometry.vertexOffset),
										.uvOffset = static_cast<uint32_t>(geometry.uvOffset),
										.normalOffset = static_cast<uint32_t>(geometry.normalOffset),
										.tangentOffset = static_cast<uint32_t>(geometry.tangentOffset),
										.indexOffset = static_cast<uint32_t>(geometry.indexOffset),
										.materialIndex = geometry.materialIndex });
			m_geometries.push_back(std::move(geometry));
		}
	}

	for (cgltf_size i = 0; i < node->children_count; ++i) {
		addNode(data, node->children[i], localTranslation, localRotation, localScale);
	}
}

void ModelLoader::copySceneGeometries(cgltf_data* data, cgltf_scene* scene) {
	// the layout, order and number of geometries stays the same, so one can just increment a global counter in order to
	// get the geometry for a given primitive
	size_t geometryIndex = 0;
	for (cgltf_size i = 0; i < scene->nodes_count; ++i) {
		copyNodeGeometries(data, scene->nodes[i], geometryIndex);
	}
}

void ModelLoader::copyNodeGeometries(cgltf_data* data, cgltf_node* node, size_t& currentGeometryIndex) {
	if (node->mesh) {
		for (cgltf_size i = 0; i < node->mesh->primitives_count; ++i) {
			cgltf_primitive* primitive = node->mesh->primitives + i;

			if (primitive->type != cgltf_primitive_type_triangles) {
				continue;
			}

			for (cgltf_size j = 0; j < primitive->attributes_count; ++j) {
				cgltf_attribute* attribute = primitive->attributes + j;

				auto vertexAccessorIterator = std::find_if(
					m_copiedVertexDataAccessors.begin(), m_copiedVertexDataAccessors.end(),
					[attribute](const CopiedAccessor& accessor) { return attribute->data == accessor.accessor; });
				bool hasConsideredVertexAccessor = vertexAccessorIterator != m_copiedVertexDataAccessors.end();

				switch (attribute->type) {
					case cgltf_attribute_type_position:
						if (!hasConsideredVertexAccessor) {
							std::memcpy(m_vertexData + (m_currentVertexDataOffset / sizeof(float)),
										reinterpret_cast<uint8_t*>(attribute->data->buffer_view->buffer->data) +
											attribute->data->buffer_view->offset + attribute->data->offset,
										attribute->data->count * 3 * sizeof(float));

							m_geometries[currentGeometryIndex].vertexOffset = m_currentVertexDataOffset;
							m_copiedVertexDataAccessors.push_back({ attribute->data, m_currentVertexDataOffset });
							m_currentVertexDataOffset += attribute->data->count * 3 * sizeof(float);
						} else {
							m_geometries[currentGeometryIndex].vertexOffset += vertexAccessorIterator->bufferOffset;
						}
						break;
					case cgltf_attribute_type_normal:
						if (!hasConsideredVertexAccessor) {
							std::memcpy(m_vertexData + (m_currentVertexDataOffset / sizeof(float)),
										reinterpret_cast<uint8_t*>(attribute->data->buffer_view->buffer->data) +
											attribute->data->buffer_view->offset + attribute->data->offset,
										attribute->data->count * 3 * sizeof(float));

							m_geometries[currentGeometryIndex].normalOffset = m_currentVertexDataOffset;
							m_copiedVertexDataAccessors.push_back({ attribute->data, m_currentVertexDataOffset });
							m_currentVertexDataOffset += attribute->data->count * 3 * sizeof(float);
						} else {
							m_geometries[currentGeometryIndex].normalOffset += vertexAccessorIterator->bufferOffset;
						}
						break;
					case cgltf_attribute_type_tangent:
						if (!hasConsideredVertexAccessor) {
							std::memcpy(m_vertexData + (m_currentVertexDataOffset / sizeof(float)),
										reinterpret_cast<uint8_t*>(attribute->data->buffer_view->buffer->data) +
											attribute->data->buffer_view->offset + attribute->data->offset,
										attribute->data->count * 4 * sizeof(float));

							m_geometries[currentGeometryIndex].tangentOffset = m_currentVertexDataOffset;
							m_copiedVertexDataAccessors.push_back({ attribute->data, m_currentVertexDataOffset });
							m_currentVertexDataOffset += attribute->data->count * 4 * sizeof(float);
						} else {
							m_geometries[currentGeometryIndex].tangentOffset += vertexAccessorIterator->bufferOffset;
						}
						break;
					case cgltf_attribute_type_texcoord:
						if (!hasConsideredVertexAccessor) {
							std::memcpy(m_vertexData + (m_currentVertexDataOffset / sizeof(float)),
										reinterpret_cast<uint8_t*>(attribute->data->buffer_view->buffer->data) +
											attribute->data->buffer_view->offset + attribute->data->offset,
										attribute->data->count * 2 * sizeof(float));

							m_geometries[currentGeometryIndex].uvOffset = m_currentVertexDataOffset;
							m_copiedVertexDataAccessors.push_back({ attribute->data, m_currentVertexDataOffset });
							m_currentVertexDataOffset += attribute->data->count * 2 * sizeof(float);
						} else {
							m_geometries[currentGeometryIndex].uvOffset += vertexAccessorIterator->bufferOffset;
						}
						break;
				}
			}

			auto indexAccessorIterator = std::find_if(
				m_copiedIndexDataAccessors.begin(), m_copiedIndexDataAccessors.end(),
				[primitive](const CopiedAccessor& accessor) { return primitive->indices == accessor.accessor; });
			bool hasConsideredIndexAccessor = indexAccessorIterator != m_copiedIndexDataAccessors.end();

			if (!hasConsideredIndexAccessor) {
				if (primitive->indices->component_type != cgltf_component_type_r_32u) {
					uint32_t* dstData =
						reinterpret_cast<uint32_t*>(m_indexData + (m_currentIndexDataOffset / sizeof(float)));
					switch (primitive->indices->component_type) {
						case cgltf_component_type_r_8u: {
							uint8_t* srcData =
								reinterpret_cast<uint8_t*>(primitive->indices->buffer_view->buffer->data) +
								primitive->indices->buffer_view->offset + primitive->indices->offset;
							for (cgltf_size i = 0; i < primitive->indices->count; ++i) {
								dstData[i] = srcData[i]; // convert uint8 to uint32
							}
						} break;
						case cgltf_component_type_r_16u: {
							uint16_t* srcData = reinterpret_cast<uint16_t*>(
								reinterpret_cast<uint8_t*>(primitive->indices->buffer_view->buffer->data) +
								primitive->indices->buffer_view->offset +
								primitive->indices
									->offset); // convert to uint8*, offset by byte count, then convert to uint16*
							for (cgltf_size i = 0; i < primitive->indices->count; ++i) {
								dstData[i] = srcData[i]; // convert uint16 to uint32
							}
						} break;
					}
				} else
					std::memcpy(m_indexData + (m_currentIndexDataOffset / sizeof(float)),
								reinterpret_cast<char*>(primitive->indices->buffer_view->buffer->data) +
									primitive->indices->buffer_view->offset + primitive->indices->offset,
								primitive->indices->count * sizeof(uint32_t));

				m_geometries[currentGeometryIndex].indexOffset = m_currentIndexDataOffset;
				m_copiedIndexDataAccessors.push_back({ primitive->indices, m_currentIndexDataOffset });
				m_currentIndexDataOffset += primitive->indices->count * sizeof(uint32_t);
			} else {
				m_geometries[currentGeometryIndex].indexOffset = indexAccessorIterator->bufferOffset;
			}
			++currentGeometryIndex;
		}
	}

	for (cgltf_size i = 0; i < node->children_count; ++i) {
		copyNodeGeometries(data, node->children[i], currentGeometryIndex);
	}
}

void ModelLoader::addMaterial(cgltf_data* data, cgltf_material* material) {
	Material newMaterial{};
	newMaterial.albedoTextureIndex = -1;
	newMaterial.emissiveTextureIndex = -1;
	newMaterial.metallicRoughnessTextureIndex = -1;
	newMaterial.normalTextureIndex = -1;

	if (material->has_clearcoat || material->has_pbr_specular_glossiness || material->has_sheen ||
		material->has_transmission || material->has_volume) {
		printf("Clearcoat, PBR specular glossiness, sheen, transmission or volumes are used in a material despite not "
			   "being supported at the moment!\n");
	} else if (material->has_pbr_metallic_roughness) {
		if (material->has_ior) {
			newMaterial.ior = material->ior.ior;
		}
		if (material->normal_texture.texture) {
			newMaterial.normalTextureIndex = material->normal_texture.texture - data->textures;
		}
		if (material->emissive_texture.texture) {
			newMaterial.emissiveTextureIndex = material->emissive_texture.texture - data->textures;
		}
		if (material->pbr_metallic_roughness.base_color_texture.texture) {
			newMaterial.albedoTextureIndex =
				material->pbr_metallic_roughness.base_color_texture.texture - data->textures;
		}
		if (material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
			newMaterial.metallicRoughnessTextureIndex =
				material->pbr_metallic_roughness.base_color_texture.texture - data->textures;
		}

		std::memcpy(newMaterial.albedoScale, material->pbr_metallic_roughness.base_color_factor, 4 * sizeof(float));
		std::memcpy(newMaterial.emissiveFactor, material->emissive_factor, 3 * sizeof(float));
		newMaterial.roughnessFactor = material->pbr_metallic_roughness.roughness_factor;
		newMaterial.metallicFactor = material->pbr_metallic_roughness.metallic_factor;
	}
	m_materials.push_back(std::move(newMaterial));
}

void ModelLoader::addTexture(cgltf_data* data, cgltf_texture* texture) {
	Texture newTexture;
	if (!texture->sampler) {
		newTexture.sampler = m_fallbackSampler;
	} else {
		newTexture.sampler = m_textureSamplers[texture->sampler - data->samplers];
	}

	newTexture.image = m_textureImages[texture->image - data->images];

	m_textures.push_back(std::move(newTexture));
}

void ModelLoader::addImage(cgltf_data* data, cgltf_image* image, const std::string_view& gltfPath) {
	ImageData imageData;
	int numChannels = 4;
	if (image->buffer_view) {
		imageData.data = stbi_load_from_memory(
			reinterpret_cast<stbi_uc*>(image->buffer_view->buffer->data) + image->buffer_view->offset,
			image->buffer_view->size, &imageData.width, &imageData.height, &numChannels, numChannels);
		imageData.size = image->buffer_view->size;

	} else {
		std::filesystem::path path = std::filesystem::path(gltfPath);
		std::string directoryString = path.parent_path().string();
		directoryString.push_back(static_cast<char>(std::filesystem::path::preferred_separator));
		imageData.data = stbi_load((directoryString + image->uri).c_str(), &imageData.width, &imageData.height,
								   &numChannels, numChannels);
		imageData.size = imageData.width * imageData.height * 4;
	}
	m_combinedImageSize += imageData.size;
	m_maxImageSize = std::max(m_maxImageSize, m_imageData.size());
	m_imageData.push_back(imageData);

	VkImageCreateInfo imageCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_SRGB,
		.extent = { .width = static_cast<uint32_t>(imageData.width),
					.height = static_cast<uint32_t>(imageData.height),
					.depth = 1 },
		.mipLevels = static_cast<uint32_t>(log2(std::min(imageData.width, imageData.height))),
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	VkImage createdImage;
	verifyResult(vkCreateImage(m_device.device(), &imageCreateInfo, nullptr, &createdImage));
	m_allocator.bindDeviceImage(createdImage, 0);
	m_textureImages.push_back(createdImage);
}

void ModelLoader::addSampler(cgltf_data* data, cgltf_sampler* sampler) {
	VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	switch (sampler->wrap_s) {
		case 0x2901:
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case 0x8370:
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			break;
		case 0x812F:
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			break;
	}
	switch (sampler->wrap_t) {
		case 0x2901:
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case 0x8370:
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			break;
		case 0x812F:
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			break;
	}
	switch (sampler->mag_filter) {
		case 0x2600:
		case 0x2700:
		case 0x2702:
			samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
			break;
		case 0x2601:
		case 0x2701:
		case 0x2703:
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			break;
	}
	switch (sampler->min_filter) {
		case 0x2600:
		case 0x2700:
		case 0x2702:
			samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
			break;
		case 0x2601:
		case 0x2701:
		case 0x2703:
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			break;
	}
	switch (sampler->min_filter) {
		case 0x2700:
		case 0x2701:
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		default:
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}

	VkSampler resultSampler;
	vkCreateSampler(m_device.device(), &samplerCreateInfo, nullptr, &resultSampler);
	m_textureSamplers.push_back(resultSampler);
}
