# ECS COMPLETION TODO

STATUS: Ready after restart
PRIORITY: HIGH - Critical for ECS

## TASK 1: MeshRenderSystem Pipeline Creation (CRITICAL)

### Files:
1. HolyVulkanEngine/include/hvk/ecs/systems/hvk_mesh_render_system.hpp
2. HolyVulkanEngine/src/hvk/ecs/systems/hvk_mesh_render_system.cpp

### Changes Needed:
- Add 3 VkPipeline members (opaque, masked, blended)
- Add 2 VkShaderModule members (vertex, fragment)
- Add GraphicsPipelineCache pointer
- Implement full init() with shader loading and 3 pipeline creation
- Implement three-pass render() (opaque, masked, blended)
- Add cleanup() method

## TASK 2: ApplicationCreateInfo Refactor

### File:
HolyVulkanEngine/include/hvk/ecs/hvk_application.hpp

### Changes:
Replace individual params with:
- WindowCreateInfo windowCI
- DeviceCreateInfo deviceCI

Update constructor in hvk_application.cpp to use windowCI and deviceCI

## REFERENCE

See app/main.cpp lines 400-600 for complete pipeline creation example
Pattern: Create 3 pipelines (opaque/masked/blended) with proper blend states

## BUILD TEST
cmake --build build/debug --config Debug

