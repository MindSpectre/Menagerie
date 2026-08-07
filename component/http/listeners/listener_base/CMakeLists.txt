##############################################################################
# Http Listeners — ListenerBase (abstract interface; header-only)
##############################################################################
add_library(${Http}.Listeners.Base INTERFACE listener_base.hpp)

target_include_directories(${Http}.Listeners.Base INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Listeners.Base INTERFACE
        ${Http}.Routing.Router
        Boost::asio
)
##############################################################################
