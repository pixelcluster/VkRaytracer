#include <Raytracer.hpp>
#include <volk.h>
#include <ErrorHelper.hpp>


int main() {
	verifyResult(volkInitialize());

	HardwareSphereRaytracer raytracer = HardwareSphereRaytracer(640, 480);

	while (raytracer.update()) {
	}
}