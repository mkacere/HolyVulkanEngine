# Holy Vulkan Engine

A simple Vulkan abstraction layer to help beginners get started with graphics programming using Vulkan.

> "I am working on something holy. You are a bunch of squirrels." — In memory of Terry A. Davis

## Overview

Holy Vulkan Engine (HVK) is a Vulkan-based 3D rendering engine that provides a simplified abstraction layer over Vulkan for graphics programming. Built with modern C++20, the engine follows a modular architecture with distinct subsystems for graphics, scene management, resource loading, and pluggable render systems.

## Features

- **Vulkan Abstraction**: Simplified wrappers for Vulkan device, swapchain, pipelines, and buffers
- **GLTF Model Loading**: Full support for GLTF 2.0 models with PBR textures via tinygltf
- **Pluggable Render Systems**: Extensible architecture for adding custom rendering systems
- **Scene Management**: Game object system with transform hierarchies
- **Camera System**: First-person camera with mouse look and WASD movement
- **Shader Management**: Automatic GLSL to SPIR-V compilation during build
- **Descriptor Management**: Simplified descriptor set creation and binding

![HVK Miku Demo](.images/miku_demo.png)

## Prerequisites

- C++20 compatible compiler (MSVC 2019+, GCC 10+, or Clang 10+)
- Vulkan SDK (LunarG SDK or equivalent) with `glslangValidator`
- CMake 3.16 or higher
- Git (for submodule management)

## Building

### Clone the Repository

```bash
git clone <repository-url>
cd HolyVulkanEngineWorkSpace
git submodule update --init --recursive
```

### Configure and Build

The project uses CMake presets for easy configuration:

```bash
# Configure for debug
cmake --preset debug

# Configure for release
cmake --preset release

# Build
cmake --build build/debug --config Debug
cmake --build build/release --config Release
```

### Run the Application

```bash
# Debug build
./build/debug/bin/HVKApp.exe

# Release build
./build/release/bin/HVKApp.exe
```

## Project Structure

```
HolyVulkanEngine/          # Engine library
├── include/hvk/           # Public headers
│   ├── gfx/              # Graphics/rendering subsystem
│   ├── scene/            # Scene management
│   ├── resources/        # Resource loading
│   └── systems/          # Render systems
└── src/hvk/              # Implementation files

app/                       # Application executable
└── main.cpp              # Entry point

shaders/                   # GLSL shader sources
└── *.vert, *.frag        # Vertex and fragment shaders
```

## Architecture

### Core Subsystems

#### Graphics (gfx/)
- **HvkDevice**: Vulkan instance, device, and queue management
- **HvkWindow**: GLFW window wrapper with surface creation
- **HvkSwapChain**: Swapchain, render pass, and framebuffer management
- **HvkRenderer**: Main rendering coordinator and render loop
- **HvkPipeline**: Graphics pipeline creation and configuration
- **HvkBuffer**: Vertex, index, and uniform buffer management
- **HvkDescriptors**: Descriptor set layouts and pools

#### Scene (scene/)
- **HvkGameObject**: Entity-component system for scene objects
- **HvkCamera**: First-person camera with input handling

#### Resources (resources/)
- **HvkModel**: GLTF model loader with texture and material support

#### Systems (systems/)
- **IRenderSystem**: Abstract interface for render systems
- **ModelRenderSystem**: GLTF model rendering implementation

### Render System Pattern

The engine uses a pluggable render system architecture. Each system implements the `IRenderSystem` interface:

```cpp
class IRenderSystem {
public:
    virtual void init() = 0;           // Initialize resources
    virtual void render(FrameInfo& frameInfo) = 0;  // Record draw commands
    virtual void onResize() = 0;       // Handle window resize
    virtual void cleanup() = 0;        // Destroy resources
};
```

Systems are registered with the renderer:

```cpp
renderer.addRenderSystem(
    std::make_unique<ModelRenderSystem>(device, "path/to/model.glb")
);
```

## Usage Example

```cpp
// Initialize engine components
HvkWindow window(800, 600, "Application");
HvkDevice device(window);
HvkRenderer renderer(window, device);

// Create camera
HvkCamera camera;

// Add render systems
renderer.addRenderSystem(
    std::make_unique<ModelRenderSystem>(device, "models/scene.glb")
);

// Main loop
while (!window.shouldClose()) {
    window.pollEvents();
    camera.update(deltaTime);
    renderer.drawFrame(camera);
}
```

## Adding Custom Render Systems

1. Create a class inheriting from `IRenderSystem`
2. Implement all virtual methods (`init`, `render`, `onResize`, `cleanup`)
3. Create pipeline and allocate resources in `init()`
4. Record draw commands in `render()`
5. Handle extent-dependent resources in `onResize()`
6. Clean up Vulkan objects in `cleanup()`
7. Register the system with the renderer

## Camera Controls

- **Mouse**: Look around (cursor is captured)
- **WASD**: Move forward/left/backward/right
- **Space**: Move up
- **Shift**: Move down

## Shader Compilation

Shaders are automatically compiled from GLSL to SPIR-V during the build process:

1. Place `.vert` and `.frag` files in the `shaders/` directory
2. CMake will invoke `glslangValidator` to compile them to `.spv` files
3. Load compiled shaders by referencing the `.spv` file path

## License

See LICENSE file for details.

*First commit: basic engine skeleton with core abstractions.*
