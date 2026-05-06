cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(OpenVRSmoothingOverlay)
message("OpenVR-Smoothing - Overlay")

set(SHARED_DIR ${CMAKE_SOURCE_DIR}/lib/OpenVR-PairDriver/src/common)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/IPCClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/IPCClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.h
)

set(IMGUI_BACKEND_FILES
    ${CMAKE_SOURCE_DIR}/lib/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/lib/imgui/backends/imgui_impl_glfw.cpp
)

add_executable(OpenVRSmoothingOverlay WIN32 ${SOURCES} ${IMGUI_BACKEND_FILES})

# Pull in the shared wire header (Protocol.h) from the OpenVR-PairDriver
# submodule so this overlay's IPC client matches the driver layout exactly.
target_include_directories(OpenVRSmoothingOverlay
    PUBLIC ${CMAKE_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${SHARED_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/lib
    PUBLIC ${CMAKE_SOURCE_DIR}/lib/openvr/headers
    PUBLIC ${CMAKE_SOURCE_DIR}/lib/imgui/backends
)

target_link_libraries(OpenVRSmoothingOverlay
    PRIVATE opengl32.lib
    PRIVATE glfw
    PRIVATE gl3w
    PRIVATE imgui)

target_compile_definitions(OpenVRSmoothingOverlay
    PRIVATE NOMINMAX
    PRIVATE UNICODE
    PRIVATE _UNICODE
    PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

# Build-time version stamp from version.txt at the repo root.
file(READ "${CMAKE_SOURCE_DIR}/version.txt" SMOOTHING_VERSION_RAW)
string(STRIP "${SMOOTHING_VERSION_RAW}" SMOOTHING_VERSION_STAMP)
target_compile_definitions(OpenVRSmoothingOverlay
    PRIVATE "SMOOTHING_VERSION_STRING=\"${SMOOTHING_VERSION_STAMP}\"")

set_target_properties(OpenVRSmoothingOverlay PROPERTIES
    OUTPUT_NAME "OpenVR-Smoothing"
    LINK_FLAGS_DEBUG          "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup"
    LINK_FLAGS_RELEASE        "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    LINK_FLAGS_MINSIZEREL     "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
