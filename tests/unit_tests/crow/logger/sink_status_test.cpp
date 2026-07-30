#include <menagerie/crow>

#include <gtest/gtest.h>

namespace {
    using namespace menagerie::crow;

    /// Minimal concrete sink: exposes the protected health API so the base-class
    /// bookkeeping can be driven directly.
    class ProbeSink final : public Sink {
    public:
        void process(const LogEvent&) noexcept override {
        }
        void flush() noexcept override {
        }

        [[nodiscard]] bool should_log(LogLevel, std::string_view) const noexcept override {
            return true;
        }

        using Sink::note_failure;
        using Sink::note_success;
        using Sink::publish_threshold;
        using Sink::set_status;
    };
}  // namespace

TEST(SinkStatusTest, PackRoundTripsAllFields) {
    constexpr std::uint64_t retry_at = 1'234'567'890;
    const auto word                  = detail::pack_dispatch(SinkStatus::Degraded, LogLevel::Warning, retry_at);
    const auto hint                  = detail::unpack_dispatch(word);

    EXPECT_EQ(hint.status, SinkStatus::Degraded);
    EXPECT_EQ(hint.threshold, LogLevel::Warning);
    EXPECT_EQ(hint.retry_at_ms, retry_at);
}

// Compile-time proof that the schedule still folds: backoff_ms delegates to
// chrono::exponential_backoff, and a non-constexpr regression there would silently
// move this arithmetic to runtime on a path the janitor walks per sweep.
static_assert(menagerie::crow::detail::backoff_ms(0) == 0U);
static_assert(menagerie::crow::detail::backoff_ms(1) == 1000U);
static_assert(menagerie::crow::detail::backoff_ms(6) == 32000U);
static_assert(menagerie::crow::detail::backoff_ms(7) == 60000U);

TEST(SinkStatusTest, BackoffDoublesThenCaps) {
    EXPECT_EQ(detail::backoff_ms(0), 0U);
    EXPECT_EQ(detail::backoff_ms(1), 1000U);
    EXPECT_EQ(detail::backoff_ms(2), 2000U);
    EXPECT_EQ(detail::backoff_ms(6), 32000U);
    EXPECT_EQ(detail::backoff_ms(7), 60000U);
    EXPECT_EQ(detail::backoff_ms(99), 60000U);
}

TEST(SinkStatusTest, NewSinkIsHealthyAndAcceptsEverything) {
    const ProbeSink sink;
    const auto hint = sink.dispatch_hint();

    EXPECT_EQ(hint.status, SinkStatus::Healthy);
    EXPECT_EQ(hint.threshold, LogLevel::Trace);
    EXPECT_EQ(sink.undelivered(), 0U);
    EXPECT_TRUE(sink.last_error().empty());
}

TEST(SinkStatusTest, StatusChangeRecordsReasonAndPreservesThreshold) {
    ProbeSink sink;
    sink.publish_threshold(LogLevel::Error);
    sink.set_status(SinkStatus::Dead, "cannot open /nope/app.log");

    const auto hint = sink.dispatch_hint();
    EXPECT_EQ(hint.status, SinkStatus::Dead);
    EXPECT_EQ(hint.threshold, LogLevel::Error);  // threshold survives a status change
    EXPECT_EQ(sink.last_error(), "cannot open /nope/app.log");
}

TEST(SinkStatusTest, FailurePushesRetryDeadlineOutAndSuccessClearsIt) {
    ProbeSink sink;
    const auto before = detail::steady_now_ms();

    sink.note_failure();  // first failure: zero delay, so it is already due
    const auto first = sink.dispatch_hint();
    EXPECT_TRUE(retry_due(first, before));

    sink.note_failure();  // second failure: backoff now applies, pushing the deadline out
    const auto second = sink.dispatch_hint();
    EXPECT_GE(second.retry_at_ms, before + detail::backoff_ms(1));
    EXPECT_FALSE(retry_due(second, before));

    sink.note_success();
    const auto cleared = sink.dispatch_hint();
    EXPECT_EQ(cleared.retry_at_ms, 0U);
    EXPECT_TRUE(retry_due(cleared, before));
}

TEST(SinkStatusTest, UndeliveredAccumulates) {
    ProbeSink sink;
    sink.add_undelivered(4);
    sink.add_undelivered(6);
    EXPECT_EQ(sink.undelivered(), 10U);
}
