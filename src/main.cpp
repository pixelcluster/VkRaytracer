#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <volk.h>
#include <cmath>

int main() {
	verifyResult(volkInitialize());
	glfwInit();

	std::vector<Sphere> spheres = std::vector<Sphere>(50 * 50);

	HardwareSphereRaytracer raytracer = HardwareSphereRaytracer(640, 480, spheres.size());

	for (size_t i = 0; i < 50; ++i) {
		for (size_t j = 0; j < 50; ++j) {
			float floatIndex = static_cast<float>(i) * 50.0f + static_cast<float>(j);
			spheres[j + i * 50] = Sphere{ .position = { 2.0f * i, -2.0f, 2.0f * j, 1.0f },
										 .radius = 0.5f,
										 .color = { sinf(floatIndex) * 2.0f - 1.0f, cosf(floatIndex) * 2.0f - 1.0f,
													cosf(floatIndex) * sinf(floatIndex) * 2.0f - 1.0f, 0.5f } };
		}
	}

	while (raytracer.update(spheres)) {
		 for (size_t i = 0; i < 50; ++i) {
			for (size_t j = 0; j < 50; ++j) {
				float floatIndex = static_cast<float>(i) * 50.0f + static_cast<float>(j);
				float y = 2.0f + (j % 2 ? sinf(glfwGetTime() + j * 0.1) : cosf(glfwGetTime() + j * 0.1));
				spheres[j + i * 50] = Sphere{ .position = { 2.0f * i, -y, 2.0f * j, 1.0f },
											 .radius = 0.5f,
											 .color = { fabs(sinf(floatIndex)), fabs(cosf(floatIndex)),
														fabs(cosf(floatIndex) * sinf(floatIndex)), 0.5f } };
			}
		}
	}
}