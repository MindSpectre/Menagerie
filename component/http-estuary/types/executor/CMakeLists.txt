##############################################################################
# Http Types — concrete executor / strand / stream aliases (header-only)
##############################################################################
add_library(${Estuary}.Types.Executor INTERFACE executor.hpp)

target_include_directories(${Estuary}.Types.Executor INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Types.Executor INTERFACE
        Boost::asio
        Boost::beast
)
##############################################################################
