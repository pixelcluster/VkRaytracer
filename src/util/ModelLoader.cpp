#include <util/ModelLoader.hpp>



ModelLoader::ModelLoader(RayTracingDevice& device, MemoryAllocator& allocator, const std::string_view& gltfFilename)
	: m_device(device), m_allocator(allocator) {}

ModelLoader::~ModelLoader() {}
