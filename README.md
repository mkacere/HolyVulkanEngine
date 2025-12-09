# Holy Vulkan Engine

A modern Vulkan-based 3D rendering engine built with C++20, featuring an Entity Component System architecture powered by EnTT and integrated Jolt Physics.

> "I am working on something holy. You are a bunch of squirrels." — In memory of Terry A. Davis

## Demo

[![Physics Integration Demo](https://img.youtube.com/vi/ZwCx2pgL3sw/maxresdefault.jpg)](https://www.youtube.com/watch?v=ZwCx2pgL3sw)

**[Watch the physics integration demo on YouTube](https://www.youtube.com/watch?v=ZwCx2pgL3sw)**

## Features

### Core Engine
- **Modern Vulkan 1.3**: Dynamic rendering, timeline semaphores, and synchronization2
- **Entity Component System**: High-performance architecture using EnTT
- **Physics Simulation**: Integrated Jolt Physics for rigid body dynamics and collision detection
- **Scene Management**: Hierarchical transforms with parent-child relationships
- **Resource Management**: Automatic GPU memory allocation via VMA with deferred deletion

### Rendering
- **PBR Materials**: Physically-based rendering with metallic-roughness workflow
- **GLTF 2.0 Loading**: Full scene import with meshes, materials, textures, and hierarchies
- **MSAA Support**: Configurable multisample anti-aliasing (2x/4x/8x)
- **Fractal Rendering**: Raymarched 3D fractals (Mandelbulb, Julia sets, Menger sponge)
- **Debug Visualization**: Debug line rendering for physics shapes and debug overlays
- **Shader Pipeline**: Automatic GLSL to SPIR-V compilation with reflection-based descriptor generation

### Application Framework
- **Batteries-Included Setup**: `Application` class handles initialization, rendering loop, and resource management
- **Spawn API**: Simple entity spawning with `spawnModel()`, `spawnCamera()`, `spawnPointLight()`
- **Camera Controller**: FPS and fly modes with WASD + mouse control
- **Dear ImGui**: Built-in UI framework with docking and multi-viewport support

![HVK Miku Demo](.images/miku_demo.png)
*GLTF model rendering with PBR materials and multiple lights*

![HVK Grid Demo](.images/grid_demo.png)
*Infinite grid visualization with Dear ImGui integration*

## Quick Start

### Prerequisites
- **C++20 Compiler**: MSVC 2019+, GCC 10+, or Clang 10+
- **Vulkan SDK 1.3+**: With `glslangValidator` in PATH
- **CMake 3.16+**
- **Git**: For submodule management

### Build Instructions

```bash
# Clone with submodules
git clone <repository-url>
cd HolyVulkanEngineWorkSpace
git submodule update --init --recursive

# Configure and build
cmake --preset debug
cmake --build build/debug --config Debug

# Run demo applications
./build/debug/bin/Debug/ECSDemo.exe        # ECS demo with hierarchy
./build/debug/bin/Debug/GridDemo.exe       # Infinite grid
./build/debug/bin/Debug/PrimitivesDemo.exe # Basic shapes
./build/debug/bin/Debug/HVKApp.exe         # GLTF model viewer
```

For release builds, use `cmake --preset release` and `--config Release`.

## Usage Example

```cpp
#include <hvk/ecs.hpp>
#include <hvk/core.hpp>

using namespace hvk;

int main() {
    // Configure window
    WindowCreateInfo windowCI{
        .title = "My Game",
        .mode = WindowMode::Auto
    };

    // Configure device
    DeviceCreateInfo deviceCI{
        .debugVerbosity = DebugVerbosity::Warn
    };

    // Configure application
    ApplicationCreateInfo appCI{};
    appCI.windowCI = windowCI;
    appCI.deviceCI = deviceCI;
    appCI.createDefaultCamera = true;
    appCI.createDefaultLights = true;
    appCI.autoRegisterSystems = true;
    appCI.enableImGui = true;
    appCI.enableCameraController = true;

    Application app(appCI);

    // Initialize scene
    app.onInit([](Application& app) {
        Scene& scene = app.scene();

        // Spawn a model
        auto entity = scene.spawnModel(
            PROJECT_ROOT "/assets/models/my_model.glb",
            glm::vec3(0, 0, 0)
        );

        // Add lights
        scene.spawnPointLight(
            glm::vec3(5, 5, 5),      // position
            glm::vec3(1, 0.8, 0.6),  // color
            10.0f,                    // intensity
            20.0f                     // radius
        );
    });

    // Update logic
    app.onUpdate([](Application& app, float deltaTime) {
        Scene& scene = app.scene();

        // Update entities
        auto view = scene.view<TransformComponent>();
        for (auto [entity, transform] : view.each()) {
            // Your game logic here
        }
    });

    return app.run();
}
```

See `app/ecs_demo.cpp` for a complete example with hierarchy and advanced features.

## Architecture

The engine is organized into modular subsystems:

**ECS Framework** (`hvk/ecs/`) - Application, Scene, Components, Systems
**Graphics** (`hvk/gfx/`) - Vulkan abstraction, pipelines, descriptors, resources
**Scene Management** (`hvk/scene/`) - Camera, transforms, hierarchies
**Resource Loading** (`hvk/resources/`) - GLTF models, materials, textures
**Physics** (`hvk/physics/`) - Jolt Physics integration (in development)
**Core Systems** (`hvk/core/`) - Input, timing
**UI Layer** (`hvk/ui/`) - ImGui integration

### Key Components

```cpp
TransformComponent          // Position, rotation, scale
MeshComponent              // Renderable mesh reference
CameraComponent            // Camera parameters
ParentComponent            // Hierarchy parent
ChildrenComponent          // Hierarchy children
RigidBodyComponent         // Physics body
VelocityComponent          // Linear/angular velocity
PointLightComponent        // Point light
DirectionalLightComponent  // Directional light
```

### Key Systems

```cpp
TransformSystem      // Updates world matrices
HierarchySystem      // Propagates parent-child transforms
CameraSystem         // Updates camera matrices
MeshRenderSystem     // Renders visible meshes
PhysicsSystem        // Physics simulation (Jolt)
```

## Controls

| Input | Action |
|-------|--------|
| **WASD** | Move camera |
| **Mouse** | Look around |
| **Space** | Move up |
| **Ctrl** | Move down |
| **Shift** | Sprint |
| **ESC** | Toggle cursor lock |

## Creating Custom Systems

Inherit from `ISystem` for logic systems or `IRenderSystem` for rendering:

```cpp
class MySystem : public hvk::ISystem {
public:
    void update(Scene& scene, float deltaTime) override {
        auto view = scene.view<MyComponent>();
        for (auto [entity, comp] : view.each()) {
            // Update logic
        }
    }
};

// Register with scene
scene.registerSystem<MySystem>();
```

## Shader Development

Shaders are automatically compiled during build:

1. Place `.vert` and `.frag` files in `shaders/` directory
2. Use GLSL 450 syntax
3. Build with CMake - outputs `.spv` files
4. Load with `loadSpirv(PROJECT_ROOT "/shaders/myshader.vert.spv")`

Descriptor set conventions:
- **Set 0**: Global (Scene, Camera, Lights)
- **Set 1**: Material (Textures, material data)
- **Push Constants**: Per-draw data

## Dependencies

All dependencies are included as submodules:

- **GLFW** - Window and input
- **GLM** - Math library
- **EnTT** - ECS framework
- **Jolt Physics** - Physics simulation
- **tinygltf** - GLTF 2.0 loader
- **stb_image** - Image loading
- **VMA** - GPU memory allocator
- **SPIRV-Reflect** - Shader reflection
- **Dear ImGui** - UI framework

## Contributing

Contributions are welcome! Follow these conventions:

- All code in `namespace hvk`
- PascalCase for types, camelCase for functions/variables
- Public API in `include/hvk/`, implementation in `src/hvk/`
- Use `VK_CHECK()` macro for Vulkan calls
- Suffix components with `Component`, systems with `System`

## License

See LICENSE file for details.

---

*Built with C++20, Vulkan 1.3, EnTT ECS, and Jolt Physics.*
