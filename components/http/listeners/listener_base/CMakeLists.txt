##############################################################################
# Http Listeners — ListenerBase (abstract interface; header-only)
##############################################################################
add_library(${HTTP}.Listeners.Base INTERFACE listener_base.hpp)

target_include_directories(${HTTP}.Listeners.Base INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Listeners.Base INTERFACE
        ${HTTP}.Routing.Router
        Boost::asio
)
##############################################################################
