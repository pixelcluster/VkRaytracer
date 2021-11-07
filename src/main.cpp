#include <Raytracer.hpp>
#include <volk.h>
#include <ErrorHelper.hpp>


int main() {
	verifyResult(volkInitialize());

	HardwareRaytracer raytracer = HardwareRaytracer(640, 480);

	while (raytracer.update()) {
	}
}