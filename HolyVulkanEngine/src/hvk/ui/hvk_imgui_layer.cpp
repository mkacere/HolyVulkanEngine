#include <hvk/ui/hvk_imgui_layer.hpp>
#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_window.h>
#include <hvk/gfx/hvk_cmd_list.hpp>
#include <hvk/gfx/hvk_config.h>

// ImGui core
#include <imgui.h>

// ImGui backends
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <stdexcept>
#include <algorithm>

namespace hvk {

    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------

    ImGuiLayer::ImGuiLayer(const ImGuiLayerCreateInfo& ci)
        : device_(ci.device)
        , window_(ci.window)
        , debugName_(ci.debugName)
    {
        if (!device_) {
            throw std::runtime_error("ImGuiLayer: device is required");
        }
        if (!window_) {
            throw std::runtime_error("ImGuiLayer: window is required");
        }

        // Create descriptor pool for ImGui
        create_descriptor_pool(ci);

        // Setup ImGui context
        setup_imgui_context(ci);

        // Initialize GLFW backend
        ImGui_ImplGlfw_InitForVulkan(window_, true);

        // Initialize Vulkan backend
        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.ApiVersion = VK_API_VERSION_1_4;
        init_info.Instance = device_->instance();
        init_info.PhysicalDevice = device_->physical();
        init_info.Device = device_->device();
        init_info.QueueFamily = device_->graphics().family;
        init_info.Queue = device_->graphics().handle;
        init_info.DescriptorPool = descriptorPool_;
        init_info.DescriptorPoolSize = 0; // We created our own pool
        init_info.MinImageCount = ci.framesInFlight;
        init_info.ImageCount = ci.framesInFlight;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = nullptr;

        // Setup pipeline info for dynamic rendering
        if (ci.renderPass == VK_NULL_HANDLE) {
            // Use dynamic rendering
            init_info.UseDynamicRendering = true;

            // Setup pipeline rendering create info for dynamic rendering
            VkPipelineRenderingCreateInfo pipelineRenderingCI{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
            pipelineRenderingCI.colorAttachmentCount = 1;
            pipelineRenderingCI.pColorAttachmentFormats = &ci.colorFormat;
            pipelineRenderingCI.depthAttachmentFormat = ci.depthFormat;

            init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingCI;
            init_info.PipelineInfoMain.MSAASamples = ci.msaaSamples;
        } else {
            // Use traditional render pass
            init_info.UseDynamicRendering = false;
            init_info.PipelineInfoMain.RenderPass = ci.renderPass;
            init_info.PipelineInfoMain.Subpass = 0;
            init_info.PipelineInfoMain.MSAASamples = ci.msaaSamples;
        }

        if (!ImGui_ImplVulkan_Init(&init_info)) {
            throw std::runtime_error("ImGuiLayer: Failed to initialize Vulkan backend");
        }

        // Font textures are created automatically on first NewFrame() call

        initialized_ = true;
    }

    // -------------------------------------------------------------------------
    // Destructor
    // -------------------------------------------------------------------------

    ImGuiLayer::~ImGuiLayer() {
        destroy();
    }

    void ImGuiLayer::destroy() {
        if (!initialized_) return;

        // Shutdown backends
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        // Destroy ImGui context if we created it
        if (ownedContext_) {
            ImGui::DestroyContext();
        }

        // Destroy descriptor pool
        if (descriptorPool_ != VK_NULL_HANDLE && device_) {
            vkDestroyDescriptorPool(device_->device(), descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }

        initialized_ = false;
    }

    // -------------------------------------------------------------------------
    // Move semantics
    // -------------------------------------------------------------------------

    ImGuiLayer::ImGuiLayer(ImGuiLayer&& o) noexcept {
        move_from(std::move(o));
    }

    ImGuiLayer& ImGuiLayer::operator=(ImGuiLayer&& o) noexcept {
        if (this != &o) {
            destroy();
            move_from(std::move(o));
        }
        return *this;
    }

    void ImGuiLayer::move_from(ImGuiLayer&& o) noexcept {
        device_ = o.device_;
        window_ = o.window_;
        descriptorPool_ = o.descriptorPool_;
        initialized_ = o.initialized_;
        ownedContext_ = o.ownedContext_;
        debugName_ = std::move(o.debugName_);

        o.device_ = nullptr;
        o.window_ = nullptr;
        o.descriptorPool_ = VK_NULL_HANDLE;
        o.initialized_ = false;
        o.ownedContext_ = false;
    }

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------

    void ImGuiLayer::newFrame() {
        if (!initialized_) return;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::render(CmdList& cmd) {
        render(cmd.handle());
    }

    void ImGuiLayer::render(VkCommandBuffer commandBuffer) {
        if (!initialized_) return;

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        // Avoid rendering when minimized
        if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f) {
            return;
        }

        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);

        // Update and render additional platform windows (multi-viewport support)
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void ImGuiLayer::onResize(uint32_t width, uint32_t height) {
        // ImGui handles resize automatically via GLFW callbacks
        // This method is here for explicit resize handling if needed
        (void)width;
        (void)height;
    }

    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------

    void ImGuiLayer::create_descriptor_pool(const ImGuiLayerCreateInfo& ci) {
        // Create descriptor pool for ImGui
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, ci.maxSamplers },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ci.maxSamplers },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, ci.maxSamplers },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
        };

        VkDescriptorPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = ci.maxSets;
        pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
        pool_info.pPoolSizes = pool_sizes;

        VK_CHECK(vkCreateDescriptorPool(device_->device(), &pool_info, nullptr, &descriptorPool_));

        // Set debug name if available
        if (!debugName_.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                                  reinterpret_cast<uint64_t>(descriptorPool_),
                                  (debugName_ + "_pool").c_str());
        }
    }

    void ImGuiLayer::setup_imgui_context(const ImGuiLayerCreateInfo& ci) {
        // Create ImGui context (or use existing one)
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if (!ctx) {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ownedContext_ = true;
        }

        ImGuiIO& io = ImGui::GetIO();

        // Enable docking if requested
        if (ci.enableDocking) {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }

        // Enable multi-viewport if requested (experimental)
        if (ci.enableViewports) {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        // Setup style
        setup_imgui_style();
    }

    void ImGuiLayer::setup_imgui_style() {
        // Use a clean dark style
        ImGui::StyleColorsDark();

        // Customize style for better readability
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FramePadding = ImVec2(5.0f, 3.0f);
        style.ItemSpacing = ImVec2(8.0f, 4.0f);

        // Adjust colors for better contrast
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.10f, 0.94f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.17f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.28f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.52f, 0.78f, 0.55f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    }

} // namespace hvk
