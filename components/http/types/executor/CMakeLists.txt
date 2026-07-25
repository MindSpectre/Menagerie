##############################################################################
# Http Types — concrete executor / strand / stream aliases (header-only)
##############################################################################
add_library(${HTTP}.Types.Executor INTERFACE executor.hpp)

target_include_directories(${HTTP}.Types.Executor INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Types.Executor INTERFACE
        Boost::asio
        Boost::beast
)
##############################################################################
