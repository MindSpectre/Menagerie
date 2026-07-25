##############################################################################
# Http Server — ServerObserver (single typed observer interface, header-only)
##############################################################################
add_library(${HTTP}.Server.Observer INTERFACE server_observer.hpp)

target_include_directories(${HTTP}.Server.Observer INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Server.Observer INTERFACE
        ${HTTP}.Routing.Router
        ${HTTP}.Types
        Boost::asio
)
##############################################################################
