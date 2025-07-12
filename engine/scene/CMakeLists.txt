file(GLOB_RECURSE SCENE_SRC CONFIGURE_DEPENDS *.cpp *.h)

add_library(hvk_scene STATIC ${SCENE_SRC})

target_include_directories(hvk_scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(hvk_scene
    PUBLIC hvk_core
)