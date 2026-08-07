##############################################################################
# Http Types — concrete executor / strand / stream aliases (header-only)
##############################################################################
add_library(${Http}.Types.Executor INTERFACE executor.hpp)

target_include_directories(${Http}.Types.Executor INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Types.Executor INTERFACE
        Boost::asio
        Boost::beast
)
##############################################################################
