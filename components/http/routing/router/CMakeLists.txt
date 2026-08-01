##############################################################################
# Http Routing — Router (dispatch facade for drivers)
##############################################################################
add_library(${Http}.Routing.Router STATIC router.cpp)

target_include_directories(${Http}.Routing.Router PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Routing.Router
        PUBLIC
        ${Http}.Routing.RouteRegistry
        ${Http}.Types.AsyncOutcome
        Boost::asio
        PRIVATE
        ${Http}.Types.ErrorResponses
)
##############################################################################
