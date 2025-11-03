# HolyVulkanEngine/src/hvk/ecs/CMakeLists.txt
# ECS subsystem: EnTT-based entity component system

file(GLOB_RECURSE ECS_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
target_sources(HVKEngine PRIVATE ${ECS_SOURCES})
