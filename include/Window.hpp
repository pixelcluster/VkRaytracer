#pragma once

#include <string>
#define GLFW_INCLUDE_VULKAN
#define VK_NO_PROTOTYPES
#include <GLFW/glfw3.h>

class Window {
  public:
	Window(const std::string_view& windowName);
	Window(const std::string_view& windowName, size_t windowWidth, size_t windowHeight);
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) = default;
	Window& operator=(Window&&) = default;

	VkSurfaceKHR createSurface(VkInstance instance) const;

	size_t width() const { return m_width; }
	size_t height() const { return m_height; }

	bool windowSizeDirty() const { return m_windowSizeDirty; }

	void pollEvents();
	void waitEvents();

	bool shouldWindowClose() const;

	bool keyPressed(int keyCode) const { return glfwGetKey(m_window, keyCode); }

  private:

	size_t m_width, m_height;
	bool m_windowSizeDirty = false;

	GLFWwindow* m_window;
};