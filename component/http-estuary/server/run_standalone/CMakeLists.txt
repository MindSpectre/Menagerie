##############################################################################
# Http Server — run_standalone ("Http owns the process" convenience)
##############################################################################
add_library(${Estuary}.Server.RunStandalone STATIC run_standalone.cpp)

target_include_directories(${Estuary}.Server.RunStandalone PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Server.RunStandalone
        PUBLIC
        ${Estuary}.Server.Core
        Boost::asio
)
##############################################################################
