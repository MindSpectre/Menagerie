##############################################################################
# Http Connection — Connection / StreamConnection concepts (header-only)
##############################################################################
add_library(${Albatross}.Connection.Concepts INTERFACE
        connection_concepts.hpp
)

target_include_directories(${Albatross}.Connection.Concepts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Connection.Concepts INTERFACE
        ${Albatross}.Types.Enums
        ${Albatross}.Types.Executor
        Boost::asio
)
##############################################################################
