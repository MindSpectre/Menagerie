##############################################################################
# Http Routing — RouteRegistry (exact + parametric route table)
##############################################################################
add_library(${Http}.Routing.RouteRegistry STATIC route_registry.cpp)

target_include_directories(${Http}.Routing.RouteRegistry PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Routing.RouteRegistry
        PUBLIC
        ${Http}.Routing.Middleware
        ${Http}.Types.Enums
        ${Http}.Types.Errors
        Menagerie::Common::Beavers
        Boost::container
        Boost::unordered
        PRIVATE
        ${Http}.Types.UrlDecode
)
##############################################################################
