/**
 * ECS Demo - Comprehensive demonstration of Holy Vulkan Engine's ECS features
 *
 * This demo showcases:
 * 1. ApplicationCreateInfo pattern (batteries-included setup)
 * 2. Spawn helpers (spawnModel, spawnCamera, spawnPointLight)
 * 3. Hierarchy system (parent-child transforms)
 * 4. Auto-registered systems (Transform, Hierarchy, Camera, MeshRender)
 * 5. Scene API usage
 *
 * Controls:
 * - WASD: Move camera
 * - Mouse: Look around
 * - Space/Ctrl: Up/Down
 * - Shift: Sprint
 * - ESC: Toggle cursor
 */

#include <hvk/ecs.hpp>
#include <hvk/core.hpp>
#include <imgui.h>
#include <iostream>

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
#endif

using namespace hvk;

int main() {
    try {
        std::cout << "=== Holy Vulkan Engine - ECS Demo ===" << std::endl;
        std::cout << "Demonstrating new ECS architecture with intuitive API\n" << std::endl;

        // 1. Application Setup - Batteries-Included Approach
        std::cout << "[1/4] Creating application with default setup..." << std::endl;

        // Configure window
        WindowCreateInfo windowCI{
        //.width = 1280,
        //.height = 720,
        .title = "HVK ECS Demo - Hierarchy & Spawn API",
        .mode = WindowMode::Auto
        };

        // Configure device (use defaults)
        DeviceCreateInfo deviceCI{
            .debugVerbosity = DebugVerbosity::Warn
        };

        // Configure application
        ApplicationCreateInfo appCI{};
        appCI.windowCI = windowCI;
        appCI.deviceCI = deviceCI;
        appCI.createDefaultCamera = true;   // Auto-create camera at (0, 5, 10)
        appCI.createDefaultLights = true;   // Auto-create sun + 2 point lights
        appCI.autoRegisterSystems = true;   // Auto-add core systems
        appCI.enableImGui = true;
        appCI.enableCameraController = true;

        Application app(appCI);

        std::cout << "  ✓ Application created with auto-setup" << std::endl;
        std::cout << "  ✓ Default camera, lights, and systems registered" << std::endl;

        // 2. Spawn Entities - Simplified API
        std::cout << "\n[2/4] Spawning entities using helper API..." << std::endl;

        app.onInit([](Application& app) {
            Scene& scene = app.scene();

            // Load a central model (parent)
            std::cout << "  Loading central parent model..." << std::endl;
            auto centerModel = scene.spawnModel(
                std::string(PROJECT_ROOT) + "/assets/models/miku.glb",
                //std::string(PROJECT_ROOT) + "/assets/models/medieval_arcade.glb",
                glm::vec3(0.0f, 0.0f, 0.0f)
            );

            if (centerModel != entt::null) {
                // Rename for easier finding
                if (auto* name = scene.getComponent<NameComponent>(centerModel)) {
                    name->name = "CenterParent";
                }
                std::cout << "  ✓ Central parent model loaded" << std::endl;
            }

            // 3. Hierarchy - Parent-Child Transforms
            std::cout << "\n[3/4] Creating hierarchy (parent-child transforms)..." << std::endl;

            // Create orbiting children around central entity
            for (int i = 0; i < 4; ++i) {
                float angle = (i / 4.0f) * 2.0f * 3.14159f;
                float radius = 5.0f;
                glm::vec3 localPos(
                    radius * cos(angle),
                    0.5f,
                    radius * sin(angle)
                );

                // Spawn smaller models as children
                auto childModel = scene.spawnModel(
                    std::string(PROJECT_ROOT) + "/assets/models/miku_walking.glb",
                    localPos
                );

                if (childModel != entt::null) {
                    // Scale down the child models
                    if (auto* transform = scene.getComponent<TransformComponent>(childModel)) {
                        transform->scale = glm::vec3(0.5f);
                    }

                    // Rename for easier finding
                    if (auto* name = scene.getComponent<NameComponent>(childModel)) {
                        name->name = "Child_" + std::to_string(i);
                    }

                    std::cout << "  ✓ Child " << i << " model loaded at local pos ("
                              << localPos.x << ", " << localPos.y << ", " << localPos.z << ")" << std::endl;

                    // Set parent-child relationship
                    scene.setParent(childModel, centerModel);
                }
            }

            std::cout << "  ✓ Hierarchy created: 1 parent with 4 children" << std::endl;

            // Add additional light to illuminate the scene
            std::cout << "\n  Adding extra light for better visualization..." << std::endl;
            scene.spawnPointLight(
                glm::vec3(0.0f, 5.0f, 0.0f),   // Above the scene
                glm::vec3(1.0f, 0.9f, 0.8f),   // Warm white
                5.0f,                           // Intensity
                20.0f                           // Radius
            );

            std::cout << "\n[4/4] Initialization complete!" << std::endl;
            std::cout << "\n=== Scene Summary ===" << std::endl;
            std::cout << "  Models: Kawasaki Ninja H2 (center) + 4x HK MP7 (orbiting)" << std::endl;
            std::cout << "  Entities: 1 parent + 4 children + camera + 4 lights = 10 total" << std::endl;
            std::cout << "  Systems: Transform, Hierarchy, Camera, MeshRender (auto-registered)" << std::endl;
            std::cout << "  Hierarchy: Children orbit around rotating parent" << std::endl;
            std::cout << "\n=== Controls (FLY MODE) ===" << std::endl;
            std::cout << "  W/S: Fly Forward/Backward" << std::endl;
            std::cout << "  A/D: Fly Left/Right" << std::endl;
            std::cout << "  Space: Fly UP ⬆" << std::endl;
            std::cout << "  Ctrl: Fly DOWN ⬇" << std::endl;
            std::cout << "  Mouse: Look around (360 degrees)" << std::endl;
            std::cout << "  Shift: Sprint (3x speed)" << std::endl;
            std::cout << "  ESC: Toggle cursor lock" << std::endl;
            std::cout << "\nStarting main loop...\n" << std::endl;

            // Lock cursor for FPS-style camera control
            Input::setCursorMode(GLFW_CURSOR_DISABLED);
        });

        // 4. Update Loop - Demonstrate Hierarchy
        app.onUpdate([](Application& app, float deltaTime) {
            Scene& scene = app.scene();

            // Toggle cursor lock with ESC
            if (Input::wasKeyJustPressed(GLFW_KEY_ESCAPE)) {
                if (Input::isCursorDisabled()) {
                    Input::setCursorMode(GLFW_CURSOR_NORMAL);
                } else {
                    Input::setCursorMode(GLFW_CURSOR_DISABLED);
                }
            }

            // Find the center parent entity and rotate it
            auto centerModel = scene.findEntity("CenterParent");
            
            // Rotate parent (children will follow via hierarchy system)
            if (centerModel != entt::null) {
                auto* transform = scene.getComponent<TransformComponent>(centerModel);
                if (transform) {
                    // Rotate around Y axis
                    float rotSpeed = 0.5f;  // radians per second
                    glm::quat rotation = glm::angleAxis(rotSpeed * deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));
                    transform->rotation = rotation * transform->rotation;
                }
            }
        });

        // ImGui window for info
        app.onImGui([](Application& /*app*/) {
            ImGui::Begin("ECS Demo - Flight Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Holy Vulkan Engine - ECS Architecture");
            ImGui::Separator();
            ImGui::Text("FPS: %.1f", Time::fps());
            ImGui::Text("Frame Time: %.3f ms", Time::averageFrameTime());
            ImGui::Separator();

            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "FLY MODE ACTIVE");
            ImGui::Separator();
            ImGui::Text("Flight Controls:");
            ImGui::BulletText("W/S: Fly Forward/Backward");
            ImGui::BulletText("A/D: Fly Left/Right");
            ImGui::BulletText("SPACE: Fly UP");
            ImGui::BulletText("CTRL: Fly DOWN");
            ImGui::BulletText("SHIFT: Sprint (3x speed)");
            ImGui::BulletText("MOUSE: Free look (360)");
            ImGui::BulletText("ESC: Toggle cursor");

            ImGui::Separator();
            ImGui::TextWrapped("Features Demonstrated:");
            ImGui::BulletText("ApplicationCreateInfo pattern");
            ImGui::BulletText("Auto-registered systems");
            ImGui::BulletText("Spawn helpers (spawnModel, etc.)");
            ImGui::BulletText("Hierarchy system (parent rotates, children follow)");
            ImGui::BulletText("Free flight camera controller");
            ImGui::End();
        });

        // Run the application
        app.run();

        std::cout << "\nDemo completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
