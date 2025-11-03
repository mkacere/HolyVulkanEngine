#!/bin/bash

# ECS headers
mv HolyVulkanEngine/include/hvk/ecs/components.hpp HolyVulkanEngine/include/hvk/ecs/hvk_components.hpp
mv HolyVulkanEngine/include/hvk/ecs/logic_system_adapter.hpp HolyVulkanEngine/include/hvk/ecs/hvk_logic_system_adapter.hpp
mv HolyVulkanEngine/include/hvk/ecs/render_components.hpp HolyVulkanEngine/include/hvk/ecs/hvk_render_components.hpp
mv HolyVulkanEngine/include/hvk/ecs/resource_manager.hpp HolyVulkanEngine/include/hvk/ecs/hvk_resource_manager.hpp
mv HolyVulkanEngine/include/hvk/ecs/scene.hpp HolyVulkanEngine/include/hvk/ecs/hvk_scene.hpp
mv HolyVulkanEngine/include/hvk/ecs/system.hpp HolyVulkanEngine/include/hvk/ecs/hvk_system.hpp
mv HolyVulkanEngine/include/hvk/ecs/world.hpp HolyVulkanEngine/include/hvk/ecs/hvk_world.hpp

# ECS systems headers
mv HolyVulkanEngine/include/hvk/ecs/systems/camera_system.hpp HolyVulkanEngine/include/hvk/ecs/systems/hvk_camera_system.hpp
mv HolyVulkanEngine/include/hvk/ecs/systems/mesh_render_system.hpp HolyVulkanEngine/include/hvk/ecs/systems/hvk_mesh_render_system.hpp
mv HolyVulkanEngine/include/hvk/ecs/systems/transform_system.hpp HolyVulkanEngine/include/hvk/ecs/systems/hvk_transform_system.hpp

# ECS sources
mv HolyVulkanEngine/src/hvk/ecs/application.cpp HolyVulkanEngine/src/hvk/ecs/hvk_application.cpp
mv HolyVulkanEngine/src/hvk/ecs/resource_manager.cpp HolyVulkanEngine/src/hvk/ecs/hvk_resource_manager.cpp
mv HolyVulkanEngine/src/hvk/ecs/scene.cpp HolyVulkanEngine/src/hvk/ecs/hvk_scene.cpp
mv HolyVulkanEngine/src/hvk/ecs/world.cpp HolyVulkanEngine/src/hvk/ecs/hvk_world.cpp

# ECS systems sources
mv HolyVulkanEngine/src/hvk/ecs/systems/camera_system.cpp HolyVulkanEngine/src/hvk/ecs/systems/hvk_camera_system.cpp
mv HolyVulkanEngine/src/hvk/ecs/systems/mesh_render_system.cpp HolyVulkanEngine/src/hvk/ecs/systems/hvk_mesh_render_system.cpp
mv HolyVulkanEngine/src/hvk/ecs/systems/transform_system.cpp HolyVulkanEngine/src/hvk/ecs/systems/hvk_transform_system.cpp

echo "All files renamed successfully!"
