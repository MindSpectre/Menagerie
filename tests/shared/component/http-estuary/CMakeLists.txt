add_library(${SHARED_TESTING_TARGET}.Estuary INTERFACE
        http_test_fixture.hpp
        server_test_fixture.hpp
        test_tls_cert.hpp
        tls_client.hpp
)

target_include_directories(${SHARED_TESTING_TARGET}.Estuary INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SHARED_TESTING_TARGET}.Estuary INTERFACE
        Menagerie.Component.Estuary.Listeners
        Menagerie.Component.Estuary.Drivers
        Menagerie.Component.Estuary.Routing
        Menagerie.Component.Estuary.Connection
        Menagerie.Component.Estuary.Types
        Menagerie.Component.Estuary.Config
)