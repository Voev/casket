cmake_minimum_required(VERSION 3.10)
file(STRINGS "version.txt" VERSION_STRING)

project(casket
        LANGUAGES C CXX
        VERSION ${VERSION_STRING})

# Set path to CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Custom options
option (ENABLE_UNIT_TESTS "Enable unit tests" OFF)
option (ENABLE_CODE_COVERAGE "Enable code coverage" OFF)

# Include CMake modules
include(ConfigureCompiler)
include(FindPackages)
include(CodeCoverage)

# Configure result directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Coverage options
if (ENABLE_CODE_COVERAGE)
    add_code_coverage()
endif (ENABLE_CODE_COVERAGE)

# Entry point for sources
add_subdirectory(include)
add_subdirectory(src)

# Test options
if (ENABLE_UNIT_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif (ENABLE_UNIT_TESTS)
