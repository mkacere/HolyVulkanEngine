# engine/systems/CMakeLists.txt

# Gather all .cpp files in this directory
file(GLOB SYSTEM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Add them to the engine static library
target_sources(HVKEngine PRIVATE
    ${SYSTEM_SOURCES}
)

# Make sure we can #include "obj_render_system.hpp", etc.
target_include_directories(HVKEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
