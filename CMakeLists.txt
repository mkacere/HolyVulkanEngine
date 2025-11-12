cmake_minimum_required(VERSION 3.16)
project(HVKWorkspace LANGUAGES C CXX)

# Global C++ config
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------
# Optimal Compiler Flags
# -----------------------------
if(MSVC)
    # MSVC optimizations
    add_compile_options(
        /W4                     # High warning level
        /permissive-            # Strict standards conformance
        /Zc:__cplusplus         # Enable updated __cplusplus macro
        /Zc:inline              # Remove unreferenced functions/data
        /Zc:throwingNew         # Assume operator new throws on failure
        /MP                     # Multi-processor compilation
        /bigobj                 # Large object files for templates
    )

    # Suppress warnings from external libraries (MSVC 2017+)
    add_compile_options(
        /external:W0            # Warning level 0 for external headers
        /external:anglebrackets # Treat angle-bracket includes as external
    )

    # Release optimizations (maximum performance)
    add_compile_options(
        $<$<CONFIG:Release>:/O2>        # Maximum optimization (speed)
        $<$<CONFIG:Release>:/Oi>        # Enable intrinsic functions
        $<$<CONFIG:Release>:/Ot>        # Favor fast code
        $<$<CONFIG:Release>:/GL>        # Whole program optimization
        $<$<CONFIG:Release>:/Gy>        # Function-level linking
        $<$<CONFIG:Release>:/fp:fast>   # Fast floating-point model
    )

    # Release linker flags
    add_link_options(
        $<$<CONFIG:Release>:/LTCG>      # Link-time code generation
        $<$<CONFIG:Release>:/OPT:REF>   # Eliminate unreferenced functions
        $<$<CONFIG:Release>:/OPT:ICF>   # Identical COMDAT folding
    )

    # Debug flags (demos use default CMake Debug settings)
    add_compile_options(
        $<$<CONFIG:Debug>:/Zi>          # Full debug info (PDB)
    )
else()
    # GCC/Clang optimizations
    add_compile_options(
        -Wall                   # Enable most warnings
        -pedantic               # Strict ISO C++ compliance
    )

    # Release optimizations (maximum performance)
    add_compile_options(
        $<$<CONFIG:Release>:-O3>            # Maximum optimization
        $<$<CONFIG:Release>:-march=native>  # Optimize for current CPU
        $<$<CONFIG:Release>:-flto>          # Link-time optimization
        $<$<CONFIG:Release>:-ffast-math>    # Fast floating-point math
        $<$<CONFIG:Release>:-funroll-loops> # Unroll loops
    )

    # Debug flags (demos use default CMake Debug settings)
    add_compile_options(
        $<$<CONFIG:Debug>:-g>               # Debug symbols
    )
endif()

# Output directories (good defaults)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# -----------------------------
# Shader compilation: GLSL -> SPIR-V
# -----------------------------
file(GLOB_RECURSE SHADER_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders/*.frag"
)

find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator glslangvalidator
    DOC "Path to glslangValidator"
)
if (NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found. Install LunarG SDK or glslang tools.")
endif()

set(SPIRV_BINARY_FILES)
foreach(GLSL_FILE IN LISTS SHADER_SOURCE_FILES)
    get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
    get_filename_component(FILE_DIR  ${GLSL_FILE} DIRECTORY)
    set(SPV_FILE "${FILE_DIR}/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT "${SPV_FILE}"
        COMMAND ${GLSLANG_VALIDATOR} -V "${GLSL_FILE}" -o "${SPV_FILE}"
        DEPENDS "${GLSL_FILE}"
        COMMENT "Compiling shader: ${FILE_NAME} -> ${FILE_NAME}.spv"
        VERBATIM
    )
    list(APPEND SPIRV_BINARY_FILES "${SPV_FILE}")
endforeach()

# Build all shaders by default
add_custom_target(compile_shaders ALL
    DEPENDS ${SPIRV_BINARY_FILES}
    COMMENT "Building SPIR-V shaders"
)

# -----------------------------
# Subprojects
# -----------------------------
add_subdirectory(HolyVulkanEngine)   # defines HVKEngine target
add_subdirectory(app)                # defines HVKApp target

# Make sure the app builds shaders first
add_dependencies(HVKApp compile_shaders)
