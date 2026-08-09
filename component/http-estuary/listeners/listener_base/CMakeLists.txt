##############################################################################
# Http Listeners — ListenerBase (abstract interface; header-only)
##############################################################################
add_library(${Estuary}.Listeners.Base INTERFACE listener_base.hpp)

target_include_directories(${Estuary}.Listeners.Base INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Listeners.Base INTERFACE
        ${Estuary}.Routing.Router
        Boost::asio
)
##############################################################################
