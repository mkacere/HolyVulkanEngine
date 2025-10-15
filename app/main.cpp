#include <hvk/gfx.hpp>
#include <hvk/resources.hpp>
#include <hvk/scene.hpp>
#include <hvk/core.hpp>

#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>

#ifndef PROJECT_ROOT
#define PROJECT_ROOT .
#endif

using namespace hvk;

static std::vector<uint32_t> loadSpirv(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error(std::string("Failed to open SPIR-V: ") + path);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
    if (bytes.size() % 4) throw std::runtime_error("SPIR-V file size not multiple of 4");
    std::vector<uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return words;
}

int main() {
    try {
        std::cout << "=== Holy Vulkan Engine - GLTF Model Demo ===" << std::endl;

        // 1) Initialize window + device --------------------------------------------
        std::cout << "[1/9] Creating window and Vulkan device..." << std::endl;
        Window window({
            .width = 560,
            .height = 780,
            .title = "HVK - Model Demo (WASD + Mouse to move, ESC to toggle cursor)",
            .mode = WindowMode::Auto
        });
        Device device(window, { .debugVerbosity = DebugVerbosity::Warn });

        // 2) Swapchain -------------------------------------------------------------
        std::cout << "[2/9] Creating swapchain..." << std::endl;
        Swapchain swap({
            .device = &device,
            .surface = device.surface(),
            .desiredExtent = { window.framebufferSize().width, window.framebufferSize().height },
            .preferMailbox = true,
            .desiredImageCount = 3,
            .extraUsage = 0,
            .preferredFormats = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB },
            .debugBaseName = "swap"
        });

        // 3) Frame sync ------------------------------------------------------------
        std::cout << "[3/9] Setting up frame synchronization..." << std::endl;
        FrameSyncCreateInfo syncCI{};
        syncCI.device = device.device();
        syncCI.graphicsQueueFamilyIndex = device.graphics().family;
        syncCI.graphicsQueue = device.graphics().handle;
        syncCI.presentQueue = device.present().handle;
        syncCI.framesInFlight = (std::max)(2u, swap.imageCount());
        syncCI.preferTimelineSemaphore = true;
        FrameSync sync(syncCI);

        // 4) Staging uploader ------------------------------------------------------
        std::cout << "[4/9] Creating staging uploader..." << std::endl;
        StagingUploader uploader({
            .device = &device,
            .queue = device.graphics().handle,
            .queueFamilyIndex = device.graphics().family,
            .framesInFlight = sync.frameCount(),
            .bytesPerFrame = 64 * 1024 * 1024,  // 64 MB per frame for model loading
            .debugBaseName = "upload"
        });

        // 5) Initialize core systems -----------------------------------------------
        std::cout << "[5/9] Initializing Input and Time systems..." << std::endl;
        Input::init(window.glfwHandle());
        Time::init();

        // 6) Create caches ---------------------------------------------------------
        std::cout << "[6/9] Creating descriptor and pipeline caches..." << std::endl;
        SamplerCache samplerCache(&device, "samplers");
        PipelineLayoutCache layoutCache(&device, "plc");
        GraphicsPipelineCache pipeCache(&device, "gpc");

        DescriptorAllocatorCreateInfo allocCI{};
        allocCI.device = &device;
        allocCI.maxSetsPerPool = 256;
        allocCI.debugName = "desc_alloc";
        DescriptorAllocator descAllocator(allocCI);

        // 7) Load GLTF model -------------------------------------------------------
        std::cout << "[7/9] Loading GLTF model from assets..." << std::endl;

        // Create material descriptor set layout
        DescriptorSetLayout materialLayout = Material::createDescriptorSetLayout(device);

        // Load model
        const char* modelPath = PROJECT_ROOT "/assets/models/miku.glb";
        //const char* modelPath = PROJECT_ROOT "/assets/models/Crystar_Kokoro_Fudoji.glb";
        //const char* modelPath = PROJECT_ROOT "/assets/models/kawashaki_ninja_h2.glb";
        std::cout << "    Loading: " << modelPath << std::endl;

        GltfLoaderOptions loaderOptions;
        loaderOptions.generateMipmaps = true;
        loaderOptions.loadMaterials = true;
        loaderOptions.loadTextures = true;
        loaderOptions.flipTextureY = false;
        loaderOptions.forceLinearTextures = false;
        loaderOptions.verbose = true;

        Model model = GltfLoader::loadFromFile(
            device,
            uploader,
            descAllocator,
            materialLayout,
            samplerCache,
            modelPath,
            loaderOptions
        );

        std::cout << "    Model loaded successfully!" << std::endl;
        std::cout << "    - Textures: " << model.textureCount() << std::endl;
        std::cout << "    - Materials: " << model.materialCount() << std::endl;
        std::cout << "    - Meshes: " << model.meshCount() << std::endl;
        std::cout << "    - Nodes: " << model.nodeCount() << std::endl;

        // 8) Setup global descriptors ----------------------------------------------
        std::cout << "[8/9] Setting up global descriptors (Scene, Camera, Lights)..." << std::endl;

        DescriptorSetLayout globalLayout = GlobalDescriptorLayout::create(device);

        GlobalDescriptorSet globalDescriptors;
        globalDescriptors.init(device, descAllocator, globalLayout, sync.frameCount());

        // Initialize scene data
        SceneData sceneData{};
        sceneData.ambientColor = glm::vec4(0.3f, 0.3f, 0.35f, 1.0f); // Simple ambient
        sceneData.fogColor = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f);
        sceneData.fogRange = glm::vec2(50.0f, 100.0f);

        // Setup lights
        LightBuffer lightBuffer;

        // Directional light (sun) - simple intensity
        lightBuffer.lights.push_back(Light::makeDirectional(
            glm::vec3(-0.3f, -1.0f, -0.5f),  // direction
            glm::vec3(1.0f, 1.0f, 1.0f),     // white light
            0.7f                              // intensity
        ));

        // Point light (key light)
        lightBuffer.lights.push_back(Light::makePoint(
            glm::vec3(5.0f, 3.0f, 5.0f),     // position
            glm::vec3(1.0f, 1.0f, 1.0f),     // color (white)
            3.0f,                             // intensity
            20.0f                             // range
        ));

        // Point light (fill light)
        lightBuffer.lights.push_back(Light::makePoint(
            glm::vec3(-5.0f, 2.0f, 3.0f),    // position
            glm::vec3(0.4f, 0.5f, 0.8f),     // color (cool blue)
            1.5f,                             // intensity
            15.0f                             // range
        ));

        lightBuffer.lightCount = static_cast<uint32_t>(lightBuffer.lights.size());

        // 9) Setup camera and controller -------------------------------------------
        std::cout << "[9/9] Setting up camera and controls..." << std::endl;

        float aspect = static_cast<float>(window.framebufferSize().width) /
                       static_cast<float>(window.framebufferSize().height);

        // Frame camera to model bounds so it’s visible regardless of asset scale
        const auto& bounds = model.bounds();
        glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
        float diag = glm::length(bounds.max - bounds.min);
        float radius = (diag > 0.0001f) ? (diag * 0.5f) : 1.0f;
        glm::vec3 viewDir = glm::normalize(glm::vec3(1.0f, 0.5f, 1.0f));
        glm::vec3 camPos = center + viewDir * (radius * 2.5f);

        Camera camera = Camera::createPerspective(
            camPos,                         // position
            center,                         // look-at model center
            60.0f,                          // FOV
            aspect,                         // aspect ratio
            0.1f,                           // near plane
            std::max(1000.0f, radius * 10.0f) // far plane
        );

        CameraController cameraController;
        cameraController.setMode(CameraController::Mode::FPS);
        cameraController.setMoveSpeed(5.0f);
        cameraController.setFastMoveMultiplier(3.0f);
        cameraController.setMouseSensitivity(0.15f);

        // Start with cursor disabled for FPS controls
        Input::setCursorMode(GLFW_CURSOR_DISABLED);

        std::cout << "\n=== Controls ===" << std::endl;
        std::cout << "  WASD: Move camera" << std::endl;
        std::cout << "  Mouse: Look around" << std::endl;
        std::cout << "  Space: Move up" << std::endl;
        std::cout << "  Ctrl: Move down" << std::endl;
        std::cout << "  Shift: Sprint" << std::endl;
        std::cout << "  ESC: Toggle cursor lock" << std::endl;
        std::cout << "\n[10/11] Creating rendering pipeline..." << std::endl;

        // 10) Query MSAA support and create MSAA + depth buffers ------------------
        // Query max MSAA sample count supported by device
        VkSampleCountFlags counts = device.limits().framebufferColorSampleCounts &
                                     device.limits().framebufferDepthSampleCounts;

        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        if (counts & VK_SAMPLE_COUNT_4_BIT) {
            msaaSamples = VK_SAMPLE_COUNT_4_BIT;
            std::cout << "    MSAA: Enabled (4x samples)" << std::endl;
        } else if (counts & VK_SAMPLE_COUNT_2_BIT) {
            msaaSamples = VK_SAMPLE_COUNT_2_BIT;
            std::cout << "    MSAA: Enabled (2x samples)" << std::endl;
        } else {
            std::cout << "    MSAA: Disabled (not supported)" << std::endl;
        }

        VkExtent2D ext = swap.extent();

        // Create MSAA color buffer (if MSAA enabled)
        GpuImage msaaColorImage;
        ImageView msaaColorView;
        if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
            GpuImageCreateInfo msaaColorCI{};
            msaaColorCI.device = &device;
            msaaColorCI.format = swap.colorFormat();
            msaaColorCI.width = ext.width;
            msaaColorCI.height = ext.height;
            msaaColorCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
            msaaColorCI.samples = msaaSamples;
            msaaColorCI.debugName = "msaa_color";

            msaaColorImage = GpuImage(msaaColorCI);

            ImageViewCreateInfo msaaColorViewCI{};
            msaaColorViewCI.device = &device;
            msaaColorViewCI.image = msaaColorImage.handle();
            msaaColorViewCI.format = swap.colorFormat();
            msaaColorViewCI.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            msaaColorViewCI.debugName = "msaa_color_view";

            msaaColorView.create(msaaColorViewCI);
        }

        // Create depth buffer with MSAA samples
        GpuImageCreateInfo depthCI{};
        depthCI.device = &device;
        depthCI.format = VK_FORMAT_D32_SFLOAT;
        depthCI.width = ext.width;
        depthCI.height = ext.height;
        depthCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthCI.samples = msaaSamples;
        depthCI.debugName = "depth";

        GpuImage depthImage(depthCI);

        ImageViewCreateInfo depthViewCI{};
        depthViewCI.device = &device;
        depthViewCI.image = depthImage.handle();
        depthViewCI.format = VK_FORMAT_D32_SFLOAT;
        depthViewCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewCI.range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewCI.debugName = "depth_view";

        ImageView depthView;
        depthView.create(depthViewCI);

        // 11) Load shaders and create pipeline ------------------------------------
        auto vs_model = loadSpirv(PROJECT_ROOT "/shaders/model.vert.spv");
        auto fs_model = loadSpirv(PROJECT_ROOT "/shaders/model.frag.spv");

        ShaderStageDesc vs{};
        {
            VkShaderModuleCreateInfo sci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
            sci.codeSize = vs_model.size() * 4; sci.pCode = vs_model.data();
            VkShaderModule mod{}; VK_CHECK(vkCreateShaderModule(device.device(), &sci, nullptr, &mod));
            vs.stage = VK_SHADER_STAGE_VERTEX_BIT; vs.module = mod; vs.entry = "main";
        }
        ShaderStageDesc fs{};
        {
            VkShaderModuleCreateInfo sci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
            sci.codeSize = fs_model.size() * 4; sci.pCode = fs_model.data();
            VkShaderModule mod{}; VK_CHECK(vkCreateShaderModule(device.device(), &sci, nullptr, &mod));
            fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fs.module = mod; fs.entry = "main";
        }

        // Pipeline layout: Set 0 (global) + Set 1 (material) + Push constants
        PipelineLayoutDesc pipeLayoutDesc{};
        pipeLayoutDesc.setLayouts.push_back(globalLayout.handle());
        pipeLayoutDesc.setLayouts.push_back(materialLayout.handle());

        // Push constants: mat4 model + mat4 normalMatrix + MaterialParams = 208 bytes
        // Layout: [model: 64] [normalMatrix: 64] [MaterialParams: 80] = 208 bytes total
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = 208; // 2 x mat4 (128) + MaterialParams (80)
        pipeLayoutDesc.pushConstants.push_back(pushRange);

        VkPipelineLayout modelPipelineLayout = layoutCache.get(pipeLayoutDesc);

        // Vertex input (matches hvk::Vertex)
        VertexInputDesc vi{};
        vi.bindings.push_back(Vertex::getBindingDescription());
        vi.attributes = Vertex::getAttributeDescriptions();

        // Color blend (1 attachment, alpha blending enabled for transparent materials)
        ColorBlendState cbs{};
        cbs.attachments.resize(1);
        cbs.attachments[0].blendEnable = VK_TRUE;
        cbs.attachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        // Standard alpha blending: srcAlpha * srcColor + (1 - srcAlpha) * dstColor
        cbs.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cbs.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbs.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        cbs.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cbs.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbs.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

        // Depth test enabled
        DepthStencilState dss{};
        dss.depthTestEnable = VK_TRUE;
        dss.depthWriteEnable = VK_TRUE;
        dss.depthCompare = VK_COMPARE_OP_LESS;

        // Raster state
        RasterState rs{};
        // Disable culling to avoid winding issues across assets
        rs.cullMode = VK_CULL_MODE_NONE;
        // With Vulkan-style projection (Y flipped), front faces appear clockwise.
        // Use CLOCKWISE to avoid backface culling of visible geometry.
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // Set MSAA sample count
        rs.rasterSamples = msaaSamples;

        // Render formats
        RenderFormats fmts{};
        fmts.colorFormats = { swap.colorFormat() };
        fmts.depthFormat = VK_FORMAT_D32_SFLOAT;

        // Create graphics pipeline
        GraphicsPipelineDesc gp{};
        gp.layout = modelPipelineLayout;
        gp.stages = { vs, fs };
        gp.vertexInput = vi;
        gp.raster = rs;
        gp.depthStencil = dss;
        gp.colorBlend = cbs;
        gp.formats = fmts;
        gp.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipeline modelPipeline = pipeCache.get(gp);

        // Destroy shader modules
        vkDestroyShaderModule(device.device(), vs.module, nullptr);
        vkDestroyShaderModule(device.device(), fs.module, nullptr);

        std::cout << "[11/11] Starting main loop...\n" << std::endl;

        // 12) Main loop ------------------------------------------------------------
        uint64_t frameNumber = 0;
        bool needDepthRecreate = false;

        while (!window.shouldClose()) {
            window.poll();

            // Update core systems
            Input::update();
            Time::update();

            // Toggle cursor lock with ESC
            if (Input::wasKeyJustPressed(GLFW_KEY_ESCAPE)) {
                if (Input::isCursorDisabled()) {
                    Input::setCursorMode(GLFW_CURSOR_NORMAL);
                } else {
                    Input::setCursorMode(GLFW_CURSOR_DISABLED);
                }
            }

            // Update camera
            cameraController.update(camera);

            // Handle window resize
            if (window.wasResized()) {
                device.waitIdle();
                if (swap.recreateForWindow(window)) {
                    camera.updateAspectRatio(
                        window.framebufferSize().width,
                        window.framebufferSize().height
                    );
                    needDepthRecreate = true;
                }
                window.clearResizedFlag();
            }

            // Recreate MSAA and depth buffers if needed
            if (needDepthRecreate) {
                ext = swap.extent();

                // Recreate MSAA color buffer
                if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
                    msaaColorImage = GpuImage();
                    msaaColorView = ImageView();

                    GpuImageCreateInfo newMsaaColorCI{};
                    newMsaaColorCI.device = &device;
                    newMsaaColorCI.format = swap.colorFormat();
                    newMsaaColorCI.width = ext.width;
                    newMsaaColorCI.height = ext.height;
                    newMsaaColorCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
                    newMsaaColorCI.samples = msaaSamples;
                    newMsaaColorCI.debugName = "msaa_color";

                    msaaColorImage = GpuImage(newMsaaColorCI);

                    ImageViewCreateInfo newMsaaColorViewCI{};
                    newMsaaColorViewCI.device = &device;
                    newMsaaColorViewCI.image = msaaColorImage.handle();
                    newMsaaColorViewCI.format = swap.colorFormat();
                    newMsaaColorViewCI.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                    newMsaaColorViewCI.debugName = "msaa_color_view";

                    msaaColorView.create(newMsaaColorViewCI);
                }

                // Recreate depth buffer
                depthImage = GpuImage();
                depthView = ImageView();

                GpuImageCreateInfo newDepthCI{};
                newDepthCI.device = &device;
                newDepthCI.format = VK_FORMAT_D32_SFLOAT;
                newDepthCI.width = ext.width;
                newDepthCI.height = ext.height;
                newDepthCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                newDepthCI.samples = msaaSamples;
                newDepthCI.debugName = "depth";

                depthImage = GpuImage(newDepthCI);

                ImageViewCreateInfo newDepthViewCI{};
                newDepthViewCI.device = &device;
                newDepthViewCI.image = depthImage.handle();
                newDepthViewCI.format = VK_FORMAT_D32_SFLOAT;
                newDepthViewCI.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
                newDepthViewCI.range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                newDepthViewCI.debugName = "depth_view";

                depthView.create(newDepthViewCI);
                needDepthRecreate = false;
            }

            // Begin frame
            if (sync.beginFrame() != VK_SUCCESS) continue;

            uint32_t imageIndex = 0;
            VkResult acq = sync.acquireNextImage(swap.handle(), imageIndex);
            if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
                sync.endFrame();
                if (swap.recreateForWindow(window)) {
                    camera.updateAspectRatio(
                        window.framebufferSize().width,
                        window.framebufferSize().height
                    );
                }
                continue;
            }

            uint32_t frameIndex = sync.currentFrameIndex();

            // Update scene data
            sceneData.time = Time::totalTime();
            sceneData.deltaTime = Time::deltaTime();
            sceneData.frameCount = static_cast<uint32_t>(Time::frameCount());

            // Update global descriptors
            globalDescriptors.updateScene(frameIndex, sceneData);
            globalDescriptors.updateCamera(frameIndex, camera.toCameraData(
                window.framebufferSize().width,
                window.framebufferSize().height
            ));
            globalDescriptors.updateLights(frameIndex, lightBuffer);

            // Record commands
            CmdList cmd(sync.cmd());

            // Transition images
            hvk::barrier::Batch b;
            ext = swap.extent();

            if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
                // MSAA path: transition MSAA color to attachment, swap to resolve
                b.imgs.push_back(hvk::barrier::make_image_barrier_full(
                    msaaColorImage.handle(), swap.colorFormat(),
                    hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::ColorAttachment));
                b.imgs.push_back(hvk::barrier::make_image_barrier_full(
                    swap.image(imageIndex), swap.colorFormat(),
                    hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::ColorAttachment));
            } else {
                // Non-MSAA path: transition swap image to attachment
                b.imgs.push_back(hvk::barrier::make_image_barrier_full(
                    swap.image(imageIndex), swap.colorFormat(),
                    hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::ColorAttachment));
            }

            // Transition depth image to depth attachment
            b.imgs.push_back(hvk::barrier::make_image_barrier_full(
                depthImage.handle(), VK_FORMAT_D32_SFLOAT,
                hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::DepthStencilAttachment));
            hvk::barrier::submit(cmd.handle(), b);
            b.clear();

            // Setup color attachment (MSAA if enabled, otherwise direct to swap)
            ColorAttachment colorAtt{};
            if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
                // Render to MSAA buffer
                colorAtt.view = msaaColorView.handle();
                colorAtt.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                colorAtt.clear = VkClearColorValue{ {0.1f, 0.1f, 0.15f, 1.0f} };
                // Resolve to swapchain
                colorAtt.resolveView = swap.imageView(imageIndex);
                colorAtt.resolveLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                colorAtt.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            } else {
                // Render directly to swapchain
                colorAtt.view = swap.imageView(imageIndex);
                colorAtt.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                colorAtt.clear = VkClearColorValue{ {0.1f, 0.1f, 0.15f, 1.0f} };
            }

            DepthAttachment depthAtt{};
            depthAtt.view = depthView.handle();
            depthAtt.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAtt.clear = VkClearDepthStencilValue{ 1.0f, 0 };

            VkRect2D renderArea{ {0, 0}, {ext.width, ext.height} };
            cmd.beginRendering(renderArea, {colorAtt}, &depthAtt, 0);
            // We already flip Y in the projection matrix; use a regular (non-flipped) viewport.
            cmd.setViewportScissor(ext, false);

            // Bind pipeline and global descriptors
            cmd.bindGraphicsPipeline(modelPipeline);
            VkDescriptorSet globalSet = globalDescriptors.get(frameIndex);
            vkCmdBindDescriptorSets(
                cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                modelPipelineLayout, 0, 1, &globalSet, 0, nullptr
            );

            // Render model
            // Apply root transform to fix model orientation if needed
            // Example: Rotate 90° around X-axis to convert Z-up to Y-up
            // glm::mat4 modelTransform = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 modelTransform = glm::mat4(1.0f); // Identity for now
            model.draw(cmd, modelPipelineLayout, globalSet, modelTransform);

            cmd.endRendering();

            // Transition to present
            b.imgs.push_back(hvk::barrier::color_to_present(
                swap.image(imageIndex), swap.colorFormat()));
            hvk::barrier::submit(cmd.handle(), b);

            // Submit and present
            VkResult pr = sync.submitAndPresent(swap.handle(), imageIndex);
            if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
                // Will recreate next loop
            }

            sync.endFrame();
            frameNumber++;

            // Print FPS every second
            if (frameNumber % 60 == 0) {
                std::cout << "FPS: " << Time::fps()
                          << " | Frame time: " << Time::averageFrameTime() << "ms"
                          << " | Cam pos: ("
                          << camera.position().x << ", "
                          << camera.position().y << ", "
                          << camera.position().z << ")"
                          << std::endl;
            }
        }

        // Cleanup
        std::cout << "\nShutting down..." << std::endl;
        device.waitIdle();
        Input::cleanup();

        std::cout << "Done!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
