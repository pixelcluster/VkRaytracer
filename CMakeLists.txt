cmake_minimum_required(VERSION 3.15)

project(VkRaytracer VERSION 0.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(dependencies/glfw)
add_subdirectory(dependencies/volk)

if(UNIX AND NOT APPLE)
	option(USE_WAYLAND "Use Wayland instead of X11")
	if(USE_WAYLAND)
		set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
		set(GLFW_USE_WAYLAND TRUE)
	endif()
elseif(WIN32)
	set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()

file(GLOB_RECURSE CPP_SOURCES CONFIGURE_DEPENDS 
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

find_package(Vulkan REQUIRED FATAL_ERROR)

add_executable(VkRaytracer ${CPP_SOURCES} ${HEADERS})

target_include_directories(VkRaytracer PUBLIC "include" Vulkan_INCLUDE_DIRS)

target_link_libraries(VkRaytracer glfw volk::volk)