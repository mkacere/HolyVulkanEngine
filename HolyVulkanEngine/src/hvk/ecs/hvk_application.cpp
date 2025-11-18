#include <hvk/ecs/hvk_application.hpp>
#include <hvk/ecs/systems/hvk_transform_system.hpp>
#include <hvk/ecs/systems/hvk_camera_system.hpp>
#include <hvk/ecs/systems/hvk_hierarchy_system.hpp>
#include <hvk/ecs/systems/hvk_mesh_render_system.hpp>
#include <hvk/ecs/systems/hvk_billboard_render_system.hpp>
#include <hvk/ecs/hvk_render_components.hpp>
#include <hvk/ecs/hvk_logic_system_adapter.hpp>
#include <hvk/gfx/hvk_barriers.hpp>
#include <hvk/resources/hvk_material.h>
#include <hvk/scene/hvk_camera.hpp>
#include <stdexcept>

namespace hvk {

Application::Application(const ApplicationCreateInfo& createInfo)
    : createInfo_(createInfo)
{
    // Handle Window injection
    if (createInfo_.window) {
        window_ = createInfo_.window;  // Use injected window (non-owning)
    } else {
        // Create our own window using windowCI
        ownedWindow_ = std::make_unique<Window>(createInfo_.windowCI);
        window_ = ownedWindow_.get();
    }

    // Handle Device injection
    if (createInfo_.device) {
        device_ = createInfo_.device;  // Use injected device (non-owning)
    } else {
        // Create our own device using deviceCI
        ownedDevice_ = std::make_unique<Device>(*window_, createInfo_.deviceCI);
        device_ = ownedDevice_.get();
    }
}

Application::~Application() {
    if (device_) {
        device_->waitIdle();
    }

    Input::cleanup();
}

void Application::run() {
    if (!initialized_) {
        initCore();
        initScene();
        initSystems();

        if (createInfo_.enableImGui) {
            initImGui();
        }

        if (createInfo_.createDefaultCamera || createInfo_.createDefaultLights) {
            initDefaultEntities();
        }

        // Call user init callback
        if (initCallback_) {
            initCallback_(*this);
        }

        // Wait for any pending uploads to complete
        // (Model loading already submits and waits, but this ensures everything is done)
        device_->waitIdle();

        initialized_ = true;
    }

    mainLoop();
}

// ============================================================================
// Initialization
// ============================================================================

void Application::initCore() {
    // Window and Device are already initialized in constructor (either owned or injected)

    // Swapchain
    swapchain_ = std::make_unique<Swapchain>(SwapchainCreateInfo{
        .device = device_,
        .surface = device_->surface(),
        .desiredExtent = { window_->framebufferSize().width, window_->framebufferSize().height },
        .preferMailbox = true,
        .desiredImageCount = 3,
        .extraUsage = 0,
        .preferredFormats = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB },
        .debugBaseName = "swap"
    });

    // Frame synchronization
    FrameSyncCreateInfo syncCI{};
    syncCI.device = device_->device();
    syncCI.graphicsQueueFamilyIndex = device_->graphics().family;
    syncCI.graphicsQueue = device_->graphics().handle;
    syncCI.presentQueue = device_->present().handle;
    syncCI.framesInFlight = (std::max)(createInfo_.framesInFlight, swapchain_->imageCount());
    syncCI.preferTimelineSemaphore = true;
    frameSync_ = std::make_unique<FrameSync>(syncCI);

    // Staging uploader
    stagingUploader_ = std::make_unique<StagingUploader>(StagingUploaderCreateInfo{
        .device = device_,
        .queue = device_->graphics().handle,
        .queueFamilyIndex = device_->graphics().family,
        .framesInFlight = frameSync_->frameCount(),
        .bytesPerFrame = 64 * 1024 * 1024,
        .debugBaseName = "upload"
    });

    // Input & Time (static classes)
    Input::init(window_->glfwHandle());
    Time::init();

