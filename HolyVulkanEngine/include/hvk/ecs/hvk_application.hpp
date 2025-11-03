#ifndef HVK_ECS_APPLICATION_HPP
#define HVK_ECS_APPLICATION_HPP

#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/gfx/hvk_window.h>
#include <hvk/core/hvk_input.hpp>
#include <hvk/core/hvk_time.hpp>
#include <hvk/scene/hvk_camera_controller.hpp>
#include <hvk/gfx/hvk_scene_data.hpp>
#include <hvk/gfx/hvk_light_data.hpp>
#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_swapchain.h>
#include <hvk/gfx/hvk_frame_sync.h>
#include <hvk/gfx/hvk_staging_uploader.h>
#include <hvk/gfx/hvk_sampler_cache.h>
#include <hvk/gfx/hvk_pipeline_layout_cache.h>
#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <hvk/gfx/hvk_descriptors.h>
#include <hvk/gfx/hvk_deferred_deletion.hpp>
#include <hvk/gfx/hvk_global_descriptors.hpp>
#include <hvk/ui/hvk_imgui_layer.hpp>

#include <memory>
#include <functional>

namespace hvk {

// Forward declarations for optional injection
class Window;
class Device;

/**
 * ApplicationCreateInfo - Configuration for Application
 *
 * Follows the CreateInfo pattern used throughout HVK.
 * Supports both "batteries-included" defaults and custom injection.
 *
 * Simple usage (defaults):
 *   ApplicationCreateInfo appCI{};
 *   appCI.windowCI.title = "My Game";
 *   appCI.windowCI.width = 1920;
 *   appCI.windowCI.height = 1080;
 *   Application app(appCI);
 *
 * Advanced usage (custom injection):
 *   Window customWindow({.width = 2560, .title = "Custom"});
 *   Device customDevice(customWindow, {.debugVerbosity = DebugVerbosity::Verbose});
 *
 *   ApplicationCreateInfo appCI{};
 *   appCI.window = &customWindow;      // Inject custom window (non-owning)
 *   appCI.device = &customDevice;      // Inject custom device (non-owning)
 *   Application app(appCI);
 */
struct ApplicationCreateInfo {
    // Window configuration (used if window == nullptr)
    // Uses WindowCreateInfo defaults: 1280x720, "Engine", WindowMode::Auto
    WindowCreateInfo windowCI{};

    // Device configuration (used if device == nullptr)
    // Uses DeviceCreateInfo defaults: DebugVerbosity::Info (in Debug), validation enabled
    DeviceCreateInfo deviceCI{};

    // Frame sync configuration
    uint32_t framesInFlight = 2;

    // Optional injection (non-owning pointers)
    // If provided, Application will use these instead of creating its own
    Window* window = nullptr;     // If null, Application creates its own
    Device* device = nullptr;     // If null, Application creates its own

    // Features
    bool enableImGui = true;
    bool enableCameraController = true;  // Enable FPS camera controls (WASD + mouse)
    bool autoRegisterSystems = true;     // Auto-add TransformSystem, CameraSystem, HierarchySystem, MeshRenderSystem
    bool createDefaultCamera = false;    // Auto-create a perspective camera at (0, 5, 10) looking at origin
    bool createDefaultLights = false;    // Auto-create sun (directional) and key/fill lights

    ApplicationCreateInfo() = default;
};

/**
 * Application - High-level application framework with ECS integration
 *
 * Design principles:
 * - Easy setup: Create window, device, swapchain, scene all in one place
 * - Default options: Sensible defaults with full control available
 * - Custom injection: Optionally provide custom Window/Device via CreateInfo
 * - Callback-based: User provides init/update/render callbacks
 * - Built-in systems: ImGui, input, time management
 *
 * Simple usage:
 *   ApplicationCreateInfo appCI{};
 *   appCI.title = "My Game";
 *   Application app(appCI);
 *   app.run();
 *
 * Advanced usage (custom injection):
 *   Window myWindow({.width = 2560, .title = "Custom"});
 *   ApplicationCreateInfo appCI{};
 *   appCI.window = &myWindow;
 *   Application app(appCI);
 */
class Application {
public:
    /**
     * Constructor
     *
     * @param createInfo Application configuration (supports custom injection)
     */
    explicit Application(const ApplicationCreateInfo& createInfo = ApplicationCreateInfo());

    ~Application();

    // Move-only
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // ========================================================================
    // Main Loop
    // ========================================================================

    /**
     * Run the application main loop
     *
     * Calls init callbacks, then runs the main loop until window closes.
     */
    void run();

    /**
     * Request application exit
     */
    void quit() { shouldQuit_ = true; }

    // ========================================================================
    // Callbacks
    // ========================================================================

    using InitCallback = std::function<void(Application&)>;
    using UpdateCallback = std::function<void(Application&, float deltaTime)>;
    using RenderCallback = std::function<void(Application&)>;
    using ImGuiCallback = std::function<void(Application&)>;

