##############################################################################
# Http Types — concrete executor / strand / stream aliases (header-only)
##############################################################################
add_library(${Albatross}.Types.Executor INTERFACE executor.hpp)

target_include_directories(${Albatross}.Types.Executor INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Types.Executor INTERFACE
        Boost::asio
        Boost::beast
)
##############################################################################
