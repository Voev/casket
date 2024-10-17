# Sources
file(GLOB_RECURSE SOURCES *.cpp)

# Set target library
add_executable(test_sql ${SOURCES})

# Dependencies
target_link_libraries(test_sql casket ${SQLITE3_LIBRARY})
