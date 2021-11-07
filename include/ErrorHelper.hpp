#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <cassert>

inline void verifyResult(VkResult result) { assert(result >= 0); }