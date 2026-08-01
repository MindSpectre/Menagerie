##############################################################################
# Http Types — Body (value-type SBO type-erasure)
##############################################################################
add_library(${Http}.Types.Body STATIC body.cpp)

target_include_directories(${Http}.Types.Body PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Types.Body
        PUBLIC
        ${Http}.Types.AsyncOutcome
        ${Http}.Types.Errors
        Boost::asio
        Menagerie::Common::Beavers
        JsonCpp::JsonCpp
        PRIVATE
        ${Http}.Types.UrlDecode
)
##############################################################################
