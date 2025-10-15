# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Holy Vulkan Engine (HVK) is a Vulkan 1.4-based 3D rendering engine that provides a modern C++20 abstraction layer over Vulkan. The engine is currently undergoing a major architectural refactor toward a more extensible, cache-oriented design with dynamic rendering, improved resource management, and better separation of concerns.

## Build Commands

### Initial Setup
```bash
# Clone with submodules
git submodule update --init --recursive
```

### Configuration
```bash
# Configure using CMake presets
cmake --preset debug          # Debug build
cmake --preset release        # Release build
```

### Building
```bash
# Build debug (using CMake build preset)
cmake --build --preset build-debug
# Or directly:
cmake --build build/debug --config Debug

# Build release (using CMake build preset)
cmake --build --preset build-release
# Or directly:
cmake --build build/release --config Release
```

### Running
```bash
# Run debug build
./build/debug/bin/HVKApp.exe

# Run release build
./build/release/bin/HVKApp.exe
```

### Shader Compilation
Shaders are automatically compiled from GLSL to SPIR-V during the CMake build process. Place `.vert` and `.frag` files in the `shaders/` directory, and `glslangValidator` will compile them to `.spv` files. The build system will fail if `glslangValidator` is not found in PATH (requires Vulkan SDK).

## Architecture

### Current Refactoring State
The codebase is in transition from an old render pass-based architecture to a modern dynamic rendering architecture. Many files have `.old` suffixes (e.g., `hvk_renderer.h.old`, `hvk_camera.h.old`, `hvk_model.h.old`), and some headers in `hvk/gfx.hpp` are commented out (e.g., `hvk_renderer.h`, `hvk_buffer.h`).

**Working example:** `app/main.cpp` demonstrates the new architecture with a triangle rendering demo. The commented-out code at the bottom shows the old pattern for reference.

The new architecture focuses on:
- **Dynamic rendering** (Vulkan 1.3+) instead of render passes
- **Cache-based pipelines** with `PipelineLayoutCache` and `GraphicsPipelineCache`
- **Explicit synchronization** via `FrameSync` with timeline semaphores
- **Command list abstraction** via `CmdList` wrapper
- **RenderGraph** for declarative multi-pass rendering (optional, alternative to manual command recording)
- **Deferred deletion** for safe Vulkan resource cleanup
- **GPU profiler** integration for performance measurement
- **Staging uploader** for efficient CPU→GPU transfers

### Module Structure

