##############################################################################
# Http Types — Headers (allocator-bound tagged union)
##############################################################################
add_library(${Http}.Types.Headers STATIC headers.cpp)

target_include_directories(${Http}.Types.Headers PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Types.Headers PUBLIC
        Boost::beast
        Boost::container
        Menagerie::Common::Beavers
)
##############################################################################
