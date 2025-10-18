# Holy Vulkan Engine

A modern Vulkan-based 3D rendering engine built with C++20, designed to simplify graphics programming while maintaining flexibility and performance.

> "I am working on something holy. You are a bunch of squirrels." — In memory of Terry A. Davis

## Overview

Holy Vulkan Engine (HVK) provides a clean, modular abstraction layer over Vulkan, making 3D graphics programming more accessible without sacrificing control. The engine features automatic resource management, GLTF model loading with PBR materials, a pluggable render system architecture, and modern Vulkan 1.3 dynamic rendering.

![HVK Miku Demo](.images/miku_demo.png)
*GLTF model rendering with PBR materials, multi-light setup, and MSAA*

![HVK Grid Imgui Demo](.images/grid_demo.png)
*Infinite grid visualization with Dear ImGui integration*

![HVK Triangle](.images/triangle.png)
*Getting started: the classic triangle*

## Features

### Rendering
- **Modern Vulkan API**: Built on Vulkan 1.3 with dynamic rendering (no legacy render passes)
- **MSAA Support**: Runtime-configurable multisample anti-aliasing (2x/4x/8x)
- **PBR Materials**: Physically-based rendering with metallic-roughness workflow
- **GLTF 2.0 Loading**: Full scene import with meshes, materials, textures, and transforms
- **Automatic Shader Compilation**: GLSL to SPIR-V conversion during build
- **SPIR-V Reflection**: Automatic descriptor set layout generation from shaders

### Architecture
- **Pluggable Render Systems**: Modular rendering via the `IRenderSystem` interface
- **Smart Resource Management**: Automatic memory allocation via Vulkan Memory Allocator (VMA)
- **Deferred Deletion**: Safe resource cleanup with frame-in-flight tracking
- **Descriptor Caching**: Pipeline, layout, and sampler caches to minimize overhead
- **Staging Buffers**: Efficient CPU→GPU transfers with per-frame staging

### Scene & Camera
- **Flexible Camera System**: Perspective/orthographic cameras with FPS controller
- **Transform Hierarchy**: Scene graph with position, rotation, scale
- **Multi-Light Support**: Point, directional, and spot lights with per-scene light buffer

### Developer Experience
- **Dear ImGui Integration**: Built-in UI framework with docking and multi-viewport support
- **Debug Utilities**: Vulkan validation layers, debug labels, and GPU profiling
- **Hot-Reload Ready**: Modular architecture supports runtime system swapping
- **CMake Presets**: One-command configuration and build

## Prerequisites

- **Compiler**: C++20 compatible (MSVC 2019+, GCC 10+, or Clang 10+)
- **Vulkan SDK**: LunarG SDK 1.3+ with `glslangValidator` in PATH
- **CMake**: Version 3.16 or higher
- **Git**: For submodule initialization

## Quick Start

### 1. Clone with Submodules

```bash
git clone <repository-url>
cd HolyVulkanEngineWorkSpace
git submodule update --init --recursive
```

### 2. Configure and Build

```bash
# Configure (run once, or after CMake changes)
cmake --preset debug

# Build
cmake --build build/debug --config Debug
```

For release builds, use `cmake --preset release` and `--config Release`.

### 3. Run a Demo

Three demo applications are included:

```bash
# Main demo: GLTF model viewer with PBR materials
./build/debug/bin/Debug/HVKApp.exe

# Primitives demo: Basic shapes (cubes, spheres, etc.)
./build/debug/bin/Debug/PrimitivesDemo.exe

# Grid demo: Infinite grid visualization
./build/debug/bin/Debug/GridDemo.exe
```

### 4. Add Your Own Models

Place `.glb` or `.gltf` files in `assets/models/` and update the model path in `app/main.cpp`:

```cpp
const char* modelPath = PROJECT_ROOT "/assets/models/your_model.glb";
```

## Controls

