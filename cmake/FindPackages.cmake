find_package(SQLITE3 REQUIRED)
find_package(EXPAT REQUIRED)


if (ENABLE_UNIT_TESTS)
    find_package(GTest REQUIRED)
endif (ENABLE_UNIT_TESTS)
