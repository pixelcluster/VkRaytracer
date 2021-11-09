#include <RayTracingDevice.hpp>
#include <SwapchainHelper.hpp>
#include <algorithm>
#include <cstring>
#include <exception>
#include <iostream>
#include <numeric>
#include <optional>
#include <string>
#include <volk.h>

class DeviceNotFoundException : public std::exception {
  public:
	const char* what() const noexcept override { return "No device with the specified features was found!"; }
};

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
													VkDebugUtilsMessageTypeFlagsEXT messageTypes,
													const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
													void* pUserData) {
	std::cout << pCallbackData->pMessage << '\n';
	return VK_FALSE;
}

RayTracingDevice::RayTracingDevice(size_t windowWidth, size_t windowHeight, bool enableHardwareRaytracing)
	: m_window("Vulkan Hardware Ray Tracing Test", windowWidth, windowHeight) {
	VkApplicationInfo info = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
							   .pApplicationName = "Vulkan Raytracer",
							   .applicationVersion = 1,
							   .apiVersion = VK_API_VERSION_1_2 };

	uint32_t requiredExtensionCount;
	const char** extensions = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);

	std::vector<const char*> extensionNames;
	extensionNames.reserve(requiredExtensionCount + enableDebugUtils);

	for (uint32_t i = 0; i < requiredExtensionCount; ++i) {
		extensionNames.push_back(extensions[i]);
	}

	if constexpr (enableDebugUtils) {
		extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	std::vector<const char*> instanceLayerNames;
	instanceLayerNames.reserve(enableValidation);

	if constexpr (enableValidation) {
		instanceLayerNames.push_back("VK_LAYER_KHRONOS_validation");
	}

	VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
		.pfnUserCallback = debugMessageCallback
	};

	VkInstanceCreateInfo instanceCreateInfo = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
												.pNext = &messengerCreateInfo,
												.pApplicationInfo = &info,
												.enabledLayerCount = static_cast<uint32_t>(instanceLayerNames.size()),
												.ppEnabledLayerNames = instanceLayerNames.data(),
												.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
												.ppEnabledExtensionNames = extensionNames.data() };

	verifyResult(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

	volkLoadInstanceOnly(m_instance);

	verifyResult(vkCreateDebugUtilsMessengerEXT(m_instance, &messengerCreateInfo, nullptr, &m_messenger));

	m_surface = m_window.createSurface(m_instance);

	std::vector<VkPhysicalDevice> devices =
		enumerate<VkInstance, VkPhysicalDevice>(m_instance, vkEnumeratePhysicalDevices);
	std::optional<VkPhysicalDevice> chosenDevice;

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo deviceQueueCreateInfo;

	for (auto& device : devices) {
		std::vector<VkExtensionProperties> extensions = enumerate<VkPhysicalDevice, VkExtensionProperties, const char*>(
			device, nullptr, vkEnumerateDeviceExtensionProperties);

		auto accelerationStructureIterator = std::find_if(extensions.begin(), extensions.end(), [](auto& properties) {
			return std::strcmp(properties.extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0;
		});

		auto rayTracingIterator = std::find_if(extensions.begin(), extensions.end(), [](auto& properties) {
			return std::strcmp(properties.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0;
		});

		auto swapchainIterator = std::find_if(extensions.begin(), extensions.end(), [](auto& properties) {
			return std::strcmp(properties.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
		});

		if ((accelerationStructureIterator == extensions.end() || rayTracingIterator == extensions.end()) &&
				enableHardwareRaytracing ||
			swapchainIterator == extensions.end())
			continue;

		if (enableHardwareRaytracing) {
			VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
			};
			VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
				.pNext = &rayTracingFeatures
			};
			VkPhysicalDeviceFeatures2 features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
												   .pNext = &accelerationStructureFeatures };

			vkGetPhysicalDeviceFeatures2(device, &features);

			if (!accelerationStructureFeatures.accelerationStructure || !rayTracingFeatures.rayTracingPipeline) {
				continue;
			}
		}

		std::vector<VkQueueFamilyProperties> queueFamilyProperties;
		uint32_t familyPropertyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &familyPropertyCount, nullptr);
		queueFamilyProperties.resize(familyPropertyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &familyPropertyCount, queueFamilyProperties.data());

		bool foundQueue = false;

		for (size_t i = 0; i < queueFamilyProperties.size(); ++i) {
			auto& properties = queueFamilyProperties[i];
			VkBool32 surfaceSupport;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &surfaceSupport);

			if ((properties.queueFlags & VK_QUEUE_COMPUTE_BIT) && surfaceSupport) {
				deviceQueueCreateInfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
										  .queueFamilyIndex = static_cast<uint32_t>(i),
										  .queueCount = 1,
										  .pQueuePriorities = &queuePriority };
				m_queueFamilyIndex = static_cast<uint32_t>(i);
				foundQueue = true;
				break;
			}
		}

		if (foundQueue) {
			chosenDevice = device;
			break;
		}
	}

	if (!chosenDevice.has_value())
		throw DeviceNotFoundException();

	m_physicalDevice = chosenDevice.value();

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, .rayTracingPipeline = VK_TRUE
	};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
		.pNext = &rayTracingFeatures,
		.accelerationStructure = VK_TRUE
	};
	VkPhysicalDeviceFeatures2 features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
										   .pNext =
											   enableHardwareRaytracing ? &accelerationStructureFeatures : nullptr };

	std::vector<const char*> deviceExtensionNames;
	if (enableHardwareRaytracing) {
		deviceExtensionNames.reserve(4);
		deviceExtensionNames.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		deviceExtensionNames.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		deviceExtensionNames.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	} else {
		deviceExtensionNames.reserve(1);
	}
	deviceExtensionNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	VkDeviceCreateInfo deviceCreateInfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
											.pNext = &features,
											.queueCreateInfoCount = 1,
											.pQueueCreateInfos = &deviceQueueCreateInfo,
											.enabledLayerCount = static_cast<uint32_t>(instanceLayerNames.size()),
											.ppEnabledLayerNames = instanceLayerNames.data(),
											.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionNames.size()),
											.ppEnabledExtensionNames = deviceExtensionNames.data() };

	verifyResult(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));

	volkLoadDevice(m_device);

	vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);

	if constexpr (enableDebugUtils) {
		setObjectName(m_device, VK_OBJECT_TYPE_DEVICE, reinterpret_cast<uint64_t>(m_device), "Main device");
		setObjectName(m_device, VK_OBJECT_TYPE_SURFACE_KHR, reinterpret_cast<uint64_t>(m_surface),
					  "Presentation surface");
		setObjectName(m_device, VK_OBJECT_TYPE_PHYSICAL_DEVICE, reinterpret_cast<uint64_t>(m_physicalDevice),
					  "Chosen physical device");
		setObjectName(m_device, VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64_t>(m_queue),
					  "Main ray-tracing/compute queue");
	}

	for (size_t i = 0; i < frameInFlightCount; ++i) {
		createPerFrameData(i);
	}

	m_swapchain = createSwapchain(m_physicalDevice, m_device, m_surface,
								  { static_cast<uint32_t>(m_window.width()), static_cast<uint32_t>(m_window.height()) },
								  VK_NULL_HANDLE, enableDebugUtils);

	createSwapchainResources();

	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);
}

