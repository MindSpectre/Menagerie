##############################################################################
# Http Connection — Connection / StreamConnection concepts (header-only)
##############################################################################
add_library(${HTTP}.Connection.Concepts INTERFACE
        connection_concepts.hpp
)

target_include_directories(${HTTP}.Connection.Concepts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Connection.Concepts INTERFACE
        ${HTTP}.Types.Enums
        ${HTTP}.Types.Executor
        Boost::asio
)
##############################################################################
