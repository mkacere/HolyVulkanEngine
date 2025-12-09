/**
 * PhysicsDemo - Jolt Physics integration demonstration
 *
 * Features:
 * - Falling boxes with dynamic physics
 * - Static ground plane
 * - Debug wireframe visualization
 * - Interactive camera controls
 * - Real-time physics at 60Hz with interpolation
 */

#include <hvk/ecs.hpp>
#include <imgui.h>
#include <iostream>

using namespace hvk;

// Physics Demo Setup

void setupScene(Application& app) {
    Scene& scene = app.scene();

    // Create ground plane (static box)
    {
        auto ground = scene.createEntity("Ground");

        auto& transform = scene.addComponent<TransformComponent>(ground);
        transform.position = glm::vec3(0.0f, -0.5f, 0.0f);
        transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f);

        auto& rb = scene.addComponent<RigidBodyComponent>(ground);
        rb.type = RigidBodyComponent::Type::Static;
        rb.friction = 0.8f;
        rb.restitution = 0.3f;

        scene.addComponent<ColliderComponent>(ground,
            ColliderComponent::createBox(glm::vec3(20.0f, 0.5f, 20.0f))
        );
    }

    // Create falling boxes in a grid pattern
    const int gridSize = 5;
    const float spacing = 2.0f;
    const float startHeight = 10.0f;

    for (int x = 0; x < gridSize; ++x) {
        for (int z = 0; z < gridSize; ++z) {
            auto box = scene.createEntity("Box");

            float xPos = (x - gridSize / 2.0f) * spacing;
            float zPos = (z - gridSize / 2.0f) * spacing;
            float yPos = startHeight + (x + z) * 0.5f; // Stagger heights

            auto& transform = scene.addComponent<TransformComponent>(box);
            transform.position = glm::vec3(xPos, yPos, zPos);
            transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            transform.scale = glm::vec3(1.0f);

            // Vary box sizes slightly
            float size = 0.4f + (x + z) * 0.05f;

            auto& rb = scene.addComponent<RigidBodyComponent>(box);
            rb.type = RigidBodyComponent::Type::Dynamic;
            rb.mass = size * size * size * 10.0f; // Mass scales with volume
            rb.friction = 0.5f;
            rb.restitution = 0.2f;
            rb.linearDamping = 0.01f;
            rb.angularDamping = 0.05f;
            rb.useGravity = true;

            scene.addComponent<ColliderComponent>(box,
                ColliderComponent::createBox(glm::vec3(size, size, size))
            );

            // Add velocity component for querying
            scene.addComponent<VelocityComponent>(box);

            // Add previous transform for interpolation
            scene.addComponent<PreviousTransformComponent>(box);
        }
    }

    // Create a few spheres
    for (int i = 0; i < 3; ++i) {
        auto sphere = scene.createEntity("Sphere");

        float xPos = (i - 1) * 3.0f;
        float yPos = startHeight + 15.0f;

        auto& transform = scene.addComponent<TransformComponent>(sphere);
        transform.position = glm::vec3(xPos, yPos, 0.0f);
        transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f);

        auto& rb = scene.addComponent<RigidBodyComponent>(sphere);
        rb.type = RigidBodyComponent::Type::Dynamic;
        rb.mass = 5.0f;
        rb.friction = 0.3f;
        rb.restitution = 0.6f; // Bouncy spheres
        rb.useGravity = true;

        scene.addComponent<ColliderComponent>(sphere,
            ColliderComponent::createSphere(0.5f)
        );

        scene.addComponent<VelocityComponent>(sphere);
        scene.addComponent<PreviousTransformComponent>(sphere);
    }

    std::cout << "[PhysicsDemo] Scene created with " << (gridSize * gridSize + 3) << " dynamic objects" << std::endl;
}

void setupCamera(Application& app) {
    Scene& scene = app.scene();

    // Find default camera
    auto cameraEntity = scene.findEntity("DefaultCamera");
    if (cameraEntity != entt::null) {
        auto* transform = scene.getComponent<TransformComponent>(cameraEntity);
        if (transform) {
            // Position camera elevated and offset to view the falling boxes from an angle
            transform->position = glm::vec3(18.0f, 20.0f, 18.0f);

            // Look at the center where blocks will fall and settle (mid-height of the action)
            glm::vec3 lookAtPoint = glm::vec3(0.0f, 5.0f, 0.0f);
            glm::vec3 direction = glm::normalize(lookAtPoint - transform->position);
            transform->rotation = glm::quatLookAt(direction, glm::vec3(0.0f, 1.0f, 0.0f));

            // Note: Camera controller will automatically initialize from this rotation on first update
        }
    }

    std::cout << "[PhysicsDemo] Camera configured" << std::endl;
}

// Main Application

