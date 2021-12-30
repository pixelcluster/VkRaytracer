#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <cmath>
#include <util/AccelerationStructureBuilder.hpp>
#include <util/ModelLoader.hpp>
#include <volk.h>

int main(int argc, const char** argv) {
	verifyResult(volkInitialize());
	glfwInit();

	RayTracingDevice device = RayTracingDevice(640, 480, true);

	std::vector<std::string_view> gltfFilenames;
	gltfFilenames.reserve(argc - 1);

	for (int i = 1; i < argc; ++i) {
		gltfFilenames.push_back(std::string_view(argv[i]));
	}

	std::vector<Sphere> spheres = {
		{ .position = { 0.0f, -4.0f, 0.0f }, .radius = 0.5f, .color = { 0.4f, 0.3f, 0.6f, 500.0f } }
	};

	MemoryAllocator allocator = MemoryAllocator(device);
	OneTimeDispatcher dispatcher = OneTimeDispatcher(device);
	ModelLoader loader = ModelLoader(device, allocator, dispatcher, gltfFilenames);
	AccelerationStructureBuilder builder = AccelerationStructureBuilder(
		device, allocator, dispatcher, loader,
		spheres, 0, 1);
	PipelineBuilder pipelineBuilder =
		PipelineBuilder(device, allocator, dispatcher, loader.textureDescriptorSetLayout(), 8);

	TriangleMeshRaytracer raytracer = TriangleMeshRaytracer(device, allocator, loader, pipelineBuilder, builder);

	while (raytracer.update()) {
	}
}