#include <hvk/gfx.hpp>
#include <fstream>
#include <vector>
#include <cstring>

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

struct Vertex { float pos[2]; float color[3]; };

int main() {
    // 1) Window + Device --------------------------------------------------------
    Window window({ .width = 1280, .height = 720, .title = "HVK Triangle", .mode = WindowMode::Windowed });
    Device device(window, { .debugVerbosity = DebugVerbosity::Info }); // DeviceCreateInfo defaults enable dynamic rendering + sync2

    // 2) Swapchain --------------------------------------------------------------
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

    // 3) Frame sync (use graphics queue for everything to avoid ownership xfers)
    FrameSyncCreateInfo syncCI{};
    syncCI.device = device.device();
    syncCI.graphicsQueueFamilyIndex = device.graphics().family;
    syncCI.graphicsQueue = device.graphics().handle;
    syncCI.presentQueue = device.present().handle;
    syncCI.framesInFlight = (std::max)(2u, swap.imageCount());
    syncCI.preferTimelineSemaphore = true;
    FrameSync sync(syncCI);

    // 4) One vertex buffer + upload --------------------------------------------
    // Triangle in NDC with RGB colors
    const Vertex tri[3] = {
        { {-0.5f, -0.5f}, {1,0,0} },
        { { 0.5f, -0.5f}, {0,1,0} },
        { { 0.0f,  0.5f}, {0,0,1} },
    };

    // GPU-local VB
    GpuBuffer vbuf({
        .device = &device,
        .size = sizeof(tri),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .persistentMap = false,
        .debugName = "vb.triangle"
        });

    // Staging uploader on the graphics queue (simple path)
    StagingUploader uploader({
        .device = &device,
        .queue = device.graphics().handle,
        .queueFamilyIndex = device.graphics().family,
        .framesInFlight = sync.frameCount(),
        .bytesPerFrame = 4 * 1024 * 1024,
        .debugBaseName = "upload"
        });
    uploader.beginFrame(0);
    auto sl = uploader.write(tri, sizeof(tri));
    uploader.copyBuffer(vbuf.handle(), 0, sl);
    uploader.submit(); // submit the upload
    uploader.waitCurrent(); // wait for upload to complete - now data is on the GPU; no extra barriers needed

    // 5) Pipeline layout + pipeline (dynamic rendering) -------------------------
    PipelineLayoutCache layoutCache(&device, "plc");
    GraphicsPipelineCache pipeCache(&device, "gpc");

    // Empty layout (no descriptors/push)
    VkPipelineLayout layout = layoutCache.get({});

    // Load shaders
    auto vs_bin = loadSpirv("../../../../shaders/tri.vert.spv");
    auto fs_bin = loadSpirv("../../../../shaders/tri.frag.spv");

    ShaderStageDesc vs{};
    {
        VkShaderModuleCreateInfo sci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        sci.codeSize = vs_bin.size() * 4; sci.pCode = vs_bin.data();
        VkShaderModule mod{}; VK_CHECK(vkCreateShaderModule(device.device(), &sci, nullptr, &mod));
        vs.stage = VK_SHADER_STAGE_VERTEX_BIT; vs.module = mod; vs.entry = "main";
    }
    ShaderStageDesc fs{};
    {
        VkShaderModuleCreateInfo sci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        sci.codeSize = fs_bin.size() * 4; sci.pCode = fs_bin.data();
        VkShaderModule mod{}; VK_CHECK(vkCreateShaderModule(device.device(), &sci, nullptr, &mod));
        fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fs.module = mod; fs.entry = "main";
    }

    // Vertex input
    VertexInputDesc vi{};
    vi.bindings.push_back({ /*binding*/0, /*stride*/(uint32_t)sizeof(Vertex), /*rate*/VK_VERTEX_INPUT_RATE_VERTEX });
    vi.attributes.push_back({ /*loc*/0, /*binding*/0, /*format*/VK_FORMAT_R32G32_SFLOAT,       /*offset*/0 });
    vi.attributes.push_back({ /*loc*/1, /*binding*/0, /*format*/VK_FORMAT_R32G32B32_SFLOAT,    /*offset*/(uint32_t)offsetof(Vertex,color) });

    // Color blend (1 attachment, no blending)
    ColorBlendState cbs{};
    cbs.attachments.resize(1);
    cbs.attachments[0].blendEnable = VK_FALSE;
    cbs.attachments[0].colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Depth off
    DepthStencilState dss{ .depthTestEnable = VK_FALSE, .depthWriteEnable = VK_FALSE };

    // Raster
    RasterState rs{};
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Formats for dynamic rendering
    RenderFormats fmts{};
    fmts.colorFormats = { swap.colorFormat() };

    GraphicsPipelineDesc gp{};
    gp.layout = layout;
    gp.stages = { vs, fs };
    gp.vertexInput = vi;
    gp.raster = rs;
    gp.depthStencil = dss;
    gp.colorBlend = cbs;
    gp.formats = fmts;
    gp.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipeline pipeline = pipeCache.get(gp);

    // We can destroy shader modules after pipeline creation
    vkDestroyShaderModule(device.device(), vs.module, nullptr);
    vkDestroyShaderModule(device.device(), fs.module, nullptr);

    // 6) Main loop --------------------------------------------------------------
    while (!window.shouldClose()) {
        window.poll();

        // Recreate swapchain on resize/minimize restore
        if (window.wasResized()) {
            device.waitIdle();
            if (swap.recreateForWindow(window)) {
                // If format changed, request a pipeline variant for the new format
                fmts.colorFormats[0] = swap.colorFormat();
                gp.formats = fmts;
                pipeline = pipeCache.get(gp);
            }
            window.clearResizedFlag();
        }

        // Begin the frame & acquire an image
        if (sync.beginFrame() != VK_SUCCESS) continue;

        uint32_t imageIndex = 0;
        VkResult acq = sync.acquireNextImage(swap.handle(), imageIndex);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
            sync.endFrame();
            if (swap.recreateForWindow(window)) {
                fmts.colorFormats[0] = swap.colorFormat();
                gp.formats = fmts;
                pipeline = pipeCache.get(gp);
            }
            continue;
        }

        // Record commands
        CmdList cmd(sync.cmd()); // already begun with ONE_TIME by FrameSync

        // Transition swap image → attachment
        hvk::barrier::Batch b;
        b.imgs.push_back(hvk::barrier::make_image_barrier_full(
            swap.image(imageIndex), swap.colorFormat(),
            hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::ColorAttachment)); // present→attach also OK
        hvk::barrier::submit(cmd.handle(), b); b.clear();

        // Begin dynamic rendering to the swap image view
        VkExtent2D ext = swap.extent();
        VkClearColorValue clear{ {0.1f, 0.2f, 0.9f, 1.0f} };
        cmd.beginRenderingColor(swap.imageView(imageIndex), ext, clear);
        cmd.setViewportScissor(ext, true);

        // Bind pipeline + VB and draw
        cmd.bindGraphicsPipeline(pipeline);
        cmd.bindVertexBuffer(0, vbuf.handle(), 0);
        cmd.draw(3);

        cmd.endRendering();

        // Transition to present
        b.imgs.push_back(hvk::barrier::color_to_present(swap.image(imageIndex), swap.colorFormat()));
        hvk::barrier::submit(cmd.handle(), b);

        // Submit + present (FrameSync ends the command buffer internally)
        VkResult pr = sync.submitAndPresent(swap.handle(), imageIndex);
        if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
            // Recreate next loop
        }
        sync.endFrame();
    }

    device.waitIdle();
    return 0;
}




