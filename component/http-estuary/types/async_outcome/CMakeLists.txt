##############################################################################
# Http Types — AsyncOutcome / AsyncResponse aliases (header-only)
##############################################################################
add_library(${Estuary}.Types.AsyncOutcome INTERFACE async_outcome.hpp)

target_include_directories(${Estuary}.Types.AsyncOutcome INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Types.AsyncOutcome INTERFACE
        Boost::asio
        Menagerie::Common::Beaver
        ${Estuary}.Types.Executor
)
##############################################################################
