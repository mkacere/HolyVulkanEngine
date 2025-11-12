#include <hvk/gfx.hpp>
#include <hvk/resources.hpp>
#include <hvk/resources/hvk_cubemap.h>
#include <hvk/scene.hpp>
#include <hvk/core.hpp>
#include <hvk/ui.hpp>

#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <algorithm>

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

// Scan the models directory for .glb and .gltf files
static std::vector<std::string> scanModelFiles(const std::string& modelsDir) {
    namespace fs = std::filesystem;
    std::vector<std::string> modelFiles;

    try {
        if (!fs::exists(modelsDir) || !fs::is_directory(modelsDir)) {
            std::cout << "Warning: Models directory not found: " << modelsDir << std::endl;
            return modelFiles;
        }

        for (const auto& entry : fs::directory_iterator(modelsDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".glb" || ext == ".gltf") {
                    modelFiles.push_back(entry.path().filename().string());
                }
            }
        }

        // Sort alphabetically for consistent ordering
        std::sort(modelFiles.begin(), modelFiles.end());

    } catch (const std::exception& e) {
        std::cout << "Error scanning models directory: " << e.what() << std::endl;
    }

    return modelFiles;
}

int main() {
    try {
        std::cout << "=== Holy Vulkan Engine - GLTF Model Demo ===" << std::endl;

        // 1) Initialize window + device --------------------------------------------
        std::cout << "[1/9] Creating window and Vulkan device..." << std::endl;
        Window window({
            .width = 1280,
            .height = 720,
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
        std::cout << "[7/9] Scanning and loading GLTF models..." << std::endl;

        // Create material descriptor set layout
        DescriptorSetLayout materialLayout = Material::createDescriptorSetLayout(device);

        // Scan available models
        std::string modelsDir = std::string(PROJECT_ROOT) + "/assets/models";
        std::vector<std::string> availableModels = scanModelFiles(modelsDir);

        if (availableModels.empty()) {
            std::cerr << "ERROR: No models found in " << modelsDir << std::endl;
            std::cerr << "Please add .glb or .gltf files to the assets/models directory." << std::endl;
            return 1;
        }

        std::cout << "    Found " << availableModels.size() << " model(s):" << std::endl;
        for (size_t i = 0; i < availableModels.size(); ++i) {
            std::cout << "      [" << i << "] " << availableModels[i] << std::endl;
        }

        // Model loader state
        int currentModelIndex = 0;  // Start with first model
        int selectedModelIndex = 0; // UI selection
        bool requestLoadModel = false;
        std::string loadErrorMessage;
        glm::vec3 modelCenter = glm::vec3(0.0f);  // Model center for centering transform

        // Load initial model
        std::string initialModelPath = modelsDir + "/" + availableModels[currentModelIndex];
        std::cout << "    Loading initial model: " << availableModels[currentModelIndex] << std::endl;

        GltfLoaderOptions loaderOptions;
        loaderOptions.generateMipmaps = true;
        loaderOptions.loadMaterials = true;
        loaderOptions.loadTextures = true;
        loaderOptions.flipTextureY = false;
        loaderOptions.forceLinearTextures = false;
        loaderOptions.verbose = false;

        Model model = GltfLoader::loadFromFile(
            device,
            uploader,
            descAllocator,
            materialLayout,
            samplerCache,
            initialModelPath.c_str(),
            loaderOptions
        );

        std::cout << "    Model loaded successfully!" << std::endl;
        std::cout << "    - Textures: " << model.textureCount() << std::endl;
        std::cout << "    - Materials: " << model.materialCount() << std::endl;
        std::cout << "    - Meshes: " << model.meshCount() << std::endl;
        std::cout << "    - Nodes: " << model.nodeCount() << std::endl;

        // Calculate initial model center for centering transform (use WORLD-SPACE bounds)
        const auto initialBounds = model.worldBounds();
        modelCenter = (initialBounds.min + initialBounds.max) * 0.5f;  // Update persistent variable
        std::cout << "    - World bounds: min(" << initialBounds.min.x << ", " << initialBounds.min.y << ", " << initialBounds.min.z
                  << ") max(" << initialBounds.max.x << ", " << initialBounds.max.y << ", " << initialBounds.max.z << ")" << std::endl;
        std::cout << "    - World center: (" << modelCenter.x << ", " << modelCenter.y << ", " << modelCenter.z << ")" << std::endl;

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

        // Adaptive camera setup based on model bounds (model will be centered at origin)
        glm::vec3 modelSize = initialBounds.max - initialBounds.min;

        // Calculate the diagonal of the bounding box to determine model scale
        float modelDiagonal = glm::length(modelSize);

        // Position camera at a distance proportional to model size
        // Distance formula: ensure the model fits in view with some padding
        float fovRadians = glm::radians(60.0f);
        float distance = (modelDiagonal * 0.5f) / glm::tan(fovRadians * 0.5f) * 1.5f;  // 1.5x for padding

        // Position camera: offset from origin (since model is centered at 0,0,0)
        // Look at origin from a 45-degree angle (front-right, elevated view)
        // Use higher Y component (1.0 instead of 0.5) for better viewing angle
        glm::vec3 cameraOffset = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)) * distance;
        glm::vec3 camPos = cameraOffset;  // Position relative to origin

        // Camera looks at origin (0,0,0) where the model is centered
        Camera camera = Camera::createPerspective(
            camPos,           // camera position (adaptive)
            glm::vec3(0.0f),  // look-at target (origin)
            60.0f,            // FOV
            aspect,           // aspect ratio
            0.1f,             // near plane
            distance * 10.0f  // far plane (based on model scale)
        );

        // Calculate yaw and pitch from camera direction to initialize FPS controller correctly
        glm::vec3 direction = glm::normalize(glm::vec3(0.0f) - camPos);  // Direction from camera to origin
        float yaw = glm::degrees(atan2(direction.z, direction.x));       // Horizontal angle
        float pitch = glm::degrees(asin(direction.y));                    // Vertical angle

        CameraController cameraController;
        cameraController.setMode(CameraController::Mode::FPS);
        cameraController.setYaw(yaw);      // Initialize with correct yaw
        cameraController.setPitch(pitch);  // Initialize with correct pitch
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

        // 10) Optional: Load skybox cubemap ---------------------------------------
        std::cout << "[10/12] Loading skybox (optional)..." << std::endl;
        bool hasSkybox = false;
        Cubemap skybox;
        DescriptorSetLayout skyboxLayout;
        VkDescriptorSet skyboxSet = VK_NULL_HANDLE;
        VkPipelineLayout skyboxPipelineLayout = VK_NULL_HANDLE;
        VkPipeline skyboxPipeline = VK_NULL_HANDLE;

        {
            std::array<std::string, 6> faces = {
                std::string(PROJECT_ROOT "/assets/skybox/px.png"), // +X (right)
                std::string(PROJECT_ROOT "/assets/skybox/nx.png"), // -X (left)
                std::string(PROJECT_ROOT "/assets/skybox/py.png"), // +Y (top)
                std::string(PROJECT_ROOT "/assets/skybox/ny.png"), // -Y (bottom)
                std::string(PROJECT_ROOT "/assets/skybox/pz.png"), // +Z (front)
                std::string(PROJECT_ROOT "/assets/skybox/nz.png")  // -Z (back)
            };

            skybox = Cubemap::loadFromFiles(device, uploader, samplerCache, faces, /*mips*/true, /*sRGB*/true, "skybox");
            if (skybox) {
                // Create skybox descriptor set layout: set 1, binding 0 = combined image sampler (cube)
                DescriptorSetLayoutCreateInfo lci{};
                lci.device = &device;
                VkDescriptorSetLayoutBinding b{};
                b.binding = 0;
                b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                b.descriptorCount = 1;
                b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                lci.bindings.push_back(b);
                lci.debugName = "set1_skybox";
                skyboxLayout = DescriptorSetLayout{ lci };

                skyboxSet = descAllocator.allocate(skyboxLayout);
                DescriptorWrites writes;
                writes.writeImage(skyboxSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    skybox.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, skybox.sampler());
                writes.commit(device.device());
                hasSkybox = true;
            } else {
                std::cout << "    Skybox not found (place px/nx/py/ny/pz/nz in assets/skybox). Skipping." << std::endl;
            }
        }

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

        // Skybox pipeline (if skybox loaded)
        if (hasSkybox) {
            auto vs_sky = loadSpirv(PROJECT_ROOT "/shaders/skybox.vert.spv");
            auto fs_sky = loadSpirv(PROJECT_ROOT "/shaders/skybox.frag.spv");

            ShaderStageDesc vsSky{};
            {
                VkShaderModuleCreateInfo sci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
                sci.codeSize = vs_sky.size() * 4; sci.pCode = vs_sky.data();
                VkShaderModule mod{}; VK_CHECK(vkCreateShaderModule(device.device(), &sci, nullptr, &mod));
                vsSky.stage = VK_SHADER_STAGE_VERTEX_BIT; vsSky.module = mod; vsSky.entry = "main";
            }
            ShaderStageDesc fsSky{};
            {
                VkShaderModuleCreateInfo sci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
                sci.codeSize = fs_sky.size() * 4; sci.pCode = fs_sky.data();
                VkShaderModule mod{}; VK_CHECK(vkCreateShaderModule(device.device(), &sci, nullptr, &mod));
                fsSky.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fsSky.module = mod; fsSky.entry = "main";
            }

            // Pipeline layout: Set 0 (global), Set 1 (skybox)
            PipelineLayoutDesc skyPL{};
            skyPL.setLayouts.push_back(globalLayout.handle());
            skyPL.setLayouts.push_back(skyboxLayout.handle());
            skyboxPipelineLayout = layoutCache.get(skyPL);

            // No vertex input (fullscreen triangle from gl_VertexIndex)
            VertexInputDesc viSky{};

            RasterState rsSky{};
            rsSky.cullMode = VK_CULL_MODE_NONE;
            rsSky.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rsSky.rasterSamples = msaaSamples;

            DepthStencilState dssSky{};
            dssSky.depthTestEnable = VK_FALSE;  // skybox does not need depth test
            dssSky.depthWriteEnable = VK_FALSE; // and does not write depth

            ColorBlendState cbsSky{};
            cbsSky.attachments.resize(1);
            cbsSky.attachments[0].blendEnable = VK_FALSE;
            cbsSky.attachments[0].colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            RenderFormats fmtsSky{};
            fmtsSky.colorFormats = { swap.colorFormat() };
            fmtsSky.depthFormat = VK_FORMAT_D32_SFLOAT;

            GraphicsPipelineDesc gpSky{};
            gpSky.layout = skyboxPipelineLayout;
            gpSky.stages = { vsSky, fsSky };
            gpSky.vertexInput = viSky;
            gpSky.raster = rsSky;
            gpSky.depthStencil = dssSky;
            gpSky.colorBlend = cbsSky;
            gpSky.formats = fmtsSky;
            gpSky.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

            skyboxPipeline = pipeCache.get(gpSky);

            // Destroy temp shader modules
            vkDestroyShaderModule(device.device(), vsSky.module, nullptr);
            vkDestroyShaderModule(device.device(), fsSky.module, nullptr);
        }

        // Vertex input (matches hvk::Vertex)
        VertexInputDesc vi{};
        vi.bindings.push_back(Vertex::getBindingDescription());
        vi.attributes = Vertex::getAttributeDescriptions();

        // ===== Three-Pass Transparency Setup =====
        // We create three pipelines for GLTF-compliant transparency rendering:
        // 1. Opaque: depth write ON, blending OFF
        // 2. Masked: depth write ON, blending OFF (alpha cutoff in shader)
        // 3. Blended: depth write OFF, blending ON

        // Raster state (shared by all pipelines)
        RasterState rs{};
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rs.rasterSamples = msaaSamples;

        // Render formats (shared by all pipelines)
        RenderFormats fmts{};
        fmts.colorFormats = { swap.colorFormat() };
        fmts.depthFormat = VK_FORMAT_D32_SFLOAT;

        // === Pipeline 1: Opaque Materials ===
        DepthStencilState dssOpaque{};
        dssOpaque.depthTestEnable = VK_TRUE;
        dssOpaque.depthWriteEnable = VK_TRUE;   // Write depth
        dssOpaque.depthCompare = VK_COMPARE_OP_LESS;

        ColorBlendState cbsOpaque{};
        cbsOpaque.attachments.resize(1);
        cbsOpaque.attachments[0].blendEnable = VK_FALSE;  // No blending for opaque
        cbsOpaque.attachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        GraphicsPipelineDesc gpOpaque{};
        gpOpaque.layout = modelPipelineLayout;
        gpOpaque.stages = { vs, fs };
        gpOpaque.vertexInput = vi;
        gpOpaque.raster = rs;
        gpOpaque.depthStencil = dssOpaque;
        gpOpaque.colorBlend = cbsOpaque;
        gpOpaque.formats = fmts;
        gpOpaque.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipeline opaquePipeline = pipeCache.get(gpOpaque);

        // === Pipeline 2: Masked Materials (Alpha Cutout with Alpha-to-Coverage) ===
        // Alpha-to-Coverage: AAA industry standard for hair/foliage
        // - Converts fragment alpha to MSAA coverage mask
        // - Gives smooth edges without blending
        // - Depth writes enabled = proper depth ordering
        // - Used by Unreal Engine, Unity, and virtually all AAA games

        DepthStencilState dssMasked{};
        dssMasked.depthTestEnable = VK_TRUE;
        dssMasked.depthWriteEnable = VK_TRUE;   // Write depth (cutout acts like opaque)
        dssMasked.depthCompare = VK_COMPARE_OP_LESS;

        ColorBlendState cbsMasked{};
        cbsMasked.attachments.resize(1);
        cbsMasked.attachments[0].blendEnable = VK_FALSE;  // No blending (alpha-to-coverage handles edges)
        cbsMasked.attachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        // Enable alpha-to-coverage for smooth hair/eyelash edges (AAA industry standard)
        RasterState rsMasked = rs;  // Copy base raster state
        rsMasked.alphaToCoverageEnable = VK_TRUE;  // KEY: This enables alpha-to-coverage!

        GraphicsPipelineDesc gpMasked{};
        gpMasked.layout = modelPipelineLayout;
        gpMasked.stages = { vs, fs };
        gpMasked.vertexInput = vi;
        gpMasked.raster = rsMasked;  // Use raster state with alpha-to-coverage enabled
        gpMasked.depthStencil = dssMasked;
        gpMasked.colorBlend = cbsMasked;
        gpMasked.formats = fmts;
        gpMasked.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipeline maskedPipeline = pipeCache.get(gpMasked);

        // === Pipeline 3: Blended Materials (True Transparency) ===
        DepthStencilState dssBlended{};
        dssBlended.depthTestEnable = VK_TRUE;
        dssBlended.depthWriteEnable = VK_FALSE;  // NO depth write (fixes transparency issue!)
        dssBlended.depthCompare = VK_COMPARE_OP_LESS;

        ColorBlendState cbsBlended{};
        cbsBlended.attachments.resize(1);
        cbsBlended.attachments[0].blendEnable = VK_TRUE;  // Enable blending
        cbsBlended.attachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        // Standard alpha blending: srcAlpha * srcColor + (1 - srcAlpha) * dstColor
        cbsBlended.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cbsBlended.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbsBlended.attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        cbsBlended.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cbsBlended.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbsBlended.attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

        GraphicsPipelineDesc gpBlended{};
        gpBlended.layout = modelPipelineLayout;
        gpBlended.stages = { vs, fs };
        gpBlended.vertexInput = vi;
        gpBlended.raster = rs;
        gpBlended.depthStencil = dssBlended;
        gpBlended.colorBlend = cbsBlended;
        gpBlended.formats = fmts;
        gpBlended.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipeline blendedPipeline = pipeCache.get(gpBlended);

        std::cout << "    Created 3 pipelines: Opaque, Masked, Blended" << std::endl;

        // Destroy shader modules
        vkDestroyShaderModule(device.device(), vs.module, nullptr);
        vkDestroyShaderModule(device.device(), fs.module, nullptr);

        std::cout << "[11/12] Initializing ImGui..." << std::endl;

        // 12) ImGui initialization -------------------------------------------------
        ImGuiLayerCreateInfo imguiCI{};
        imguiCI.device = &device;
        imguiCI.window = window.glfwHandle();
        imguiCI.framesInFlight = sync.frameCount();
        imguiCI.colorFormat = swap.colorFormat();
        imguiCI.depthFormat = VK_FORMAT_D32_SFLOAT;  // Match the depth buffer format
        imguiCI.msaaSamples = msaaSamples;
        imguiCI.enableDocking = true;

        ImGuiLayer imgui(imguiCI);

        std::cout << "[12/12] Starting main loop...\n" << std::endl;

        // 13) Main loop ------------------------------------------------------------
        uint64_t frameNumber = 0;
        bool needDepthRecreate = false;

        while (!window.shouldClose()) {
            window.poll();

            // Update core systems
            Input::update();
            Time::update();

            // Begin ImGui frame
            imgui.newFrame();

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

            // ImGui debug windows
            {
                // Performance window
                ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::Text("FPS: %.1f", Time::fps());
                ImGui::Text("Frame Time: %.3f ms", Time::averageFrameTime());
                ImGui::Text("Delta Time: %.4f s", Time::deltaTime());
                ImGui::Text("Frame Count: %llu", Time::frameCount());
                ImGui::Text("Total Time: %.2f s", Time::totalTime());
                ImGui::Separator();
                ImGui::Text("Swapchain: %ux%u", swap.extent().width, swap.extent().height);
                ImGui::Text("MSAA: %dx", msaaSamples);
                ImGui::End();

                // Camera info window
                ImGui::Begin("Camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                auto pos = camera.position();
                ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
                auto rot = camera.eulerAngles();
                ImGui::Text("Rotation: (%.1f, %.1f, %.1f)", rot.x, rot.y, rot.z);
                ImGui::Text("FOV: %.1f", camera.fovY());
                ImGui::Separator();
                ImGui::Text("Controller Mode: FPS");
                ImGui::Text("Move Speed: %.1f", cameraController.moveSpeed());
                ImGui::Text("Mouse Sensitivity: %.2f", cameraController.mouseSensitivity());
                ImGui::End();

                // Scene info window
                ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::Text("Current Model: %s", availableModels[currentModelIndex].c_str());
                ImGui::Text("Textures: %u", model.textureCount());
                ImGui::Text("Materials: %u", model.materialCount());
                ImGui::Text("Meshes: %u", model.meshCount());
                ImGui::Text("Nodes: %u", model.nodeCount());
                ImGui::Separator();
                ImGui::Text("Lights: %u", lightBuffer.lightCount);
                ImGui::ColorEdit3("Ambient", &sceneData.ambientColor.x);
                ImGui::End();

                // Model Loader window
                ImGui::Begin("Model Loader", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::Text("Available Models (%zu)", availableModels.size());
                ImGui::Separator();

                // Dropdown to select model
                if (ImGui::BeginCombo("Select Model", availableModels[selectedModelIndex].c_str())) {
                    for (int i = 0; i < static_cast<int>(availableModels.size()); ++i) {
                        bool isSelected = (selectedModelIndex == i);
                        if (ImGui::Selectable(availableModels[i].c_str(), isSelected)) {
                            selectedModelIndex = i;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Spacing();

                // Show if selected model is different from current
                bool canLoad = (selectedModelIndex != currentModelIndex);
                if (!canLoad) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Model already loaded)");
                }

                // Load button
                ImGui::BeginDisabled(!canLoad);
                if (ImGui::Button("Load Selected Model", ImVec2(200, 30))) {
                    requestLoadModel = true;
                    loadErrorMessage.clear();
                }
                ImGui::EndDisabled();

                // Show error message if loading failed
                if (!loadErrorMessage.empty()) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error:");
                    ImGui::TextWrapped("%s", loadErrorMessage.c_str());
                }

                ImGui::End();

                // Close button - only visible when cursor is enabled
                if (!Input::isCursorDisabled()) {
                    ImGui::Begin("Application", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
                    ImGui::Text("Press ESC to toggle cursor lock");
                    ImGui::Separator();
                    if (ImGui::Button("Close Application", ImVec2(200, 30))) {
                        window.setShouldClose(true);
                    }
                    ImGui::End();
                }
            }

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

                // Notify ImGui of resize
                imgui.onResize(ext.width, ext.height);
            }

            // Handle dynamic model loading
            if (requestLoadModel && selectedModelIndex != currentModelIndex) {
                std::cout << "\n=== Loading new model ===" << std::endl;
                std::cout << "Unloading: " << availableModels[currentModelIndex] << std::endl;
                std::cout << "Loading: " << availableModels[selectedModelIndex] << std::endl;

                try {
                    // Wait for all GPU operations to complete
                    device.waitIdle();

                    // Destroy old model (RAII will handle cleanup)
                    model = Model();

                    // Load new model
                    std::string newModelPath = modelsDir + "/" + availableModels[selectedModelIndex];
                    model = GltfLoader::loadFromFile(
                        device,
                        uploader,
                        descAllocator,
                        materialLayout,
                        samplerCache,
                        newModelPath.c_str(),
                        loaderOptions
                    );

                    std::cout << "Model loaded successfully!" << std::endl;
                    std::cout << "  - Textures: " << model.textureCount() << std::endl;
                    std::cout << "  - Materials: " << model.materialCount() << std::endl;
                    std::cout << "  - Meshes: " << model.meshCount() << std::endl;
                    std::cout << "  - Nodes: " << model.nodeCount() << std::endl;

                    // Update model center for centering transform (use WORLD-SPACE bounds)
                    const auto bounds = model.worldBounds();
                    modelCenter = (bounds.min + bounds.max) * 0.5f;
                    std::cout << "  - World bounds: min(" << bounds.min.x << ", " << bounds.min.y << ", " << bounds.min.z
                              << ") max(" << bounds.max.x << ", " << bounds.max.y << ", " << bounds.max.z << ")" << std::endl;
                    std::cout << "  - World center: (" << modelCenter.x << ", " << modelCenter.y << ", " << modelCenter.z << ")" << std::endl;

                    // Update camera to fit new model bounds (model is centered at origin)
                    modelSize = bounds.max - bounds.min;
                    modelDiagonal = glm::length(modelSize);
                    fovRadians = glm::radians(60.0f);
                    distance = (modelDiagonal * 0.5f) / glm::tan(fovRadians * 0.5f) * 1.5f;
                    cameraOffset = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)) * distance;
                    glm::vec3 newCamPos = cameraOffset;  // Position relative to origin

                    camera.setPosition(newCamPos);
                    camera.lookAt(glm::vec3(0.0f));  // Look at origin
                    camera.setFarPlane(distance * 10.0f);  // Update far plane for new model size!

                    // Update FPS controller orientation to match new camera direction
                    glm::vec3 newDirection = glm::normalize(glm::vec3(0.0f) - newCamPos);
                    float newYaw = glm::degrees(atan2(newDirection.z, newDirection.x));
                    float newPitch = glm::degrees(asin(newDirection.y));
                    cameraController.setYaw(newYaw);
                    cameraController.setPitch(newPitch);

                    // Update current index
                    currentModelIndex = selectedModelIndex;
                    loadErrorMessage.clear();

                } catch (const std::exception& e) {
                    std::cerr << "ERROR loading model: " << e.what() << std::endl;
                    loadErrorMessage = std::string("Failed to load model: ") + e.what();

                    // Revert selection if load failed
                    selectedModelIndex = currentModelIndex;
                }

                requestLoadModel = false;
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

            // Skybox (optional)
            if (hasSkybox) {
                cmd.bindGraphicsPipeline(skyboxPipeline);
                VkDescriptorSet sets[2] = { globalDescriptors.get(frameIndex), skyboxSet };
                vkCmdBindDescriptorSets(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                    skyboxPipelineLayout, 0, 2, sets, 0, nullptr);
                cmd.draw(3);
            }

            // === Three-Pass Rendering ===
            // Pass 1: Opaque materials (depth write ON)
            // Pass 2: Masked materials (depth write ON, shader alpha cutoff)
            // Pass 3: Blended materials (depth write OFF, alpha blending)

            VkDescriptorSet globalSet = globalDescriptors.get(frameIndex);

            // Apply centering transform to place model at world origin (0,0,0)
            glm::mat4 modelTransform = glm::translate(glm::mat4(1.0f), -modelCenter);

            // Pass 1: Render opaque materials
            cmd.bindGraphicsPipeline(opaquePipeline);
            vkCmdBindDescriptorSets(
                cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                modelPipelineLayout, 0, 1, &globalSet, 0, nullptr
            );
            model.drawOpaque(cmd, modelPipelineLayout, globalSet, modelTransform);

            // Pass 2: Render masked materials (alpha cutout)
            cmd.bindGraphicsPipeline(maskedPipeline);
            vkCmdBindDescriptorSets(
                cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                modelPipelineLayout, 0, 1, &globalSet, 0, nullptr
            );
            model.drawMasked(cmd, modelPipelineLayout, globalSet, modelTransform);

            // Pass 3: Render blended materials (transparent, sorted back-to-front)
            cmd.bindGraphicsPipeline(blendedPipeline);
            vkCmdBindDescriptorSets(
                cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                modelPipelineLayout, 0, 1, &globalSet, 0, nullptr
            );
            model.drawBlended(cmd, modelPipelineLayout, globalSet, camera.position(), modelTransform);

            // Render ImGui
            imgui.render(cmd);

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
