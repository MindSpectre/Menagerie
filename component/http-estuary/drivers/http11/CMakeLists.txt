##############################################################################
# Http Drivers — Http11Driver (real Http/1.1 over Boost.Beast)
##############################################################################
add_library(${Estuary}.Drivers.Http11 STATIC
        http11_driver.cpp
        http11_driver.hpp
        http11_config.hpp
)

target_include_directories(${Estuary}.Drivers.Http11 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Drivers.Http11
        PUBLIC
        ${Estuary}.Types
        ${Estuary}.Routing.Router
        ${Estuary}.Connection.Concepts
        Boost::beast
        Boost::asio
)
##############################################################################
