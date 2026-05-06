cmake_minimum_required(VERSION 3.15)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project("OpenVR-Smoothing")

# Centralised output directory for built artifacts -- keeps the post-build
# zip-staging logic in build.ps1 simple.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/artifacts)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/artifacts)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/artifacts)

# LTCG for Release. The overlay is small but the cost is negligible and any
# inlining win across IPC/UI translation units is welcome.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Treat warnings on shape-mismatch / missing-return / printf format as errors.
# Mirrors the OpenVR-PairDriver and OpenVR-SpaceCalibrator warning sets.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
    add_compile_options(
        /we4715 /we4828
        /we4473 /we4474 /we4475 /we4476 /we4477 /we4478
        /we4775 /we4776 /we4777 /we4778
        /we4002 /we4003
        /MP
        /utf-8
        /GF)
endif()

add_subdirectory(src)
add_subdirectory(lib)
