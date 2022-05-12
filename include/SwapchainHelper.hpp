#pragma once

#define VK_NO_PROTOTYPES
#include <DebugHelper.hpp>
#include <EnumerationHelper.hpp>
#include <ErrorHelper.hpp>
#include <vector>
#include <volk.h>
#include <vulkan/vulkan.h>
#include <algorithm>

VkSwapchainKHR createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
							   VkExtent2D imageExtent, VkSwapchainKHR oldSwapchain, bool nameSwapchain) {
	VkSurfaceCapabilitiesKHR capabilities;
	verifyResult(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities));

	if(!(capabilities.minImageExtent.width <= imageExtent.width &&
		   capabilities.minImageExtent.height <= imageExtent.height &&
		   capabilities.maxImageExtent.width >= imageExtent.width &&
		   capabilities.maxImageExtent.height >= imageExtent.height)) return oldSwapchain;

	size_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount && imageCount > capabilities.maxImageCount) {
		imageCount = capabilities.maxImageCount;
	}

	std::vector<VkPresentModeKHR> presentModes = enumerate<VkPhysicalDevice, VkPresentModeKHR, VkSurfaceKHR>(
		physicalDevice, surface, vkGetPhysicalDeviceSurfacePresentModesKHR);

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end()) {
		// presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
													 .surface = surface,
													 .minImageCount = static_cast<uint32_t>(imageCount),
													 .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
													 .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
													 .imageExtent = imageExtent,
													 .imageArrayLayers = 1,
													 .imageUsage = VK_IMAGE_USAGE_STORAGE_BIT,
													 .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
													 .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
													 .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
													 .presentMode = presentMode,
													 .clipped = VK_FALSE,
													 .oldSwapchain = oldSwapchain };
	VkSwapchainKHR swapchain;
	verifyResult(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain));

	if (nameSwapchain)
		setObjectName(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, swapchain,
					  "Main presentation swapchain");

	if (oldSwapchain) {
		vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
	}

	return swapchain;
}