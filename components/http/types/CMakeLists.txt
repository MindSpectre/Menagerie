##############################################################################
# Http Types — protocol-agnostic core types
#
# Each type is its own leaf target with its own include directory, so headers
# cross-reference each other as <body.hpp>/<headers.hpp>/... (never ../). The
# dotted ${HTTP}.Types target below is an INTERFACE aggregate that links
# every leaf — link it when you want the whole layer. There is intentionally
# no Menagerie::Component::Http::Types alias.
##############################################################################
add_subdirectory(enums)
add_subdirectory(executor)
add_subdirectory(async_outcome)
add_subdirectory(url_decode)
add_subdirectory(http_date)
add_subdirectory(headers)
add_subdirectory(errors)
add_subdirectory(body)
add_subdirectory(response)
add_subdirectory(request)
add_subdirectory(response_factory)
add_subdirectory(request_context)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${HTTP}.Types INTERFACE)

target_link_libraries(${HTTP}.Types INTERFACE
        ${HTTP}.Types.Enums
        ${HTTP}.Types.Executor
        ${HTTP}.Types.AsyncOutcome
        ${HTTP}.Types.UrlDecode
        ${HTTP}.Types.HttpDate
        ${HTTP}.Types.Headers
        ${HTTP}.Types.Errors
        ${HTTP}.Types.ErrorResponses
        ${HTTP}.Types.Body
        ${HTTP}.Types.Response
        ${HTTP}.Types.Request
        ${HTTP}.Types.ResponseFactory
        ${HTTP}.Types.RequestContext
)
##############################################################################
