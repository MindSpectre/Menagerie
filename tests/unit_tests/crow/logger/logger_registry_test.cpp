#include <atomic>
#include <menagerie/crow>
#include <sstream>
#include <thread>

#include "counting_sink.hpp"

#include <gtest/gtest.h>

using namespace menagerie::crow;
using crow_test::CountingSink;

TEST(LoggerRegistryTest, RemoveSinkStopsDelivery) {
    Logger logger;
    auto sink = std::make_shared<CountingSink>(TRC);
    logger.add_sink(sink);

    logger.log(INF, "before removal");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto delivered = sink->events();
    ASSERT_GT(delivered, 0U);

    EXPECT_TRUE(logger.remove_sink(sink));
    EXPECT_FALSE(logger.remove_sink(sink));  // second removal is a no-op

    logger.log(INF, "after removal");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(sink->events(), delivered);

    logger.shutdown();
}

TEST(LoggerRegistryTest, SinkReportSurfacesStatusAndCounters) {
    Logger logger;
    auto sink = std::make_shared<CountingSink>(TRC);
    logger.add_sink(sink);

    const auto report = logger.sink_report();

    ASSERT_EQ(report.size(), 1U);
    EXPECT_EQ(report[0].sink, sink);
    EXPECT_EQ(report[0].status, SinkStatus::Healthy);
    EXPECT_EQ(report[0].undelivered, 0U);
    EXPECT_TRUE(report[0].last_error.empty());

    logger.shutdown();
}

TEST(LoggerRegistryTest, ConcurrentAddAndRemoveWhileLogging) {
    Logger logger;
    auto permanent = std::make_shared<CountingSink>(TRC);
    logger.add_sink(permanent);

    std::atomic<bool> stop{false};
    std::vector<std::thread> producers;
    producers.reserve(8);
    for (int t = 0; t < 8; ++t) {
        producers.emplace_back([&logger, &stop] {
            while (!stop.load(std::memory_order_relaxed)) {
                logger.log(INF, "concurrent");
            }
        });
    }

    for (int i = 0; i < 200; ++i) {
        auto churn = std::make_shared<CountingSink>(TRC);
        logger.add_sink(churn);
        EXPECT_TRUE(logger.remove_sink(churn));
    }

    stop.store(true, std::memory_order_relaxed);
    for (auto& producer : producers) {
        producer.join();
    }

    logger.shutdown();
    EXPECT_GT(permanent->events(), 0U);
}

TEST(LoggerConfigDefaultsTest, PoolSizeDefaultsToTwo) {
    const auto config = LoggerConfig::Builder{}.finalize();
    EXPECT_EQ(config.pool_size(), 2U);
    EXPECT_EQ(config.health_check_interval(), std::chrono::milliseconds{1000});
}
