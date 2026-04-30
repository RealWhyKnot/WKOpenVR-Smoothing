cmake_minimum_required (VERSION 3.15)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY   		${CMAKE_BINARY_DIR}/artifacts)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY   		${CMAKE_BINARY_DIR}/artifacts)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY   		${CMAKE_BINARY_DIR}/artifacts)

function(adjust_bin_paths lib)
  set_target_properties(${lib}
		PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY 			${CMAKE_BINARY_DIR}/artifacts
		LIBRARY_OUTPUT_DIRECTORY 			${CMAKE_BINARY_DIR}/artifacts
		ARCHIVE_OUTPUT_DIRECTORY 			${CMAKE_BINARY_DIR}/artifacts
	)
endfunction()

project("FingerSmoothing")

# Enable link-time code generation (LTCG / IPO) for Release. The smoothing
# math runs per skeletal-update inside vrserver.exe and benefits from
# cross-TU inlining. Off in Debug to keep build/iter times reasonable.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Include project
add_subdirectory ("src")

# Libs
add_subdirectory ("lib")

if ("${CMAKE_CXX_COMPILER}" MATCHES "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
	add_compile_options(-Werror=return-type /MP)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
	add_compile_options(-Werror=return-type)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
	add_compile_options(/Qdiag-error-return)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
	add_compile_options(
		/we4715 # not all control paths return a value
		/we4828 # disallow invalid characters
		/we4473 # printf-like: not enough arguments passed for format string
		/we4474 # printf-like: too many arguments passed for format string
		/we4475 # printf-like: length modifier cannot be used with type field
		/we4476 # printf-like: unknown type field character in format specifier
		/we4477 # printf-like: format string requires an argument of different type
		/we4478 # printf-like: positional and non-positional placeholders cannot be mixed
		/we4775 # nonstandard extension used in format string
		/we4776 # % is not allowed in the format string
		/we4777 # printf-like: format string requires an argument of different type
		/we4778 # printf-like: unterminated format string
		/we4002 # too many actual parameters for macro
		/we4003 # not enough actual parameters for macro
		/MP # multiprocessor compilation
		/utf-8 # utf-8 source & exec
		/GF) # eliminate duplicate strings

	add_compile_options(
		$<$<CONFIG:Release>:/fp:fast>
		$<$<CONFIG:Release>:/arch:AVX2>)
endif()
