##############################################################################
# Http Types — Body (value-type SBO type-erasure)
##############################################################################
add_library(${HTTP}.Types.Body STATIC body.cpp)

target_include_directories(${HTTP}.Types.Body PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Types.Body
        PUBLIC
        ${HTTP}.Types.AsyncOutcome
        ${HTTP}.Types.Errors
        Boost::asio
        Menagerie::Common::Beavers
        JsonCpp::JsonCpp
        PRIVATE
        ${HTTP}.Types.UrlDecode
)
##############################################################################
