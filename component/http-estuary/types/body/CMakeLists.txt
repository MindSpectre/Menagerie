##############################################################################
# Http Types — Body (value-type SBO type-erasure)
##############################################################################
add_library(${Estuary}.Types.Body STATIC body.cpp)

target_include_directories(${Estuary}.Types.Body PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Types.Body
        PUBLIC
        ${Estuary}.Types.AsyncOutcome
        ${Estuary}.Types.Errors
        Boost::asio
        Menagerie::Common::Beaver
        JsonCpp::JsonCpp
        PRIVATE
        ${Estuary}.Types.UrlDecode
)
##############################################################################
