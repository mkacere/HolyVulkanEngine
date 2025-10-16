#ifndef HVK_IMGUI_LAYER_HPP
#define HVK_IMGUI_LAYER_HPP

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string_view>

// Forward declarations
struct GLFWwindow;

namespace hvk {

    // Forward declarations
    class Device;
    class Window;
    class CmdList;

    /**
     * ImGuiLayerCreateInfo
     *
     * Configuration for ImGui integration with Vulkan backend.
     */
    struct ImGuiLayerCreateInfo {
        const Device* device = nullptr;               // required
        GLFWwindow* window = nullptr;                 // required (GLFW window handle)
        VkRenderPass renderPass = VK_NULL_HANDLE;     // optional (not needed for dynamic rendering)
        uint32_t framesInFlight = 3;                  // number of frames in flight
        VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB; // swapchain format
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;    // depth buffer format (for dynamic rendering)
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT; // MSAA samples

        // Descriptor pool sizing for ImGui (per pool)
        uint32_t maxSets = 1000;
        uint32_t maxSamplers = 1000;

        // ImGui config flags
        bool enableDocking = true;
        bool enableViewports = false;  // Multi-viewport (experimental)

        std::string_view debugName = "imgui";
    };

    /**
     * ImGuiLayer
     *
     * RAII wrapper for Dear ImGui integration with Vulkan.
     * Manages ImGui context, Vulkan backend, and descriptor resources.
     *
     * Usage:
     *   1. Create after Device, Window, Swapchain
     *   2. Call newFrame() at the start of each frame
     *   3. Issue ImGui commands (ImGui::Begin/End, etc.)
     *   4. Call render(cmd) to record draw commands
     *   5. Destroy after device.waitIdle()
     */
    class ImGuiLayer {
    public:
        ImGuiLayer() = default;
        explicit ImGuiLayer(const ImGuiLayerCreateInfo& ci);
        ~ImGuiLayer();

        // No copy
        ImGuiLayer(const ImGuiLayer&) = delete;
        ImGuiLayer& operator=(const ImGuiLayer&) = delete;

        // Move semantics
        ImGuiLayer(ImGuiLayer&& o) noexcept;
        ImGuiLayer& operator=(ImGuiLayer&& o) noexcept;

        /**
         * Begin a new ImGui frame.
         * Call this at the start of each frame, before any ImGui commands.
         */
        void newFrame();

        /**
         * Render ImGui draw data into the command buffer.
         * Call this after all ImGui commands and before ending the render pass.
         *
         * @param cmd Command list to record into
         */
        void render(CmdList& cmd);

        /**
         * Render ImGui draw data (overload for raw VkCommandBuffer).
         *
         * @param commandBuffer Vulkan command buffer
         */
        void render(VkCommandBuffer commandBuffer);

        /**
         * Handle window resize.
         * Call when the swapchain is recreated.
         *
         * @param width New framebuffer width
         * @param height New framebuffer height
         */
        void onResize(uint32_t width, uint32_t height);

        /**
         * Check if ImGui is initialized and ready to use.
         */
        explicit operator bool() const { return initialized_; }

        /**
         * Get the ImGui descriptor pool (for advanced usage).
         */
        VkDescriptorPool descriptorPool() const { return descriptorPool_; }

    private:
        void destroy();
        void move_from(ImGuiLayer&& o) noexcept;

        void create_descriptor_pool(const ImGuiLayerCreateInfo& ci);
        void setup_imgui_context(const ImGuiLayerCreateInfo& ci);
        void setup_imgui_style();

    private:
        const Device* device_ = nullptr;
        GLFWwindow* window_ = nullptr;

        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;

        bool initialized_ = false;
        bool ownedContext_ = false;  // true if we created the ImGui context

        std::string debugName_;
    };

} // namespace hvk

#endif // HVK_IMGUI_LAYER_HPP
