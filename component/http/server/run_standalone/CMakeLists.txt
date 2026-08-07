##############################################################################
# Http Server — run_standalone ("Http owns the process" convenience)
##############################################################################
add_library(${Http}.Server.RunStandalone STATIC run_standalone.cpp)

target_include_directories(${Http}.Server.RunStandalone PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Server.RunStandalone
        PUBLIC
        ${Http}.Server.Core
        Boost::asio
)
##############################################################################
