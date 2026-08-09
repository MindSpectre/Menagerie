##############################################################################
# Http Drivers — Http3Driver (SCAFFOLD; ngtcp2 + nghttp3 not linked yet)
##############################################################################
add_library(${Estuary}.Drivers.Http3 INTERFACE
        http3_driver.hpp
)

target_include_directories(${Estuary}.Drivers.Http3 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Drivers.Http3 INTERFACE
        ${Estuary}.Drivers.Concept
        ${Estuary}.Connection.Concepts
        ${Estuary}.Routing.Router
        Menagerie::Common::Crow
)
##############################################################################
