#include <ErrorHelper.hpp>
#include <Window.hpp>

bool Window::m_didInit = false;

Window::Window(const std::string_view& windowName) {
	if (!m_didInit)
		glfwInit();
	glfwWindowHint(GLFW_STICKY_KEYS, GLFW_TRUE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	const GLFWvidmode* primaryMonitorVidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	m_window = glfwCreateWindow(primaryMonitorVidmode->width, primaryMonitorVidmode->height, windowName.data(), monitor,
								nullptr);

	m_width = static_cast<size_t>(primaryMonitorVidmode->width);
	m_height = static_cast<size_t>(primaryMonitorVidmode->height);
}

Window::Window(const std::string_view& windowName, size_t windowWidth, size_t windowHeight) {
	if (!m_didInit)
		glfwInit();
	glfwWindowHint(GLFW_STICKY_KEYS, GLFW_TRUE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_window = glfwCreateWindow(static_cast<int>(windowWidth), static_cast<int>(windowHeight), windowName.data(),
								nullptr, nullptr);

	m_width = windowWidth;
	m_height = windowHeight;
}

Window::~Window() { glfwDestroyWindow(m_window); }

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
	VkSurfaceKHR surface;
	glfwCreateWindowSurface(instance, m_window, nullptr, &surface);
	return surface;
}

void Window::pollEvents() {
	m_windowSizeDirty = false;
	glfwPollEvents();

	int newWidth, newHeight;
	glfwGetFramebufferSize(m_window, &newWidth, &newHeight);

	size_t newWidthU = static_cast<size_t>(newWidth);
	size_t newHeightU = static_cast<size_t>(newHeight);

	if (newWidthU != m_width || newHeightU != m_height) {
		m_width = newWidthU;
		m_height = newHeightU;
		m_windowSizeDirty = true;
	}
}

void Window::waitEvents() {
	m_windowSizeDirty = false;
	glfwWaitEvents();

	int newWidth, newHeight;
	glfwGetFramebufferSize(m_window, &newWidth, &newHeight);

	size_t newWidthU = static_cast<size_t>(newWidth);
	size_t newHeightU = static_cast<size_t>(newHeight);

	if (newWidthU != m_width || newHeightU != m_height) {
		m_windowSizeDirty = true;
	}
}

bool Window::shouldWindowClose() { return !glfwWindowShouldClose(m_window); }
