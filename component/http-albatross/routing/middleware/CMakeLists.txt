##############################################################################
# Http Routing — middleware shape (header-only)
##############################################################################
add_library(${Albatross}.Routing.Middleware INTERFACE middleware.hpp)

target_include_directories(${Albatross}.Routing.Middleware INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Routing.Middleware INTERFACE
        ${Albatross}.Types.AsyncOutcome
        ${Albatross}.Types.RequestContext
)
##############################################################################