RayTracingDevice::~RayTracingDevice() {
	VkFence fences[frameInFlightCount];
	for (size_t i = 0; i < frameInFlightCount; ++i) {
		fences[i] = m_perFrameData[i].fence;
	}
	verifyResult(vkWaitForFences(m_device, frameInFlightCount, fences, VK_TRUE, UINT64_MAX));

	for (size_t i = 0; i < frameInFlightCount; ++i) {
		vkDestroyFence(m_device, m_perFrameData[i].fence, nullptr);
		vkDestroySemaphore(m_device, m_perFrameData[i].presentReadySemaphore, nullptr);
		vkDestroySemaphore(m_device, m_perFrameData[i].acquireDoneSemaphore, nullptr);
		vkDestroyCommandPool(m_device, m_perFrameData[i].pool, nullptr);
	}

	for (auto& view : m_swapchainViews) {
		vkDestroyImageView(m_device, view, nullptr);
	}

	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	vkDestroyDevice(m_device, nullptr);

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

	vkDestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);

	vkDestroyInstance(m_instance, nullptr);
}

void RayTracingDevice::createPerFrameData(size_t index) {
	VkCommandPool pool;
	VkCommandBuffer commandBuffer;

	VkCommandPoolCreateInfo poolCreateInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
											   .queueFamilyIndex = m_queueFamilyIndex };
	verifyResult(vkCreateCommandPool(m_device, &poolCreateInfo, nullptr, &pool));

	VkCommandBufferAllocateInfo bufferAllocateInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
													   .commandPool = pool,
													   .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
													   .commandBufferCount = 1 };
	verifyResult(vkAllocateCommandBuffers(m_device, &bufferAllocateInfo, &commandBuffer));

	VkSemaphore acquireDoneSemaphore;
	VkSemaphore presentReadySemaphore;

	VkSemaphoreCreateInfo semaphoreCreateInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	verifyResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &acquireDoneSemaphore));
	verifyResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &presentReadySemaphore));

	VkFence fence;
	VkFenceCreateInfo fenceCreateInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
										  .flags = VK_FENCE_CREATE_SIGNALED_BIT };
	verifyResult(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &fence));

	PerFrameData data = PerFrameData{ .pool = pool,
									  .commandBuffer = commandBuffer,
									  .acquireDoneSemaphore = acquireDoneSemaphore,
									  .presentReadySemaphore = presentReadySemaphore,
									  .fence = fence };

	if constexpr (enableDebugUtils) {
		std::string indexString = std::to_string(index);

		setObjectName(m_device, VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64_t>(pool),
					  "Command pool for frame " + indexString + " in flight");
		setObjectName(m_device, VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<uint64_t>(commandBuffer),
					  "Command buffer for frame " + indexString + " in flight");
		setObjectName(m_device, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(acquireDoneSemaphore),
					  "Image acquire wait semaphore for frame " + indexString + " in flight");
		setObjectName(m_device, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(presentReadySemaphore),
					  "Present signal semaphore for frame " + indexString + " in flight");
		setObjectName(m_device, VK_OBJECT_TYPE_FENCE, reinterpret_cast<uint64_t>(fence),
					  "Fence for frame " + indexString + " in flight");
	}
	m_perFrameData[index] = data;
}

