#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <cmath>
#include <util/AccelerationStructureBuilder.hpp>
#include <util/ModelLoader.hpp>
#include <volk.h>

int main(int argc, const char** argv) {
	verifyResult(volkInitialize());
	glfwInit();

	RayTracingDevice device = RayTracingDevice(true);

	std::vector<std::string_view> gltfFilenames;
	gltfFilenames.reserve(argc - 1);

	for (int i = 1; i < argc; ++i) {
		gltfFilenames.push_back(std::string_view(argv[i]));
	}

	std::vector<Sphere> spheres = {
		{ .position = { -8.3395f, -0.76978f, -2.3374f, }, .radius = 0.5f, .color = { 0.8f, 0.6f, 0.6f, 100.0f } },
		{ .position = { 8.9656f, -0.76978f, -2.3374f }, .radius = 0.5f, .color = { 0.4f, 0.7f, 0.6f, 100.0f } },
		{ .position = { 0.73348522f, -3.92734623f, 1.05059690f }, .radius = 0.5f, .color = { 0.8f, 0.5f, 0.4f, 80.0f } }
	};

	MemoryAllocator allocator = MemoryAllocator(device);
	OneTimeDispatcher dispatcher = OneTimeDispatcher(device);
	ModelLoader loader = ModelLoader(device, allocator, dispatcher, gltfFilenames);
	AccelerationStructureBuilder builder =
		AccelerationStructureBuilder(device, allocator, dispatcher, loader, spheres, 0, 1);
	PipelineBuilder pipelineBuilder =
		PipelineBuilder(device, allocator, dispatcher,
						loader.textures().empty() ? VK_NULL_HANDLE : loader.textureDescriptorSetLayout(), 8);

	TriangleMeshRaytracer raytracer = TriangleMeshRaytracer(device, allocator, loader, pipelineBuilder, builder);

	while (raytracer.update()) {
	}
}