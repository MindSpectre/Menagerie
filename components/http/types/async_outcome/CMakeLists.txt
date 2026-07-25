##############################################################################
# Http Types — AsyncOutcome / AsyncResponse aliases (header-only)
##############################################################################
add_library(${HTTP}.Types.AsyncOutcome INTERFACE async_outcome.hpp)

target_include_directories(${HTTP}.Types.AsyncOutcome INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Types.AsyncOutcome INTERFACE
        Boost::asio
        Menagerie::Common::Beavers
        ${HTTP}.Types.Executor
)
##############################################################################
