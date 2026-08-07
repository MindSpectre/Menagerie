##############################################################################
# Http Types — protocol-agnostic core types
#
# Each type is its own leaf target with its own include directory, so headers
# cross-reference each other as <body.hpp>/<headers.hpp>/... (never ../). The
# dotted ${Http}.Types target below is an INTERFACE aggregate that links
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
add_library(${Http}.Types INTERFACE)

target_link_libraries(${Http}.Types INTERFACE
        ${Http}.Types.Enums
        ${Http}.Types.Executor
        ${Http}.Types.AsyncOutcome
        ${Http}.Types.UrlDecode
        ${Http}.Types.HttpDate
        ${Http}.Types.Headers
        ${Http}.Types.Errors
        ${Http}.Types.ErrorResponses
        ${Http}.Types.Body
        ${Http}.Types.Response
        ${Http}.Types.Request
        ${Http}.Types.ResponseFactory
        ${Http}.Types.RequestContext
)
##############################################################################
