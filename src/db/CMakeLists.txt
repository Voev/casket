# Sources
file(GLOB_RECURSE SOURCES *.cpp)

# Set target library
add_library(casket_db STATIC ${SOURCES})

# Dependencies
target_link_libraries(casket_db PUBLIC casket_headers)