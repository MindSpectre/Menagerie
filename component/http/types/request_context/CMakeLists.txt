##############################################################################
# Http Types — RequestContext (accessors, params, bag, ctx factories)
##############################################################################
add_library(${Http}.Types.RequestContext STATIC request_context.cpp)

target_include_directories(${Http}.Types.RequestContext PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Types.RequestContext
        PUBLIC
        ${Http}.Types.Request
        ${Http}.Types.Response
        ${Http}.Types.Body
        ${Http}.Types.Headers
        ${Http}.Types.Enums
        Boost::container
        PRIVATE
        ${Http}.Types.UrlDecode
)
##############################################################################
