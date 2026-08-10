##############################################################################
# Http Server — run_standalone ("Http owns the process" convenience)
##############################################################################
add_library(${Albatross}.Server.RunStandalone STATIC run_standalone.cpp)

target_include_directories(${Albatross}.Server.RunStandalone PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Server.RunStandalone
        PUBLIC
        ${Albatross}.Server.Core
        Boost::asio
)
##############################################################################
