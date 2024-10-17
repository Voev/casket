# Sources
file(GLOB_RECURSE SOURCES *.cpp)

# Set target library
add_executable(generator ${SOURCES})

# Dependencies
target_link_libraries(generator casket ${EXPAT_LIBRARIES})
