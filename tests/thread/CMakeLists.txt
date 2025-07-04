# Application name
set(TEST_NAME casket_thread_test)

# Sources
file(GLOB_RECURSE SOURCES *.cpp)

# Set executable target
add_executable(${TEST_NAME} ${SOURCES})

# Set dependencies
target_link_libraries(${TEST_NAME}
    PRIVATE
        casket
        Threads::Threads
        GTest::GTest
        GTest::gtest_main)

# Discover tests
gtest_discover_tests(
    ${TEST_NAME}
    XML_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEST_NAME}.reports
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# Code coverage
target_code_coverage(${TEST_NAME} AUTO PRIVATE)