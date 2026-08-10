##############################################################################
# Http Types — shared RFC 3986 url_decode
##############################################################################
add_library(${Albatross}.Types.UrlDecode STATIC url_decode.cpp)

target_include_directories(${Albatross}.Types.UrlDecode PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
