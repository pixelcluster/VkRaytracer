#include <ErrorHelper.hpp>
//#include <Raytracer.hpp>
#include <cmath>
#include <util/ModelLoader.hpp>
#include <util/AccelerationStructureBuilder.hpp>
#include <volk.h>

int main(int argc, const char** argv) {
	verifyResult(volkInitialize());
	glfwInit();

	std::vector<Sphere> spheres = std::vector<Sphere>(15 * 15);
	std::vector<size_t> lightSphereIndices = std::vector<size_t>(15 * 15 / 2);

	for (size_t i = 0, j = 0; i < 15 * 15 && j < 15 * 15 / 2; ++i) {
		if (i % 2) {
			lightSphereIndices[j] = i;
			++j;
		}
	}

	//HardwareSphereRaytracer raytracer = HardwareSphereRaytracer(640, 480, spheres.size(), { 0, 3 });

	RayTracingDevice device = RayTracingDevice(640, 480, true);

	std::vector<std::string_view> gltfFilenames;
	gltfFilenames.reserve(argc - 1);

	for (int i = 1; i < argc; ++i) {
		gltfFilenames.push_back(std::string_view(argv[i]));
	}

	MemoryAllocator allocator = MemoryAllocator(device);
	OneTimeDispatcher dispatcher = OneTimeDispatcher(device);
	ModelLoader loader = ModelLoader(device, allocator, dispatcher, gltfFilenames);

	AccelerationStructureBuilder builder =
		AccelerationStructureBuilder(device, allocator, dispatcher, loader, { {} }, 0, 0);

	for (size_t i = 0; i < 15; ++i) {
		for (size_t j = 0; j < 15; ++j) {
			double time = 0.f;
			// glfwGetTime();

			float floatIndex = static_cast<float>(i) * 15.0f + static_cast<float>(j) + 0.8;
			float y = 0.54f; //+(j % 2 ? sinf(time + j * 0.1) : cosf(time + j * 0.1));
			spheres[j + i * 15] = Sphere{ .position = { 2.0f * i, -y, 2.0f * j },
										  .radius = 0.5f,
										  .color = { static_cast<float>(fabs(sinf(floatIndex - 0.3f))),
													 static_cast<float>(fabs(cosf(floatIndex))),
													 static_cast<float>(fabs(sinf(floatIndex - 2.31f))), 0.5f } };

			if (j + i == 0 || (j + i * 15) == 3) { // if ((j + i * 50) % 2) {
				// negative color.a indicates radiance
				spheres[j + i * 15].position[0] += 10.0f;
				spheres[j + i * 15].position[1] -= 3.4f;
				spheres[j + i * 15].position[2] += 10.0f;
				spheres[j + i * 15].color[2] /= 100.0f;
				spheres[j + i * 15].color[3] = -200.0f;
			}

			float colorLength = sqrtf(spheres[j + i * 15].color[0] * spheres[j + i * 15].color[0] +
									  spheres[j + i * 15].color[1] * spheres[j + i * 15].color[1] +
									  spheres[j + i * 15].color[2] * spheres[j + i * 15].color[2]);

			spheres[j + i * 15].color[0] /= colorLength;
			spheres[j + i * 15].color[1] /= colorLength;
			spheres[j + i * 15].color[2] /= colorLength;
		}
	}

	while (!device.window().shouldWindowClose()) {
		device.beginFrame();
		device.endFrame();
	}
}