#pragma once

#define VK_NO_PROTOTYPES
#include <DebugHelper.hpp>
#include <ErrorHelper.hpp>
#include <fstream>
#include <volk.h>
#include <vulkan/vulkan.h>

inline VkShaderModule createShaderModule(VkDevice device, const std::string& filePath) {
	std::ifstream shaderBinaryStream = std::ifstream(filePath, std::ios::ate | std::ios::binary);
	assert(shaderBinaryStream.is_open());
	std::streampos codeEnd = shaderBinaryStream.tellg();
	shaderBinaryStream.seekg(std::ios::beg);
	size_t codeSize = codeEnd - shaderBinaryStream.tellg();

	uint32_t* code = new uint32_t[codeSize / sizeof(uint32_t)];
	shaderBinaryStream.read(reinterpret_cast<char*>(code), codeSize);

	VkShaderModule shaderModule;

	VkShaderModuleCreateInfo shaderModuleCreateInfo = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
														.codeSize = codeSize,
														.pCode = code };

	verifyResult(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule));
	delete[] code;
	return shaderModule;
}