# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Holy Vulkan Engine (HVK) is a Vulkan-based 3D rendering engine designed to provide a simplified abstraction layer over Vulkan for graphics programming. The engine uses modern C++20 and follows a modular architecture with distinct subsystems.

## Build System

### Prerequisites
- C++20 compatible compiler
- Vulkan SDK (LunarG SDK or equivalent) with `glslangValidator` for shader compilation
- CMake 3.16+
- GLFW, GLM, tinygltf, and stb libraries (included as submodules in `HolyVulkanEngine/external/`)

### Build Commands

```bash
# Configure using CMake presets
cmake --preset debug          # Debug configuration
cmake --preset release        # Release configuration

# Build
cmake --build build/debug --config Debug
cmake --build build/release --config Release

# Run the application
./build/debug/bin/HVKApp.exe      # Debug
./build/release/bin/HVKApp.exe    # Release
```

### Shader Compilation
Shaders are automatically compiled from GLSL to SPIR-V during the build process. Shader source files (`.vert`, `.frag`) are located in `shaders/` and compiled to `.spv` files in-place using `glslangValidator`.

## Architecture

### Module Organization

The engine is organized into a library (`HVKEngine`) and an application (`HVKApp`):

```
HolyVulkanEngine/          # Engine library
├── include/hvk/           # Public headers
│   ├── gfx/              # Graphics/rendering subsystem
│   ├── scene/            # Scene management (camera, game objects)
│   ├── resources/        # Resource loading (models, textures)
│   └── systems/          # Render systems (IRenderSystem interface)
└── src/hvk/              # Implementation files (mirrors include structure)

app/                       # Application executable
└── main.cpp              # Entry point

shaders/                   # GLSL shader sources
└── *.vert, *.frag        # Vertex and fragment shaders
```

### Core Subsystems

#### 1. **Graphics (gfx/)**
Vulkan abstraction layer providing core rendering functionality:

- **HvkDevice**: Manages Vulkan instance, physical/logical device, queues, and command pools
- **HvkWindow**: GLFW window wrapper with Vulkan surface creation
- **HvkSwapChain**: Handles swapchain, render passes, and framebuffers
- **HvkRenderer**: Main rendering coordinator that manages the render loop and delegates to render systems
- **HvkPipeline**: Graphics pipeline creation and management
- **HvkBuffer**: Vertex/index/uniform buffer management
- **HvkDescriptors**: Descriptor set layouts, pools, and management

Key data structures:
- `FrameInfo`: Contains per-frame rendering data (command buffer, camera, game objects, frame index/time)
- `GlobalUbo`: Uniform buffer object for projection/view matrices and lighting (supports up to 100 point lights)

#### 2. **Scene (scene/)**
Scene graph and camera management:

- **HvkGameObject**: Entity-component system for scene objects (transform, model, etc.)
- **HvkCamera**: First-person camera with mouse look and WASD movement

#### 3. **Resources (resources/)**
Asset loading and management:

- **HvkModel**: GLTF model loading via tinygltf, with support for:
  - Vertex data (position, color, normal, UV)
  - PBR textures (base color, metallic-roughness, normal maps, emissive)
  - Vertex/index buffer creation
  - Descriptor set management for textures

#### 4. **Systems (systems/)**
Pluggable render system architecture:

- **IRenderSystem**: Abstract interface that all render systems implement
  - `init()`: Called once after swapchain creation
  - `render()`: Records draw commands each frame
  - `onResize()`: Handles swapchain recreation (e.g., window resize)
  - `cleanup()`: Destroys Vulkan resources

- **ModelRenderSystem**: Concrete implementation that renders GLTF models with textures

### Render System Pattern

The engine uses a plugin-style architecture where render systems are added to the renderer:

```cpp
renderer.addRenderSystem(
    std::make_unique<ModelRenderSystem>(device, "path/to/model.glb")
);
```

Each system manages its own:
- Pipeline creation
- Descriptor sets
- Draw commands

The renderer calls systems during `drawFrame()`, providing a `FrameInfo` struct with all necessary context.

### Rendering Flow

1. **HvkRenderer::drawFrame()** begins the frame (acquires swapchain image, begins command buffer)
2. For each registered render system:
   - System's `render()` is called with current `FrameInfo`
   - System records its draw commands to the command buffer
3. Renderer ends the render pass and submits the command buffer
4. Present the frame to the swapchain

## Development Notes

### Adding a New Render System

1. Create a class inheriting from `IRenderSystem` in `HolyVulkanEngine/include/hvk/systems/`
2. Implement all pure virtual methods: `init()`, `render()`, `onResize()`, `cleanup()`
3. In `init()`: Create pipeline, descriptor layouts, allocate resources
4. In `render()`: Bind pipeline, bind descriptors, record draw commands
5. In `onResize()`: Recreate any extent-dependent resources (e.g., pipeline)
6. In `cleanup()`: Destroy all Vulkan objects
7. Add implementation to `HolyVulkanEngine/src/hvk/systems/`
8. Register with renderer in `app/main.cpp` using `addRenderSystem()`

### Working with Shaders

- Place GLSL shaders in `shaders/` directory
- Use `.vert` extension for vertex shaders, `.frag` for fragment shaders
- Shaders are auto-compiled to `.spv` during build
- Load shaders by referencing the compiled `.spv` file path

### Camera Controls

The default camera implementation (HvkCamera) provides:
- Mouse look (cursor is captured)
- WASD movement
- Update via `camera.update(deltaTime)` each frame

### GLTF Model Loading

Models are loaded using `HvkModel::createModelFromFile()`. The builder pattern extracts:
- Mesh data (vertices, indices)
- PBR material textures (base color, metallic-roughness, normal, emissive)
- Texture coordinates for each texture type

## Important Constraints

- The engine uses Vulkan's depth range [0, 1] (set via `GLM_FORCE_DEPTH_ZERO_TO_ONE`)
- GLM is configured to use radians (`GLM_FORCE_RADIANS`)
- Maximum point lights in GlobalUbo: 100 (must match shader definition)
- Uses std140 layout for uniform buffers
- C++20 is required (uses features like concepts, ranges, etc.)
