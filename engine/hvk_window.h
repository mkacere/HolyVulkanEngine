#ifndef HVK_WINDOW
#define HVK_WINDOW

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <string>

namespace hvk {

	class HvkWindow
	{
	public:
		HvkWindow(int width, int height, std::string title);
		~HvkWindow();

		HvkWindow(const HvkWindow&) = delete;
		HvkWindow& operator=(const HvkWindow&) = delete;

		bool shouldClose() { return glfwWindowShouldClose(window_); }
		VkExtent2D getExtent() { return { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_) }; }
		bool wasWindowResized() const { return framebufferResized_; }
		void resetWindowResizedFlag() { framebufferResized_ = false; }
		GLFWwindow* getGLFWwindow() { return window_; }

		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

	private:
		static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

		int width_;
		int height_;
		bool framebufferResized_ = false;

		std::string windowTitle_;
		GLFWwindow* window_;
	};
}

#endif // HVK_WINDOW