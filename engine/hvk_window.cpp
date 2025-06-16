#include "hvk_window.h"

#include <stdexcept>
#include <iostream>

namespace hvk {

	HvkWindow::HvkWindow(int width, int height, std::string title)
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		bool fullscreen = (width == 0 || height == 0);
		GLFWmonitor* monitor = nullptr;
		const GLFWvidmode* mode = nullptr;

		if (fullscreen) {
			glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

			monitor = glfwGetPrimaryMonitor();
			mode = glfwGetVideoMode(monitor);

			width_ = mode->width;
			height_ = mode->height;
		}
		else {
			width_ = width;
			height_ = height;
		}

		window_ = glfwCreateWindow(width_, height_, title.c_str(), nullptr, nullptr);

		if (fullscreen) {
			glfwSetWindowPos(window_, 0, 0);
		}

		glfwSetWindowUserPointer(window_, this);
		glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);

		lastFrameTime_ = glfwGetTime();
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

	float HvkWindow::getFrameTime()
	{
		double current = glfwGetTime();
		double delta = current - lastFrameTime_;
		lastFrameTime_ = current;
		return static_cast<float>(delta);
	}
}