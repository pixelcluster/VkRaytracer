#pragma once

#include <Window.hpp>
#include <vector>

class HardwareRaytracer {
  public:
	HardwareRaytracer(size_t windowWidth, size_t windowHeight);

	HardwareRaytracer(const HardwareRaytracer&) = delete;
	HardwareRaytracer& operator=(const HardwareRaytracer&) = delete;
	HardwareRaytracer(HardwareRaytracer&&) = default;
	HardwareRaytracer& operator=(HardwareRaytracer&&) = default;

	~HardwareRaytracer();

	bool update();

  private:
	static constexpr bool m_enableDebugUtils = true;
	static constexpr bool m_enableValidation = true;
	static constexpr uint32_t m_frameInFlightCount = 3;

	void createPerFrameData(size_t index);
	void createSwapchainResources();

	bool canRecreateSwapchain();

	Window m_window;

	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_messenger;

	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;
	uint32_t m_queueFamilyIndex;
	VkQueue m_queue;

	VkSurfaceKHR m_surface;
	VkSwapchainKHR m_swapchain;
	bool m_isSwapchainGood = true;
	std::vector<VkImageView> m_swapchainViews;
	std::vector<VkImage> m_swapchainImages;

	struct PerFrameData {
		VkCommandPool pool;
		VkCommandBuffer commandBuffer;
		VkSemaphore acquireDoneSemaphore;
		VkSemaphore presentReadySemaphore;
		VkFence fence;
	};

	PerFrameData m_perFrameData[m_frameInFlightCount];
	uint32_t m_currentFrameIndex = 0;
};