/*#include <hvk/gfx.hpp>
#include <hvk/systems.hpp>

#include <GLFW/glfw3.h>
#include <memory>

// Simple mouse‐move callback that forwards to your HvkCamera
static void mouseCallback(GLFWwindow* win, double xpos, double ypos) {
    auto cam = reinterpret_cast<hvk::HvkCamera*>(glfwGetWindowUserPointer(win));
    cam->processMouseMovement(xpos, ypos);
}

int main() {
    hvk::WindowCreateInfo wci{};
    wci.title = "HolyVulkanEngine Example";
    wci.mode = hvk::WindowMode::Auto;

    hvk::Window window{ wci };
    hvk::Device device{ window };

    // 2) Set up the camera (pass in the GLFWwindow* so it can hook callbacks)
    float aspect = window.windowSize().width / float(window.windowSize().height);
    hvk::HvkCamera camera{
        window.glfwHandle(),     // raw GLFW handle
        glm::radians(60.0f),        // vertical FOV
        aspect,                     // aspect ratio
        0.1f,                       // near plane
        100.0f                      // far plane
    };

    // tell GLFW to forward mouse movements to our camera
    glfwSetWindowUserPointer(window.glfwHandle(), &camera);
    glfwSetCursorPosCallback(window.glfwHandle(), mouseCallback);
    // hide the cursor and capture it for FPS‐style look
    glfwSetInputMode(window.glfwHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 3) Create renderer and add our ModelRenderSystem
    hvk::HvkRenderer renderer{ window, device };
    renderer.addRenderSystem(
        std::make_unique<hvk::ModelRenderSystem>(
            device,
            "../../../../assets/models/Crystar_Kokoro_Fudoji.glb"
        )
    );

    // optional placeholder for future passes
    hvk::HvkGameObject::Map gameObjects;

    // 4) Main loop: track both absolute time (for animation) and deltaTime (for the camera)
    float lastTime = static_cast<float>(glfwGetTime());
    while (!window.shouldClose()) {
        glfwPollEvents();
        

        // compute timing
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // update camera (process WASD, etc.)
        camera.update(deltaTime);

        // render: we pass currentTime as our “frameTime” so systems can animate
        renderer.drawFrame(currentTime, camera, gameObjects);
    }

    vkDeviceWaitIdle(device.device());
    return 0;
}
*/