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

file(GLOB_RECURSE SHADER_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders/*.frag"
)

foreach(GLSL ${SHADER_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    get_filename_component(FILE_DIR ${GLSL} DIRECTORY)
    set(SPV "${FILE_DIR}/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPV}
        COMMAND glslc ${GLSL} -o ${SPV}
        DEPENDS ${GLSL}
        COMMENT "Compiling shader: ${GLSL}"
        VERBATIM
    )

    list(APPEND SPIRV_BINARY_FILES ${SPV})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_BINARY_FILES})