| Input | Action |
|-------|--------|
| **WASD** | Move camera forward/left/backward/right |
| **Mouse** | Look around (when cursor is locked) |
| **Space** | Move up |
| **Ctrl** | Move down |
| **Shift** | Sprint (faster movement) |
| **ESC** | Toggle cursor lock (enables/disables ImGui interaction) |

## Project Structure

```
HolyVulkanEngineWorkSpace/
├── HolyVulkanEngine/          # Engine library (static)
│   ├── include/hvk/           # Public API headers
│   │   ├── core/              # Input, Time systems
│   │   ├── gfx/               # Vulkan abstraction (Device, Swapchain, Pipelines, etc.)
│   │   ├── scene/             # Camera, Transform hierarchy
│   │   ├── resources/         # Model, Material, GLTF loading
│   │   ├── systems/           # Render system interfaces
│   │   └── ui/                # ImGui integration
│   ├── src/hvk/               # Implementation files
│   └── external/              # Third-party libraries (GLFW, GLM, tinygltf, VMA, ImGui, etc.)
├── app/                       # Demo applications
│   ├── main.cpp               # GLTF model viewer demo
│   ├── primitives_demo.cpp    # Primitive shapes demo
│   └── grid_demo.cpp          # Grid visualization demo
├── shaders/                   # GLSL shader sources (auto-compiled to .spv)
└── assets/                    # Models, textures, etc.
```

## Architecture Overview

### Core Subsystems

#### `hvk::gfx` - Graphics & Rendering
- **Device**: Vulkan device, queues, and physical device properties
- **Window**: GLFW window with Vulkan surface
- **Swapchain**: Image acquisition and presentation with automatic resize handling
- **FrameSync**: Frame-in-flight synchronization (supports timeline semaphores)
- **CmdList**: Command buffer wrapper with convenience methods
- **GpuResources**: Unified buffer/image management via VMA
- **StagingUploader**: Efficient CPU→GPU data streaming
- **PipelineLayoutCache**: Deduplicates pipeline layouts
- **GraphicsPipelineCache**: Deduplicates graphics pipelines
- **DescriptorAllocator**: Pool-based descriptor set allocation

#### `hvk::scene` - Scene Management
- **Camera**: Perspective/orthographic cameras with Vulkan-style projection
- **CameraController**: FPS-style movement and mouse look
- **Transform**: Hierarchical 3D transformations

#### `hvk::resources` - Asset Loading
- **GltfLoader**: GLTF 2.0 file loading with PBR materials
- **Model**: Mesh, material, and texture container
- **Material**: PBR material with descriptor set management
- **Vertex**: Standard vertex format (position, normal, tangent, UV, color)

#### `hvk::systems` - Pluggable Rendering
- **IRenderSystem**: Abstract interface for render systems
- **ModelRenderSystem**: GLTF model rendering with PBR shading

#### `hvk::ui` - User Interface
- **ImGuiLayer**: Dear ImGui integration with docking and multi-viewport

### Render System Pattern

Rendering is implemented as pluggable systems inheriting from `IRenderSystem`:

```cpp
class MyRenderSystem : public hvk::IRenderSystem {
public:
    void init(VkRenderPass renderPass, VkExtent2D extent) override {
        // Create pipelines, allocate descriptors
    }

    void render(const hvk::FrameInfo& frameInfo) override {
        // Record draw commands
    }

    void onResize(VkRenderPass renderPass, VkExtent2D extent) override {
        // Recreate extent-dependent resources
    }

    void cleanup() override {
        // Destroy Vulkan objects
    }
};
```

This modular design allows mixing different rendering techniques (models, primitives, post-processing, etc.) in a single application.

### Descriptor Set Conventions

- **Set 0**: Global descriptors (Scene, Camera, Lights) - bound once per frame
- **Set 1**: Material descriptors (textures, material params) - bound per material
- **Push Constants**: Per-draw data (model matrix, normal matrix, material overrides)

## Usage Example

Here's a minimal example demonstrating the modern API:

