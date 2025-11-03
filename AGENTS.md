# Repository Guidelines

## Project Structure & Module Organization
- `HolyVulkanEngine/` static library with modules: `core`, `gfx`, `resources`, `scene`, `systems`, `ui`.
- Public headers in `HolyVulkanEngine/include/hvk/**`; implementations in `HolyVulkanEngine/src/hvk/**`.
- Apps in `app/` (`HVKApp`, `PrimitivesDemo`, `GridDemo`).
- Shaders in `shaders/` (GLSL); compiled SPIR‑V `.spv` emitted alongside sources.
- Assets in `assets/` (models, textures). Build output in `build/<preset>/bin/<Config>/`.

## Build, Test, and Development Commands
- Configure (VS 2022 x64 via presets):
  - `cmake --preset debug` (or `release`)
- Build:
  - `cmake --build --preset build-debug` (or `build-release`)
- Run demos (Debug):
  - `build/debug/bin/Debug/HVKApp.exe`
  - `build/debug/bin/Debug/PrimitivesDemo.exe`
  - `build/debug/bin/Debug/GridDemo.exe`
- Notes: GLSL compiles via `glslangValidator` during build. Ensure Vulkan SDK is installed and on `PATH`.

## Coding Style & Naming Conventions
- C++20. 4‑space indentation, no tabs. Brace on same line.
- Names: Types `PascalCase` (e.g., `Device`), functions/vars `camelCase` (e.g., `framebufferSize`), constants `SCREAMING_SNAKE_CASE` when appropriate.
- Files: `hvk_*.(hpp|cpp)` snake_case within module folders; headers under `include/hvk/**` mirror `src/hvk/**`.
- Prefer RAII, `std::` types, and explicit lifetimes; avoid raw `new/delete`.

## Testing Guidelines
- No formal unit tests yet. Validate changes by running demos above.
- Expect verbose console logs and Vulkan validation messages in Debug.
- If adding tests later, follow CTest naming: `tests/<area>/test_<name>.cpp` and wire via CMake + `ctest`.

## Commit & Pull Request Guidelines
- Commit messages: imperative mood, short summary, optional scope. Examples:
  - `feat(gfx): add pipeline cache stats`
  - `fix(resources): correct GLTF tangent import`
- PRs: include description, linked issues, build preset used, and screenshots (from `.images/` or app output) when UI/visuals change.
- Keep changes focused; update `CMakeLists.txt` when adding modules, headers, or sources.

## Security & Configuration Tips
- Install LunarG Vulkan SDK 1.3+; ensure `glslangValidator` is on `PATH` (`%VULKAN_SDK%\Bin`).
- Use latest GPU drivers. Prefer Debug builds with validation layers enabled during development.
- Don’t commit large binaries to `assets/` unnecessarily; consider `.gitattributes`/LFS if needed.

