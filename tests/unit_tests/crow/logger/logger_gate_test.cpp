#include <atomic>
#include <format>
#include <menagerie/crow>
#include <thread>

#include "counting_sink.hpp"

#include <gtest/gtest.h>

using namespace menagerie::crow;
using crow_test::CountingSink;

namespace {
    /// Instrumented log() argument: its std::formatter specialization below bumps
    /// format_calls, so a test can observe directly whether std::format_to ever ran
    /// for a given log() call -- not just infer it from what a sink received, which
    /// the consumer-side threshold check (added in the same change as the producer
    /// gate) can make look identical either way.
    struct FormatTracker {
        static inline std::atomic<int> format_calls{0};
    };
}  // namespace

template <>
struct std::formatter<FormatTracker> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const FormatTracker&, FormatContext& ctx) const {
        FormatTracker::format_calls.fetch_add(1, std::memory_order_relaxed);
        return std::formatter<std::string_view>::format("tracked", ctx);
    }
};

TEST(LoggerGateTest, EmptyLoggerDropsEverything) {
    const Logger logger;
    EXPECT_EQ(logger.gate_threshold(), detail::drop_all_threshold);
}

TEST(LoggerGateTest, GateTracksTheLowestRegisteredThreshold) {
    Logger logger;

    auto strict = std::make_shared<CountingSink>(ERR);
    logger.add_sink(strict);
    EXPECT_EQ(logger.gate_threshold(), static_cast<std::uint8_t>(ERR));

    auto chatty = std::make_shared<CountingSink>(DBG);
    logger.add_sink(chatty);
    EXPECT_EQ(logger.gate_threshold(), static_cast<std::uint8_t>(DBG));

    ASSERT_TRUE(logger.remove_sink(chatty));
    EXPECT_EQ(logger.gate_threshold(), static_cast<std::uint8_t>(ERR));

    logger.shutdown();
}

TEST(LoggerGateTest, EventsBelowTheGateReachNoSink) {
    Logger logger;
    auto sink = std::make_shared<CountingSink>(ERR);
    logger.add_sink(sink);

    for (int i = 0; i < 1000; ++i) {
        logger.log(DBG, "below every threshold");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(sink->events(), 0U);
    EXPECT_EQ(sink->batches(), 0U);

    logger.shutdown();
}

TEST(LoggerGateTest, EventsBelowTheGateNeverReachFormat) {
    Logger logger;
    auto sink = std::make_shared<CountingSink>(ERR);
    logger.add_sink(sink);

    FormatTracker::format_calls.store(0, std::memory_order_relaxed);
    const FormatTracker tracker;

    // Below the gate (ERR): the producer must return before std::format_to runs.
    logger.log(DBG, std::string_view{}, std::source_location::current(), "below the gate: {}", tracker);
    EXPECT_EQ(FormatTracker::format_calls.load(std::memory_order_relaxed), 0)
        << "std::format_to ran for an event no sink accepts";

    // At the gate (ERR): formatting must actually happen, proving the instrument fires.
    logger.log(ERR, std::string_view{}, std::source_location::current(), "at the gate: {}", tracker);
    EXPECT_EQ(FormatTracker::format_calls.load(std::memory_order_relaxed), 1)
        << "instrument never fired even for an event that clears the gate";

    logger.shutdown();
}

TEST(LoggerGateTest, BatchesSkipSinksWhoseThresholdNothingClears) {
    Logger logger;
    auto chatty = std::make_shared<CountingSink>(DBG);
    auto strict = std::make_shared<CountingSink>(FAT);
    logger.add_sink(chatty);
    logger.add_sink(strict);

    for (int i = 0; i < 100; ++i) {
        logger.log(DBG, "passes the gate, clears only the chatty sink");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_GT(chatty->events(), 0U);
    EXPECT_EQ(strict->batches(), 0U);  // never posted: nothing in the batch clears FAT

    logger.shutdown();
}

TEST(LoggerGateTest, StreamProxyIsSilentBelowTheGate) {
    Logger logger;
    auto sink = std::make_shared<CountingSink>(ERR);
    logger.add_sink(sink);

    logger.stream(DBG, "prefix") << "never published";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(sink->events(), 0U);

    logger.shutdown();
}
