##############################################################################
# Http Listeners — ListenerBase (abstract interface; header-only)
##############################################################################
add_library(${Albatross}.Listeners.Base INTERFACE listener_base.hpp)

target_include_directories(${Albatross}.Listeners.Base INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Listeners.Base INTERFACE
        ${Albatross}.Routing.Router
        Boost::asio
)
##############################################################################
