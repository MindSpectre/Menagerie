##############################################################################
# Http Types — AsyncOutcome / AsyncResponse aliases (header-only)
##############################################################################
add_library(${Http}.Types.AsyncOutcome INTERFACE async_outcome.hpp)

target_include_directories(${Http}.Types.AsyncOutcome INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Types.AsyncOutcome INTERFACE
        Boost::asio
        Menagerie::Common::Beavers
        ${Http}.Types.Executor
)
##############################################################################
