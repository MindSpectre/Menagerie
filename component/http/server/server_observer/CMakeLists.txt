##############################################################################
# Http Server — ServerObserver (single typed observer interface, header-only)
##############################################################################
add_library(${Http}.Server.Observer INTERFACE server_observer.hpp)

target_include_directories(${Http}.Server.Observer INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Server.Observer INTERFACE
        ${Http}.Routing.Router
        ${Http}.Types
        Boost::asio
)
##############################################################################