    /**
     * Set initialization callback
     *
     * Called once before main loop starts.
     * Use this to create entities, load resources, etc.
     */
    void onInit(InitCallback callback) { initCallback_ = callback; }

    /**
     * Set update callback
     *
     * Called every frame before rendering.
     * Use this for game logic, physics, etc.
     */
    void onUpdate(UpdateCallback callback) { updateCallback_ = callback; }

    /**
     * Set render callback
     *
     * Called during rendering, after scene systems have rendered.
     * Use this for custom rendering.
     */
    void onRender(RenderCallback callback) { renderCallback_ = callback; }

    /**
     * Set ImGui callback
     *
     * Called during ImGui frame.
     * Use this for custom UI windows.
     */
    void onImGui(ImGuiCallback callback) { imguiCallback_ = callback; }

    // ========================================================================
    // Accessors
    // ========================================================================

    Scene& scene() { return *scene_; }
    const Scene& scene() const { return *scene_; }

    Window& window() { return *window_; }
    const Window& window() const { return *window_; }

    Device& device() { return *device_; }
    const Device& device() const { return *device_; }

    Swapchain& swapchain() { return *swapchain_; }
    const Swapchain& swapchain() const { return *swapchain_; }

    // Note: For custom-injected Window/Device, user retains ownership
    // Application will NOT destroy injected instances

    // Note: Input and Time are static classes, accessed via Input:: and Time::

    CmdList& cmd() { return *cmdList_; }

    uint32_t frameIndex() const { return frameIndex_; }

    // ========================================================================
    // Camera Management
    // ========================================================================

    /**
     * Get the active camera entity
     *
     * @return Active camera entity (entt::null if none)
     */
    entt::entity activeCamera() const;

    /**
     * Set the active camera
     *
     * Marks specified camera as active, deactivates all others.
     * Also updates camera aspect ratio if window is valid.
     *
     * @param entity Camera entity to activate
     */
    void setActiveCamera(entt::entity entity);

    /**
     * Get camera controller (if enabled)
     *
     * @return Pointer to camera controller (nullptr if disabled)
     */
    CameraController* cameraController() { return cameraController_.get(); }
    const CameraController* cameraController() const { return cameraController_.get(); }

private:
    // Initialization
    void initCore();
    void initScene();
    void initSystems();
    void initImGui();
    void initDefaultEntities();  // Create default camera/lights if requested

    // Main loop
    void mainLoop();
    void handleResize();
    void beginFrame();
    void update();
    void render();
    void endFrame();

    // Helper methods
    void updateGlobalDescriptors();     // Update SceneData, CameraData, LightBuffer
    void collectLightsFromScene();      // Collect lights from ECS into lightBuffer_
    void updateCameraController();      // Update camera controller if enabled

    // Configuration
    ApplicationCreateInfo createInfo_;

    // Core systems (may be owned or injected)
    std::unique_ptr<Window> ownedWindow_;   // Only used if window not injected
    std::unique_ptr<Device> ownedDevice_;   // Only used if device not injected
    Window* window_;                         // Points to either ownedWindow_ or injected
    Device* device_;                         // Points to either ownedDevice_ or injected

    std::unique_ptr<Swapchain> swapchain_;
    std::unique_ptr<FrameSync> frameSync_;
    std::unique_ptr<CmdList> cmdList_;

    // Resource management
    std::unique_ptr<StagingUploader> stagingUploader_;
    std::unique_ptr<SamplerCache> samplerCache_;
    std::unique_ptr<PipelineLayoutCache> pipelineLayoutCache_;
    std::unique_ptr<GraphicsPipelineCache> pipelineCache_;
    std::unique_ptr<DeferredDeletion> deferredDeletion_;

    // Descriptors
    std::unique_ptr<DescriptorAllocator> descriptorAllocator_;
    std::unique_ptr<DescriptorSetLayout> globalDescLayout_;
    std::unique_ptr<GlobalDescriptorSet> globalDescSet_;
    std::unique_ptr<DescriptorSetLayout> materialDescLayout_;

    // ECS
    std::unique_ptr<Scene> scene_;

    // UI
    std::unique_ptr<ImGuiLayer> imgui_;

    // Callbacks
    InitCallback initCallback_;
    UpdateCallback updateCallback_;
    RenderCallback renderCallback_;
    ImGuiCallback imguiCallback_;

    // State
    uint32_t frameIndex_ = 0;
    uint32_t swapImageIndex_ = 0;
    bool shouldQuit_ = false;
    bool initialized_ = false;

    // Rendering resources
    GpuImage depthImage_;
    ImageView depthImageView_;

    // Camera management
    std::unique_ptr<CameraController> cameraController_;

    // Global descriptor data (CPU-side)
    SceneData sceneData_;
    LightBuffer lightBuffer_;
};

} // namespace hvk

#endif // HVK_ECS_APPLICATION_HPP
