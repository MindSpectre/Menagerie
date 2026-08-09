##############################################################################
# Http Server — ServerObserver (single typed observer interface, header-only)
##############################################################################
add_library(${Albatross}.Server.Observer INTERFACE server_observer.hpp)

target_include_directories(${Albatross}.Server.Observer INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Server.Observer INTERFACE
        ${Albatross}.Routing.Router
        ${Albatross}.Types
        Boost::asio
)
##############################################################################
