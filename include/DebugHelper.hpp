#pragma once

#define VK_NO_PROTOTYPES
#include <ErrorHelper.hpp>
#include <string>
#include <volk.h>
#include <vulkan/vulkan.h>

inline void setObjectName(VkDevice device, VkObjectType type, uint64_t handle, const std::string& name) {
	VkDebugUtilsObjectNameInfoEXT nameInfo = { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
											   .objectType = type,
											   .objectHandle = handle,
											   .pObjectName = name.c_str() };
	verifyResult(vkSetDebugUtilsObjectNameEXT(device, &nameInfo));
}