##############################################################################
# Http Routing — registry, controllers, bake step, groups, router
#
# Same convention as types/: each thing is its own leaf target owning its
# include dir (headers cross-reference as <route_registry.hpp>, never ../).
# The dotted ${HTTP}.Routing target below is an INTERFACE aggregate.
##############################################################################
add_subdirectory(middleware)
add_subdirectory(route_registry)
add_subdirectory(controller)
add_subdirectory(group)
add_subdirectory(router)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${HTTP}.Routing INTERFACE)

target_link_libraries(${HTTP}.Routing INTERFACE
        ${HTTP}.Routing.Middleware
        ${HTTP}.Routing.RouteRegistry
        ${HTTP}.Routing.Controller
        ${HTTP}.Routing.Group
        ${HTTP}.Routing.Router
)
##############################################################################
