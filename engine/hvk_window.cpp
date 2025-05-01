#include "hvk_window.h"

#include <stdexcept>
#include <iostream>

namespace hvk {

	HvkWindow::HvkWindow(int width, int height, std::string title) : width_(width), height_(height)
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(window_, this);

		glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
	}

	HvkWindow::~HvkWindow()
	{
		glfwDestroyWindow(window_);
		glfwTerminate();
	}

	void HvkWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
	{
		if (glfwCreateWindowSurface(instance, window_, nullptr, surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}

	void HvkWindow::framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto hvkWindow = reinterpret_cast<HvkWindow*>(glfwGetWindowUserPointer(window));
		hvkWindow->framebufferResized_ = true;
		hvkWindow->width_ = width;
		hvkWindow->height_ = height;
	}
}