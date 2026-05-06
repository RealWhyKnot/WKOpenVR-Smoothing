cmake_minimum_required(VERSION 3.24)

project("lib")

# GLFW (window + GL context). Submodule pinned to 3.4.
message("Adding GLFW...")
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(glfw)

# Dear ImGui (UI). Submodule pinned to 1.91 ish; the backend files for OpenGL3
# and GLFW are compiled directly into the overlay target rather than into this
# imgui static lib, mirroring how SC consumes them.
message("Adding ImGui...")
add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/lib/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/lib/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/lib/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/lib/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/lib/imgui/imgui_widgets.cpp)
target_include_directories(imgui PUBLIC ${CMAKE_SOURCE_DIR}/lib/imgui)
target_compile_definitions(imgui PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

# gl3w (OpenGL function loader).
message("Adding gl3w...")
add_library(gl3w STATIC ${CMAKE_SOURCE_DIR}/lib/gl3w/src/gl3w.c)
target_include_directories(gl3w PUBLIC ${CMAKE_SOURCE_DIR}/lib/gl3w/include)

# Solution-folder grouping for the Visual Studio sidebar.
set_property(TARGET glfw PROPERTY FOLDER "lib/glfw")
set_property(TARGET update_mappings PROPERTY FOLDER "lib/glfw")
set_property(TARGET imgui PROPERTY FOLDER "lib")
set_property(TARGET gl3w PROPERTY FOLDER "lib")
