##############################################################################
# Http Routing — GroupBinding (header-only)
##############################################################################
add_library(${Albatross}.Routing.Group INTERFACE group.hpp)

target_include_directories(${Albatross}.Routing.Group INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Routing.Group INTERFACE
        ${Albatross}.Routing.Controller
        ${Albatross}.Routing.RouteRegistry
)
##############################################################################
