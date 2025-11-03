#!/bin/bash

# Function to update includes in a file
update_file() {
    local file="$1"
    
    # ECS includes
    sed -i 's|#include <hvk/ecs/application\.hpp>|#include <hvk/ecs/hvk_application.hpp>|g' "$file"
    sed -i 's|#include <hvk/ecs/components\.hpp>|#include <hvk/ecs/hvk_components.hpp>|g' "$file"
    sed -i 's|#include <hvk/ecs/logic_system_adapter\.hpp>|#include <hvk/ecs/hvk_logic_system_adapter.hpp>|g' "$file"
    sed -i 's|#include <hvk/ecs/render_components\.hpp>|#include <hvk/ecs/hvk_render_components.hpp>|g' "$file"
    sed -i 's|#include <hvk/ecs/resource_manager\.hpp>|#include <hvk/ecs/hvk_resource_manager.hpp>|g' "$file"
    sed -i 's|#include <hvk/ecs/scene\.hpp>|#include <hvk/ecs/hvk_scene.hpp>|g' "$file"
    sed -i 's|#include <hvk/ecs/system\.hpp>|#include <hvk/ecs/hvk_system.hpp>|g' "$file"
    sed -i 's|#include <hvk/ecs/world\.hpp>|#include <hvk/ecs/hvk_world.hpp>|g' "$file"
    
    # ECS systems includes
    sed -i 's|#include <hvk/ecs/systems/camera_system\.hpp>|#include <hvk/ecs/systems/hvk_camera_system.hpp>|g' "$file"
    sed -i 's|#include <hvk/ecs/systems/mesh_render_system\.hpp>|#include <hvk/ecs/systems/hvk_mesh_render_system.hpp>|g' "$file"
    sed -i 's|#include <hvk/ecs/systems/transform_system\.hpp>|#include <hvk/ecs/systems/hvk_transform_system.hpp>|g' "$file"
}

# Update all headers
find HolyVulkanEngine/include/hvk/ecs -name "*.hpp" | while read file; do
    echo "Updating: $file"
    update_file "$file"
done

# Update all sources
find HolyVulkanEngine/src/hvk/ecs -name "*.cpp" | while read file; do
    echo "Updating: $file"
    update_file "$file"
done

# Update demo files
find app -name "*.cpp" | while read file; do
    echo "Updating: $file"
    update_file "$file"
done

echo "All includes updated!"
