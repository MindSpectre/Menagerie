##############################################################################
# Http Routing — RouteRegistry (exact + parametric route table)
##############################################################################
add_library(${HTTP}.Routing.RouteRegistry STATIC route_registry.cpp)

target_include_directories(${HTTP}.Routing.RouteRegistry PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Routing.RouteRegistry
        PUBLIC
        ${HTTP}.Routing.Middleware
        ${HTTP}.Types.Enums
        ${HTTP}.Types.Errors
        Menagerie::Common::Beavers
        Boost::container
        Boost::unordered
        PRIVATE
        ${HTTP}.Types.UrlDecode
)
##############################################################################
