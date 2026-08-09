##############################################################################
# Http Types — Body (value-type SBO type-erasure)
##############################################################################
add_library(${Albatross}.Types.Body STATIC body.cpp)

target_include_directories(${Albatross}.Types.Body PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Types.Body
        PUBLIC
        ${Albatross}.Types.AsyncOutcome
        ${Albatross}.Types.Errors
        Boost::asio
        Menagerie::Common::Beaver
        JsonCpp::JsonCpp
        PRIVATE
        ${Albatross}.Types.UrlDecode
)
##############################################################################
