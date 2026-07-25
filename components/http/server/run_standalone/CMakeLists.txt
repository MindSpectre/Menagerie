##############################################################################
# Http Server — run_standalone ("HTTP owns the process" convenience)
##############################################################################
add_library(${HTTP}.Server.RunStandalone STATIC run_standalone.cpp)

target_include_directories(${HTTP}.Server.RunStandalone PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Server.RunStandalone
        PUBLIC
        ${HTTP}.Server.Core
        Boost::asio
)
##############################################################################
