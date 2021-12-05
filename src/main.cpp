#include <ErrorHelper.hpp>
#include <Raytracer.hpp>
#include <volk.h>
#include <cmath>

int main() {
	verifyResult(volkInitialize());
	glfwInit();

	std::vector<Sphere> spheres = std::vector<Sphere>(15 * 15);
	std::vector<size_t> lightSphereIndices = std::vector<size_t>(15 * 15 / 2);

	for(size_t i = 0, j = 0; i < 15 * 15 && j < 15 * 15 / 2; ++i) {
		if(i  % 2) {
			lightSphereIndices[j] = i;
			++j;
		}
	}

	HardwareSphereRaytracer raytracer = HardwareSphereRaytracer(640, 480, spheres.size(), { 0, 19 });

	for (size_t i = 0; i < 15; ++i) {
		for (size_t j = 0; j < 15; ++j) {
			float floatIndex = static_cast<float>(i) * 15.0f + static_cast<float>(j) + 0.8;
			spheres[j + i * 15] = Sphere{ .position = { 2.0f * i, -20.0f, 2.0f * j, 1.0f },
										 .radius = 2.0f,
										 .color = { sinf(floatIndex) * 2.0f - 1.0f, cosf(floatIndex) * 2.0f - 1.0f,
													cosf(floatIndex) * sinf(floatIndex) * 2.0f - 1.0f, 0.5f } };
		}
	}

	while (raytracer.update(spheres)) {
		 for (size_t i = 0; i < 15; ++i) {
			for (size_t j = 0; j < 15; ++j) {
				float floatIndex = static_cast<float>(i) * 15.0f + static_cast<float>(j) + 0.8;
				float y = 2.0f + (j % 2 ? sinf(glfwGetTime() + j * 0.1) : cosf(glfwGetTime() + j * 0.1));
				spheres[j + i * 15] = Sphere{ .position = { 2.0f * i, -y, 2.0f * j, 1.0f },
											 .radius = 0.5f,
											 .color = { static_cast<float>(fabs(sinf(floatIndex))), static_cast<float>(fabs(cosf(floatIndex))),
														static_cast<float>(fabs(cosf(floatIndex) * sinf(floatIndex))), 0.5f } };
				if (j + i == 0 || (j + i * 15) == 19) { // if ((j + i * 50) % 2) {
					//negative color.a indicates radiance
					spheres[j + i * 15].position[0] += 10.0f;
					spheres[j + i * 15].position[1] -= 4.0f;
					spheres[j + i * 15].position[2] += 10.0f;
					spheres[j + i * 15].color[3] = -50000.0f;
				}
			}
		}
	}
}