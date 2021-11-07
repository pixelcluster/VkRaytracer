#pragma once

#include <vector>
#define VK_NO_PROTOTYPES
#include <ErrorHelper.hpp>
#include <vulkan/vulkan.h>

template <typename HandleType, typename Enumerant, typename... AdditionalParams>
using AdditionalParameterEnumerationPFN = VkResult(VKAPI_PTR*)(HandleType type, AdditionalParams... params,
															   uint32_t* count, Enumerant* pEnumerants);
template <typename HandleType, typename Enumerant>
using EnumerationPFN = VkResult(VKAPI_PTR*)(HandleType type, uint32_t* count, Enumerant* pEnumerants);

template <typename HandleType, typename Enumerant, typename... AdditionalParams>
std::vector<Enumerant> enumerate(
	HandleType handle, AdditionalParams... params,
	AdditionalParameterEnumerationPFN<HandleType, Enumerant, AdditionalParams...> pfnEnumerator) {
	std::vector<Enumerant> values;
	uint32_t valueCount;
	VkResult enumerationResult;

	do {
		verifyResult(pfnEnumerator(handle, params..., &valueCount, nullptr));
		values.resize(valueCount);
		enumerationResult = pfnEnumerator(handle, params..., &valueCount, values.data());
	} while (enumerationResult == VK_INCOMPLETE);

	verifyResult(enumerationResult);
	return values;
}

template <typename HandleType, typename Enumerant>
std::vector<Enumerant> enumerate(HandleType handle, EnumerationPFN<HandleType, Enumerant> pfnEnumerator) {
	std::vector<Enumerant> values;
	uint32_t valueCount;
	VkResult enumerationResult;

	do {
		verifyResult(pfnEnumerator(handle, &valueCount, nullptr));
		values = std::vector<Enumerant>(valueCount);
		enumerationResult = pfnEnumerator(handle, &valueCount, values.data());
	} while (enumerationResult == VK_INCOMPLETE);

	verifyResult(enumerationResult);
	return values;
}