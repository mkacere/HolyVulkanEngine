#include <hvk/gfx.hpp>
#include <hvk/resources.hpp>
#include <hvk/scene.hpp>
#include <hvk/core.hpp>

#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
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
        std::cout << "=== Holy Vulkan Engine - Primitive Shapes Demo ===" << std::endl;

        // 1) Initialize window + device
        std::cout << "[1/9] Creating window and Vulkan device..." << std::endl;
        Window window({
            .width = 1280,
            .height = 720,
            .title = "HVK - Primitives Demo",
            .mode = WindowMode::Auto
        });
        Device device(window, { .debugVerbosity = DebugVerbosity::Warn });

        // 2) Swapchain
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

        // 3) Frame sync
        std::cout << "[3/9] Setting up frame synchronization..." << std::endl;
        FrameSyncCreateInfo syncCI{};
        syncCI.device = device.device();
        syncCI.graphicsQueueFamilyIndex = device.graphics().family;
        syncCI.graphicsQueue = device.graphics().handle;
        syncCI.presentQueue = device.present().handle;
        syncCI.framesInFlight = (std::max)(2u, swap.imageCount());
        syncCI.preferTimelineSemaphore = true;
        FrameSync sync(syncCI);

        // 4) Staging uploader
        std::cout << "[4/9] Creating staging uploader..." << std::endl;
        StagingUploader uploader({
            .device = &device,
            .queue = device.graphics().handle,
            .queueFamilyIndex = device.graphics().family,
            .framesInFlight = sync.frameCount(),
            .bytesPerFrame = 64 * 1024 * 1024,
            .debugBaseName = "upload"
        });

        // 5) Initialize core systems
        std::cout << "[5/9] Initializing Input and Time systems..." << std::endl;
        Input::init(window.glfwHandle());
        Time::init();

        // 6) Create caches
        std::cout << "[6/9] Creating descriptor and pipeline caches..." << std::endl;
        SamplerCache samplerCache(&device, "samplers");
        PipelineLayoutCache layoutCache(&device, "plc");
        GraphicsPipelineCache pipeCache(&device, "gpc");

        DescriptorAllocatorCreateInfo allocCI{};
        allocCI.device = &device;
        allocCI.maxSetsPerPool = 256;
        allocCI.debugName = "desc_alloc";
        DescriptorAllocator descAllocator(allocCI);

        // 7) Create all 6 primitive shapes
        std::cout << "[7/9] Creating primitive shapes..." << std::endl;

        DescriptorSetLayout materialLayout = Material::createDescriptorSetLayout(device);

        // Create model to hold all primitives
        Model primitivesModel;
        primitivesModel.createDefaultTextures(device, uploader, samplerCache);

        // Reserve space to prevent vector reallocation (which would invalidate material pointers)
        primitivesModel.reserveMaterials(6);
        primitivesModel.reserveMeshes(6);
        primitivesModel.reserveNodes(6);

        // Define shapes with their properties
        struct ShapeInfo {
            std::string name;
            glm::vec4 color;
            glm::vec3 position;
        };

        std::vector<ShapeInfo> shapes = {
            {"Cube",     glm::vec4(1.0f, 0.3f, 0.3f, 1.0f), glm::vec3(-3.0f, 0.0f, -1.5f)},
            {"Sphere",   glm::vec4(0.3f, 1.0f, 0.3f, 1.0f), glm::vec3(0.0f, 0.0f, -1.5f)},
            {"Plane",    glm::vec4(0.3f, 0.3f, 1.0f, 1.0f), glm::vec3(3.0f, 0.0f, -1.5f)},
            {"Cylinder", glm::vec4(1.0f, 1.0f, 0.3f, 1.0f), glm::vec3(-3.0f, 0.0f, 1.5f)},
            {"Capsule",  glm::vec4(1.0f, 0.3f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.5f)},
            {"Cone",     glm::vec4(0.3f, 1.0f, 1.0f, 1.0f), glm::vec3(3.0f, 0.0f, 1.5f)}
        };

        for (size_t i = 0; i < shapes.size(); ++i) {
            const auto& shapeInfo = shapes[i];
            std::cout << "    Creating " << shapeInfo.name << "..." << std::endl;

            // Generate geometry based on shape type
            std::vector<Vertex> verts;
            std::vector<uint32_t> indices;

            if (shapeInfo.name == "Cube") {
                auto [v, idx] = Primitives::createCube(1.0f);
                verts = std::move(v);
                indices = std::move(idx);
            } else if (shapeInfo.name == "Sphere") {
                auto [v, idx] = Primitives::createSphere(0.6f, 32, 16);
                verts = std::move(v);
                indices = std::move(idx);
            } else if (shapeInfo.name == "Plane") {
                auto [v, idx] = Primitives::createPlane(2.0f, 2.0f, 10, 10);
                verts = std::move(v);
                indices = std::move(idx);
            } else if (shapeInfo.name == "Cylinder") {
                auto [v, idx] = Primitives::createCylinder(0.5f, 1.2f, 32, true);
                verts = std::move(v);
                indices = std::move(idx);
            } else if (shapeInfo.name == "Capsule") {
                auto [v, idx] = Primitives::createCapsule(0.4f, 1.0f, 32, 8);
                verts = std::move(v);
                indices = std::move(idx);
            } else if (shapeInfo.name == "Cone") {
                auto [v, idx] = Primitives::createCone(0.6f, 1.2f, 32, true);
                verts = std::move(v);
                indices = std::move(idx);
            }

            std::cout << "      " << verts.size() << " verts, " << indices.size() << " indices" << std::endl;

            // Create material with unique color
            Material mat = MaterialBuilder()
                .withBaseColorFactor(shapeInfo.color)
                .withMetallicFactor(0.1f)
                .withRoughnessFactor(0.6f)
                .withName(shapeInfo.name + "Mat")
                .build(device, descAllocator, materialLayout,
                       primitivesModel.defaultWhiteTexture(),
                       primitivesModel.defaultNormalTexture(),
                       primitivesModel.defaultMetallicRoughnessTexture());

            size_t matIdx = primitivesModel.addMaterial(std::move(mat));

            // Create mesh with proper upload frame management
            uploader.beginFrame(0);
            Mesh mesh;
            mesh.create(device, uploader, verts, indices, primitivesModel.material(matIdx), shapeInfo.name);
            uploader.submit();
            uploader.waitCurrent();

            size_t meshIdx = primitivesModel.addMesh(std::move(mesh));

            // Create node with position
            Node node;
            node.name = shapeInfo.name;
            node.meshIndex = static_cast<int32_t>(meshIdx);
            node.localTransform = glm::translate(glm::mat4(1.0f), shapeInfo.position);
            node.worldTransform = node.localTransform;
            primitivesModel.addNode(node);
        }

        std::cout << "    All primitives created successfully!" << std::endl;

        // 8) Setup global descriptors
        std::cout << "[8/9] Setting up global descriptors..." << std::endl;
        DescriptorSetLayout globalLayout = GlobalDescriptorLayout::create(device);
        GlobalDescriptorSet globalDescriptors;
        globalDescriptors.init(device, descAllocator, globalLayout, sync.frameCount());

        SceneData sceneData{};
        sceneData.ambientColor = glm::vec4(0.3f, 0.3f, 0.35f, 1.0f);

        LightBuffer lightBuffer;
        lightBuffer.lights.push_back(Light::makeDirectional(
            glm::vec3(-0.3f, -1.0f, -0.5f), glm::vec3(1.0f, 1.0f, 1.0f), 0.7f));
        lightBuffer.lightCount = 1;

        // 9) Setup camera
        std::cout << "[9/9] Setting up camera..." << std::endl;
        float aspect = static_cast<float>(window.framebufferSize().width) /
                       static_cast<float>(window.framebufferSize().height);

        Camera camera = Camera::createPerspective(
            glm::vec3(3.0f, 2.0f, 3.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            60.0f, aspect, 0.1f, 1000.0f);

        CameraController cameraController;
        cameraController.setMode(CameraController::Mode::FPS);
        cameraController.setMoveSpeed(5.0f);
        Input::setCursorMode(GLFW_CURSOR_DISABLED);

        std::cout << "\n[10/11] Creating rendering pipeline..." << std::endl;

        // 10) Create MSAA + depth buffers
        VkSampleCountFlags counts = device.limits().framebufferColorSampleCounts &
                                     device.limits().framebufferDepthSampleCounts;
        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        if (counts & VK_SAMPLE_COUNT_4_BIT) msaaSamples = VK_SAMPLE_COUNT_4_BIT;

        VkExtent2D ext = swap.extent();

        GpuImage msaaColorImage;
        ImageView msaaColorView;
        if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
            GpuImageCreateInfo msaaCI{};
            msaaCI.device = &device;
            msaaCI.format = swap.colorFormat();
            msaaCI.width = ext.width;
            msaaCI.height = ext.height;
            msaaCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
            msaaCI.samples = msaaSamples;
            msaaCI.debugName = "msaa_color";
            msaaColorImage = GpuImage(msaaCI);

            ImageViewCreateInfo viewCI{};
            viewCI.device = &device;
            viewCI.image = msaaColorImage.handle();
            viewCI.format = swap.colorFormat();
            viewCI.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            msaaColorView.create(viewCI);
        }

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
        ImageView depthView;
        depthView.create(depthViewCI);

        // 11) Load shaders and create pipeline
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

        PipelineLayoutDesc pipeLayoutDesc{};
        pipeLayoutDesc.setLayouts.push_back(globalLayout.handle());
        pipeLayoutDesc.setLayouts.push_back(materialLayout.handle());
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = 208;
        pipeLayoutDesc.pushConstants.push_back(pushRange);
        VkPipelineLayout modelPipelineLayout = layoutCache.get(pipeLayoutDesc);

        VertexInputDesc vi{};
        vi.bindings.push_back(Vertex::getBindingDescription());
        vi.attributes = Vertex::getAttributeDescriptions();

        ColorBlendState cbs{};
        cbs.attachments.resize(1);
        cbs.attachments[0].blendEnable = VK_TRUE;
        cbs.attachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        cbs.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cbs.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbs.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        cbs.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cbs.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbs.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

        DepthStencilState dss{};
        dss.depthTestEnable = VK_TRUE;
        dss.depthWriteEnable = VK_TRUE;
        dss.depthCompare = VK_COMPARE_OP_LESS;

        RasterState rs{};
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rs.rasterSamples = msaaSamples;

        RenderFormats fmts{};
        fmts.colorFormats = { swap.colorFormat() };
        fmts.depthFormat = VK_FORMAT_D32_SFLOAT;

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

        vkDestroyShaderModule(device.device(), vs.module, nullptr);
        vkDestroyShaderModule(device.device(), fs.module, nullptr);

        std::cout << "[11/11] Starting main loop...\n" << std::endl;

        // 12) Main loop
        uint64_t frameNumber = 0;
        while (!window.shouldClose()) {
            window.poll();
            Input::update();
            Time::update();

            if (Input::wasKeyJustPressed(GLFW_KEY_ESCAPE)) {
                if (Input::isCursorDisabled()) Input::setCursorMode(GLFW_CURSOR_NORMAL);
                else Input::setCursorMode(GLFW_CURSOR_DISABLED);
            }

            cameraController.update(camera);

            if (sync.beginFrame() != VK_SUCCESS) continue;

            uint32_t imageIndex = 0;
            VkResult acq = sync.acquireNextImage(swap.handle(), imageIndex);
            if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
                sync.endFrame();
                continue;
            }

            uint32_t frameIndex = sync.currentFrameIndex();

            sceneData.time = Time::totalTime();
            sceneData.deltaTime = Time::deltaTime();
            sceneData.frameCount = static_cast<uint32_t>(Time::frameCount());

            globalDescriptors.updateScene(frameIndex, sceneData);
            globalDescriptors.updateCamera(frameIndex, camera.toCameraData(
                window.framebufferSize().width, window.framebufferSize().height));
            globalDescriptors.updateLights(frameIndex, lightBuffer);

            CmdList cmd(sync.cmd());

            hvk::barrier::Batch b;
            ext = swap.extent();

            if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
                b.imgs.push_back(hvk::barrier::make_image_barrier_full(
                    msaaColorImage.handle(), swap.colorFormat(),
                    hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::ColorAttachment));
                b.imgs.push_back(hvk::barrier::make_image_barrier_full(
                    swap.image(imageIndex), swap.colorFormat(),
                    hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::ColorAttachment));
            } else {
                b.imgs.push_back(hvk::barrier::make_image_barrier_full(
                    swap.image(imageIndex), swap.colorFormat(),
                    hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::ColorAttachment));
            }

            b.imgs.push_back(hvk::barrier::make_image_barrier_full(
                depthImage.handle(), VK_FORMAT_D32_SFLOAT,
                hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::DepthStencilAttachment));
            hvk::barrier::submit(cmd.handle(), b);
            b.clear();

            ColorAttachment colorAtt{};
            if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
                colorAtt.view = msaaColorView.handle();
                colorAtt.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                colorAtt.clear = VkClearColorValue{ {0.1f, 0.1f, 0.15f, 1.0f} };
                colorAtt.resolveView = swap.imageView(imageIndex);
                colorAtt.resolveLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                colorAtt.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            } else {
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
            cmd.setViewportScissor(ext, false);

            cmd.bindGraphicsPipeline(modelPipeline);
            VkDescriptorSet globalSet = globalDescriptors.get(frameIndex);
            vkCmdBindDescriptorSets(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    modelPipelineLayout, 0, 1, &globalSet, 0, nullptr);

            glm::mat4 modelTransform = glm::mat4(1.0f);
            primitivesModel.draw(cmd, modelPipelineLayout, globalSet, modelTransform);

            cmd.endRendering();

            b.imgs.push_back(hvk::barrier::color_to_present(swap.image(imageIndex), swap.colorFormat()));
            hvk::barrier::submit(cmd.handle(), b);

            VkResult pr = sync.submitAndPresent(swap.handle(), imageIndex);
            if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
                // Will recreate next loop
            }

            sync.endFrame();
            frameNumber++;

            if (frameNumber % 60 == 0) {
                std::cout << "FPS: " << Time::fps() << " | Cam: ("
                          << camera.position().x << ", "
                          << camera.position().y << ", "
                          << camera.position().z << ")" << std::endl;
            }
        }

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