void RayTracingDevice::createSwapchainResources() {
	for (auto& view : m_swapchainViews) {
		vkDestroyImageView(m_device, view, nullptr);
	}
	m_swapchainViews.clear();

	m_swapchainImages = enumerate<VkDevice, VkImage, VkSwapchainKHR>(m_device, m_swapchain, vkGetSwapchainImagesKHR);
	m_swapchainViews.reserve(m_swapchainImages.size());

	size_t imageIndexCounter = 0;
	for (auto& image : m_swapchainImages) {
		VkImageView view;

		VkImageViewCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
											 .image = image,
											 .viewType = VK_IMAGE_VIEW_TYPE_2D,
											 .format = VK_FORMAT_B8G8R8A8_UNORM,
											 .components = {
												 .r = VK_COMPONENT_SWIZZLE_IDENTITY,
												 .g = VK_COMPONENT_SWIZZLE_IDENTITY,
												 .b = VK_COMPONENT_SWIZZLE_IDENTITY,
												 .a = VK_COMPONENT_SWIZZLE_IDENTITY,
											 },
											.subresourceRange = { 
												.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
												.baseMipLevel = 0,
												.levelCount = 1,
												.baseArrayLayer = 0,
												.layerCount = 1 } };

		verifyResult(vkCreateImageView(m_device, &createInfo, nullptr, &view));

		if constexpr (enableDebugUtils) {
			setObjectName(m_device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image),
						  std::string("Swapchain image") + std::to_string(imageIndexCounter++).c_str());
			setObjectName(m_device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(view),
						  std::string("Swapchain image view") + std::to_string(imageIndexCounter++).c_str());
		}
		m_swapchainViews.push_back(view);
	}
}

