cmake_minimum_required(VERSION 3.16)
project(HVKEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output folders
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# External libraries
add_subdirectory(external/glfw)

# Add subdirectories
add_subdirectory(engine)
add_subdirectory(app)

# 1) collect all your GLSL into a list
file(GLOB_RECURSE SHADER_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders/*.frag"
)

# 2) find the validator
find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator glslangvalidator
    HINTS ENV PATH
    DOC "Path to the glslangValidator executable"
)
if (NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found! Please install the LunarG SDK or glslang tools.")
endif()

# 3) prepare the outputs list
set(SPIRV_BINARY_FILES)

# 4) loop and compile in-place
foreach(GLSL_FILE IN LISTS SHADER_SOURCE_FILES)
    get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
    get_filename_component(FILE_DIR  ${GLSL_FILE} DIRECTORY)

    # e.g. shaders/triangle.vert.spv
    set(SPV_FILE "${FILE_DIR}/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT    "${SPV_FILE}"
        COMMAND   ${GLSLANG_VALIDATOR} -V "${GLSL_FILE}" -o "${SPV_FILE}"
        DEPENDS   "${GLSL_FILE}"
        COMMENT   "🔨 Compiling shader ${FILE_NAME} → ${FILE_NAME}.spv"
        VERBATIM
    )

    list(APPEND SPIRV_BINARY_FILES "${SPV_FILE}")
endforeach()

# 5) ensure they all get built before your app
add_custom_target(compile_shaders ALL
    DEPENDS ${SPIRV_BINARY_FILES}
    COMMENT "Building all SPIR-V shaders via glslangValidator"
)