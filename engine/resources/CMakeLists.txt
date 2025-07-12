file(GLOB_RECURSE RESOURCE_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(hvk_res STATIC ${RESOURCE_SRC})

target_include_directories(hvk_res PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/tinygltf
)

target_link_libraries(hvk_res
    PUBLIC hvk_gfx
)
