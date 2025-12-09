/**
 * @file debug_line_demo.cpp
 * @brief Debug Line Rendering System Demo
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Demonstrates the DebugLineRenderSystem capabilities:
 * - Coordinate axes
 * - Wireframe primitives (cube, sphere, capsule)
 * - Grid rendering
 * - Bounding boxes
 * - Parametric curves
 * - Dynamic line updates
 */

#include <hvk/ecs.hpp>
#include <hvk/core/hvk_time.hpp>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>

using namespace hvk;

// ============================================================================
// Demo State
// ============================================================================

struct DemoState {
    // Toggles
    bool showAxes = true;
    bool showGrid = true;
    bool showWireframeCube = true;
    bool showWireframeSphere = true;
    bool showBoundingBox = true;
    bool showSpiral = true;
    bool showNormals = false;
    bool animateRotation = true;

    // Parameters
    float gridSize = 20.0f;
    float gridSpacing = 1.0f;
    int spiralSegments = 100;
    float cubeRotationSpeed = 45.0f;  // degrees per second
    float cubeSize = 2.0f;
    int sphereSegments = 16;
    float sphereRadius = 1.5f;

    // Animation state
    float cubeRotation = 0.0f;
};

// ============================================================================
// Helper Functions
// ============================================================================

void drawAxes(DebugLineRenderSystem* debugLines, float length = 5.0f) {
    // X axis (red)
    debugLines->addLine(glm::vec3(0.0f), glm::vec3(length, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // Y axis (green)
    debugLines->addLine(glm::vec3(0.0f), glm::vec3(0.0f, length, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Z axis (blue)
    debugLines->addLine(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, length), glm::vec3(0.0f, 0.0f, 1.0f));
}

void drawGrid(DebugLineRenderSystem* debugLines, float size, float spacing, float height = 0.0f) {
    glm::vec3 gridColor(0.3f, 0.3f, 0.3f);
    int numLines = static_cast<int>(size / spacing);

    // Lines parallel to X axis
    for (int i = -numLines; i <= numLines; ++i) {
        float z = i * spacing;
        debugLines->addLine(
            glm::vec3(-size, height, z),
            glm::vec3(size, height, z),
            gridColor
        );
    }

    // Lines parallel to Z axis
    for (int i = -numLines; i <= numLines; ++i) {
        float x = i * spacing;
        debugLines->addLine(
            glm::vec3(x, height, -size),
            glm::vec3(x, height, size),
            gridColor
        );
    }
}

void drawWireframeCube(DebugLineRenderSystem* debugLines, const glm::vec3& center, float size, const glm::vec3& color, float rotation = 0.0f) {
    float halfSize = size * 0.5f;

    // Define 8 corners of the cube (local space)
    glm::vec3 corners[8] = {
        glm::vec3(-halfSize, -halfSize, -halfSize),
        glm::vec3( halfSize, -halfSize, -halfSize),
        glm::vec3( halfSize,  halfSize, -halfSize),
        glm::vec3(-halfSize,  halfSize, -halfSize),
        glm::vec3(-halfSize, -halfSize,  halfSize),
        glm::vec3( halfSize, -halfSize,  halfSize),
        glm::vec3( halfSize,  halfSize,  halfSize),
        glm::vec3(-halfSize,  halfSize,  halfSize),
    };

    // Apply rotation around Y axis
    float cosR = std::cos(glm::radians(rotation));
    float sinR = std::sin(glm::radians(rotation));

    for (int i = 0; i < 8; ++i) {
        float x = corners[i].x;
        float z = corners[i].z;
        corners[i].x = x * cosR - z * sinR;
        corners[i].z = x * sinR + z * cosR;
        corners[i] += center;
    }

    // Bottom face
    debugLines->addLine(corners[0], corners[1], color);
    debugLines->addLine(corners[1], corners[2], color);
    debugLines->addLine(corners[2], corners[3], color);
    debugLines->addLine(corners[3], corners[0], color);

    // Top face
    debugLines->addLine(corners[4], corners[5], color);
    debugLines->addLine(corners[5], corners[6], color);
    debugLines->addLine(corners[6], corners[7], color);
    debugLines->addLine(corners[7], corners[4], color);

    // Vertical edges
    debugLines->addLine(corners[0], corners[4], color);
    debugLines->addLine(corners[1], corners[5], color);
    debugLines->addLine(corners[2], corners[6], color);
    debugLines->addLine(corners[3], corners[7], color);
}

void drawWireframeSphere(DebugLineRenderSystem* debugLines, const glm::vec3& center, float radius, const glm::vec3& color, int segments = 16) {
    // Draw 3 orthogonal circles (latitude/longitude style)

    // XY circle (around Z axis)
    for (int i = 0; i < segments; ++i) {
        float angle1 = (i / static_cast<float>(segments)) * glm::two_pi<float>();
        float angle2 = ((i + 1) / static_cast<float>(segments)) * glm::two_pi<float>();

        glm::vec3 p1 = center + glm::vec3(
            radius * std::cos(angle1),
            radius * std::sin(angle1),
            0.0f
        );
        glm::vec3 p2 = center + glm::vec3(
            radius * std::cos(angle2),
            radius * std::sin(angle2),
            0.0f
        );

        debugLines->addLine(p1, p2, color);
    }

    // XZ circle (around Y axis)
    for (int i = 0; i < segments; ++i) {
        float angle1 = (i / static_cast<float>(segments)) * glm::two_pi<float>();
        float angle2 = ((i + 1) / static_cast<float>(segments)) * glm::two_pi<float>();

        glm::vec3 p1 = center + glm::vec3(
            radius * std::cos(angle1),
            0.0f,
            radius * std::sin(angle1)
        );
        glm::vec3 p2 = center + glm::vec3(
            radius * std::cos(angle2),
            0.0f,
            radius * std::sin(angle2)
        );

        debugLines->addLine(p1, p2, color);
    }

    // YZ circle (around X axis)
    for (int i = 0; i < segments; ++i) {
        float angle1 = (i / static_cast<float>(segments)) * glm::two_pi<float>();
        float angle2 = ((i + 1) / static_cast<float>(segments)) * glm::two_pi<float>();

        glm::vec3 p1 = center + glm::vec3(
            0.0f,
            radius * std::cos(angle1),
            radius * std::sin(angle1)
        );
        glm::vec3 p2 = center + glm::vec3(
            0.0f,
            radius * std::cos(angle2),
            radius * std::sin(angle2)
        );

        debugLines->addLine(p1, p2, color);
    }
}

void drawBoundingBox(DebugLineRenderSystem* debugLines, const glm::vec3& min, const glm::vec3& max, const glm::vec3& color) {
    // Define 8 corners
    glm::vec3 corners[8] = {
        glm::vec3(min.x, min.y, min.z),
        glm::vec3(max.x, min.y, min.z),
        glm::vec3(max.x, max.y, min.z),
        glm::vec3(min.x, max.y, min.z),
        glm::vec3(min.x, min.y, max.z),
        glm::vec3(max.x, min.y, max.z),
        glm::vec3(max.x, max.y, max.z),
        glm::vec3(min.x, max.y, max.z),
    };

    // Bottom face
    debugLines->addLine(corners[0], corners[1], color);
    debugLines->addLine(corners[1], corners[2], color);
    debugLines->addLine(corners[2], corners[3], color);
    debugLines->addLine(corners[3], corners[0], color);

    // Top face
    debugLines->addLine(corners[4], corners[5], color);
    debugLines->addLine(corners[5], corners[6], color);
    debugLines->addLine(corners[6], corners[7], color);
    debugLines->addLine(corners[7], corners[4], color);

    // Vertical edges
    debugLines->addLine(corners[0], corners[4], color);
    debugLines->addLine(corners[1], corners[5], color);
    debugLines->addLine(corners[2], corners[6], color);
    debugLines->addLine(corners[3], corners[7], color);
}

void drawSpiral(DebugLineRenderSystem* debugLines, const glm::vec3& center, float radius, float height, int segments, const glm::vec3& color) {
    for (int i = 0; i < segments; ++i) {
        float t1 = i / static_cast<float>(segments);
        float t2 = (i + 1) / static_cast<float>(segments);

        float angle1 = t1 * 4.0f * glm::two_pi<float>();  // 4 rotations
        float angle2 = t2 * 4.0f * glm::two_pi<float>();

        glm::vec3 p1 = center + glm::vec3(
            radius * std::cos(angle1),
            (t1 - 0.5f) * height,
            radius * std::sin(angle1)
        );
        glm::vec3 p2 = center + glm::vec3(
            radius * std::cos(angle2),
            (t2 - 0.5f) * height,
            radius * std::sin(angle2)
        );

        // Color gradient from red to yellow
        glm::vec3 lineColor = glm::mix(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), t1);

        debugLines->addLine(p1, p2, lineColor);
    }
}

// ============================================================================
// Main Application
// ============================================================================

int main() {
    try {
        // Create application
        ApplicationCreateInfo appCI{};
        appCI.windowCI.width = 1920;
        appCI.windowCI.height = 1080;
        appCI.windowCI.title = "Debug Line Rendering Demo";
        appCI.deviceCI.debugVerbosity = DebugVerbosity::Error;
        appCI.createDefaultCamera = true;       // Auto-create camera
        appCI.autoRegisterSystems = true;       // Auto-add core systems
        appCI.enableImGui = true;
        appCI.enableCameraController = true;    // Enable WASD + mouse controls

        Application app(appCI);
        DemoState state;

        // Store pointer to debug line system for later use
        DebugLineRenderSystem* debugLines = nullptr;

        // ====================================================================
        // Initialization
        // ====================================================================

        app.onInit([&](Application& app) {
            Scene& scene = app.scene();

            // Add debug line render system
            auto debugLineSystemPtr = std::make_unique<DebugLineRenderSystem>();
            debugLines = debugLineSystemPtr.get();  // Store raw pointer before moving
            scene.addSystem(std::move(debugLineSystemPtr));

            // Configure camera position
            auto cameraEntity = scene.findEntity("DefaultCamera");
            if (cameraEntity != entt::null) {
                auto* transform = scene.getComponent<TransformComponent>(cameraEntity);
                if (transform) {
                    transform->position = glm::vec3(10.0f, 8.0f, 10.0f);
                    transform->rotation = glm::quatLookAt(
                        glm::normalize(glm::vec3(0.0f) - transform->position),
                        glm::vec3(0.0f, 1.0f, 0.0f)
                    );
                }
            }

            std::cout << "Debug Line Demo: Initialized" << std::endl;
            std::cout << "Controls:" << std::endl;
            std::cout << "  WASD - Move camera" << std::endl;
            std::cout << "  Mouse - Look around" << std::endl;
            std::cout << "  Use ImGui window to toggle visualizations" << std::endl;
        });

        // ====================================================================
        // Update Loop
        // ====================================================================

        app.onUpdate([&](Application& app, float deltaTime) {
            // Check if debug line system is available
            if (!debugLines) return;

            // Update animation
            if (state.animateRotation) {
                state.cubeRotation += state.cubeRotationSpeed * deltaTime;
                if (state.cubeRotation > 360.0f) {
                    state.cubeRotation -= 360.0f;
                }
            }

            // Draw visualizations
            if (state.showAxes) {
                drawAxes(debugLines, 5.0f);
            }

            if (state.showGrid) {
                drawGrid(debugLines, state.gridSize, state.gridSpacing, 0.0f);
            }

            if (state.showWireframeCube) {
                glm::vec3 cubeCenter(-3.0f, state.cubeSize * 0.5f, 0.0f);
                drawWireframeCube(debugLines, cubeCenter, state.cubeSize, glm::vec3(1.0f, 1.0f, 0.0f), state.cubeRotation);

                // Draw bounding box around cube
                if (state.showBoundingBox) {
                    float halfSize = state.cubeSize * 0.707f;  // Diagonal size / sqrt(2)
                    glm::vec3 min = cubeCenter - glm::vec3(halfSize);
                    glm::vec3 max = cubeCenter + glm::vec3(halfSize);
                    drawBoundingBox(debugLines, min, max, glm::vec3(0.0f, 1.0f, 1.0f));
                }
            }

            if (state.showWireframeSphere) {
                glm::vec3 sphereCenter(3.0f, state.sphereRadius, 0.0f);
                drawWireframeSphere(debugLines, sphereCenter, state.sphereRadius, glm::vec3(1.0f, 0.0f, 1.0f), state.sphereSegments);
            }

            if (state.showSpiral) {
                glm::vec3 spiralCenter(0.0f, 5.0f, -5.0f);
                drawSpiral(debugLines, spiralCenter, 2.0f, 4.0f, state.spiralSegments, glm::vec3(1.0f, 0.5f, 0.0f));
            }
        });

        // ====================================================================
        // ImGui
        // ====================================================================

        app.onImGui([&](Application& app) {
            ImGui::Begin("Debug Line Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("FPS: %.1f (%.2f ms)", Time::fps(), Time::averageFrameTime() * 1000.0f);

            if (debugLines) {
                ImGui::Text("Lines Rendered: %zu", debugLines->lineCount());
            }

            ImGui::Separator();
            ImGui::Text("Visualizations");

            ImGui::Checkbox("Show Axes", &state.showAxes);
            ImGui::Checkbox("Show Grid", &state.showGrid);
            if (state.showGrid) {
                ImGui::Indent();
                ImGui::SliderFloat("Grid Size", &state.gridSize, 5.0f, 50.0f);
                ImGui::SliderFloat("Grid Spacing", &state.gridSpacing, 0.5f, 5.0f);
                ImGui::Unindent();
            }

            ImGui::Checkbox("Show Wireframe Cube", &state.showWireframeCube);
            if (state.showWireframeCube) {
                ImGui::Indent();
                ImGui::SliderFloat("Cube Size", &state.cubeSize, 0.5f, 5.0f);
                ImGui::Checkbox("Animate Rotation", &state.animateRotation);
                if (state.animateRotation) {
                    ImGui::SliderFloat("Rotation Speed", &state.cubeRotationSpeed, 0.0f, 180.0f);
                } else {
                    ImGui::SliderFloat("Rotation Angle", &state.cubeRotation, 0.0f, 360.0f);
                }
                ImGui::Unindent();
            }

            ImGui::Checkbox("Show Bounding Box", &state.showBoundingBox);

            ImGui::Checkbox("Show Wireframe Sphere", &state.showWireframeSphere);
            if (state.showWireframeSphere) {
                ImGui::Indent();
                ImGui::SliderFloat("Sphere Radius", &state.sphereRadius, 0.5f, 3.0f);
                ImGui::SliderInt("Sphere Segments", &state.sphereSegments, 8, 32);
                ImGui::Unindent();
            }

            ImGui::Checkbox("Show Spiral", &state.showSpiral);
            if (state.showSpiral) {
                ImGui::Indent();
                ImGui::SliderInt("Spiral Segments", &state.spiralSegments, 20, 200);
                ImGui::Unindent();
            }

            ImGui::Separator();
            ImGui::Text("Camera Controls");
            ImGui::BulletText("WASD - Move");
            ImGui::BulletText("Mouse - Look");

            ImGui::End();
        });

        // ====================================================================
        // Run
        // ====================================================================

        app.run();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
