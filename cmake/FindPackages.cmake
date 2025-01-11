find_package(Threads)

if (ENABLE_UNIT_TESTS)
    find_package(GTest REQUIRED)
    include(GoogleTest)
endif (ENABLE_UNIT_TESTS)
