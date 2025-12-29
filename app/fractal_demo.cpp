/**
 * Fractal Demo - 3D Raymarched Fractals Explorer
 *
 * This demo showcases:
 * 1. FractalRenderSystem - 3D raymarched fractals (Mandelbulb, Julia sets, etc.)
 * 2. Real-time parameter control via ImGui
 * 3. Camera navigation through fractal spaces
 * 4. Quality/performance trade-offs
 * 5. Dynamic coloring and lighting
 *
 * Controls:
 * - WASD: Move camera
 * - Mouse: Look around
 * - Space/Ctrl: Up/Down
 * - Shift: Sprint
 * - ESC: Toggle cursor
 * - ImGui: Adjust all fractal parameters
 */

#include <hvk/ecs.hpp>
#include <hvk/core.hpp>
#include <hvk/ecs/systems/hvk_fractal_render_system.hpp>
#include <imgui.h>
#include <iostream>

using namespace hvk;

int main() {
    try {
        std::cout << "=== Holy Vulkan Engine - Fractal Explorer ===" << std::endl;
        std::cout << "3D Raymarched Fractals with Real-time Control\n" << std::endl;

        // Application Setup
        std::cout << "[1/2] Creating application..." << std::endl;

        // Configure window
        WindowCreateInfo windowCI{
            .title = "HVK Fractal Explorer - 3D Raymarched Fractals",
            .mode = WindowMode::Auto
        };

        // Configure device
        DeviceCreateInfo deviceCI{
            .debugVerbosity = DebugVerbosity::Warn
        };

        // Configure application
        ApplicationCreateInfo appCI{};
        appCI.windowCI = windowCI;
        appCI.deviceCI = deviceCI;
        appCI.createDefaultCamera = true;       // Camera at origin looking forward
        appCI.createDefaultLights = false;      // We don't need lights (fractal has its own)
        appCI.autoRegisterSystems = true;       // Need this for camera controller to work!
        appCI.enableImGui = true;
        appCI.enableCameraController = true;

        Application app(appCI);

        std::cout << "  ✓ Application created" << std::endl;

        // Keep a pointer to the fractal system for ImGui control
        FractalRenderSystem* fractalSystem = nullptr;

        // Initialize Scene
        std::cout << "\n[2/2] Registering fractal render system..." << std::endl;

        app.onInit([&fractalSystem](Application& app) {
            Scene& scene = app.scene();

            // Register the fractal render system
            auto fractalSystemPtr = std::make_unique<FractalRenderSystem>();
            fractalSystem = fractalSystemPtr.get();
            scene.addSystem(std::move(fractalSystemPtr));

            std::cout << "  ✓ FractalRenderSystem registered" << std::endl;

            // Position camera at a good starting point for Mandelbulb
            auto cameraEntity = scene.findEntity("DefaultCamera");
            if (cameraEntity != entt::null) {
                auto* transform = scene.getComponent<TransformComponent>(cameraEntity);
                if (transform) {
                    transform->position = glm::vec3(0.0f, 0.0f, 3.0f);
                }
            }

            std::cout << "\n=== Scene Summary ===" << std::endl;
            std::cout << "  Render System: FractalRenderSystem (raymarching)" << std::endl;
            std::cout << "  Camera: Free-flight controller (WASD + Mouse)" << std::endl;
            std::cout << "  Default Fractal: Mandelbulb (power 8)" << std::endl;
            std::cout << "\n=== Controls ===" << std::endl;
            std::cout << "  W/S: Fly Forward/Backward" << std::endl;
            std::cout << "  A/D: Fly Left/Right" << std::endl;
            std::cout << "  Space: Fly UP" << std::endl;
            std::cout << "  Ctrl: Fly DOWN" << std::endl;
            std::cout << "  Mouse: Look around" << std::endl;
            std::cout << "  Shift: Sprint (3x speed)" << std::endl;
            std::cout << "  ESC: Toggle cursor lock" << std::endl;
            std::cout << "\nStarting main loop...\n" << std::endl;

            // Lock cursor for FPS-style camera control
            Input::setCursorMode(GLFW_CURSOR_DISABLED);
        });

        // Update Loop
        app.onUpdate([](Application& app, float deltaTime) {
            Scene& scene = app.scene();

            // Debug: Print camera position every 60 frames
            static int frameCount = 0;
            if (frameCount++ % 60 == 0) {
                auto camEntity = scene.findEntity("DefaultCamera");
                if (camEntity != entt::null) {
                    auto* transform = scene.getComponent<TransformComponent>(camEntity);
                    if (transform) {
                        auto& pos = transform->position;
                        std::cout << "Camera pos: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                    }
                }
            }

            // Toggle cursor lock with ESC
            if (Input::wasKeyJustPressed(GLFW_KEY_ESCAPE)) {
                if (Input::isCursorDisabled()) {
                    Input::setCursorMode(GLFW_CURSOR_NORMAL);
                    std::cout << "Cursor unlocked" << std::endl;
                } else {
                    Input::setCursorMode(GLFW_CURSOR_DISABLED);
                    std::cout << "Cursor locked" << std::endl;
                }
            }

            // Debug: Check if WASD is being pressed
            if (Input::isKeyPressed(GLFW_KEY_W)) {
                std::cout << "W key pressed!" << std::endl;
            }
        });

        // ImGui - Fractal Parameter Controls
        app.onImGui([&fractalSystem](Application& /*app*/) {
            if (!fractalSystem) return;

            // Get reference to fractal parameters
            auto& params = fractalSystem->getParams();

            // Main control window
            ImGui::Begin("Fractal Explorer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("Holy Vulkan Engine - Fractal Explorer");
            ImGui::Separator();
            ImGui::Text("FPS: %.1f", Time::fps());
            ImGui::Text("Frame Time: %.3f ms", Time::averageFrameTime());
            ImGui::Separator();

            // Fractal Type Selection
            if (ImGui::CollapsingHeader("Fractal Type", ImGuiTreeNodeFlags_DefaultOpen)) {
                const char* fractalTypes[] = { "Mandelbulb", "Quaternion Julia", "Menger Sponge" };
                int currentType = static_cast<int>(params.fractalType);

                if (ImGui::Combo("Type", &currentType, fractalTypes, 3)) {
                    params.fractalType = static_cast<uint32_t>(currentType);
                }

                // Type-specific parameters
                if (currentType == 0) {
                    // Mandelbulb
                    ImGui::SliderFloat("Power", &params.power, 2.0f, 16.0f, "%.1f");
                    ImGui::SliderInt("Iterations", reinterpret_cast<int*>(&params.maxIterations), 5, 50);
                } else if (currentType == 1) {
                    // Quaternion Julia
                    ImGui::Text("Julia Constant (quaternion):");
                    ImGui::SliderFloat("C.x", &params.juliaC.x, -1.0f, 1.0f, "%.3f");
                    ImGui::SliderFloat("C.y", &params.juliaC.y, -1.0f, 1.0f, "%.3f");
                    ImGui::SliderFloat("C.z", &params.juliaC.z, -1.0f, 1.0f, "%.3f");
                    ImGui::SliderFloat("C.w", &params.juliaC.w, -1.0f, 1.0f, "%.3f");
                    ImGui::SliderInt("Iterations", reinterpret_cast<int*>(&params.maxIterations), 5, 50);
                } else if (currentType == 2) {
                    // Menger Sponge
                    ImGui::SliderFloat("Subdivision Level", &params.power, 1.0f, 6.0f, "%.0f");
                }
            }

            // Quality Settings
            if (ImGui::CollapsingHeader("Quality", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderInt("Max Steps", reinterpret_cast<int*>(&params.maxSteps), 32, 256);
                ImGui::SliderFloat("Epsilon", &params.epsilon, 0.0001f, 0.01f, "%.5f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Max Distance", &params.maxDistance, 5.0f, 50.0f, "%.1f");

                // Performance hint
                float estimatedCost = (params.maxSteps / 128.0f) * (0.01f / params.epsilon);
                ImGui::Text("Relative Cost: %.1fx", estimatedCost);
                if (estimatedCost > 2.0f) {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "High quality (may be slow)");
                } else if (estimatedCost < 0.5f) {
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Fast (lower quality)");
                }
            }

            // Lighting
            if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat3("Light Direction", &params.lightDir.x, -1.0f, 1.0f);
                params.lightDir = glm::normalize(params.lightDir);
                ImGui::ColorEdit3("Light Color", &params.lightColor.r);
                ImGui::SliderFloat("Ambient", &params.ambientStrength, 0.0f, 1.0f);
                ImGui::SliderFloat("AO Strength", &params.aoStrength, 0.0f, 1.0f);
            }

            // Coloring
            if (ImGui::CollapsingHeader("Coloring", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::ColorEdit3("Color 1", &params.color1.r);
                ImGui::ColorEdit3("Color 2", &params.color2.r);
                ImGui::SliderFloat("Color Mix", &params.colorMix, 0.0f, 1.0f);
            }

            // Psychedelic Effects
            if (ImGui::CollapsingHeader("Psychedelic Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Iteration Color Mix", &params.iterationColorMix, 0.0f, 1.0f, "%.2f");
                ImGui::Text("  0 = Solid colors, 1 = Full rainbow");

                ImGui::SliderFloat("Depth Color Shift", &params.depthColorShift, 0.0f, 10.0f, "%.2f");
                ImGui::Text("  Controls hue change with depth");

                ImGui::SliderFloat("Color Cycle Speed", &params.colorCycleSpeed, 0.0f, 2.0f, "%.2f");
                ImGui::Text("  Speed of color animation");

                ImGui::SliderFloat("Edge Glow", &params.glowIntensity, 0.0f, 2.0f, "%.2f");
                ImGui::Text("  Intensity of rim lighting");

                ImGui::Separator();
                if (ImGui::Button("TRIPPY MODE (Max Settings)", ImVec2(-1, 0))) {
                    params.iterationColorMix = 1.0f;
                    params.depthColorShift = 5.0f;
                    params.colorCycleSpeed = 1.0f;
                    params.glowIntensity = 1.5f;
                    params.enableAnimation = 1;
                }
                if (ImGui::Button("Subtle Mode (Min Settings)", ImVec2(-1, 0))) {
                    params.iterationColorMix = 0.0f;
                    params.depthColorShift = 0.0f;
                    params.colorCycleSpeed = 0.0f;
                    params.glowIntensity = 0.0f;
                }
            }

            // Animation
            if (ImGui::CollapsingHeader("Animation")) {
                bool animEnabled = params.enableAnimation != 0;
                if (ImGui::Checkbox("Enable Animation", &animEnabled)) {
                    params.enableAnimation = animEnabled ? 1 : 0;
                }
                if (animEnabled) {
                    ImGui::Text("Time: %.2f s", params.time);
                }
            }

            // Presets
            if (ImGui::CollapsingHeader("Presets")) {
                if (ImGui::Button("Classic Mandelbulb", ImVec2(-1, 0))) {
                    params.fractalType = 0;
                    params.power = 8.0f;
                    params.maxIterations = 20;
                    params.maxSteps = 128;
                    params.epsilon = 0.001f;
                }
                if (ImGui::Button("High-Power Mandelbulb", ImVec2(-1, 0))) {
                    params.fractalType = 0;
                    params.power = 12.0f;
                    params.maxIterations = 25;
                    params.maxSteps = 192;
                    params.epsilon = 0.0005f;
                }
                if (ImGui::Button("Julia Set 1", ImVec2(-1, 0))) {
                    params.fractalType = 1;
                    params.juliaC = glm::vec4(0.18f, 0.88f, 0.24f, 0.16f);
                    params.maxIterations = 20;
                    params.maxSteps = 128;
                }
                if (ImGui::Button("Julia Set 2", ImVec2(-1, 0))) {
                    params.fractalType = 1;
                    params.juliaC = glm::vec4(-0.2f, 0.6f, 0.2f, 0.2f);
                    params.maxIterations = 20;
                    params.maxSteps = 128;
                }
                if (ImGui::Button("Menger Sponge", ImVec2(-1, 0))) {
                    params.fractalType = 2;
                    params.power = 4.0f;
                    params.maxSteps = 128;
                    params.epsilon = 0.001f;
                }
                if (ImGui::Button("Reset to Default", ImVec2(-1, 0))) {
                    fractalSystem->resetParams();
                }
            }

            // Camera help
            if (ImGui::CollapsingHeader("Controls")) {
                ImGui::BulletText("W/S: Fly Forward/Backward");
                ImGui::BulletText("A/D: Fly Left/Right");
                ImGui::BulletText("SPACE: Fly UP");
                ImGui::BulletText("CTRL: Fly DOWN");
                ImGui::BulletText("SHIFT: Sprint (3x speed)");
                ImGui::BulletText("MOUSE: Free look");
                ImGui::BulletText("ESC: Toggle cursor");
                ImGui::Separator();
                ImGui::TextWrapped("Tip: Fly close to the fractal surface for detailed views!");
            }

            ImGui::End();
        });

        // Run the application
        app.run();

        std::cout << "\nFractal Explorer completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