    // Camera controller (optional)
    if (createInfo_.enableCameraController) {
        cameraController_ = std::make_unique<CameraController>();
    }

    // Command list placeholder (will get actual one during render)
    cmdList_ = std::make_unique<CmdList>(frameSync_->cmd());

    // Caches
    samplerCache_ = std::make_unique<SamplerCache>(device_, "samplers");
    pipelineLayoutCache_ = std::make_unique<PipelineLayoutCache>(device_, "plc");
    pipelineCache_ = std::make_unique<GraphicsPipelineCache>(device_, "gpc");

    // Descriptor allocator
    DescriptorAllocatorCreateInfo allocCI{};
    allocCI.device = device_;
    allocCI.maxSetsPerPool = 256;
    allocCI.debugName = "desc_alloc";
    descriptorAllocator_ = std::make_unique<DescriptorAllocator>(allocCI);

    // Material descriptor set layout
    materialDescLayout_ = std::make_unique<DescriptorSetLayout>(
        Material::createDescriptorSetLayout(*device_)
    );

    // Global descriptors
    globalDescLayout_ = std::make_unique<DescriptorSetLayout>(
        GlobalDescriptorLayout::create(*device_)
    );

    globalDescSet_ = std::make_unique<GlobalDescriptorSet>();
    globalDescSet_->init(*device_, *descriptorAllocator_, *globalDescLayout_, frameSync_->frameCount());

    // Deferred deletion
    deferredDeletion_ = std::make_unique<DeferredDeletion>(*device_, frameSync_->frameCount());
}

void Application::initScene() {
    scene_ = std::make_unique<Scene>(
        *device_,
        *stagingUploader_,
        *samplerCache_,
        *descriptorAllocator_,
        *materialDescLayout_,
        pipelineCache_.get(),
        globalDescLayout_.get(),
        globalDescSet_.get()
    );
}

void Application::initSystems() {
    if (!createInfo_.autoRegisterSystems) {
        return;  // User will manually add systems
    }

    // Add core logic systems (wrapped to work with Scene)
    scene_->addSystem(std::make_unique<LogicSystemAdapter>(
        std::make_unique<TransformSystem>()
    ));

    scene_->addSystem(std::make_unique<LogicSystemAdapter>(
        std::make_unique<HierarchySystem>()
    ));

    // Add camera system (wrapped to work with Scene)
    scene_->addSystem(std::make_unique<LogicSystemAdapter>(
        std::make_unique<CameraSystem>()
    ));

    // Add mesh render system
    scene_->addSystem(std::make_unique<MeshRenderSystem>());

    // Add billboard render system
    scene_->addSystem(std::make_unique<BillboardRenderSystem>());
}

void Application::initImGui() {
    // For simplicity, disable MSAA in Application class
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    ImGuiLayerCreateInfo imguiCI{};
    imguiCI.device = device_;
    imguiCI.window = window_->glfwHandle();
    imguiCI.framesInFlight = frameSync_->frameCount();
    imguiCI.colorFormat = swapchain_->colorFormat();
    imguiCI.depthFormat = VK_FORMAT_D32_SFLOAT;
    imguiCI.msaaSamples = msaaSamples;
    imguiCI.enableDocking = true;

    imgui_ = std::make_unique<ImGuiLayer>(imguiCI);
}

