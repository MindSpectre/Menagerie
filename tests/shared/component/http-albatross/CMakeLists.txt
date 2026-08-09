add_library(${SHARED_TESTING_TARGET}.Albatross INTERFACE
        http_test_fixture.hpp
        server_test_fixture.hpp
        test_tls_cert.hpp
        tls_client.hpp
)

target_include_directories(${SHARED_TESTING_TARGET}.Albatross INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SHARED_TESTING_TARGET}.Albatross INTERFACE
        Menagerie.Component.Albatross.Listeners
        Menagerie.Component.Albatross.Drivers
        Menagerie.Component.Albatross.Routing
        Menagerie.Component.Albatross.Connection
        Menagerie.Component.Albatross.Types
        Menagerie.Component.Albatross.Config
)