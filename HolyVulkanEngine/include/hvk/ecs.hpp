/**
 * @file ecs.hpp
 * @brief ECS framework aggregate header
 * @author Holy Vulkan Engine
 * @date 2025
 * Includes all ECS component, system, and world headers.
 */

#ifndef HVK_ECS_HPP
#define HVK_ECS_HPP

/**
 * HVK ECS - Convenience header for EnTT-based ECS system
 *
 * Include this header to get access to all ECS functionality:
 * - Components (TransformComponent, MeshComponent, CameraComponent, etc.)
 * - Scene (high-level scene management)
 * - Application (easy application framework)
 * - ResourceManager (asset management)
 * - Systems (TransformSystem, MeshRenderSystem, etc.)
 *
 * Usage:
 *   #include <hvk/ecs.hpp>
 *
 *   int main() {
 *       hvk::Application app("My App", 1920, 1080);
 *
 *       app.onInit([](hvk::Application& app) {
 *           auto model = app.scene().loadModel("assets/models/foo.glb");
 *       });
 *
 *       app.run();
 *   }
 */

// Core ECS (Pure - No Vulkan dependencies)
#include <hvk/ecs/hvk_world.hpp>
#include <hvk/ecs/hvk_components.hpp>

// Rendering Bridge (Vulkan integration)
#include <hvk/ecs/hvk_render_components.hpp>
#include <hvk/ecs/hvk_resource_manager.hpp>
#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/ecs/hvk_application.hpp>

// Logic Systems (Pure ECS)
#include <hvk/ecs/systems/hvk_transform_system.hpp>
#include <hvk/ecs/systems/hvk_camera_system.hpp>
#include <hvk/ecs/systems/hvk_hierarchy_system.hpp>

// Render Systems (Vulkan bridge)
#include <hvk/ecs/systems/hvk_mesh_render_system.hpp>
#include <hvk/ecs/systems/hvk_billboard_render_system.hpp>

// EnTT registry (for advanced users)
#include <entt/entt.hpp>

#endif // HVK_ECS_HPP
