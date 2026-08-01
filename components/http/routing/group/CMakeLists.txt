##############################################################################
# Http Routing — GroupBinding (header-only)
##############################################################################
add_library(${Http}.Routing.Group INTERFACE group.hpp)

target_include_directories(${Http}.Routing.Group INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Routing.Group INTERFACE
        ${Http}.Routing.Controller
        ${Http}.Routing.RouteRegistry
)
##############################################################################
