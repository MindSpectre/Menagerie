##############################################################################
# Http Server — ServerObserver (single typed observer interface, header-only)
##############################################################################
add_library(${Estuary}.Server.Observer INTERFACE server_observer.hpp)

target_include_directories(${Estuary}.Server.Observer INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Server.Observer INTERFACE
        ${Estuary}.Routing.Router
        ${Estuary}.Types
        Boost::asio
)
##############################################################################
