##############################################################################
# Http Routing — Router (dispatch facade for drivers)
##############################################################################
add_library(${Albatross}.Routing.Router STATIC router.cpp)

target_include_directories(${Albatross}.Routing.Router PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Routing.Router
        PUBLIC
        ${Albatross}.Routing.RouteRegistry
        ${Albatross}.Types.AsyncOutcome
        Boost::asio
        PRIVATE
        ${Albatross}.Types.ErrorResponses
)
##############################################################################
