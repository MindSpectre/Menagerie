##############################################################################
# Test Stopwatch benchmark
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Stopwatch
        chrono-cuckoo/stopwatch/test_printing_stopwatch.cpp
        chrono-cuckoo/stopwatch/test_stopwatch.cpp
        chrono-cuckoo/stopwatch/main.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Stopwatch
        PRIVATE
        Menagerie::Common::Cuckoo
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Thread Pool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.ThreadPool
        concurrency-starling/thread_pool/test_thread_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.ThreadPool
        PRIVATE
        Menagerie::Common::Starling
        Menagerie::Common::Rabbit
        Menagerie::Common::Cuckoo
        ${TEST_LIBS}

)
##############################################################################

##############################################################################
# Test Thread Pool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Disruptor
        concurrency-starling/disruptor/test_disruptor.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Disruptor
        PRIVATE
        Menagerie::Common::Starling
        Menagerie::Common::Rabbit
        Menagerie::Common::Cuckoo
        ${TEST_LIBS}

)
##############################################################################


##############################################################################
# Test Chameleon (unit)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Chameleon
        render-chameleon/main.cpp
        render-chameleon/colors_test.cpp
        render-chameleon/separators_test.cpp
        render-chameleon/table_test.cpp
        render-chameleon/section_test.cpp
        render-chameleon/box_test.cpp
        render-chameleon/composition_test.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Chameleon
        PRIVATE
        Menagerie::Common::Chameleon
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test crow
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Crow
        logger-crow/logger_provider_test.cpp
        logger-crow/entry_tests.cpp
        logger-crow/main.cpp
        logger-crow/logger/file_sink_test.cpp
        logger-crow/logger/console_sink_test.cpp
        logger-crow/logger/logger_ordering_test.cpp
        logger-crow/logger/sink_status_test.cpp
        logger-crow/logger/sink_health_test.cpp
        logger-crow/logger/file_naming_test.cpp
        logger-crow/logger/rotation_test.cpp
        logger-crow/logger/logger_registry_test.cpp
        logger-crow/logger/logger_gate_test.cpp
        logger-crow/prefix_filter_test.cpp
        logger-crow/prefix_integration_test.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Crow
        PRIVATE
        Menagerie::Common::Crow
        ${TEST_LIBS}
)
##############################################################################


##############################################################################
#  Spider uint test
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Spider
        locator-spider/test_spider.cpp
)

target_link_libraries(${UNIT_TESTING_TARGET}.Spider
        Menagerie::Common::Spider
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Bowerbird
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Bowerbird
        algorithms-bowerbird/test_sliding_window_sorter.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Bowerbird
        PRIVATE
        Menagerie::Common::Bowerbird
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Beaver Result (Outcome)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Beaver.Outcome
        core-beaver/test_outcome.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Beaver.Outcome
        PRIVATE
        Menagerie::Common::Beaver
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Beaver InlineString
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Beaver.Strings.InlineString
        core-beaver/test_inline_string.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Beaver.Strings.InlineString
        PRIVATE
        Menagerie::Common::Beaver
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test EventCount
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.EventCount
        concurrency-starling/event_count/test_event_count.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.EventCount
        PRIVATE
        Menagerie::Common::Starling
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test EventCount - forced condition_variable fallback path
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.EventCount.Fallback
        concurrency-starling/event_count/test_event_count.cpp
)
target_compile_definitions(${UNIT_TESTING_TARGET}.EventCount.Fallback
        PRIVATE
        EVENT_COUNT_FORCE_FALLBACK
)
target_link_libraries(${UNIT_TESTING_TARGET}.EventCount.Fallback
        PRIVATE
        Menagerie::Common::Starling
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test ResourcePool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.ResourcePool
        concurrency-starling/resource_pool/test_resource_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.ResourcePool
        PRIVATE
        Menagerie::Common::Starling
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test AsyncResourcePool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.AsyncResourcePool
        concurrency-starling/resource_pool/test_async_resource_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.AsyncResourcePool
        PRIVATE
        Menagerie::Common::Starling
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Pangolin (ConfigInterface machinery + JSON field codecs)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Pangolin
        serialization-pangolin/test_config_interface.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Pangolin
        PRIVATE
        Menagerie::Common::Pangolin
        ${TEST_LIBS}
)
##############################################################################

