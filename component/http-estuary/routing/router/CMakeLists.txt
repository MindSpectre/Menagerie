##############################################################################
# Http Routing — Router (dispatch facade for drivers)
##############################################################################
add_library(${Estuary}.Routing.Router STATIC router.cpp)

target_include_directories(${Estuary}.Routing.Router PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Routing.Router
        PUBLIC
        ${Estuary}.Routing.RouteRegistry
        ${Estuary}.Types.AsyncOutcome
        Boost::asio
        PRIVATE
        ${Estuary}.Types.ErrorResponses
)
##############################################################################