int main() {
    try {
        ApplicationCreateInfo appCI{};
        appCI.windowCI.width = 1200;
        appCI.windowCI.height = 800;
        appCI.windowCI.mode = WindowMode::Windowed;
        appCI.windowCI.title = "Physics Demo - Jolt Integration";
        appCI.deviceCI.debugVerbosity = DebugVerbosity::Error;
        appCI.createDefaultCamera = true;
        appCI.autoRegisterSystems = true;
        appCI.enableImGui = true;
        appCI.enableCameraController = true;

        Application app(appCI);

        // Store pointers to systems
        PhysicsSystem* physicsSystem = nullptr;
        DebugLineRenderSystem* debugLineSystem = nullptr;

        // Setup callback
        app.onInit([&](Application& app) {
            std::cout << "[PhysicsDemo] Initializing..." << std::endl;

            Scene& scene = app.scene();

            // Add physics system
            auto physicsSystemPtr = std::make_unique<PhysicsSystem>();
            physicsSystem = physicsSystemPtr.get();
            scene.addSystem(std::move(physicsSystemPtr));

            // Add debug line system
            auto debugLineSystemPtr = std::make_unique<DebugLineRenderSystem>();
            debugLineSystem = debugLineSystemPtr.get();
            scene.addSystem(std::move(debugLineSystemPtr));

            // Connect physics debug visualization
            if (physicsSystem && debugLineSystem) {
                physicsSystem->setDebugRenderer(debugLineSystem);
                physicsSystem->setDebugDrawEnabled(true);
                std::cout << "[PhysicsDemo] Debug visualization enabled" << std::endl;
            }

            setupCamera(app);
            setupScene(app);


            std::cout << "[PhysicsDemo] Initialization complete" << std::endl;
        });

        // Update callback - print statistics
        float statsTimer = 0.0f;
        app.onUpdate([&](Application& app, float deltaTime) {
            statsTimer += deltaTime;
            if (Input::wasKeyJustPressed(GLFW_KEY_ESCAPE)) {
                if (Input::isCursorDisabled()) {
                    Input::setCursorMode(GLFW_CURSOR_NORMAL);
                }
                else {
                    Input::setCursorMode(GLFW_CURSOR_DISABLED);
                }
            }

            if (statsTimer >= 3.0f) {
                statsTimer = 0.0f;

                // Count active entities
                auto& registry = app.scene().registry();
                auto view = registry.view<RigidBodyComponent, VelocityComponent>();

                int activeCount = 0;
                float totalKineticEnergy = 0.0f;

                for (auto [entity, rb, vel] : view.each()) {
                    //auto& vel = view.get<VelocityComponent>(entity);
                    //auto& rb = view.get<RigidBodyComponent>(entity);
                    
                    float speed = glm::length(vel.linear);
                    if (speed > 0.01f) {
                        activeCount++;
                    }

                    // KE = 0.5 * m * v^2
                    totalKineticEnergy += 0.5f * rb.mass * speed * speed;
                }


                std::cout << "[PhysicsDemo] Active: " << activeCount
                          << " | Kinetic Energy: " << totalKineticEnergy << " J" << std::endl;
            }
        });

        // ImGui callback - show physics controls
        app.onImGui([&](Application& app) {
            ImGui::Begin("Physics Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("Controls:");
            ImGui::BulletText("WASD - Move camera");
            ImGui::BulletText("Mouse - Look around");
            ImGui::BulletText("ESC - Exit");

            ImGui::Separator();

            if (physicsSystem) {
                bool debugDraw = physicsSystem->isDebugDrawEnabled();
                if (ImGui::Checkbox("Debug Visualization", &debugDraw)) {
                    physicsSystem->setDebugDrawEnabled(debugDraw);
                }
            }

            ImGui::Separator();

            // Stats
            auto& registry = app.scene().registry();
            auto dynamicView = registry.view<RigidBodyComponent>();

            int staticCount = 0;
            int dynamicCount = 0;
            int kinematicCount = 0;

            for (entt::entity entity : dynamicView) {
                auto& rb = dynamicView.get<RigidBodyComponent>(entity);
                switch (rb.type) {
                    case RigidBodyComponent::Type::Static: staticCount++; break;
                    case RigidBodyComponent::Type::Dynamic: dynamicCount++; break;
                    case RigidBodyComponent::Type::Kinematic: kinematicCount++; break;
                }
            }

            ImGui::Text("Static Bodies: %d", staticCount);
            ImGui::Text("Dynamic Bodies: %d", dynamicCount);
            ImGui::Text("Kinematic Bodies: %d", kinematicCount);
            ImGui::Text("Total: %d", staticCount + dynamicCount + kinematicCount);

            ImGui::Separator();
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

            ImGui::End();
        });

        std::cout << "[PhysicsDemo] Starting application..." << std::endl;
        app.run();
        std::cout << "[PhysicsDemo] Application terminated" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[PhysicsDemo] Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
