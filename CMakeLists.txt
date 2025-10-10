cmake_minimum_required(VERSION 3.16)
project(HVKWorkspace LANGUAGES CXX)

# Global C++ config
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
