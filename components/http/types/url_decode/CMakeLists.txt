##############################################################################
# Http Types — shared RFC 3986 url_decode
##############################################################################
add_library(${Http}.Types.UrlDecode STATIC url_decode.cpp)

target_include_directories(${Http}.Types.UrlDecode PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
