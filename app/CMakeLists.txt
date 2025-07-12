# 1. Collect all .cpp and .hpp files in the app directory
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

# 2. Create an executable target from the source files
add_executable(HVKApp ${APP_SOURCES})

# 3. Link with the unified engine library and GLFW
target_link_libraries(HVKApp
    PRIVATE
        HVKEngine     # your unified static engine library
        glfw          # external dependency
)

# 4. Optional: expose app include directory
target_include_directories(HVKApp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)