void Application::initDefaultEntities() {
    // Create default camera if requested
    if (createInfo_.createDefaultCamera) {
        auto camera = scene_->spawnCamera(
            glm::vec3(0.0f, 5.0f, 10.0f),  // Position
            glm::vec3(0.0f, 0.0f, 0.0f),   // Look at origin
            60.0f                           // FOV
        );
        scene_->setActiveCamera(camera);
    }

    // Create default lights if requested
    if (createInfo_.createDefaultLights) {
        // Sun (directional light)
        scene_->spawnDirectionalLight(
            glm::vec3(-0.3f, -1.0f, -0.5f),  // Direction
            glm::vec3(1.0f, 1.0f, 1.0f),     // Color (white)
            0.7f                              // Intensity
        );

        // Key light (point)
        scene_->spawnPointLight(
            glm::vec3(5.0f, 3.0f, 5.0f),    // Position
            glm::vec3(1.0f, 1.0f, 1.0f),    // Color (white)
            3.0f,                            // Intensity
            20.0f                            // Radius
        );

        // Fill light (point, cool blue)
        scene_->spawnPointLight(
            glm::vec3(-5.0f, 2.0f, 3.0f),   // Position
            glm::vec3(0.4f, 0.5f, 0.8f),    // Color (cool blue)
            1.5f,                            // Intensity
            15.0f                            // Radius
        );
    }
}

// ============================================================================
// Main Loop
// ============================================================================

void Application::mainLoop() {
    // Create depth buffer
    handleResize();

    while (!window_->shouldClose()) {
        window_->poll();

        Input::update();
        Time::update();

        // Update camera controller (before user update)
        updateCameraController();

        if (createInfo_.enableImGui) {
            imgui_->newFrame();
        }

        // User update callback
        if (updateCallback_) {
            updateCallback_(*this, Time::deltaTime());
        }

        // User ImGui callback
        if (createInfo_.enableImGui && imguiCallback_) {
            imguiCallback_(*this);
        }

        // Handle resize
        if (window_->wasResized()) {
            handleResize();
            window_->clearResizedFlag();
        }

        // Begin frame
        beginFrame();

        // Update scene
        update();

        // Update global descriptors before rendering
        updateGlobalDescriptors();

        // Render
        render();

        // End frame
        endFrame();
    }

    device_->waitIdle();
}

void Application::handleResize() {
    device_->waitIdle();

    if (swapchain_->recreateForWindow(*window_)) {
        VkExtent2D ext = swapchain_->extent();

        // For simplicity, disable MSAA in Application class
        // (Users can implement MSAA in their render callbacks if needed)
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

        // Recreate depth buffer
        depthImage_ = GpuImage(GpuImageCreateInfo{
            .device = device_,
            .format = VK_FORMAT_D32_SFLOAT,
            .width = ext.width,
            .height = ext.height,
            .samples = msaaSamples,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .debugName = "depth"
        });

        ImageViewCreateInfo depthViewCI{};
        depthViewCI.device = device_;
        depthViewCI.image = depthImage_.handle();
        depthViewCI.format = VK_FORMAT_D32_SFLOAT;
        depthViewCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewCI.range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewCI.debugName = "depth_view";

        depthImageView_ = ImageView();
        depthImageView_.create(depthViewCI);

        if (createInfo_.enableImGui && imgui_) {
            imgui_->onResize(ext.width, ext.height);
        }

        // Update active camera aspect ratio
        entt::entity activeCam = scene_->getActiveCamera();
        if (activeCam != entt::null) {
            auto* camComp = scene_->getComponent<CameraComponent>(activeCam);
            if (camComp) {
                camComp->aspectRatio = static_cast<float>(ext.width) / static_cast<float>(ext.height);
            }
        }
    }
}

void Application::beginFrame() {
    if (frameSync_->beginFrame() != VK_SUCCESS) {
        shouldQuit_ = true;
        return;
    }

    VkResult acq = frameSync_->acquireNextImage(swapchain_->handle(), swapImageIndex_);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        frameSync_->endFrame();
        handleResize();
        return;
    }

    frameIndex_ = frameSync_->currentFrameIndex();
}

void Application::update() {
    // Update scene systems
    scene_->update(Time::deltaTime());
}

