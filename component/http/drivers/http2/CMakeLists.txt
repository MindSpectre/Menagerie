##############################################################################
# Http Drivers — Http2Driver (SCAFFOLD; nghttp2 not linked yet)
##############################################################################
add_library(${Http}.Drivers.Http2 INTERFACE
        http2_driver.hpp
)

target_include_directories(${Http}.Drivers.Http2 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Drivers.Http2 INTERFACE
        ${Http}.Drivers.Concept
        ${Http}.Connection.Concepts
        ${Http}.Routing.Router
        Menagerie::Common::Crow
)
##############################################################################
