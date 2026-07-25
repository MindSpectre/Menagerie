##############################################################################
# Http Routing — Router (dispatch facade for drivers)
##############################################################################
add_library(${HTTP}.Routing.Router STATIC router.cpp)

target_include_directories(${HTTP}.Routing.Router PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Routing.Router
        PUBLIC
        ${HTTP}.Routing.RouteRegistry
        ${HTTP}.Types.AsyncOutcome
        Boost::asio
        PRIVATE
        ${HTTP}.Types.ErrorResponses
)
##############################################################################
