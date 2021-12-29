#include <util/OneTimeDispatcher.hpp>
#include <volk.h>
#include <ErrorHelper.hpp>

OneTimeDispatcher::OneTimeDispatcher(RayTracingDevice& device) : m_device(device) {
	VkCommandPoolCreateInfo poolCreateInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
											   .queueFamilyIndex = m_device.queueFamilyIndex() };
	verifyResult(vkCreateCommandPool(m_device.device(), &poolCreateInfo, nullptr, &m_commandPool));
}

OneTimeDispatcher::~OneTimeDispatcher() { vkDestroyCommandPool(m_device.device(), m_commandPool, nullptr); }

std::vector<VkCommandBuffer> OneTimeDispatcher::allocateOneTimeSubmitBuffers(uint32_t count) {
	std::vector<VkCommandBuffer> commandBuffers = std::vector<VkCommandBuffer>(count);
	VkCommandBufferAllocateInfo allocateInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
												 .commandPool = m_commandPool,
												 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												 .commandBufferCount = count };
	verifyResult(vkAllocateCommandBuffers(m_device.device(), &allocateInfo, commandBuffers.data()));
	return commandBuffers;
}

void OneTimeDispatcher::submit(VkCommandBuffer submitCommandBuffer, const std::vector<VkSemaphore>& signalSemaphores) {
	VkFence fence;
	VkFenceCreateInfo fenceCreateInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	vkCreateFence(m_device.device(), &fenceCreateInfo, nullptr, &fence);

	VkSubmitInfo submitInfo = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
								.commandBufferCount = 1,
								.pCommandBuffers = &submitCommandBuffer,
								.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
								.pSignalSemaphores = signalSemaphores.data() };
	verifyResult(vkQueueSubmit(m_device.queue(), 1, &submitInfo, fence));

	m_submittedCommandBuffers.push_back({ submitCommandBuffer, fence });
}

bool OneTimeDispatcher::fenceStatus(VkCommandBuffer commandBuffer) { 
	auto submittedArrayIterator =
		std::find_if(m_submittedCommandBuffers.begin(), m_submittedCommandBuffers.end(),
					 [commandBuffer](auto& structure) { return structure.commandBuffer == commandBuffer; });
	if (submittedArrayIterator == m_submittedCommandBuffers.end())
		return true;
	VkResult fenceStatus = vkGetFenceStatus(m_device.device(), submittedArrayIterator->fence);
	verifyResult(fenceStatus);
	
	return fenceStatus == VK_SUCCESS;
}

bool OneTimeDispatcher::waitForFence(VkCommandBuffer commandBuffer, uint64_t timeout) {
	auto submittedArrayIterator =
		std::find_if(m_submittedCommandBuffers.begin(), m_submittedCommandBuffers.end(),
					 [commandBuffer](auto& structure) { return structure.commandBuffer == commandBuffer; });
	if (submittedArrayIterator == m_submittedCommandBuffers.end())
		return true;
	VkResult fenceStatus = vkWaitForFences(m_device.device(), 1, &submittedArrayIterator->fence, VK_TRUE, timeout);
	verifyResult(fenceStatus);

	vkDestroyFence(m_device.device(), submittedArrayIterator->fence, nullptr);
	m_submittedCommandBuffers.erase(submittedArrayIterator);

	return fenceStatus == VK_SUCCESS;
}
