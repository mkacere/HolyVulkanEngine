# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Holy Vulkan Engine (HVK) is a Vulkan-based 3D rendering engine written in modern C++20. It provides a simplified abstraction layer over Vulkan for graphics programming, featuring GLTF model loading, pluggable render systems, and a modular architecture.

## Build System

### Prerequisites
- C++20 compatible compiler (MSVC 2019+, GCC 10+, or Clang 10+)
- Vulkan SDK with `glslangValidator` in PATH
- CMake 3.16+
- Git for submodule management

### Common Build Commands

```bash
# Configure for debug (first time or after CMakeLists.txt changes)
cmake --preset debug

# Configure for release
cmake --preset release

# Build debug
cmake --build build/debug --config Debug

# Build release
cmake --build build/release --config Release
```

### Running Demos

Three demo executables are built in `build/<preset>/bin/`:

```bash
# Main GLTF model demo (loads models from assets/)
./build/debug/bin/Debug/HVKApp.exe

# Primitive shapes demo (cubes, spheres, etc.)
./build/debug/bin/Debug/PrimitivesDemo.exe

# Infinite grid visualization demo
./build/debug/bin/Debug/GridDemo.exe
```

### Shader Compilation

Shaders are automatically compiled from GLSL to SPIR-V during CMake build:
- Place `.vert` and `.frag` files in `shaders/` directory
- CMake invokes `glslangValidator -V` to produce `.spv` files
- Load compiled shaders using the `.spv` file path in code
- Shader compilation is a build dependency of all executables

## Architecture

### Module Structure

The engine is organized into these subsystems under `HolyVulkanEngine/`:

#### `hvk/core/` - Core Systems
- **Input**: Centralized input handling (keyboard, mouse) via GLFW
- **Time**: Frame timing, delta time, FPS tracking

#### `hvk/gfx/` - Graphics & Rendering
Core Vulkan abstraction:
- **Device**: Vulkan instance, physical device, logical device, queue management
- **Window**: GLFW window wrapper with Vulkan surface creation
- **Swapchain**: Swapchain, image views, automatic recreation on resize
- **FrameSync**: Frame-in-flight synchronization using semaphores/fences (supports timeline semaphores)
- **CmdList**: Command buffer wrapper with convenience methods

Resource management:
- **GpuResources**: Unified GPU buffer and image management using VMA (Vulkan Memory Allocator)
- **StagingUploader**: Efficient CPU→GPU data transfer with per-frame staging buffers
- **DeferredDeletion**: Safe deferred destruction of Vulkan resources after frames in flight

Descriptor & Pipeline management:
- **DescriptorAllocator**: Pool-based descriptor set allocation
- **DescriptorSetLayout**: Descriptor set layout creation
- **PipelineLayoutCache**: Caches pipeline layouts to avoid duplicates
- **GraphicsPipelineCache**: Caches graphics pipelines (uses dynamic rendering, not render passes)
- **SamplerCache**: Caches texture samplers
- **ShaderReflect**: SPIR-V reflection using SPIRV-Reflect to auto-generate descriptor layouts

Uniform data (tiered system):
- **SceneData**: Per-frame scene globals (ambient color, fog, time, etc.)
- **CameraData**: Per-frame camera matrices (view, projection, view-projection)
- **LightData**: Per-frame light buffer (point, directional, spot lights)
- **GlobalDescriptorLayout/GlobalDescriptorSet**: Set 0 binding (Scene + Camera + Lights)
- **DynamicUniforms**: Dynamic uniform buffer offsets for per-object data

Utilities:
- **Barriers**: Image/buffer barrier helpers for layout transitions
- **DebugUtils**: Vulkan debug labels and markers
- **GpuProfiler**: GPU timestamp queries for performance profiling

#### `hvk/scene/` - Scene Management
- **Camera**: Perspective/orthographic camera with projection matrices (Vulkan-style Y-flip)
- **CameraController**: FPS-style camera controls (WASD movement, mouse look)
- **Transform**: 3D transform hierarchy (position, rotation, scale)

#### `hvk/resources/` - Resource Loading
- **Model**: GLTF 2.0 model representation (meshes, materials, textures, nodes)
- **GltfLoader**: Loads GLTF/GLB files via tinygltf
  - Supports PBR materials (base color, metallic-roughness, normal, emissive)
  - Automatic mipmap generation
  - Texture compression support
- **Material**: PBR material data with descriptor set management
- **Vertex**: Standard vertex format (position, normal, tangent, UV, color)

#### `hvk/systems/` - Render Systems
- **IRenderSystem**: Abstract interface for pluggable rendering systems
  - `init(renderPass, extent)`: Initialize resources after swapchain creation
  - `render(FrameInfo)`: Record draw commands
  - `onResize(renderPass, extent)`: Handle swapchain recreation
  - `cleanup()`: Destroy Vulkan resources
- **ModelRenderSystem**: GLTF model rendering implementation (PBR shading)

#### `hvk/ui/` - UI Layer
- **ImGuiLayer**: Dear ImGui integration with Vulkan backend
  - Multi-viewport support
  - Docking support
  - MSAA support

### Key Design Patterns

#### Render System Pattern
Rendering functionality is implemented as pluggable systems inheriting from `IRenderSystem`. Each system manages its own pipelines, descriptors, and draw logic. This allows mixing different rendering techniques (models, primitives, grids, etc.) in the same frame.

#### Frame Context
`FrameContext` (replacing older `FrameInfo`) provides lightweight frame-level data to render systems:
- Frame index, timing, command buffer
- Viewport/scissor/extent
- Global descriptor set (Set 0: Scene + Camera + Lights)
Render systems query ECS or other data sources directly rather than passing heavy data through the context.

