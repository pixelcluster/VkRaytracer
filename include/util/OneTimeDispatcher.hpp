#pragma once

#include <RayTracingDevice.hpp>
#include <vector>

struct SubmittedCommandBuffer {
	VkCommandBuffer commandBuffer;
	VkFence fence;
};

class OneTimeDispatcher {
  public:
	OneTimeDispatcher(RayTracingDevice& device);
	OneTimeDispatcher(const OneTimeDispatcher& other) = delete;
	OneTimeDispatcher& operator=(const OneTimeDispatcher& other) = delete;
	OneTimeDispatcher(OneTimeDispatcher&& other) = default;
	OneTimeDispatcher& operator=(OneTimeDispatcher&& other) = default;
	~OneTimeDispatcher();

	std::vector<VkCommandBuffer> allocateOneTimeSubmitBuffers(uint32_t count);

	void submit(VkCommandBuffer submitCommandBuffer, const std::vector<VkSemaphore>& signalSemaphores);

	//true if the fence was signaled
	bool fenceStatus(VkCommandBuffer commandBuffer);

	//true if wait was successful (false for timeout)
	bool waitForFence(VkCommandBuffer commandBuffer, uint64_t timeout);

  private:
	RayTracingDevice& m_device;
	VkCommandPool m_commandPool;

	std::vector<SubmittedCommandBuffer> m_submittedCommandBuffers;
};