bool RayTracingDevice::canRecreateSwapchain() {
	VkSurfaceCapabilitiesKHR capabilities;
	verifyResult(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities));
	return capabilities.maxImageExtent.width != 0 && capabilities.maxImageExtent.height != 0;
}

FrameData RayTracingDevice::beginFrame() {
	if (m_isSwapchainGood) {
		m_window.pollEvents();
	} else {
		m_window.waitEvents();
	}

	if (m_window.windowSizeDirty() || !m_isSwapchainGood) {
		if (!canRecreateSwapchain()) {
			m_isSwapchainGood = false;
			return FrameData{};
		}

		if (!m_isSwapchainGood) {
			// Swapchain recreation might start working in between the call to waitEvents and canRecreateSwapchain, so
			// the window size needs to be updated
			m_window.pollEvents();
		}

		m_swapchain =
			createSwapchain(m_physicalDevice, m_device, m_surface,
							{ static_cast<uint32_t>(m_window.width()), static_cast<uint32_t>(m_window.height()) },
							m_swapchain, enableDebugUtils);
		createSwapchainResources();
	}

	VkResult acquireResult = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
												   m_perFrameData[m_currentFrameIndex].acquireDoneSemaphore,
												   VK_NULL_HANDLE, &m_currentImageIndex);

	if (acquireResult == VK_SUBOPTIMAL_KHR) {
		m_swapchain =
			createSwapchain(m_physicalDevice, m_device, m_surface,
							{ static_cast<uint32_t>(m_window.width()), static_cast<uint32_t>(m_window.height()) },
							m_swapchain, enableDebugUtils);
		createSwapchainResources();
	} else if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
		m_isSwapchainGood = false;
		return FrameData{};
	} else {
		verifyResult(acquireResult);
	}

	m_isSwapchainGood = true;

	verifyResult(vkWaitForFences(m_device, 1, &m_perFrameData[m_currentFrameIndex].fence, VK_TRUE, 100000000));
	verifyResult(vkResetFences(m_device, 1, &m_perFrameData[m_currentFrameIndex].fence));

	verifyResult(vkResetCommandPool(m_device, m_perFrameData[m_currentFrameIndex].pool, 0));

	VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	verifyResult(vkBeginCommandBuffer(m_perFrameData[m_currentFrameIndex].commandBuffer, &beginInfo));

	return FrameData{ .commandBuffer = m_perFrameData[m_currentFrameIndex].commandBuffer,
					  .swapchainImage = m_swapchainImages[m_currentImageIndex],
					  .swapchainImageView = m_swapchainViews[m_currentImageIndex],
					  .swapchainImageIndex = m_currentImageIndex };
}

bool RayTracingDevice::endFrame() {
	verifyResult(vkEndCommandBuffer(m_perFrameData[m_currentFrameIndex].commandBuffer));

	VkPipelineStageFlags waitStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;

	VkSubmitInfo submitInfo = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
								.waitSemaphoreCount = 1,
								.pWaitSemaphores = &m_perFrameData[m_currentFrameIndex].acquireDoneSemaphore,
								.pWaitDstStageMask = &waitStageFlags,
								.commandBufferCount = 1,
								.pCommandBuffers = &m_perFrameData[m_currentFrameIndex].commandBuffer,
								.signalSemaphoreCount = 1,
								.pSignalSemaphores = &m_perFrameData[m_currentFrameIndex].presentReadySemaphore };

	verifyResult(vkQueueSubmit(m_queue, 1, &submitInfo, m_perFrameData[m_currentFrameIndex].fence));

	VkResult presentResult;

	VkPresentInfoKHR presentInfo = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
									 .waitSemaphoreCount = 1,
									 .pWaitSemaphores = &m_perFrameData[m_currentFrameIndex].presentReadySemaphore,
									 .swapchainCount = 1,
									 .pSwapchains = &m_swapchain,
									 .pImageIndices = &m_currentImageIndex,
									 .pResults = &presentResult };

	verifyResult(vkQueuePresentKHR(m_queue, &presentInfo));
	verifyResult(presentResult);

	++m_currentFrameIndex %= frameInFlightCount;

	if (presentResult == VK_SUBOPTIMAL_KHR) {
		m_swapchain =
			createSwapchain(m_physicalDevice, m_device, m_surface,
							{ static_cast<uint32_t>(m_window.width()), static_cast<uint32_t>(m_window.height()) },
							m_swapchain, enableDebugUtils);
		createSwapchainResources();
	} else if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		m_isSwapchainGood = false;
	}

	++m_currentFrameIndex %= frameInFlightCount;
	return !m_window.shouldWindowClose();
}

