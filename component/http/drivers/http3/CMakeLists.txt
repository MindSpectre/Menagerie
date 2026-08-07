##############################################################################
# Http Drivers — Http3Driver (SCAFFOLD; ngtcp2 + nghttp3 not linked yet)
##############################################################################
add_library(${Http}.Drivers.Http3 INTERFACE
        http3_driver.hpp
)

target_include_directories(${Http}.Drivers.Http3 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Drivers.Http3 INTERFACE
        ${Http}.Drivers.Concept
        ${Http}.Connection.Concepts
        ${Http}.Routing.Router
        Menagerie::Common::Crow
)
##############################################################################
