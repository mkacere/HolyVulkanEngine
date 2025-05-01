# Collect all app source files
file(GLOB_RECURSE APP_SOURCES
    *.cpp
    *.hpp
)

# Create the executable for the app
add_executable(HVKApp ${APP_SOURCES})

# Link the app with engine and external libraries
target_link_libraries(HVKApp
    HVKEngine
    glfw
)
