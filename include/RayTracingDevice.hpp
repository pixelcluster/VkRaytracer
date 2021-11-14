#pragma once

#include <Window.hpp>
#include <vector>

static constexpr bool enableDebugUtils = true;
static constexpr bool enableValidation = true;
static constexpr uint32_t frameInFlightCount = 3;

// Data provided to render one frame.
struct FrameData {
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

	VkImage swapchainImage = VK_NULL_HANDLE;
	VkImageView swapchainImageView = VK_NULL_HANDLE;
	uint32_t swapchainImageIndex = 0;
	uint32_t frameIndex = 0;
};


struct BufferAllocationRequirements {
	VkDeviceSize size, alignment;
	VkMemoryPropertyFlags memoryTypeBits;
	bool makeDedicatedAllocation;
};

class RayTracingDevice {
  public:
	RayTracingDevice(size_t windowWidth, size_t windowHeight, bool enableHardwareRaytracing);
	~RayTracingDevice();

	RayTracingDevice(const RayTracingDevice&) = delete;
	RayTracingDevice& operator=(const RayTracingDevice&) = delete;
	RayTracingDevice(RayTracingDevice&&) = default;
	RayTracingDevice& operator=(RayTracingDevice&&) = default;

	// Polls window and input events and returns frame data containing a newly acquired swapchain image index and a
	// command buffer (already begun) to record commands to. Elements will be zeroed on failure.
	FrameData beginFrame();
	// Ends the currently begun command buffer, submits it, and presents the acquired image.
	bool endFrame();

	VkDevice device() const { return m_device; }
	VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
	VkQueue queue() const { return m_queue; }
	uint32_t queueFamilyIndex() const { return m_queueFamilyIndex; }
	VkInstance instance() const { return m_instance; }

	uint32_t findBestMemoryIndex(VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred,
								 VkMemoryPropertyFlags forbidden);

	BufferAllocationRequirements requirements(VkBuffer buffer);

	void waitAllFences() const;

	const Window& window() const { return m_window; }

  private:
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

	VkPhysicalDeviceMemoryProperties m_memoryProperties;

	struct PerFrameData {
		VkCommandPool pool;
		VkCommandBuffer commandBuffer;
		VkSemaphore acquireDoneSemaphore;
		VkSemaphore presentReadySemaphore;
		VkFence fence;
	};

	PerFrameData m_perFrameData[frameInFlightCount];
	uint32_t m_currentFrameIndex = 0;
	uint32_t m_currentImageIndex = 0;
};