#pragma once

#include <string>
#define GLFW_INCLUDE_NONE
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
	
	void switchFullscreenWindowed();

	bool shouldWindowClose() const;

	bool keyPressed(int keyCode) const { return glfwGetKey(m_window, keyCode); }

	float mouseMoveX() const { return m_mouseMoveX; }
	float mouseMoveY() const { return m_mouseMoveY; }

  private:

	float m_mouseMoveX = 0.0f, m_mouseMoveY = 0.0f;

	size_t m_width, m_height;
	bool m_windowSizeDirty = false;

	bool m_lastMouseValid = false;
	double m_lastMouseX, m_lastMouseY;

	GLFWwindow* m_window;
};