void Application::render() {
    // Get command list for this frame
    *cmdList_ = CmdList(frameSync_->cmd());

    VkExtent2D ext = swapchain_->extent();

    // Transition swap image to color attachment
    barrier::Batch b;
    b.imgs.push_back(barrier::make_image_barrier_full(
        swapchain_->image(swapImageIndex_), swapchain_->colorFormat(),
        barrier::ImgUse::Undefined, barrier::ImgUse::ColorAttachment));

    b.imgs.push_back(barrier::make_image_barrier_full(
        depthImage_.handle(), VK_FORMAT_D32_SFLOAT,
        barrier::ImgUse::Undefined, barrier::ImgUse::DepthStencilAttachment));

    barrier::submit(cmdList_->handle(), b);
    b.clear();

    // Setup color and depth attachments
    ColorAttachment colorAtt{};
    colorAtt.view = swapchain_->imageView(swapImageIndex_);
    colorAtt.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.clear = VkClearColorValue{ {0.1f, 0.1f, 0.15f, 1.0f} };

    DepthAttachment depthAtt{};
    depthAtt.view = depthImageView_.handle();
    depthAtt.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.clear = VkClearDepthStencilValue{ 1.0f, 0 };

    VkRect2D renderArea{ {0, 0}, {ext.width, ext.height} };
    cmdList_->beginRendering(renderArea, {colorAtt}, &depthAtt, 0);
    cmdList_->setViewportScissor(ext, false);

    // User render callback
    if (renderCallback_) {
        renderCallback_(*this);
    }

    // Render scene (pass frame index for descriptor sets)
    scene_->render(*cmdList_, frameIndex_);

    // Render ImGui
    if (createInfo_.enableImGui && imgui_) {
        imgui_->render(*cmdList_);
    }

    cmdList_->endRendering();

    // Transition to present
    b.imgs.push_back(barrier::color_to_present(
        swapchain_->image(swapImageIndex_), swapchain_->colorFormat()));
    barrier::submit(cmdList_->handle(), b);
}

void Application::endFrame() {
    VkResult pr = frameSync_->submitAndPresent(swapchain_->handle(), swapImageIndex_);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
        // Will recreate next loop
    }

    frameSync_->endFrame();

    // Process deferred deletion queue
    deferredDeletion_->processFrame(frameIndex_);
}

// ============================================================================
// Helper Methods
// ============================================================================

void Application::updateGlobalDescriptors() {
    // Update scene data (timing, ambient, fog)
    sceneData_.updateTiming(Time::totalTime(), Time::deltaTime());

    // Collect lights from ECS
    collectLightsFromScene();

    // Get active camera
    entt::entity activeCam = scene_->getActiveCamera();

    // Update global descriptors
    if (activeCam != entt::null) {
        auto* renderCam = scene_->getComponent<RenderCamera>(activeCam);
        auto* transform = scene_->getComponent<TransformComponent>(activeCam);

        if (renderCam && transform) {
            // Build CameraData from RenderCamera + Transform components
            CameraData camData;
            camData.view = renderCam->view;
            camData.projection = renderCam->projection;
            camData.viewProjection = renderCam->viewProjection;
            camData.invView = glm::inverse(renderCam->view);
            camData.invProjection = glm::inverse(renderCam->projection);
            camData.position = glm::vec4(transform->position, 1.0f);

            // Extract forward direction from view matrix (negative Z axis in view space)
            camData.direction = glm::vec4(-renderCam->view[0][2], -renderCam->view[1][2], -renderCam->view[2][2], 0.0f);

            camData.screenSize = glm::vec2(swapchain_->extent().width, swapchain_->extent().height);

            // Update global descriptor set
            globalDescSet_->updateScene(frameIndex_, sceneData_);
            globalDescSet_->updateCamera(frameIndex_, camData);
            globalDescSet_->updateLights(frameIndex_, lightBuffer_);
        } else {
            // Camera exists but RenderCamera not computed yet - use default
            CameraData defaultCam;
            defaultCam.screenSize = glm::vec2(swapchain_->extent().width, swapchain_->extent().height);

            globalDescSet_->updateScene(frameIndex_, sceneData_);
            globalDescSet_->updateCamera(frameIndex_, defaultCam);
            globalDescSet_->updateLights(frameIndex_, lightBuffer_);
        }
    } else {
        // No active camera, use default camera data
        CameraData defaultCam;
        defaultCam.screenSize = glm::vec2(swapchain_->extent().width, swapchain_->extent().height);

        globalDescSet_->updateScene(frameIndex_, sceneData_);
        globalDescSet_->updateCamera(frameIndex_, defaultCam);
        globalDescSet_->updateLights(frameIndex_, lightBuffer_);
    }
}

