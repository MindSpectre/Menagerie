##############################################################################
# Test Http Routing layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Albatross.Routing
        routing/test_route_registry.cpp
        routing/test_controller.cpp
        routing/test_middleware.cpp
        routing/test_group.cpp
        routing/test_router.cpp
        routing/test_routing_allocation_gate.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Albatross.Routing
        PRIVATE
        Menagerie.Component.Albatross.Routing
        Menagerie.Component.Albatross.Types
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Http Connection layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Albatross.Connection
        connection/test_request_arena.cpp
        connection/test_connection_concepts.cpp
        connection/test_tcp_connection.cpp
        connection/test_tls_connection.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Albatross.Connection
        PRIVATE
        Menagerie.Component.Albatross.Connection
        Menagerie.Component.Albatross.Types
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Http Types layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Albatross.Types
        types/test_http_enums.cpp
        types/test_url_decode.cpp
        types/test_http_date.cpp
        types/test_headers.cpp
        types/test_body.cpp
        types/test_response.cpp
        types/test_response_factory.cpp
        types/test_errors.cpp
        types/test_request_context.cpp
        types/test_allocation_gate.cpp
)
# Dotted aggregate of all the per-type Http Types leaf targets (no :: alias).
# Deliberately NOT the Menagerie.Component.Albatross umbrella: per-layer links
# catch under-linking regressions in each layer's CMake (PR 7 D4); umbrella
# propagation is guarded by the dedicated ...Albatross.Umbrella target below.
target_link_libraries(${UNIT_TESTING_TARGET}.Albatross.Types
        PRIVATE
        Menagerie.Component.Albatross.Types
        Menagerie::Common::Cuckoo
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Http Drivers layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Albatross.Drivers
        drivers/test_driver_scaffolds.cpp
        drivers/test_driver_helpers.cpp
        drivers/test_http11_driver.cpp
        drivers/test_driver_allocation_gate.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Albatross.Drivers
        PRIVATE
        Menagerie.Component.Albatross.Drivers
        Menagerie.Component.Albatross.Connection
        Menagerie.Component.Albatross.Routing
        Menagerie.Component.Albatross.Types
        Boost::beast
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Http Listeners layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Albatross.Listeners
        listeners/test_connection_tracker.cpp
        listeners/test_tcp_listener.cpp
        listeners/test_build_ssl_context.cpp
        listeners/test_tls_listener.cpp
        listeners/test_quic_listener.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Albatross.Listeners
        PRIVATE
        ${SHARED_TESTING_TARGET}.Albatross
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Http Config layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Albatross.Config
        config/test_timeouts.cpp
        config/test_tls_config.cpp
        config/test_listener_config.cpp
        config/test_server_config.cpp
        config/test_load_server_config.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Albatross.Config
        PRIVATE
        Menagerie.Component.Albatross.Config
        Menagerie.Component.Albatross.Types
        JsonCpp::JsonCpp
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Http Server layer (unit — attach_default_listeners; the Server
# lifecycle itself is integration-tested)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Albatross.Server
        server/test_attach_default_listeners.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Albatross.Server
        PRIVATE
        Menagerie.Component.Albatross.Server
        Menagerie.Component.Albatross.Config
        Menagerie.Component.Albatross.Drivers
        Menagerie.Component.Albatross.Types
        Boost::asio
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test the public umbrella (PR 7): <menagerie/http> + the combined
# Menagerie.Component.Albatross library must expose every layer with NO other link.
# This target deliberately links nothing else (gtest aside) — it is the only
# place the consumer-facing contract (one include, one library) is compiled.
# It does NOT isolate per-layer LIBRARIES drops: the layer aggregates
# PUBLIC-link each other, so only .Connection and .Server are load-bearing
# here (see the measured caveat in umbrella/test_umbrella.cpp). Per-layer
# under-linking is guarded by the per-layer dotted links on the targets above (D4).
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Albatross.Umbrella
        umbrella/test_umbrella.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Albatross.Umbrella
        PRIVATE
        Menagerie.Component.Albatross
        ${TEST_LIBS}
)
##############################################################################