```cpp
#include <hvk/gfx.hpp>
#include <hvk/scene.hpp>
#include <hvk/resources.hpp>
#include <hvk/core.hpp>

using namespace hvk;

int main() {
    // 1. Create window and device
    Window window({ .width = 1280, .height = 720, .title = "HVK Demo" });
    Device device(window, { .debugVerbosity = DebugVerbosity::Warn });

    // 2. Create swapchain
    Swapchain swap({
        .device = &device,
        .surface = device.surface(),
        .desiredExtent = { window.framebufferSize().width, window.framebufferSize().height },
        .preferMailbox = true
    });

    // 3. Setup frame synchronization
    FrameSync sync({
        .device = device.device(),
        .graphicsQueueFamilyIndex = device.graphics().family,
        .graphicsQueue = device.graphics().handle,
        .presentQueue = device.present().handle,
        .framesInFlight = 2
    });

    // 4. Initialize core systems
    Input::init(window.glfwHandle());
    Time::init();

    // 5. Load a model
    StagingUploader uploader({ .device = &device, /* ... */ });
    DescriptorAllocator descAllocator({ .device = &device, /* ... */ });

    Model model = GltfLoader::loadFromFile(
        device, uploader, descAllocator,
        materialLayout, samplerCache,
        "assets/models/my_model.glb",
        { .generateMipmaps = true }
    );

    // 6. Setup camera
    Camera camera = Camera::createPerspective(
        glm::vec3(0, 2, 5),  // position
        glm::vec3(0, 0, 0),  // look-at
        60.0f,               // FOV
        16.0f / 9.0f         // aspect
    );

    CameraController controller;
    controller.setMode(CameraController::Mode::FPS);

    // 7. Main loop
    while (!window.shouldClose()) {
        window.poll();
        Input::update();
        Time::update();

        controller.update(camera);

        // Render frame...
    }

    device.waitIdle();
    return 0;
}
```

See `app/main.cpp` for a complete, production-ready example with ImGui, MSAA, lighting, and resize handling.

## Shader Development

### Automatic Compilation

Shaders are compiled automatically during the build:

1. Place `.vert` and `.frag` files in `shaders/`
2. Use GLSL 450 syntax
3. Run CMake build - `glslangValidator` produces `.spv` files
4. Load in code: `loadSpirv(PROJECT_ROOT "/shaders/myshader.vert.spv")`

### Descriptor Set Generation

Use `ShaderReflect` to automatically generate descriptor set layouts from SPIR-V:

```cpp
auto layout = ShaderReflect::createDescriptorSetLayout(device, spirvCode);
```

Alternatively, create layouts manually for full control.

## Adding Custom Render Systems

1. Create a new class inheriting from `hvk::IRenderSystem`
2. Implement `init()`, `render()`, `onResize()`, and `cleanup()`
3. Create your pipeline and descriptors in `init()`
4. Record draw commands in `render()`
5. Handle resize in `onResize()` (recreate extent-dependent resources)
6. Destroy resources in `cleanup()`
7. Add the header to `HolyVulkanEngine/include/hvk/systems.hpp`
8. Instantiate and use in your application

## Dependencies

All dependencies are included as submodules in `HolyVulkanEngine/external/`:

- **GLFW** - Window and input management
- **GLM** - Mathematics library
- **tinygltf** - GLTF 2.0 loader
- **stb** - Image loading (stb_image.h)
- **Vulkan Memory Allocator (VMA)** - GPU memory management
- **SPIRV-Reflect** - SPIR-V shader reflection
- **Dear ImGui** - Immediate mode GUI

## Contributing

Contributions are welcome! The engine follows these conventions:

- **Namespace**: All engine code lives in `namespace hvk`
- **Naming**: PascalCase for types, camelCase for functions/variables
- **Headers**: Public API in `include/hvk/`, implementation in `src/hvk/`
- **Error Handling**: Use `VK_CHECK()` macro for Vulkan calls
- **Resource Naming**: Provide `debugName` for all Vulkan objects (RenderDoc/validation layer support)

## License

See LICENSE file for details.

---

*Built with modern C++20 and Vulkan 1.3. Dedicated to making graphics programming more accessible.*
