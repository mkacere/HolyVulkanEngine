# /app/CMakeLists.txt
add_executable(HVKApp main.cpp)

# Ensure C++20 for the app too
target_compile_features(HVKApp PRIVATE cxx_std_20)


# Link against the engine library target exported by HolyVulkanEngine
target_link_libraries(HVKApp PRIVATE HVKEngine)

# Optional: set the app's runtime dir explicitly (matches root settings)
set_target_properties(HVKApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
