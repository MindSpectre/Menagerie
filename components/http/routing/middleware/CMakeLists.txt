##############################################################################
# Http Routing — middleware shape (header-only)
##############################################################################
add_library(${HTTP}.Routing.Middleware INTERFACE middleware.hpp)

target_include_directories(${HTTP}.Routing.Middleware INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Routing.Middleware INTERFACE
        ${HTTP}.Types.AsyncOutcome
        ${HTTP}.Types.RequestContext
)
##############################################################################
