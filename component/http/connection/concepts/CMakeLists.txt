##############################################################################
# Http Connection — Connection / StreamConnection concepts (header-only)
##############################################################################
add_library(${Http}.Connection.Concepts INTERFACE
        connection_concepts.hpp
)

target_include_directories(${Http}.Connection.Concepts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Connection.Concepts INTERFACE
        ${Http}.Types.Enums
        ${Http}.Types.Executor
        Boost::asio
)
##############################################################################