void Application::collectLightsFromScene() {
    lightBuffer_.clear();

    // Collect directional lights
    auto dirView = scene_->registry().view<DirectionalLightComponent>();
    for (auto entity : dirView) {
        const auto& lightComp = dirView.get<DirectionalLightComponent>(entity);

        // Get transform to extract direction
        const auto* transform = scene_->getComponent<TransformComponent>(entity);
        glm::vec3 direction = transform ? transform->rotation * glm::vec3(0.0f, 0.0f, -1.0f)
                                        : glm::vec3(0.0f, -1.0f, 0.0f);

        Light light = Light::makeDirectional(direction, lightComp.color, lightComp.intensity);
        lightBuffer_.addLight(light);
    }

    // Collect point lights
    auto pointView = scene_->registry().view<PointLightComponent, TransformComponent>();
    for (auto entity : pointView) {
        const auto& lightComp = pointView.get<PointLightComponent>(entity);
        const auto& transform = pointView.get<TransformComponent>(entity);

        Light light = Light::makePoint(transform.position, lightComp.color, lightComp.intensity, lightComp.radius);
        lightBuffer_.addLight(light);
    }

    // TODO: Add spot lights when SpotLightComponent is defined
}

void Application::updateCameraController() {
    if (!cameraController_) {
        return; // Controller disabled
    }

    // Get active camera
    entt::entity activeCam = scene_->getActiveCamera();
    if (activeCam == entt::null) {
        return; // No active camera
    }

    // Get camera and transform components
    auto* camComp = scene_->getComponent<CameraComponent>(activeCam);
    auto* transform = scene_->getComponent<TransformComponent>(activeCam);
    if (!camComp || !transform) {
        return; // Camera entity missing components
    }

    // Create temporary Camera object from ECS components
    Camera tempCamera;
    tempCamera.setPosition(transform->position);
    tempCamera.setRotation(transform->rotation);

    // Copy camera settings from CameraComponent
    if (camComp->type == CameraComponent::Type::Perspective) {
        tempCamera.setProjectionType(ProjectionType::Perspective);
        tempCamera.setFovY(camComp->fovYDegrees);
    } else {
        tempCamera.setProjectionType(ProjectionType::Orthographic);
        tempCamera.setOrthoWidth(camComp->orthoWidth);
    }
    tempCamera.setAspectRatio(camComp->aspectRatio);
    tempCamera.setNearPlane(camComp->nearPlane);
    tempCamera.setFarPlane(camComp->farPlane);

    // Update camera via controller
    cameraController_->update(tempCamera);

    // Write back position and rotation to TransformComponent
    transform->position = tempCamera.position();
    transform->rotation = tempCamera.rotation();
}

entt::entity Application::activeCamera() const {
    return scene_->getActiveCamera();
}

void Application::setActiveCamera(entt::entity entity) {
    scene_->setActiveCamera(entity);

    // Update camera aspect ratio if window is valid
    if (window_ && swapchain_) {
        auto* camComp = scene_->getComponent<CameraComponent>(entity);
        if (camComp) {
            camComp->aspectRatio = static_cast<float>(swapchain_->extent().width) /
                                   static_cast<float>(swapchain_->extent().height);
        }
    }
}

} // namespace hvk
