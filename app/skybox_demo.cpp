#include <hvk/gfx.hpp>
#include <hvk/resources.hpp>
#include <hvk/resources/hvk_cubemap.h>
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
        std::cout << "=== HVK Skybox Demo (Equirect or Cubemap) ===\n";

        // 1) Window + device
        Window window({ .width=1280, .height=720, .title="HVK Skybox Demo", .mode=WindowMode::Windowed });
        Device device(window, { .debugVerbosity = DebugVerbosity::Warn });

        // 2) Swapchain
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
        FrameSyncCreateInfo syncCI{};
        syncCI.device = device.device();
        syncCI.graphicsQueueFamilyIndex = device.graphics().family;
        syncCI.graphicsQueue = device.graphics().handle;
        syncCI.presentQueue = device.present().handle;
        syncCI.framesInFlight = (std::max)(2u, swap.imageCount());
        syncCI.preferTimelineSemaphore = true;
        FrameSync sync(syncCI);

        // 4) Staging uploader
        StagingUploader uploader({
            .device = &device,
            .queue = device.graphics().handle,
            .queueFamilyIndex = device.graphics().family,
            .framesInFlight = sync.frameCount(),
            .bytesPerFrame = 32 * 1024 * 1024,
            .debugBaseName = "upload"
        });

        // 5) Caches + descriptors
        SamplerCache samplerCache(&device, "samplers");
        PipelineLayoutCache layoutCache(&device, "plc");
        GraphicsPipelineCache pipeCache(&device, "gpc");

        DescriptorAllocator descAllocator({ .device = &device, .maxSetsPerPool = 128, .debugName = "desc_alloc" });

        DescriptorSetLayout globalLayout = GlobalDescriptorLayout::create(device);
        GlobalDescriptorSet globalDesc(device, descAllocator, globalLayout, sync.frameCount());

        // Scene + camera
        SceneData scene{}; scene.disableFog(); scene.setAmbient({0.0f,0.0f,0.0f}, 0.0f);
        LightBuffer lights; // empty is fine

        // 6) Load skybox image(s)
        bool useEquirect = false; // prefer equirect if test.png exists
        Texture equirect;
        Cubemap cubemap;

        {
            std::string eqPath = std::string(PROJECT_ROOT "/assets/skybox/test.png");
            std::ifstream test(eqPath, std::ios::binary);
            if (test.good()) {
                uploader.beginFrame(0);
                TextureLoadInfo info{};
                info.filepath = eqPath;
                info.generateMips = true;
                info.forceSRGB = true;
                info.flipY = false;
                equirect = Texture::loadFromFile(device, uploader, samplerCache, info);
                uploader.submit();
                uploader.waitCurrent();
                useEquirect = true;
                std::cout << "Loaded equirectangular sky: " << eqPath << "\n";
            } else {
                std::array<std::string,6> faces = {
                    std::string(PROJECT_ROOT "/assets/skybox/px.png"),
                    std::string(PROJECT_ROOT "/assets/skybox/nx.png"),
                    std::string(PROJECT_ROOT "/assets/skybox/py.png"),
                    std::string(PROJECT_ROOT "/assets/skybox/ny.png"),
                    std::string(PROJECT_ROOT "/assets/skybox/pz.png"),
                    std::string(PROJECT_ROOT "/assets/skybox/nz.png")
                };
                cubemap = Cubemap::loadFromFiles(device, uploader, samplerCache, faces, true, true, "skybox");
                if (cubemap) {
                    std::cout << "Loaded cubemap faces from assets/skybox/.\n";
                } else {
                    std::cout << "No skybox found. Place test.png or px/nx/py/ny/pz/nz in assets/skybox/.\n";
                }
            }
        }

        if (!useEquirect && !cubemap) {
            std::cout << "Exiting: skybox textures missing.\n";
            return 0;
        }

        // 7) Descriptor set for skybox (set=1, binding=0)
        DescriptorSetLayout skyLayout;
        VkDescriptorSet skySet = VK_NULL_HANDLE;
        {
            DescriptorSetLayoutCreateInfo lci{}; lci.device = &device;
            VkDescriptorSetLayoutBinding b{};
            b.binding = 0; b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; b.descriptorCount = 1; b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            lci.bindings.push_back(b); lci.debugName = "set1_sky";
            skyLayout = DescriptorSetLayout{ lci };
            skySet = descAllocator.allocate(skyLayout);
            DescriptorWrites writes;
            if (useEquirect) {
                writes.writeImage(skySet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, equirect.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, equirect.sampler());
            } else {
                writes.writeImage(skySet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cubemap.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cubemap.sampler());
            }
            writes.commit(device.device());
        }

        // 8) Skybox pipeline
        VkPipelineLayout skyPL = VK_NULL_HANDLE;
        VkPipeline skyPipeline = VK_NULL_HANDLE;
        {
            auto vsBytes = loadSpirv(PROJECT_ROOT "/shaders/skybox.vert.spv");
            auto fsBytes = loadSpirv(useEquirect ? PROJECT_ROOT "/shaders/skybox_equirect.frag.spv"
                                                 : PROJECT_ROOT "/shaders/skybox.frag.spv");

            ShaderStageDesc vs{}; {
                VkShaderModuleCreateInfo sci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
                sci.codeSize = vsBytes.size()*4; sci.pCode = vsBytes.data();
                VkShaderModule mod{}; VK_CHECK(vkCreateShaderModule(device.device(), &sci, nullptr, &mod));
                vs.stage = VK_SHADER_STAGE_VERTEX_BIT; vs.module = mod; vs.entry = "main";
            }
            ShaderStageDesc fs{}; {
                VkShaderModuleCreateInfo sci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
                sci.codeSize = fsBytes.size()*4; sci.pCode = fsBytes.data();
                VkShaderModule mod{}; VK_CHECK(vkCreateShaderModule(device.device(), &sci, nullptr, &mod));
                fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fs.module = mod; fs.entry = "main";
            }

            PipelineLayoutDesc pld{};
            pld.setLayouts.push_back(globalLayout.handle());
            pld.setLayouts.push_back(skyLayout.handle());
            skyPL = layoutCache.get(pld);

            VertexInputDesc vi{}; // none
            RasterState rs{}; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
            DepthStencilState dss{}; dss.depthTestEnable = VK_FALSE; dss.depthWriteEnable = VK_FALSE;
            ColorBlendState cbs{}; cbs.attachments.resize(1); cbs.attachments[0].blendEnable = VK_FALSE; cbs.attachments[0].colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            RenderFormats fmts{}; fmts.colorFormats = { swap.colorFormat() }; fmts.depthFormat = VK_FORMAT_UNDEFINED;

            GraphicsPipelineDesc gpd{};
            gpd.layout = skyPL; gpd.stages = { vs, fs }; gpd.vertexInput = vi; gpd.raster = rs; gpd.depthStencil = dss; gpd.colorBlend = cbs; gpd.formats = fmts;
            gpd.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            skyPipeline = pipeCache.get(gpd);

            // Destroy temp modules
            vkDestroyShaderModule(device.device(), vs.module, nullptr);
            vkDestroyShaderModule(device.device(), fs.module, nullptr);
        }

        // 9) Main loop
        uint64_t frameNumber = 0;
        while (!window.shouldClose()) {
            window.poll();

            auto res = sync.beginFrame();
            if (res != VK_SUCCESS) continue;
            uint32_t frameIndex = sync.currentFrameIndex();

            // Acquire next
            uint32_t imageIndex = 0; VkResult ar = sync.acquireNextImage(swap.handle(), imageIndex);
            if (ar == VK_ERROR_OUT_OF_DATE_KHR) { swap.recreate({window.framebufferSize().width, window.framebufferSize().height}); continue; }

            // Update scene + camera
            scene.time = static_cast<float>(frameNumber) / 60.0f;
            scene.deltaTime = 1.0f / 60.0f;
            scene.frameCount = static_cast<uint32_t>(frameNumber);

            float w = static_cast<float>(window.framebufferSize().width);
            float h = static_cast<float>(window.framebufferSize().height);

            CameraData cam{};
            cam.setLookAt({0,0,0}, {0,0,-1}, {0,1,0}, glm::radians(60.0f), w/h, 0.1f, 1000.0f, w, h);

            globalDesc.updateScene(frameIndex, scene); 
            globalDesc.updateCamera(frameIndex, cam);
            globalDesc.updateLights(frameIndex, lights);

            CmdList cmd(sync.cmd());

            // Transition swap image to attachment
            hvk::barrier::Batch b;
            b.imgs.push_back(hvk::barrier::make_image_barrier_full(
                swap.image(imageIndex), swap.colorFormat(),
                hvk::barrier::ImgUse::Undefined, hvk::barrier::ImgUse::ColorAttachment));
            hvk::barrier::submit(cmd.handle(), b); b.clear();

            // Begin rendering to swapchain
            VkClearColorValue clear{{0.02f,0.02f,0.03f,1.0f}};
            cmd.beginRenderingColor(
                swap.imageView(imageIndex),
                swap.extent(),
                clear,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE);
            cmd.setViewportScissor(swap.extent(), false);

            // Draw sky
            cmd.bindGraphicsPipeline(skyPipeline);
            VkDescriptorSet sets[2] = { globalDesc.get(frameIndex), skySet };
            vkCmdBindDescriptorSets(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, skyPL, 0, 2, sets, 0, nullptr);
            cmd.draw(3);

            cmd.endRendering();

            // Present barrier
            b.imgs.push_back(hvk::barrier::color_to_present(swap.image(imageIndex), swap.colorFormat()));
            hvk::barrier::submit(cmd.handle(), b);

            VkResult pr = sync.submitAndPresent(swap.handle(), imageIndex);
            if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {}

            sync.endFrame();
            frameNumber++;
        }

        device.waitIdle();
    }
    catch (const std::exception& e) {
        std::cerr << "SkyboxDemo error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
