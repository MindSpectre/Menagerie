##############################################################################
# Http Routing — GroupBinding (header-only)
##############################################################################
add_library(${HTTP}.Routing.Group INTERFACE group.hpp)

target_include_directories(${HTTP}.Routing.Group INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Routing.Group INTERFACE
        ${HTTP}.Routing.Controller
        ${HTTP}.Routing.RouteRegistry
)
##############################################################################