#### Descriptor Set Binding Convention
- **Set 0**: Global descriptors (Scene, Camera, Lights) - bound once per frame
- **Set 1**: Material descriptors (textures, material params) - bound per material
- **Push Constants**: Per-draw data (model matrix, normal matrix, material params)

#### Dynamic Rendering
The engine uses Vulkan 1.3 dynamic rendering (VK_KHR_dynamic_rendering) instead of traditional render passes. Attachments are specified at `vkCmdBeginRendering` time.

#### MSAA Support
MSAA is supported via multisampled color/depth attachments with resolve to swapchain. Sample count is queried from device limits at runtime.

#### Vulkan Coordinate System
- Projection matrices use Vulkan-style coordinates (Y-down, Z: 0 to 1)
- Viewports are not flipped (negative height) - the projection matrix handles Y-flip
- Front face winding is **clockwise** (due to Y-flip in projection)

### Dependencies (External Libraries)

Located in `HolyVulkanEngine/external/`:
- **GLFW**: Window/input management
- **GLM**: Math library (vectors, matrices, quaternions)
- **tinygltf**: GLTF 2.0 loader
- **stb**: Image loading (stb_image.h)
- **VulkanMemoryAllocator (VMA)**: GPU memory allocation
- **SPIRV-Reflect**: SPIR-V shader reflection
- **Dear ImGui**: Immediate mode GUI

## Common Development Workflows

### Adding a New Render System

1. Create a new header/source pair in `HolyVulkanEngine/src/hvk/systems/`
2. Inherit from `IRenderSystem` and implement all virtual methods
3. In `init()`: Create pipeline layout, graphics pipeline, allocate descriptors
4. In `render(FrameContext&)`: Bind pipeline, descriptors, push constants, issue draw calls
5. In `onResize()`: Recreate extent-dependent resources (if any)
6. In `cleanup()`: Destroy all Vulkan objects
7. Add the header to `HolyVulkanEngine/include/hvk/systems.hpp`
8. Register the system in a demo app (e.g., `app/main.cpp`)

### Adding New Shaders

1. Create `.vert` and/or `.frag` files in `shaders/`
2. Use GLSL 450 syntax
3. Rebuild project - CMake will auto-compile to `.spv`
4. Load in code: `loadSpirv(PROJECT_ROOT "/shaders/myshader.vert.spv")`
5. For descriptor set layouts: either use `ShaderReflect` for auto-generation or create manually

### Loading GLTF Models

1. Place `.glb` or `.gltf` files in `assets/models/`
2. Use `GltfLoader::loadFromFile()` with appropriate options:
   - `generateMipmaps`: Enable for better texture quality
   - `loadMaterials/loadTextures`: Enable for full PBR rendering
   - `flipTextureY`: Usually false for Vulkan (depends on asset)
3. The loader handles vertex/index buffer uploads via `StagingUploader`
4. Call `model.draw(cmd, pipelineLayout, globalDescSet, modelTransform)` in render loop

### Handling Window Resize

1. Detect resize: `window.wasResized()` returns true
2. Wait for device idle: `device.waitIdle()`
3. Recreate swapchain: `swap.recreateForWindow(window)`
4. Recreate extent-dependent resources (depth buffers, MSAA targets)
5. Update camera aspect ratio: `camera.updateAspectRatio(width, height)`
6. Notify render systems: `renderSystem.onResize(renderPass, newExtent)`
7. Clear resize flag: `window.clearResizedFlag()`

### Using the Global Descriptor System

1. Create global descriptor set layout: `GlobalDescriptorLayout::create(device)`
2. Initialize per-frame descriptor sets: `globalDescriptors.init(device, allocator, layout, frameCount)`
3. Each frame, update uniform data:
   ```cpp
   globalDescriptors.updateScene(frameIndex, sceneData);
   globalDescriptors.updateCamera(frameIndex, camera.toCameraData(width, height));
   globalDescriptors.updateLights(frameIndex, lightBuffer);
   ```
4. Bind Set 0 in render loop: `vkCmdBindDescriptorSets(..., 0, 1, &globalSet, ...)`

### Adding ImGui Windows

1. After `imgui.newFrame()`, use standard Dear ImGui calls:
   ```cpp
   ImGui::Begin("My Window");
   ImGui::Text("Hello: %d", value);
   ImGui::End();
   ```
2. ImGui rendering happens automatically in `imgui.render(cmd)` during command recording

## Important Notes

### Precompiled Headers
The engine uses a precompiled header at `HolyVulkanEngine/src/pch.h` containing common Vulkan/STL includes. Modify this to improve compile times when adding frequently-used headers.

### Error Handling
- Use `VK_CHECK(result)` macro to validate Vulkan calls - throws on error
- Validation layers are enabled in Debug builds (controlled by `DebugVerbosity`)

### Coordinate Conventions
- World space: +Y up, +X right, +Z forward (right-handed)
- Camera view space: +Y up, +X right, -Z forward (right-handed, looking down -Z)
- NDC: X/Y: -1 to +1, Z: 0 (near) to 1 (far), Y-down (Vulkan-style)

### Resource Naming
All Vulkan objects support debug names via `debugName` or `debugBaseName` parameters. Use these for RenderDoc/Validation Layer debugging.

### Platform Support
Currently targets Windows with Visual Studio 2022. CMake presets use `Visual Studio 17 2022` generator. Linux/macOS support would require generator adjustments and platform-specific window/input handling.
