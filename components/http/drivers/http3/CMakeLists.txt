##############################################################################
# Http Drivers — Http3Driver (SCAFFOLD; ngtcp2 + nghttp3 not linked yet)
##############################################################################
add_library(${HTTP}.Drivers.Http3 INTERFACE
        http3_driver.hpp
)

target_include_directories(${HTTP}.Drivers.Http3 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Drivers.Http3 INTERFACE
        ${HTTP}.Drivers.Concept
        ${HTTP}.Connection.Concepts
        ${HTTP}.Routing.Router
        Menagerie::Common::Crow
)
##############################################################################
