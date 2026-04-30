cmake_minimum_required (VERSION 3.24)

project("lib")

# Includes all 3rd party libraries here

# Minhook library
message("Adding minhook...")
add_subdirectory ("minhook")

# GLFW
message("Adding GLFW...")
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(glfw)

# Dear ImGui
message("Adding ImGui...")
add_library(imgui STATIC 
	${CMAKE_SOURCE_DIR}/lib/imgui/imgui.cpp
	${CMAKE_SOURCE_DIR}/lib/imgui/imgui_demo.cpp
	${CMAKE_SOURCE_DIR}/lib/imgui/imgui_draw.cpp
	${CMAKE_SOURCE_DIR}/lib/imgui/imgui_tables.cpp
	${CMAKE_SOURCE_DIR}/lib/imgui/imgui_widgets.cpp)
target_include_directories(imgui PUBLIC ${CMAKE_SOURCE_DIR}/lib/imgui)
target_compile_definitions(imgui PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

# ImPlot
message("Adding ImPlot...")
message("Adding ImGui...")
add_library(implot STATIC 
	${CMAKE_SOURCE_DIR}/lib/implot/implot.cpp
	${CMAKE_SOURCE_DIR}/lib/implot/implot_demo.cpp
	${CMAKE_SOURCE_DIR}/lib/implot/implot_items.cpp)
target_include_directories(implot PUBLIC ${CMAKE_SOURCE_DIR}/lib/implot)
target_compile_definitions(implot PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(implot PRIVATE imgui)

# gl3w
message("Adding gl3w...")
add_library(gl3w STATIC ${CMAKE_SOURCE_DIR}/lib/gl3w/src/gl3w.c)
target_include_directories(gl3w PUBLIC ${CMAKE_SOURCE_DIR}/lib/gl3w/include)

# Assign solution dirs 
set_property(TARGET minhook PROPERTY FOLDER "lib") 

set_property(TARGET glfw PROPERTY FOLDER "lib/glfw")
set_property(TARGET update_mappings PROPERTY FOLDER "lib/glfw")

set_property(TARGET imgui PROPERTY FOLDER "lib")
set_property(TARGET implot PROPERTY FOLDER "lib")
set_property(TARGET gl3w PROPERTY FOLDER "lib")