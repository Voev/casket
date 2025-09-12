# Application name
set(TARGET_NAME casket_signal_handler)

# Sources
file(GLOB_RECURSE SOURCES *.cpp)

# Set executable target
add_executable(${TARGET_NAME} ${SOURCES})

# Set dependencies
target_link_libraries(${TARGET_NAME}
    PRIVATE
        casket
)
