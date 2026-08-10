##############################################################################
# Http Types — RequestContext (accessors, params, bag, ctx factories)
##############################################################################
add_library(${Albatross}.Types.RequestContext STATIC request_context.cpp)

target_include_directories(${Albatross}.Types.RequestContext PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Types.RequestContext
        PUBLIC
        ${Albatross}.Types.Request
        ${Albatross}.Types.Response
        ${Albatross}.Types.Body
        ${Albatross}.Types.Headers
        ${Albatross}.Types.Enums
        Boost::container
        PRIVATE
        ${Albatross}.Types.UrlDecode
)
##############################################################################
