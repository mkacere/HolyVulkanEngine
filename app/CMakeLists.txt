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
