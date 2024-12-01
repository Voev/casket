add_library(CasketOptionParser INTERFACE)

target_include_directories(CasketOptionParser INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(CasketOptionParser PROPERTIES
    EXPORT_NAME OptionParser
)