uint32_t RayTracingDevice::findBestMemoryIndex(VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred,
											   VkMemoryPropertyFlags forbidden) {
	uint32_t bestFittingIndex = -1U;
	uint32_t numMatchingPreferredFlags = 0;
	uint32_t numUnrelatedFlags = -1U;

	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i) {
		if (!(m_memoryProperties.memoryTypes[i].propertyFlags & required) ||
			(m_memoryProperties.memoryTypes[i].propertyFlags & forbidden)) {
			continue;
		}

		VkMemoryPropertyFlags matchingPreferredFlags = m_memoryProperties.memoryTypes[i].propertyFlags & preferred;
		VkMemoryPropertyFlags unrelatedFlags =
			m_memoryProperties.memoryTypes[i].propertyFlags & ~(required | preferred);

		uint32_t setPreferredBitCount = std::popcount(matchingPreferredFlags);
		uint32_t setUnrelatedBitCount = std::popcount(unrelatedFlags);

		if (setPreferredBitCount > numMatchingPreferredFlags ||
			setPreferredBitCount == numMatchingPreferredFlags && setUnrelatedBitCount < numUnrelatedFlags) {
			bestFittingIndex = i;
			numMatchingPreferredFlags = setPreferredBitCount;
			numUnrelatedFlags = setUnrelatedBitCount;
		}
	}

	return bestFittingIndex;
}

BufferAllocationRequirements RayTracingDevice::requirements(VkBuffer buffer) {
	VkMemoryDedicatedRequirements dedicatedRequirements = { .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS };

	VkMemoryRequirements2 memoryRequirements = { .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
												 .pNext = &dedicatedRequirements };

	VkBufferMemoryRequirementsInfo2 memoryRequirementsInfo = { .sType =
																   VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
															   .buffer = buffer };

	vkGetBufferMemoryRequirements2(m_device, &memoryRequirementsInfo, &memoryRequirements);

	return BufferAllocationRequirements{ .size = memoryRequirements.memoryRequirements.size,
										 .alignment = memoryRequirements.memoryRequirements.alignment,
										 .memoryTypeBits = memoryRequirements.memoryRequirements.memoryTypeBits,
										 .makeDedicatedAllocation = dedicatedRequirements.requiresDedicatedAllocation ||
																	dedicatedRequirements.prefersDedicatedAllocation };
}

void RayTracingDevice::allocateDedicated(BufferAllocation& allocation, VkDeviceSize requiredSize,
										 VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred,
										 VkMemoryPropertyFlags forbidden) {
	VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
															.buffer = allocation.buffer };
	VkMemoryAllocateInfo allocateInfo = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										  .pNext = &dedicatedAllocateInfo,
										  .allocationSize = requiredSize,
										  .memoryTypeIndex = findBestMemoryIndex(required, preferred, forbidden) };

	vkAllocateMemory(m_device, &allocateInfo, nullptr, &allocation.dedicatedMemory);

	vkBindBufferMemory(m_device, allocation.buffer, allocation.dedicatedMemory, 0);
}