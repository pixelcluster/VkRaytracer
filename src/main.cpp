#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <volk.h>

int main() {
	verifyResult(volkInitialize());
	glfwInit();

	HardwareSphereRaytracer raytracer = HardwareSphereRaytracer(640, 480, 25);

	std::vector<Sphere> spheres = std::vector<Sphere>(25);

	for (size_t i = 0; i < 5; ++i) {
		for (size_t j = 0; j < 5; ++j) {
			float floatIndex = static_cast<float>(i) * 5.0f + static_cast<float>(j);
			spheres[j + i * 5] = Sphere{ .position = { 2.0f * i, 2.0f, 2.0f * j, 1.0f },
										 .radius = 0.5f,
										 .color = { sinf(floatIndex) * 2.0f - 1.0f, cosf(floatIndex) * 2.0f - 1.0f,
													cosf(floatIndex) * sinf(floatIndex) * 2.0f - 1.0f } };
		}
	}

	while (raytracer.update(spheres)) {
	}
}