**HolyVulkanEngine/** - Engine library (static library target: `HVKEngine`)
- `include/hvk/gfx/` - Graphics subsystem (Vulkan abstraction)
- `include/hvk/scene/` - Scene management (camera, game objects)
- `include/hvk/resources/` - Resource loading (GLTF models)
- `include/hvk/systems/` - Pluggable render systems

**app/** - Application executable (target: `HVKApp`)
- `main.cpp` - Entry point demonstrating engine usage

**shaders/** - GLSL shader sources
- Automatically compiled to SPIR-V during build

### Key Graphics Classes

#### Core Abstractions
- **Device** (`hvk_device.h`): Vulkan 1.4 instance/device/queue/VMA allocator wrapper. Automatically enables dynamic rendering, synchronization2, buffer device address, timeline semaphores, and other modern features when supported.
- **Window** (`hvk_window.h`): GLFW window wrapper with surface creation
- **Swapchain** (`hvk_swapchain.h`): New swapchain wrapper with image/view management
- **FrameSync** (`hvk_frame_sync.h`): Frame synchronization with timeline semaphores, acquire/present logic, and command buffer management

#### Resource Management
- **GpuBuffer** (`hvk_gpu_resources.h`): VMA-based buffer with automatic VkBuffer creation
- **GpuImage** (`hvk_gpu_resources.h`): VMA-based image with VkImage/VkImageView
- **StagingUploader** (`hvk_staging_uploader.h`): Ring-buffer staging for CPU→GPU transfers
- **DeferredDeletion** (`hvk_deferred_deletion.hpp`): Safe destruction of Vulkan objects after GPU finishes

#### Pipelines and Caching
- **PipelineLayoutCache** (`hvk_pipeline_layout_cache.h`): Deduplicates pipeline layouts based on descriptor set layouts and push constants
- **GraphicsPipelineCache** (`hvk_graphics_pipeline_cache.h`): Deduplicates graphics pipelines based on `GraphicsPipelineDesc`
- **SamplerCache** (`hvk_sampler_cache.h`): Deduplicates samplers

#### Command Recording
- **CmdList** (`hvk_cmd_list.hpp`): Thin wrapper around `VkCommandBuffer` with helper methods for binding, drawing, and dynamic rendering
- **Barriers** (`hvk_barriers.hpp`): Helper utilities for image/buffer barriers with common use cases

#### Debugging and Profiling
- **DebugUtils** (`hvk_debug_utils.h`): Vulkan debug marker integration
- **GpuProfiler** (`hvk_gpu_profiler.h`): GPU timestamp query management for performance measurement

#### Frame Graph and High-Level Rendering
- **RenderGraph** (`hvk_render_graph.h`): Declarative render graph for managing passes, resources, and automatic barrier insertion. Supports graphics and compute passes with transient/external image management. Part of the new architecture direction alongside manual `CmdList` usage.

#### Uniform Management Utilities
- **DynamicUniforms** (`hvk_dynamic_uniforms.hpp`): Helper for managing per-frame dynamic uniform buffers
- **GlobalUBO** (`hvk_global_ubo.hpp`): Common global uniform buffer structure utilities

### Old vs. New Architecture

**Old (being phased out):**
- `hvk_renderer.h.old`, `hvk_pipeline.h`, `hvk_swap_chain.h` (deleted/renamed)
- Render pass-based rendering with `VkRenderPass` and `VkFramebuffer`
- Manual synchronization with binary semaphores
- `IRenderSystem` interface expecting `VkRenderPass` in `init()`

**New (current direction):**
- Dynamic rendering (no render passes)
- Cache-oriented design for pipelines/layouts/samplers
- `FrameSync` for timeline semaphore-based synchronization
- `CmdList` for simplified command recording
- `StagingUploader` for efficient uploads
- `RenderGraph` for declarative pass-based rendering (alternative to manual `CmdList` usage)

The `app/main.cpp` shows the new pattern: create `Device`, `Swapchain`, `FrameSync`, then use `CmdList` + dynamic rendering to draw a triangle. The commented-out code at the bottom shows the old render system pattern with `HvkRenderer`, `HvkCamera`, and `ModelRenderSystem`.

### Render System Pattern (Legacy)

The old architecture used pluggable render systems via the `IRenderSystem` interface (`hvk_irender_system.hpp`):
- `init(VkRenderPass, VkExtent2D)` - Initialize resources
- `render(FrameInfo&)` - Record draw commands
- `onResize(VkRenderPass, VkExtent2D)` - Handle window resize
- `cleanup()` - Destroy resources

Systems like `ModelRenderSystem` implemented this interface. The new architecture is moving away from this pattern in favor of direct use of caches and command lists.

### Dependencies

**External libraries (in `HolyVulkanEngine/external/`):**
- **Vulkan SDK** (via `find_package(Vulkan REQUIRED)`) - Must have `glslangValidator` in PATH
- **GLFW** - Windowing (`external/glfw/`)
- **GLM** - Math library (header-only, `external/glm/`)
- **tinygltf** - GLTF 2.0 loader (header-only, `external/tinygltf/`)
- **stb** - Image loading (header-only, `external/stb/`)
- **VulkanMemoryAllocator (VMA)** - Memory management (header-only, `external/VulkanMemoryAllocator/`)
- **SPIRV-Reflect** - SPIR-V reflection for descriptor layouts (header-only, `external/SPIRV-Reflect/`)

### Precompiled Headers

The engine uses a PCH (`HolyVulkanEngine/src/pch.h`) to speed up compilation. It includes common Vulkan headers, STL containers, and GLM.

## Development Practices

### Vulkan 1.4 Features
The `Device` class is configured to request Vulkan 1.4 and enable modern features like:
- Dynamic rendering (no render passes)
- Synchronization2 (pipeline barriers with `VkDependencyInfo`)
- Buffer device address
- Timeline semaphores
- Descriptor indexing
- Maintenance 5/6 extensions

All feature requests are gracefully disabled if unsupported by the physical device.

### Naming Conventions
- Classes: `HvkClassName` or `ClassName` (namespace `hvk`)
- Files: `hvk_snake_case.h` / `.cpp`
- Vulkan handles often suffixed with underscore (private members)

### Error Handling
Use `VK_CHECK(expr)` macro for Vulkan calls. It throws `std::runtime_error` on failure.

### File Extensions
- `.h` for C-compatible or old headers
- `.hpp` for C++-only headers (new style)
- `.cpp` for implementation
- `.old` suffix indicates deprecated/refactored files

## Common Patterns

### Creating a Basic Render Loop
See `app/main.cpp` for the complete working example. The pattern is:

1. **Setup** - Create `Window`, `Device`, `Swapchain`, `FrameSync`
2. **Resources** - Create GPU buffers/images using `GpuBuffer`/`GpuImage`, upload data via `StagingUploader`
3. **Pipelines** - Use `PipelineLayoutCache` and `GraphicsPipelineCache` to create layouts and pipelines
4. **Main loop:**
   - `window.poll()` → check for events
   - Handle resize: `window.wasResized()` → `swap.recreateForWindow()` → update pipelines if format changed
   - `sync.beginFrame()` → begin frame synchronization
   - `sync.acquireNextImage(swap.handle(), imageIndex)` → acquire swapchain image
   - Get command buffer: `CmdList cmd(sync.cmd())`
   - Transition swapchain image to color attachment (use `hvk::barrier`)
   - `cmd.beginRenderingColor()` → setup dynamic rendering
   - `cmd.setViewportScissor()` → set viewport
   - `cmd.bindGraphicsPipeline()` / `cmd.bindVertexBuffer()` / `cmd.draw()` → draw calls
   - `cmd.endRendering()` → end rendering
   - Transition to present (use `hvk::barrier::color_to_present`)
   - `sync.submitAndPresent(swap.handle(), imageIndex)` → submit and present
   - `sync.endFrame()` → end frame

**Note:** Shader paths in `main.cpp` use relative paths (`"../../../../shaders/..."`) which assumes running from the build directory. Adjust paths based on your working directory.

### Using RenderGraph (Alternative Pattern)
For more complex rendering with multiple passes, use `RenderGraph` instead of manual `CmdList`:

1. Create RenderGraph: `RenderGraph graph({.device = &device, .framesInFlight = 3})`
2. Declare images: `auto img = graph.createImage({...})` or `graph.importExternalImage({...})`
3. Add passes with setup and record lambdas:
   ```cpp
   graph.addGraphicsPass("my_pass",
       [&](PassBuilder& pb) { pb.writeColor({.img = img, .load = VK_ATTACHMENT_LOAD_OP_CLEAR}); },
       [&](CmdList& cmd, const PassContext& ctx) { /* record draw commands */ });
   ```
4. Before each frame: `graph.beginFrame(frameIndex, swapExtent)`
5. Compile and execute: `graph.compile()` → `graph.execute(cmd)`

RenderGraph automatically inserts barriers and manages transient resources. Useful for multi-pass effects (shadow maps, post-processing, etc.).

### Loading GLTF Models
Use `tinygltf` directly or reference the old `HvkModel` class (`hvk_model.cpp.old`). The new architecture does not yet have a finalized model loading abstraction. The `ModelRenderSystem` class (`hvk_model_render_system.h`) provides a legacy implementation.

### Shader Reflection
`hvk_shader_reflect.h` provides utilities for parsing SPIR-V to extract descriptor set layouts and push constant ranges (used by pipeline caches).

## Important Notes

- The codebase is mid-refactor. Many systems are not yet ported to the new architecture.
- The old `HvkRenderer` and render system pattern are being phased out.
- Window resizing requires explicit swapchain recreation (`Swapchain::recreateForWindow()`).
- The engine targets Windows (GLFW + Win32 surface) but can be adapted for other platforms.
- C++20 is required (designated initializers, concepts, ranges).
