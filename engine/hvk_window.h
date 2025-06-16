#ifndef HVK_WINDOW
#define HVK_WINDOW

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
		GLFWwindow* getGlfwWindow() { return window_; }

		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
		float getFrameTime();

	private:
		static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

		int width_;
		int height_;
		bool framebufferResized_ = false;

		std::string windowTitle_;
		GLFWwindow* window_;
		double lastFrameTime_ = 0.0;
	};
}

#endif // HVK_WINDOW