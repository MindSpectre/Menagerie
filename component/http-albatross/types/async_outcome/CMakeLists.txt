##############################################################################
# Http Types — AsyncOutcome / AsyncResponse aliases (header-only)
##############################################################################
add_library(${Albatross}.Types.AsyncOutcome INTERFACE async_outcome.hpp)

target_include_directories(${Albatross}.Types.AsyncOutcome INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Types.AsyncOutcome INTERFACE
        Boost::asio
        Menagerie::Common::Beaver
        ${Albatross}.Types.Executor
)
##############################################################################
