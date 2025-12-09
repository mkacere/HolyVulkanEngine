# /app/CMakeLists.txt

# Main app - GLTF model demo
add_executable(HVKApp main.cpp)
target_compile_features(HVKApp PRIVATE cxx_std_20)
target_compile_definitions(HVKApp PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(HVKApp PRIVATE HVKEngine)
set_target_properties(HVKApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Primitives demo - Test primitive shapes
add_executable(PrimitivesDemo primitives_demo.cpp)
target_compile_features(PrimitivesDemo PRIVATE cxx_std_20)
target_compile_definitions(PrimitivesDemo PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(PrimitivesDemo PRIVATE HVKEngine)
set_target_properties(PrimitivesDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Grid demo - Infinite grid visualization
add_executable(GridDemo grid_demo.cpp)
target_compile_features(GridDemo PRIVATE cxx_std_20)
target_compile_definitions(GridDemo PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(GridDemo PRIVATE HVKEngine)
set_target_properties(GridDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Skybox demo - Equirect/cubemap background
add_executable(SkyboxDemo skybox_demo.cpp)
target_compile_features(SkyboxDemo PRIVATE cxx_std_20)
target_compile_definitions(SkyboxDemo PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(SkyboxDemo PRIVATE HVKEngine)
set_target_properties(SkyboxDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
add_dependencies(SkyboxDemo compile_shaders)

# ECS demo - EnTT integration demonstration
if(HVK_WITH_ECS)
    add_executable(ECSDemo ecs_demo.cpp)
    target_compile_features(ECSDemo PRIVATE cxx_std_20)
    target_compile_definitions(ECSDemo PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
    target_link_libraries(ECSDemo PRIVATE HVKEngine)
    set_target_properties(ECSDemo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_dependencies(ECSDemo compile_shaders)

    # Fractal demo - 3D raymarched fractals
    add_executable(FractalDemo fractal_demo.cpp)
    target_compile_features(FractalDemo PRIVATE cxx_std_20)
    target_compile_definitions(FractalDemo PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
    target_link_libraries(FractalDemo PRIVATE HVKEngine)
    set_target_properties(FractalDemo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_dependencies(FractalDemo compile_shaders)

    # Debug Line demo - Debug line rendering system showcase
    add_executable(DebugLineDemo debug_line_demo.cpp)
    target_compile_features(DebugLineDemo PRIVATE cxx_std_20)
    target_compile_definitions(DebugLineDemo PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
    target_link_libraries(DebugLineDemo PRIVATE HVKEngine)
    set_target_properties(DebugLineDemo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_dependencies(DebugLineDemo compile_shaders)

    # Physics demo - Jolt Physics integration showcase
    if(HVK_WITH_PHYSICS)
        add_executable(PhysicsDemo physics_demo.cpp)
        target_compile_features(PhysicsDemo PRIVATE cxx_std_20)
        target_compile_definitions(PhysicsDemo PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
        target_link_libraries(PhysicsDemo PRIVATE HVKEngine)
        set_target_properties(PhysicsDemo PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
        add_dependencies(PhysicsDemo compile_shaders)
    endif()
endif()
